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
	 * Trains a 9 bit book with games: every game counts its result in the nodes of the book it runs
	 * through, as a win, a draw or a loss of the side that plays the node's move. A game is followed
	 * from the start position as long as its moves are in the book; at the first move that is not,
	 * it stops counting. The book keeps its tree and its moves, only the payload is the new one,
	 * qapla-wdl-count-v1 (see book-file.h). A book that has counts already is trained further, its
	 * counts added to; any other book starts at zero. A book with scores, evaluated by --bookeval,
	 * keeps them: it is written with qapla-score-wdl-count-v1, scores and counts, and trained further
	 * as such.
	 *
	 * The games are pgn in long notation, as for pgn2gam, or game files: the result is the Result tag
	 * of a pgn or, without one, the token ending its moves; in a game file the result of its first
	 * record. A game without a result is not counted.
	 */
	struct BookWdl {
		/** The pgn and game files, in this order; a game file is told by its magic. */
		std::vector<std::string> inputs;
		/**
		 * If not empty, the trained book is also written here with the rate payload, see --wdl2rate;
		 * for a book with scores (--bookeval) with scores and rates, qapla-score-wdl-rate-v1.
		 */
		std::string ratePath;

		/**
		 * Trains, writes and prints what it counted. Returns the number of games counted.
		 * @throws std::runtime_error if a file cannot be read or written.
		 */
		uint64_t convert(const std::string& bookPath, const std::string& outPath) const;
	};
}
