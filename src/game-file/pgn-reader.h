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
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace GameFile {

	/**
	 * The pgn reading that every converter from a pgn shares: the games of a file with their tags,
	 * and the moves of a game in long algebraic notation with the values of their comments.
	 *
	 * It reads exactly as src/trainer/convert.py of the engine does - lines ended the way Python
	 * ends them, a game ended by the next tag line - because pgn2gam writes what convert.py writes,
	 * to the byte.
	 */

	using PgnTags = std::map<std::string, std::string>;

	struct PgnMove {
		int from;
		int to;
		int promotion;
		/** The value of the comment, in the unit of the engine, from the side to move. */
		std::optional<int> value;
	};

	/**
	 * The moves of a game in long notation, each with the value of its comment if it has one. A
	 * comment that holds no value is counted in badComments.
	 */
	std::vector<PgnMove> parseMovetext(const std::string& text, uint64_t& badComments);

	/**
	 * Calls the handler with the tags and the move text of every game, in the order of the file,
	 * until it returns false. The move text is the lines of the game joined by a space.
	 * @throws std::runtime_error if the file cannot be read.
	 */
	void readPgnGames(const std::string& path,
		const std::function<bool(const PgnTags& tags, const std::string& movetext)>& handler);

	/**
	 * Why the values of a file cannot be taken, or nullptr if they can. Seen from the side to move,
	 * consecutive values of a game have opposite signs almost always, seen from white they keep it;
	 * the counts are those pairs over the whole file.
	 */
	const char* perspectiveRefusal(uint64_t sameSign, uint64_t oppositeSign);

	/** Prints the share of pairs seen from the side to move, as pgn2gam always did. */
	void printPerspective(uint64_t sameSign, uint64_t oppositeSign);
}
