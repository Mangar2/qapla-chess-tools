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

/**
 * The packed game file of the Qapla engine (QAPLAGM2), the file its nnue training reads.
 *
 * This is the counterpart of src/trainer/format.py in the engine's repository, which in turn
 * follows src/book/packed-move.h and src/nnue-data/game-file.h there. Everything here has to agree
 * with those, bit for bit:
 *
 * - the eleven bit move code and how it is unpacked without a move generator,
 * - the three byte record of a played game: move, result, value code,
 * - the piece encoding of Qapla.
 *
 * The board only replays. It does not generate or check moves, and it does not have to: every move
 * in a game file was played by an engine and is legal.
 */

#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace GameFile {

	// --- the piece encoding of Qapla, see basics/types.h of the engine ----------------------------
	constexpr int NO_PIECE = 0;
	constexpr int WHITE = 0, BLACK = 1;
	constexpr int PAWN = 2, KNIGHT = 4, BISHOP = 6, ROOK = 8, QUEEN = 10, KING = 12;

	// --- the record -------------------------------------------------------------------------------
	constexpr uint32_t NO_GAME_VALUE = 0;
	constexpr uint32_t MIN_VALUE_CODE = 1;
	constexpr uint32_t MAX_VALUE_CODE = (1 << 11) - 1;
	constexpr uint32_t RESULT_LOSS = 0, RESULT_DRAW = 1, RESULT_WIN = 2, RESULT_NONE = 3;
	/** The longest game a file holds: its length is one byte. */
	constexpr size_t MAX_PLIES = 255;

	/**
	 * One ply of a game. The value is a win probability as a code, NO_GAME_VALUE for a move without
	 * one; value and result are seen from the side to move before the move.
	 */
	struct Record {
		uint32_t move;
		uint32_t value;
		uint32_t result;
	};
	using Game = std::vector<Record>;

	/**
	 * The scale the engine's value is turned into a probability with, NET_VALUE_SCALE of
	 * src/nnue/nnue-arch.h in the engine.
	 */
	constexpr double VALUE_SCALE = 400.0;

	/** The code of a probability, clamped to [0, 1]. Rounds half up, as lround() in the engine. */
	uint32_t codeOfProbability(double probability);

	/** The code of a value in the unit of the engine, where a pawn is 80 to 95. */
	uint32_t codeOfValue(int value);

	struct Move {
		int from;
		int to;
		int promotion;
	};

	/**
	 * The eleven bit code of a move, or nothing if the format cannot express it.
	 * The counterpart of unpackMove; the round trip against it is what keeps the two in step.
	 */
	std::optional<int> packMove(int departure, int destination, int promotion = NO_PIECE);

	/** Piece placement and king squares. Applies moves, nothing else. */
	struct Board {
		std::array<int8_t, 64> squares{};
		int kings[2] = { 4, 60 };
		bool whiteToMove = true;

		Board();
		void apply(const Move& move);
	};

	/**
	 * from, to and promotion of a packed move in the given placement. The departure square is the
	 * first occupied square when walking from the destination against the direction of the move:
	 * for a slider that is necessarily the moving piece, anything in between would have blocked it.
	 */
	Move unpackMove(uint32_t packed, const Board& board);

	/** Packs a record into its three bytes, as an integer. */
	constexpr uint32_t packRecord(const Record& record) {
		return (record.move & 0x7FF) | ((record.result & 0x3) << 11)
			| ((record.value & MAX_VALUE_CODE) << 13);
	}

	constexpr Record unpackRecord(uint32_t bits) {
		return Record{ .move = bits & 0x7FF, .value = (bits >> 13) & MAX_VALUE_CODE,
			.result = (bits >> 11) & 0x3 };
	}

	/**
	 * Reads a game file one game at a time. Ends at the end of the file, at a game of length zero
	 * and at a game that is cut off - the same three places format.read_games stops at.
	 */
	class Reader {
	public:
		/** @throws std::runtime_error if the file cannot be opened or is not of version 2. */
		explicit Reader(const std::string& path);
		~Reader();
		Reader(const Reader&) = delete;
		Reader& operator=(const Reader&) = delete;

		/** The next game, or false at the end. */
		bool next(Game& game);

	private:
		std::FILE* file_;
	};

	/** Writes a game file; games that are empty or longer than MAX_PLIES are left out. */
	class Writer {
	public:
		/** @throws std::runtime_error if the file cannot be created. */
		explicit Writer(const std::string& path);
		~Writer();
		Writer(const Writer&) = delete;
		Writer& operator=(const Writer&) = delete;

		void write(const Game& game);
		/** @throws std::runtime_error if a write failed. */
		void close();

	private:
		std::FILE* file_;
		bool failed_ = false;
	};
}
