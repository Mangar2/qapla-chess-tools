/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2026 Volker Böhm
 */

#include "book-prune.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "book-file.h"
#include "game-file.h"
#include "pgn-reader.h"

namespace GameFile {

	namespace {

		struct Counters {
			uint64_t read = 0;
			uint64_t throughLeaf = 0;
			uint64_t leafAgain = 0;
			uint64_t outsideBook = 0;
		};

		bool isGameFile(const std::string& path) {
			std::ifstream stream(path, std::ios::binary);
			char magic[8] = {};
			stream.read(magic, sizeof(magic));
			return stream && std::memcmp(magic, "QAPLAGM2", sizeof(magic)) == 0;
		}

		/** Follows a game through the book and marks the leaf it reaches. */
		void followGame(const Book& book, const std::vector<Move>& moves, std::vector<uint8_t>& played,
			Counters& counters) {
			++counters.read;
			Board board;
			Book::NodeIndex node = Book::ROOT;
			size_t ply = 0;
			for (; ply < moves.size(); ++ply) {
				const std::optional<uint32_t> code = packMove9(moves[ply], board);
				const Book::NodeIndex child = code ? book.findMove(node, *code) : Book::NO_NODE;
				if (child == Book::NO_NODE) break;
				board.apply(moves[ply]);
				node = child;
			}
			if (ply == 0) {
				++counters.outsideBook;
				return;
			}
			if (book.node(node).firstChild != Book::NO_NODE) return;
			++counters.throughLeaf;
			if (played[node]) ++counters.leafAgain;
			played[node] = 1;
		}

		void readPgn(const std::string& path, const Book& book, std::vector<uint8_t>& played, Counters& counters) {
			uint64_t badComments = 0;
			std::vector<Move> moves;
			readPgnGames(path, [&](const PgnTags& tags, const std::string& movetext) {
				const std::vector<PgnMove> parsed = parseMovetext(movetext, badComments);
				if (parsed.empty()) return true;
				moves.clear();
				// A game from a fen does not start at the root of the book.
				if (tags.count("FEN") == 0) {
					for (const PgnMove& move : parsed) {
						moves.push_back(Move{ .from = move.from, .to = move.to, .promotion = move.promotion });
					}
				}
				followGame(book, moves, played, counters);
				return true;
			});
		}

		void readGameFile(const std::string& path, const Book& book, std::vector<uint8_t>& played,
			Counters& counters) {
			Reader reader(path);
			const uint32_t version = reader.format().version;
			Game game;
			std::vector<Move> moves;
			while (reader.next(game)) {
				moves.clear();
				Board board;
				for (const Record& record : game) {
					moves.push_back(unpackMove(record.move, board, version));
					board.apply(moves.back());
				}
				followGame(book, moves, played, counters);
			}
		}
	}

	uint64_t BookPrune::convert(const std::string& bookPath, const std::string& outPath) const {
		Book book;
		book.read(bookPath);
		std::vector<uint8_t> played(book.size() + 1, 0);
		Counters counters;
		for (const std::string& input : inputs) {
			if (isGameFile(input)) readGameFile(input, book, played, counters);
			else readPgn(input, book, played, counters);
		}

		Book pruned;
		pruned.header = book.header;
		pruned.rootScore = book.rootScore;
		if (!book.wdl.empty()) pruned.wdl.assign(1, WdlCount{});
		if (!book.rates.empty()) pruned.rates.assign(1, WdlRate{});

		// The line last written, as pairs of a node of the book and its node in the pruned book:
		// the next line kept mostly begins like it, and only the rest has to be added.
		struct Step {
			Book::NodeIndex node;
			Book::NodeIndex prunedNode;
		};
		std::vector<Step> last;
		std::vector<Book::NodeIndex> line;

		const auto addLine = [&]() {
			size_t common = 0;
			while (common < last.size() && common < line.size() && last[common].node == line[common]) ++common;
			last.resize(common);
			Book::NodeIndex parent = common == 0 ? Book::ROOT : last.back().prunedNode;
			for (size_t ply = common; ply < line.size(); ++ply) {
				const Book::Node& node = book.node(line[ply]);
				const Book::NodeIndex added = pruned.addMove(parent, node.move);
				Book::Node& copy = pruned.node(added);
				copy.priority = node.priority;
				copy.value = node.value;
				copy.score = node.score;
				if (!book.wdl.empty()) {
					if (pruned.wdl.size() <= added) pruned.wdl.resize(added + 1);
					pruned.wdl[added] = book.wdl[line[ply]];
				}
				if (!book.rates.empty()) {
					if (pruned.rates.size() <= added) pruned.rates.resize(added + 1);
					pruned.rates[added] = book.rates[line[ply]];
				}
				last.push_back(Step{ .node = line[ply], .prunedNode = added });
				parent = added;
			}
		};

		// The leaves in the order of the book: a depth first walk. A line is written only if its
		// leaf stays, so a removed leaf takes the nodes with it that no line kept runs through.
		uint64_t leaves = 0;
		uint64_t kept = 0;
		Book::NodeIndex next = book.node(Book::ROOT).firstChild;
		if (next != Book::NO_NODE) line.push_back(next);
		while (!line.empty()) {
			const Book::Node& current = book.node(line.back());
			if (current.firstChild != Book::NO_NODE) {
				line.push_back(current.firstChild);
				continue;
			}
			++leaves;
			if (!played[line.back()]) {
				addLine();
				++kept;
			}
			while (!line.empty() && book.node(line.back()).nextSibling == Book::NO_NODE) line.pop_back();
			if (line.empty()) break;
			line.back() = book.node(line.back()).nextSibling;
		}

		// Written beside the output and renamed then: the output may be the book that was read.
		const std::string partPath = outPath + ".part";
		pruned.write(partPath);
		std::filesystem::rename(partPath, outPath);

		std::printf("games read %llu, through a leaf %llu (a leaf reached again %llu), not beginning in the book %llu\n",
			(unsigned long long)counters.read, (unsigned long long)counters.throughLeaf,
			(unsigned long long)counters.leafAgain, (unsigned long long)counters.outsideBook);
		std::printf("%llu of %llu leaves removed, %llu lines kept; %zu nodes of %zu kept\n",
			(unsigned long long)(leaves - kept), (unsigned long long)leaves, (unsigned long long)kept,
			pruned.size(), book.size());
		return leaves - kept;
	}
}
