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
#include <string>

namespace GameFile {

	/**
	 * Builds a 9 bit book from the games of a pgn or the lines of a .raw file: every game is a line
	 * from the start position, and lines that begin alike share their nodes. Both are read with the
	 * readers of qapla-engine-tester, see opening-reader.h, so a pgn may be in any notation the tester
	 * reads. Games from a fen and games with a move the format cannot express are left out, a game
	 * that is longer than maxPly is cut there.
	 *
	 * With a payload of two bytes or more, a move takes the value of its comment as the tester reads
	 * it, converted into a probability as pgn2gam converts it; where games share a move, the last game with a value there decides. Then
	 * the perspective of the values is checked as in pgn2gam, and a file that does not look like the
	 * side to move is refused.
	 */
	struct PgnToBook {
		/** Bytes behind the move of every node, 0 to 255; the value needs two. */
		uint32_t payload = 0;
		/** Stops after this many games with moves; 0 for all of them. */
		uint64_t maxGames = 0;
		/** The longest line taken, in plies; 0 for the whole game. */
		uint32_t maxPly = 0;

		/**
		 * Builds, writes and prints what it counted. Returns false if the file was refused.
		 * @throws std::runtime_error if a file cannot be read or written.
		 */
		bool convert(const std::string& pgnPath, const std::string& bookPath) const;
	};
}
