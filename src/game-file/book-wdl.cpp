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

#include "book-wdl.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

#include "book-file.h"
#include "book-wdl-rate.h"
#include "game-file.h"
#include "pgn-reader.h"

namespace GameFile {

	namespace {

		enum class Outcome { White, Black, Draw, None };

		Outcome outcomeOf(const std::string& result) {
			if (result == "1-0") return Outcome::White;
			if (result == "0-1") return Outcome::Black;
			if (result == "1/2-1/2") return Outcome::Draw;
			return Outcome::None;
		}

		struct Counters {
			uint64_t read = 0;
			uint64_t counted = 0;
			uint64_t noResult = 0;
			uint64_t outsideBook = 0;
			uint64_t whiteWins = 0;
			uint64_t blackWins = 0;
			uint64_t draws = 0;
			uint64_t nodes = 0;
			uint64_t saturated = 0;
		};

		bool isGameFile(const std::string& path) {
			std::ifstream stream(path, std::ios::binary);
			char magic[8] = {};
			stream.read(magic, sizeof(magic));
			return stream && std::memcmp(magic, "QAPLAGM2", sizeof(magic)) == 0;
		}

		void add(uint32_t& count, Counters& counters) {
			if (count == std::numeric_limits<uint32_t>::max()) {
				++counters.saturated;
				return;
			}
			++count;
		}

		/** Follows a game through the book and counts its result in every node of it. */
		void countGame(Book& book, const std::vector<Move>& moves, Outcome outcome, Counters& counters) {
			++counters.read;
			if (outcome == Outcome::None) {
				++counters.noResult;
				return;
			}
			Board board;
			Book::NodeIndex node = Book::ROOT;
			size_t ply = 0;
			for (; ply < moves.size(); ++ply) {
				const std::optional<uint32_t> code = packMove9(moves[ply], board);
				const Book::NodeIndex child = code ? book.findMove(node, *code) : Book::NO_NODE;
				if (child == Book::NO_NODE) break;
				// The side that plays the move: white in the start position, then by turns.
				const bool whiteMoves = ply % 2 == 0;
				WdlCount& count = book.wdl[child];
				if (outcome == Outcome::Draw) add(count.draws, counters);
				else if ((outcome == Outcome::White) == whiteMoves) add(count.wins, counters);
				else add(count.losses, counters);
				++counters.nodes;
				board.apply(moves[ply]);
				node = child;
			}
			if (ply == 0) {
				++counters.outsideBook;
				return;
			}
			++counters.counted;
			if (outcome == Outcome::White) ++counters.whiteWins;
			else if (outcome == Outcome::Black) ++counters.blackWins;
			else ++counters.draws;
		}

		void readPgn(const std::string& path, Book& book, Counters& counters) {
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
				const auto tag = tags.find("Result");
				const Outcome outcome = outcomeOf(tag != tags.end() ? tag->second : finalResultToken(movetext));
				countGame(book, moves, outcome, counters);
				return true;
			});
		}

		void readGameFile(const std::string& path, Book& book, Counters& counters) {
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
				// The result of a record is seen from the side to move; the first one is white's.
				Outcome outcome = Outcome::None;
				switch (game.empty() ? RESULT_NONE : game.front().result) {
				case RESULT_WIN: outcome = Outcome::White; break;
				case RESULT_LOSS: outcome = Outcome::Black; break;
				case RESULT_DRAW: outcome = Outcome::Draw; break;
				default: break;
				}
				countGame(book, moves, outcome, counters);
			}
		}
	}

	uint64_t BookWdl::convert(const std::string& bookPath, const std::string& outPath) const {
		Book book;
		book.read(bookPath);
		const bool trained = book.header.payloadId == WDL_PAYLOAD_ID && !book.wdl.empty();
		if (!trained) {
			book.wdl.assign(book.size() + 1, WdlCount{});
			book.header.payloadId = WDL_PAYLOAD_ID;
			book.header.payload = WDL_PAYLOAD;
		}

		Counters counters;
		for (const std::string& input : inputs) {
			if (isGameFile(input)) readGameFile(input, book, counters);
			else readPgn(input, book, counters);
		}

		// Written beside the output and renamed then: the output may be the book that was read.
		const std::string partPath = outPath + ".part";
		book.write(partPath);
		std::filesystem::rename(partPath, outPath);

		std::printf("%s book: games read %llu, counted %llu, without a result %llu, not beginning in the "
			"book %llu\n", trained ? "trained further" : "new counts", (unsigned long long)counters.read,
			(unsigned long long)counters.counted, (unsigned long long)counters.noResult,
			(unsigned long long)counters.outsideBook);
		std::printf("results of the counted games: 1-0 %llu, 0-1 %llu, 1/2-1/2 %llu; %llu counts in book nodes\n",
			(unsigned long long)counters.whiteWins, (unsigned long long)counters.blackWins,
			(unsigned long long)counters.draws, (unsigned long long)counters.nodes);
		if (counters.saturated != 0) {
			std::printf("WARNING: %llu counts stayed at the largest value, 2^32 - 1, instead of overflowing\n",
				(unsigned long long)counters.saturated);
		}
		if (!ratePath.empty()) writeRateBook(book, ratePath);
		return counters.counted;
	}
}
