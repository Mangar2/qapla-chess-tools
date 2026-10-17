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
	 * Splits a book into books of its lines: leaf i of the book, counted in the order of the book
	 * from 0, goes with its whole line into part (i mod parts) + 1. Every part is a book of its own
	 * with the lines it was given and every node on their way - the start of the tree is in all of
	 * them - and carries the payload of the book, node for node, the score of the start position
	 * included. The parts share no line and together hold every line of the book.
	 *
	 * Taking every tenth leaf in the order of the tree is a stratified sample: a subtree of at least
	 * ten leaves gives each part close to a tenth of them, so the parts get the first moves of the
	 * openings in the shares the whole book has them. The same book gives the same parts; a book
	 * that has grown gives others, so a book is split once it is complete.
	 */
	struct BookSplit {
		uint32_t parts = 10;

		/**
		 * The file of a part: the output name with the number of the part before the extension,
		 * two digits for up to 99 parts - start.bok gives start-01.bok to start-10.bok.
		 */
		static std::string partPath(const std::string& outPath, uint32_t part, uint32_t parts);

		/**
		 * Splits, writes the parts and prints their lines and nodes.
		 * @throws std::runtime_error if the book cannot be read or a part cannot be written.
		 */
		void convert(const std::string& bookPath, const std::string& outPath) const;
	};
}
