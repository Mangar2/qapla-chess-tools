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

#include "book-to-game-file.h"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "book-file.h"
#include "game-file.h"

namespace GameFile {

	uint64_t BookToGameFile::convert(const std::string& bookPath, const std::string& gamePath) const {
		Book book;
		book.read(bookPath);
		if (book.header.payloadId != SCORE_PAYLOAD_ID) {
			throw std::runtime_error(bookPath + " holds no search scores; give a book evaluated by "
				"qapla-engine-tester --bookeval");
		}

		// Whether the position a node's children are played from has had its value written, by the
		// index of that node; the root stands for the start position.
		std::vector<bool> written(book.size() + 1, false);
		uint64_t evaluated = 0;
		for (Book::NodeIndex node = 1; node <= book.size(); ++node) {
			const Book::NodeIndex child = book.node(node).firstChild;
			if (child != Book::NO_NODE && book.node(child).score != NO_SCORE) ++evaluated;
		}
		const Book::NodeIndex firstMove = book.node(Book::ROOT).firstChild;
		if (firstMove != Book::NO_NODE && book.node(firstMove).score != NO_SCORE) ++evaluated;

		// Written to a file of its own first, renamed when it is complete.
		const std::string partPath = gamePath + ".part";
		Writer writer(partPath, Format{});

		// A depth first walk, one frame per ply of the line: the node and the board it is played on.
		struct Frame {
			Book::NodeIndex node;
			Board board;
			Move move;
		};
		std::vector<Frame> line;
		Game game;
		uint64_t games = 0, plies = 0, withValue = 0, cut = 0, lostValues = 0;

		const auto writeLine = [&]() {
			game.clear();
			for (size_t ply = 0; ply < line.size(); ++ply) {
				const Book::NodeIndex from = ply == 0 ? Book::ROOT : line[ply - 1].node;
				const int16_t score = book.node(line[ply].node).score;
				const bool first = score != NO_SCORE && !written[from];
				if (ply >= MAX_PLIES) {
					// The game file has one byte for the length of a game.
					if (first) ++lostValues;
					continue;
				}
				const Move& move = line[ply].move;
				const std::optional<int> packed = packMove(move.from, move.to, move.promotion);
				if (!packed) throw std::runtime_error("a move of the book has no eleven bit code");
				uint32_t value = NO_GAME_VALUE;
				if (first) {
					value = codeOfValue(valueOfScore(score));
					written[from] = true;
					++withValue;
				}
				game.push_back(Record{ .move = uint32_t(*packed), .value = value, .result = RESULT_NONE });
			}
			if (line.size() > MAX_PLIES) ++cut;
			if (compact) {
				while (!game.empty() && game.back().value == NO_GAME_VALUE) game.pop_back();
				if (game.empty()) return;
			}
			writer.write(game);
			++games;
			plies += game.size();
		};

		const auto enter = [&](Book::NodeIndex node, const Board& parent) {
			const Move move = unpackMove9(book.node(node).move, parent);
			Frame frame{ .node = node, .board = parent, .move = move };
			frame.board.apply(move);
			line.push_back(frame);
		};

		const Board start;
		if (firstMove != Book::NO_NODE) enter(firstMove, start);
		while (!line.empty()) {
			const Book::Node& current = book.node(line.back().node);
			if (current.firstChild != Book::NO_NODE) {
				enter(current.firstChild, line.back().board);
				continue;
			}
			writeLine();
			// Back up to the first node with a right sibling, and go on there.
			while (!line.empty() && book.node(line.back().node).nextSibling == Book::NO_NODE) line.pop_back();
			if (line.empty()) break;
			const Book::NodeIndex sibling = book.node(line.back().node).nextSibling;
			line.pop_back();
			enter(sibling, line.empty() ? start : line.back().board);
		}
		writer.close();
		std::filesystem::rename(partPath, gamePath);

		std::printf("games %llu, plies %llu, with a value %llu; evaluated positions of the book %llu\n",
			(unsigned long long)games, (unsigned long long)plies, (unsigned long long)withValue,
			(unsigned long long)evaluated);
		if (cut != 0) {
			std::printf("%llu lines longer than %zu plies were cut, %llu values lost with them\n",
				(unsigned long long)cut, MAX_PLIES, (unsigned long long)lostValues);
		}
		return games;
	}
}
