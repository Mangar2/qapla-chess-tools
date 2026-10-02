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
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

#include "game-file.h"
#include "line-reader.h"

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
		};

		struct PgnMove {
			int from;
			int to;
			int promotion;
			std::optional<int> value;
		};

		enum class Outcome { White, Black, Draw, Unknown };

		Outcome outcomeOf(const std::string& tag) {
			if (tag == "1-0") return Outcome::White;
			if (tag == "0-1") return Outcome::Black;
			if (tag == "1/2-1/2") return Outcome::Draw;
			return Outcome::Unknown;
		}

		bool isResultToken(const std::string& token) {
			return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
		}

		bool isDigit(char c) { return c >= '0' && c <= '9'; }

		/**
		 * The value in the unit of the engine out of a pgn comment, or nothing.
		 * The comment is "+0.34", "-M5", or "+0.01, Draw by threefold repetition": the tester puts
		 * the reason a game ended behind the value of its last move.
		 */
		std::optional<int> parseValue(const std::string& comment, Counters& counters) {
			size_t begin = 0;
			while (begin < comment.size() && isPythonSpace(comment[begin])) ++begin;
			size_t end = begin;
			while (end < comment.size() && !isPythonSpace(comment[end])) ++end;
			std::string token = comment.substr(begin, end - begin);
			token = token.substr(0, token.find('/'));          // an optional /depth
			while (!token.empty() && token.back() == ',') token.pop_back();

			if (!token.empty()) {
				char* rest = nullptr;
				const double pawns = std::strtod(token.c_str(), &rest);
				if (rest != nullptr && *rest == '\0' && std::isfinite(pawns)) {
					// Python's round(), which takes halves to the even neighbour.
					return int(std::nearbyint(pawns * 100.0));
				}
			}
			// A mate announcement, however it is spelt: as far from equal as it gets.
			if (token.find_first_of("Mm") != std::string::npos) {
				return token.rfind('-', 0) == 0 ? -30000 : 30000;
			}
			++counters.badComments;
			return std::nullopt;
		}

		/** The moves of a game in long notation, each with the value of its comment if it has one. */
		std::vector<PgnMove> parseMovetext(const std::string& text, Counters& counters) {
			std::vector<PgnMove> moves;
			size_t index = 0;
			const size_t length = text.size();
			while (index < length) {
				const char character = text[index];
				if (isPythonSpace(character)) {
					++index;
					continue;
				}
				if (character == '{') {
					const size_t end = text.find('}', index);
					if (end == std::string::npos) break;
					if (!moves.empty()) {
						moves.back().value = parseValue(text.substr(index + 1, end - index - 1), counters);
					}
					index = end + 1;
					continue;
				}
				size_t end = index;
				while (end < length && !isPythonSpace(text[end]) && text[end] != '{') ++end;
				const std::string token = text.substr(index, end - index);
				index = end;
				if (isDigit(token[0]) && (token.find('.') != std::string::npos || isResultToken(token))) {
					continue;                               // a move number or the result
				}
				if (isResultToken(token)) continue;
				if (token.size() < 4 || token[0] < 'a' || token[0] > 'h') continue;   // not a move
				const int departure = (token[0] - 'a') + (token[1] - '1') * 8;
				const int destination = (token[2] - 'a') + (token[3] - '1') * 8;
				int promotion = NO_PIECE;
				if (token.size() > 4) {
					int kind;
					switch (token[4]) {
					case 'q': case 'Q': kind = QUEEN; break;
					case 'r': case 'R': kind = ROOK; break;
					case 'b': case 'B': kind = BISHOP; break;
					case 'n': case 'N': kind = KNIGHT; break;
					default: continue;
					}
					promotion = kind + (destination >= 56 ? WHITE : BLACK);
				}
				moves.push_back(PgnMove{ .from = departure, .to = destination, .promotion = promotion,
					.value = std::nullopt });
			}
			return moves;
		}

		/**
		 * The records of one game, or nothing if it cannot be stored.
		 *
		 * The counters are shared over the whole file and counted as convert.py counts them: the moves
		 * and values of a game that turns out to hold a move the format cannot express are counted all
		 * the same, and "without a value" can only be counted before the first value of the file was
		 * seen. Both shape what is printed and the perspective check, so they are kept as they are.
		 */
		std::optional<Game> gameRecords(const std::map<std::string, std::string>& tags,
			std::vector<PgnMove> moves, bool useResult, Counters& counters) {
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
			const auto result = tags.find("Result");
			const Outcome outcome = outcomeOf(result == tags.end() ? "*" : result->second);
			Game records;
			std::optional<int> previous;
			for (size_t ply = 0; ply < moves.size(); ++ply) {
				const PgnMove& move = moves[ply];
				const std::optional<int> packed = packMove(move.from, move.to, move.promotion);
				if (!packed) {
					++counters.skippedUnpackable;
					return std::nullopt;
				}
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
				records.push_back(Record{ .move = uint32_t(*packed), .value = code, .result = stored });
				++counters.moves;
			}
			bool anyValue = false;
			for (const Record& record : records) anyValue = anyValue || record.value != NO_GAME_VALUE;
			if (counters.withValue == 0 && !anyValue) {
				++counters.skippedNoValue;
				return std::nullopt;
			}
			return records;
		}

		/** Python's str.strip(), for the characters a pgn holds. */
		std::string strip(const std::string& line) {
			size_t begin = 0, end = line.size();
			while (begin < end && isPythonSpace(line[begin])) ++begin;
			while (end > begin && isPythonSpace(line[end - 1])) --end;
			return line.substr(begin, end - begin);
		}
	}

	bool PgnToGameFile::convert(const std::string& pgnPath, const std::string& gamePath) const {
		Counters counters;
		// Written to a file of its own first: a file that is refused is decided only at the end, and
		// then nothing may be left under the name of the game file.
		const std::string partPath = gamePath + ".part";
		Writer writer(partPath);

		auto flush = [&](const std::map<std::string, std::string>& tags, const std::string& movetext) {
			std::vector<PgnMove> moves = parseMovetext(movetext, counters);
			if (moves.empty()) return;
			++counters.games;
			const std::optional<Game> records = gameRecords(tags, std::move(moves), useResult, counters);
			if (records) {
				writer.write(*records);
				++counters.written;
			}
		};

		std::map<std::string, std::string> tags;
		std::string movetext;
		bool inMoves = false;
		LineReader reader(pgnPath);
		std::string line;
		while (reader.next(line)) {
			const std::string stripped = strip(line);
			if (!stripped.empty() && stripped[0] == '[') {
				if (inMoves) {
					flush(tags, movetext);
					tags.clear();
					movetext.clear();
					inMoves = false;
					if (maxGames != 0 && counters.games >= maxGames) break;
				}
				const size_t separator = stripped.find(" \"", 1);
				if (separator != std::string::npos) {
					const std::string name = stripped.substr(1, separator - 1);
					const std::string rest = stripped.substr(separator + 2);
					const size_t quote = rest.rfind('"');
					tags[name] = quote == std::string::npos ? rest : rest.substr(0, quote);
				}
				continue;
			}
			if (!stripped.empty()) {
				inMoves = true;
				if (!movetext.empty()) movetext += ' ';
				movetext += stripped;
			}
		}
		if (inMoves) flush(tags, movetext);
		writer.close();

		const uint64_t decided = counters.sameSign + counters.oppositeSign;
		const double fraction = decided != 0 ? double(counters.oppositeSign) / double(decided) : 0.0;
		std::printf("evaluations from the side to move in %.1f%% of the pairs\n", fraction * 100.0);
		bool refused = false;
		if (decided > 100 && !(fraction > 0.8 || fraction < 0.2)) {
			std::printf("Refused: the perspective of the evaluations is not clear. Neither the side "
				"to move nor white fits, so every sign could be wrong.\n");
			refused = true;
		}
		else if (decided > 100 && fraction < 0.2) {
			std::printf("Refused: the evaluations look like they are seen from white. This file "
				"stores them from the side to move; convert with the tester set to the "
				"engine's own point of view.\n");
			refused = true;
		}
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
		return true;
	}
}
