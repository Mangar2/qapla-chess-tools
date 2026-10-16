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

#include "pgn-to-game-file.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include "book-file.h"
#include "game-file.h"
#include "pgn-reader.h"

namespace GameFile {

	namespace {

		struct Counters {
			uint64_t games = 0;
			uint64_t written = 0;
			uint64_t moves = 0;
			uint64_t withValue = 0;
			uint64_t skippedFen = 0;
			uint64_t skippedNoValue = 0;
			uint64_t skippedUnpackable = 0;
			uint64_t truncated = 0;
			uint64_t badComments = 0;
			uint64_t sameSign = 0;
			uint64_t oppositeSign = 0;
			/** The results of the written games, and the games whose tag and final token disagree. */
			uint64_t whiteWins = 0;
			uint64_t blackWins = 0;
			uint64_t draws = 0;
			uint64_t noResult = 0;
			uint64_t resultConflicts = 0;
		};

		enum class Outcome { White, Black, Draw, Unknown };

		Outcome outcomeOf(const std::string& tag) {
			if (tag == "1-0") return Outcome::White;
			if (tag == "0-1") return Outcome::Black;
			if (tag == "1/2-1/2") return Outcome::Draw;
			return Outcome::Unknown;
		}

		/**
		 * The records of one game, or nothing if it cannot be stored.
		 *
		 * The counters are shared over the whole file and counted as convert.py counts them: the moves
		 * and values of a game that turns out to hold a move the format cannot express are counted all
		 * the same, and "without a value" can only be counted before the first value of the file was
		 * seen. Both shape what is printed and the perspective check, so they are kept as they are.
		 */
		struct BookCounters {
			/** Plies whose value came from the book. */
			uint64_t filled = 0;
			/** Plies of the written games that are still without a value. */
			uint64_t withoutValue = 0;
			/** Games that do not begin with a move of the book. */
			uint64_t outsideBook = 0;
		};

		/**
		 * Gives every ply without a value that lies in the book the score of the book: the game is
		 * followed through the tree as long as its moves are there, and a ply takes the score the
		 * book holds for the position it is played from, as if it had stood in its comment.
		 */
		void fillFromBook(const Book& book, std::vector<PgnMove>& moves, BookCounters& counters) {
			Board board;
			Book::NodeIndex node = Book::ROOT;
			for (size_t ply = 0; ply < moves.size(); ++ply) {
				PgnMove& move = moves[ply];
				const Move played{ .from = move.from, .to = move.to, .promotion = move.promotion };
				const std::optional<uint32_t> code = packMove9(played, board);
				node = code ? book.findMove(node, *code) : Book::NO_NODE;
				if (node == Book::NO_NODE) {
					if (ply == 0) ++counters.outsideBook;
					return;
				}
				const int16_t score = book.node(node).score;
				if (!move.value && score != NO_SCORE) {
					move.value = valueOfScore(score);
					++counters.filled;
				}
				board.apply(played);
			}
		}

		std::optional<Game> gameRecords(const PgnTags& tags, const std::string& finalToken,
			std::vector<PgnMove> moves, bool useResult, uint32_t version, Counters& counters) {
			if (tags.count("FEN") != 0) {
				// The file starts every game at the initial position; one that starts elsewhere cannot
				// be replayed.
				++counters.skippedFen;
				return std::nullopt;
			}
			if (moves.size() > MAX_PLIES) {
				moves.resize(MAX_PLIES);
				++counters.truncated;
			}
			// The Result tag decides, as in convert.py; without one, the token that ends the moves.
			const auto result = tags.find("Result");
			if (result != tags.end() && !finalToken.empty() && result->second != finalToken) {
				++counters.resultConflicts;
			}
			const Outcome outcome = outcomeOf(result != tags.end() ? result->second
				: (finalToken.empty() ? "*" : finalToken));
			Game records;
			std::optional<int> previous;
			// The nine bit move depends on the position, the game is followed on a board.
			Board board;
			for (size_t ply = 0; ply < moves.size(); ++ply) {
				const PgnMove& move = moves[ply];
				const Move played{ .from = move.from, .to = move.to, .promotion = move.promotion };
				const std::optional<uint32_t> packed = packMove(played, board, version);
				if (!packed) {
					++counters.skippedUnpackable;
					return std::nullopt;
				}
				if (version == VERSION_MOVE9) board.apply(played);
				uint32_t stored;
				if (!useResult || outcome == Outcome::Unknown) {
					stored = RESULT_NONE;
				}
				else if (outcome == Outcome::Draw) {
					stored = RESULT_DRAW;
				}
				else {
					const bool whiteToMove = ply % 2 == 0;
					const bool sideToMoveWins = whiteToMove == (outcome == Outcome::White);
					stored = sideToMoveWins ? RESULT_WIN : RESULT_LOSS;
				}
				uint32_t code = NO_GAME_VALUE;
				if (move.value) {
					code = codeOfValue(*move.value);
					++counters.withValue;
					if (previous) {
						if ((*move.value < 0) != (*previous < 0)) ++counters.oppositeSign;
						else ++counters.sameSign;
					}
					previous = move.value;
				}
				records.push_back(Record{ .move = *packed, .value = code, .result = stored });
				++counters.moves;
			}
			bool anyValue = false;
			for (const Record& record : records) anyValue = anyValue || record.value != NO_GAME_VALUE;
			if (counters.withValue == 0 && !anyValue) {
				++counters.skippedNoValue;
				return std::nullopt;
			}
			switch (outcome) {
			case Outcome::White: ++counters.whiteWins; break;
			case Outcome::Black: ++counters.blackWins; break;
			case Outcome::Draw: ++counters.draws; break;
			case Outcome::Unknown: ++counters.noResult; break;
			}
			return records;
		}

	}

	bool PgnToGameFile::convert(const std::string& pgnPath, const std::string& gamePath) const {
		Counters counters;
		BookCounters bookCounters;
		std::optional<Book> book;
		if (!bookPath.empty()) {
			book.emplace();
			book->read(bookPath);
			if (book->header.payloadId != SCORE_PAYLOAD_ID) {
				throw std::runtime_error(bookPath + " holds no search scores; give a book evaluated by "
					"qapla-engine-tester --bookeval");
			}
		}
		// Written to a file of its own first: a file that is refused is decided only at the end, and
		// then nothing may be left under the name of the game file.
		const std::string partPath = gamePath + ".part";
		Writer writer(partPath, format);

		readPgnGames(pgnPath, [&](const PgnTags& tags, const std::string& movetext) {
			std::vector<PgnMove> moves = parseMovetext(movetext, counters.badComments);
			if (moves.empty()) return true;
			++counters.games;
			if (book && tags.count("FEN") == 0) fillFromBook(*book, moves, bookCounters);
			const std::optional<Game> records = gameRecords(tags, finalResultToken(movetext), std::move(moves), useResult,
				format.version, counters);
			if (records) {
				writer.write(*records);
				++counters.written;
				for (const Record& record : *records) {
					if (record.value == NO_GAME_VALUE) ++bookCounters.withoutValue;
				}
			}
			return maxGames == 0 || counters.games < maxGames;
		});
		writer.close();

		printPerspective(counters.sameSign, counters.oppositeSign);
		const char* refusal = perspectiveRefusal(counters.sameSign, counters.oppositeSign);
		const bool refused = refusal != nullptr;
		if (refused) std::printf("%s\n", refusal);
		if (refused) {
			std::filesystem::remove(partPath);
			return false;
		}
		std::filesystem::rename(partPath, gamePath);
		std::printf("games %llu, written %llu, moves %llu, with a value %llu\n",
			(unsigned long long)counters.games, (unsigned long long)counters.written,
			(unsigned long long)counters.moves, (unsigned long long)counters.withValue);
		std::printf("skipped: from a fen %llu, without a value %llu, unpackable move %llu; "
			"truncated %llu; unreadable comments %llu\n",
			(unsigned long long)counters.skippedFen, (unsigned long long)counters.skippedNoValue,
			(unsigned long long)counters.skippedUnpackable, (unsigned long long)counters.truncated,
			(unsigned long long)counters.badComments);
		if (useResult) {
			std::printf("results: 1-0 %llu, 0-1 %llu, 1/2-1/2 %llu, none %llu; tag and final token "
				"disagree %llu\n", (unsigned long long)counters.whiteWins, (unsigned long long)counters.blackWins,
				(unsigned long long)counters.draws, (unsigned long long)counters.noResult,
				(unsigned long long)counters.resultConflicts);
			if (counters.noResult != 0) {
				std::printf("WARNING: %llu of the %llu written games have no result - no Result tag, and their "
					"moves end in * or in no result at all; their positions carry none\n",
					(unsigned long long)counters.noResult, (unsigned long long)counters.written);
			}
			if (counters.resultConflicts != 0) {
				std::printf("WARNING: %llu games have a Result tag that disagrees with the token ending "
					"their moves; the tag was taken\n", (unsigned long long)counters.resultConflicts);
			}
		}
		if (book) {
			std::printf("from the book: %llu plies filled, %llu plies of the written games still without "
				"a value, %llu games not beginning with a book move\n",
				(unsigned long long)bookCounters.filled, (unsigned long long)bookCounters.withoutValue,
				(unsigned long long)bookCounters.outsideBook);
		}
		return true;
	}
}
