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
	 * Turns a pgn of played games into a game file - src/trainer/convert.py of the engine, compiled,
	 * with the same output to the byte.
	 *
	 * The pgn has to hold its moves in long algebraic notation, as qapla-engine-tester writes them
	 * with notation=lan: then packing a move needs nothing but its two squares, and no move generator.
	 *
	 * The value of a move is read out of its comment, in pawns with two decimals. The engine reports
	 * its own unit as if it were centipawns, so a hundred times the number is the value in the unit of
	 * the engine, and from there it goes through the sigmoid into the stored win probability.
	 *
	 * The perspective of the values is checked, not assumed: seen from the side to move, consecutive
	 * values of a game have opposite signs almost always, seen from white they keep it. A file that
	 * does not clearly look like the side to move is refused and nothing is written.
	 */
	struct PgnToGameFile {
		/** Whether the result of a game goes into the file, or RESULT_NONE for every position. */
		bool useResult = true;
		/** Stops after this many games with moves; 0 for all of them. */
		uint64_t maxGames = 0;

		/**
		 * Converts and prints what it counted. Returns false if the file was refused.
		 * @throws std::runtime_error if a file cannot be read or written.
		 */
		bool convert(const std::string& pgnPath, const std::string& gamePath) const;
	};
}
