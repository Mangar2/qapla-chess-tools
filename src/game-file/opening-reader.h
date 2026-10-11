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
#include <string>
#include <vector>

#include "pgn-reader.h"

namespace GameFile {

	/**
	 * Reads lines of moves from the start position with the readers of qapla-engine-tester, copied
	 * to src/engine-tester: a pgn in any notation the tester reads, short algebraic included, or a
	 * .raw file of the tester's openings.
	 *
	 * A line of a .raw file is "<fen>; <moves>; <number>", the moves in short algebraic notation from
	 * the start position and the fen the position they lead to - book8ply.raw of the tester is one.
	 * The tester itself only reads the fen (its EpdReader leaves the move list out), so the moves are
	 * read here with its pgn parser, and the position they end in has to be the fen of the line, or
	 * the line is not taken. The number at the end is not read.
	 */
	struct OpeningCounters {
		uint64_t games = 0;
		/** Games that do not start at the start position. */
		uint64_t skippedFen = 0;
		/** Games with a move the tester cannot play. */
		uint64_t skippedIllegal = 0;
		/** Lines of a .raw file whose moves do not lead to its fen, or that are not one. */
		uint64_t skippedMismatch = 0;
	};

	/** True for the files read as .raw, by their extension. */
	bool isRawFile(const std::string& path);

	/**
	 * Calls the handler with the moves of every game that starts at the start position, until it
	 * returns false. A move has its value from the comment in a pgn, as the tester reads it.
	 * @throws std::runtime_error if the file cannot be read.
	 */
	void readOpenings(const std::string& path, OpeningCounters& counters,
		const std::function<bool(const std::vector<PgnMove>& moves)>& handler);
}
