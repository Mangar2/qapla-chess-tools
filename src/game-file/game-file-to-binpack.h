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
	 * Writes the valued positions of a game file as a Stockfish training data file (.binpack), the
	 * input of nnue-pytorch. Used to train a Stockfish net on Qapla's data.
	 *
	 * Every position that carries a value in the game file becomes one entry: the position, the move
	 * played from it, the value as a Stockfish score, the ply and the result of the game, all from the
	 * side to move. Positions without a value - the moves of the book line - are played through but
	 * not written.
	 *
	 * The value of a game file is a win probability. nnue-pytorch turns a score into its target with a
	 * win-draw-loss model, pf(s) = (1 + sigmoid((s - 270) / 380) - sigmoid((-s - 270) / 380)) / 2 with
	 * the constants of its loss. The score written is the one for which that target is the stored
	 * probability, so the net is trained towards exactly the probabilities Qapla's data holds.
	 */
	struct GameFileToBinpack {
		/** Stops after this many games; 0 for all of them. */
		uint64_t maxGames = 0;

		/**
		 * Converts and prints what it wrote. Returns the number of entries.
		 * @throws std::runtime_error if a file cannot be read, or a move is not legal in the replay.
		 */
		uint64_t convert(const std::string& gamePath, const std::string& binpackPath) const;
	};
}
