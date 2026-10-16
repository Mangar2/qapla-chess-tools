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
	 * Writes a book evaluated by qapla-engine-tester --bookeval as a game file of version 2, the one
	 * the training reads, so that every evaluated position of the book carries its value exactly once.
	 *
	 * Every line of the book, from the start position to a leaf, is one game. A ply carries the
	 * score of the position it is played from - converted as --pgn2gam book= converts it, so both
	 * give a position the same label - but only in the first line through that position, in the
	 * order of the book; in every later line the ply has no value, and the training skips it. The
	 * leaves were never searched and have no ply of their own. No game has a result.
	 */
	struct BookToGameFile {
		/**
		 * Leaves out what carries no value: a line without a position seen first in it, and the plies
		 * of a line behind its last value. The labels are the same, the file a fraction of the size.
		 */
		bool compact = false;

		/**
		 * Converts and prints what it wrote. Returns the number of games.
		 * @throws std::runtime_error if a file cannot be read or written, the book holds no search
		 *         scores, or a move cannot be expressed in the eleven bit move.
		 */
		uint64_t convert(const std::string& bookPath, const std::string& gamePath) const;
	};
}
