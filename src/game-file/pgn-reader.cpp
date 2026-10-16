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

#include "pgn-reader.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "game-file.h"
#include "line-reader.h"

namespace GameFile {

	namespace {

		bool isResultToken(const std::string& token) {
			return token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*";
		}

		bool isDigit(char c) { return c >= '0' && c <= '9'; }

		/**
		 * The value in the unit of the engine out of a pgn comment, or nothing.
		 * The comment is "+0.34", "-M5", or "+0.01, Draw by threefold repetition": the tester puts
		 * the reason a game ended behind the value of its last move.
		 */
		std::optional<int> parseValue(const std::string& comment, uint64_t& badComments) {
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
			++badComments;
			return std::nullopt;
		}

		/** Python's str.strip(), for the characters a pgn holds. */
		std::string strip(const std::string& line) {
			size_t begin = 0, end = line.size();
			while (begin < end && isPythonSpace(line[begin])) ++begin;
			while (end > begin && isPythonSpace(line[end - 1])) --end;
			return line.substr(begin, end - begin);
		}
	}

	std::vector<PgnMove> parseMovetext(const std::string& text, uint64_t& badComments) {
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
					moves.back().value = parseValue(text.substr(index + 1, end - index - 1), badComments);
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

	std::string finalResultToken(const std::string& text) {
		std::string last;
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
				index = end + 1;
				continue;
			}
			size_t end = index;
			while (end < length && !isPythonSpace(text[end]) && text[end] != '{') ++end;
			last = text.substr(index, end - index);
			index = end;
		}
		return isResultToken(last) ? last : std::string();
	}

	void readPgnGames(const std::string& path,
		const std::function<bool(const PgnTags& tags, const std::string& movetext)>& handler) {
		PgnTags tags;
		std::string movetext;
		bool inMoves = false;
		LineReader reader(path);
		std::string line;
		while (reader.next(line)) {
			const std::string stripped = strip(line);
			if (!stripped.empty() && stripped[0] == '[') {
				if (inMoves) {
					const bool more = handler(tags, movetext);
					tags.clear();
					movetext.clear();
					inMoves = false;
					if (!more) return;
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
		if (inMoves) handler(tags, movetext);
	}

	const char* perspectiveRefusal(uint64_t sameSign, uint64_t oppositeSign) {
		const uint64_t decided = sameSign + oppositeSign;
		const double fraction = decided != 0 ? double(oppositeSign) / double(decided) : 0.0;
		if (decided > 100 && !(fraction > 0.8 || fraction < 0.2)) {
			return "Refused: the perspective of the evaluations is not clear. Neither the side "
				"to move nor white fits, so every sign could be wrong.";
		}
		if (decided > 100 && fraction < 0.2) {
			return "Refused: the evaluations look like they are seen from white. This file "
				"stores them from the side to move; convert with the tester set to the "
				"engine's own point of view.";
		}
		return nullptr;
	}

	void printPerspective(uint64_t sameSign, uint64_t oppositeSign) {
		const uint64_t decided = sameSign + oppositeSign;
		const double fraction = decided != 0 ? double(oppositeSign) / double(decided) : 0.0;
		std::printf("evaluations from the side to move in %.1f%% of the pairs\n", fraction * 100.0);
	}
}
