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
#include <vector>

namespace GameFile {

	/**
	 * Removes from a book every leaf that games have played already: a game is followed from the
	 * start position through the book as long as its moves are in it, and the leaf it reaches that
	 * way - its moves up to the leaf are exactly the line of the leaf - goes. With the leaf goes its
	 * line back to the last node that still has another line through it; that node and everything
	 * above it stay. The book written holds the other lines in their order, every node on their way,
	 * and the payload of the book node for node, the score of the start position included.
	 *
	 * Made for a book grown out of an older one whose leaves were played: the new leaves that lie on
	 * those games are start positions whose games have been played. The games are pgn in long
	 * notation, as for pgn2gam, or game files; a game from a fen is not followed.
	 */
	struct BookPrune {
		/** The pgn and game files, in this order; a game file is told by its magic. */
		std::vector<std::string> inputs;

		/**
		 * Prunes, writes the book and prints the games read, the leaves removed and the lines and
		 * nodes kept. Returns the number of leaves removed.
		 * @throws std::runtime_error if a file cannot be read or written.
		 */
		uint64_t convert(const std::string& bookPath, const std::string& outPath) const;
	};
}
