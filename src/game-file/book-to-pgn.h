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
	 * Writes the lines of a 9 bit book as a pgn, one game per leaf, the moves from the start position
	 * in long algebraic notation - the form exportOpenings of the engine writes a position library
	 * in, which a tester takes as openings and pgn2book and pgn2gam read back. Every move of the book
	 * is in it, the inner ones as the beginning of the lines below them.
	 *
	 * With values, every move with a value gets it as a comment, as gam2pgn writes it; in a book
	 * evaluated by qapla-engine-tester --bookeval the search score, as the tester writes it.
	 */
	struct BookToPgn {
		/** Stops after this many lines; 0 for all of them. */
		uint64_t maxLines = 0;
		/** Writes the values into the comments. */
		bool withValues = false;

		/**
		 * Converts and prints how many lines it wrote. Returns the number of lines.
		 * @throws std::runtime_error if a file cannot be read or written, or the book is broken.
		 */
		uint64_t convert(const std::string& bookPath, const std::string& pgnPath) const;
	};
}
