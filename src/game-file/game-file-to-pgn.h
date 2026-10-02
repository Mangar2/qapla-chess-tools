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
	 * Writes the games of a game file back out as a pgn - src/trainer/export-pgn.py of the engine,
	 * compiled, with the same output to the byte.
	 *
	 * Why: the labelling pass of the engine's pipeline takes a pgn, so relabelling a set with another
	 * evaluation needs one, and a game file holds everything a pgn of played games holds - every ply,
	 * the moves of the book line included, and the result in every record. Only the notation is
	 * rebuilt: long algebraic, which is what the way back reads without a move generator. The values
	 * are not written; they are what a relabelling replaces.
	 */
	struct GameFileToPgn {
		/** Stops after this many games; 0 for all of them. */
		uint64_t maxGames = 0;

		/**
		 * Converts and prints how many games it wrote. Returns the number of games.
		 * @throws std::runtime_error if a file cannot be read or written.
		 */
		uint64_t convert(const std::string& gamePath, const std::string& pgnPath) const;
	};
}
