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
 * A file begins with the magic "QAPLAGM2" and a version, four bytes little endian, and then holds
 * game after game: a length byte, 1 to 255, and one record per ply. A length of zero, the end of the
 * file or a game cut off ends it. Two versions differ in the record:
 *
 * Version 2, the one the engine's training reads, three bytes per ply:
 *   (msb) VVVV VVVV VVVR RMMM MMMM MMMM (lsb), little endian
 *   M = the eleven bit move of src/book/packed-move.h, R = result, V = value code
 *
 * Version 3, the nine bit move of src/book9/packed-move.h and room for data of the caller's own.
 * The header has four more bytes, little endian: the payload, the number of bytes every ply carries
 * behind its move. A ply is two bytes, little endian, and then its payload:
 *   (msb) 0000 0RRM MMMM MMMM (lsb)
 *   M = the nine bit move, R = result, 0 = reserved, written as zero
 * With a payload of two bytes or more, the first two hold the value code, little endian; the bytes
 * behind them are free. With less there are no values in the file.
 *
 * This is the counterpart of src/trainer/format.py in the engine's repository, which in turn
 * follows src/book/packed-move.h, src/book9/packed-move.h and src/nnue-data/game-file.h there.
 * Everything here has to agree with those, bit for bit:
 *
 * - the eleven and the nine bit move code and how they are unpacked without a move generator,
 * - the records of a played game: move, result, value code,
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

	/** The file versions: the eleven bit move the engine reads, and the nine bit move. */
	constexpr uint32_t VERSION_MOVE11 = 2;
	constexpr uint32_t VERSION_MOVE9 = 3;
	/** The most bytes a ply of version 3 carries behind its move. */
	constexpr uint32_t MAX_PAYLOAD = 255;
	/** The payload a value needs: the value code in two bytes. */
	constexpr uint32_t VALUE_PAYLOAD = 2;

	/** What the header of a file says. */
	struct Format {
		uint32_t version = VERSION_MOVE11;
		/** Bytes behind the move of every ply; always 0 for version 2, which has no payload. */
		uint32_t payload = 0;
	};

	/**
	 * One ply of a game. The move is the packed move of the version of the file. The value is a win
	 * probability as a code, NO_GAME_VALUE for a move without one; value and result are seen from the
	 * side to move before the move.
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

	/**
	 * The value in the unit of the engine that codeOfValue turns back into the code: the inverse of
	 * the sigmoid, then the integer nearby whose code is this one, if there is one - near a
	 * probability of one half not every code has one, then it is the nearest. The two ends, a
	 * probability of 0 or 1, are the values a mate is read as.
	 */
	int valueOfCode(uint32_t code);

	/** Appends " {+0.31}": the value of a code as the tester writes it, in pawns. */
	void appendValueComment(std::string& text, uint32_t code);

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

	/**
	 * The nine bit code of a move in the position it is played from, or nothing if the format cannot
	 * express it: no piece of the side to move on the departure square, a destination the piece has
	 * no code for, or more than 16 pieces of a side. A castling move is the king moving to the c or
	 * the g file, as Qapla writes it. The move is not checked for legality.
	 */
	std::optional<uint32_t> packMove9(const Move& move, const Board& board);

	/**
	 * from, to and promotion of a nine bit move in the position it was packed in: the moving piece is
	 * the n-th piece of the side to move counted from a1, the code says where it goes. Castling comes
	 * back as the king moving to the c or the g file.
	 * @throws std::runtime_error if the code does not fit the position - the file is broken.
	 */
	Move unpackMove9(uint32_t packed, const Board& board);

	/** The code of a move in the version of a file; packMove or packMove9. */
	std::optional<uint32_t> packMove(const Move& move, const Board& board, uint32_t version);

	/** A packed move of the version of a file; unpackMove or unpackMove9. */
	Move unpackMove(uint32_t packed, const Board& board, uint32_t version);

	/** Packs a record of version 2 into its three bytes, as an integer. */
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
		/** @throws std::runtime_error if the file cannot be opened or is not of version 2 or 3. */
		explicit Reader(const std::string& path);
		~Reader();
		Reader(const Reader&) = delete;
		Reader& operator=(const Reader&) = delete;

		/** The next game, or false at the end. */
		bool next(Game& game);

		const Format& format() const { return format_; }

	private:
		std::FILE* file_;
		Format format_;
		std::vector<uint8_t> data_;
	};

	/** Writes a game file; games that are empty or longer than MAX_PLIES are left out. */
	class Writer {
	public:
		/**
		 * The payload of the format is only for version 3; behind the value, if it has room for one,
		 * it is written as zeros.
		 * @throws std::runtime_error if the file cannot be created or the format is not one.
		 */
		explicit Writer(const std::string& path, const Format& format = {});
		~Writer();
		Writer(const Writer&) = delete;
		Writer& operator=(const Writer&) = delete;

		void write(const Game& game);
		/** @throws std::runtime_error if a write failed. */
		void close();

	private:
		std::FILE* file_;
		Format format_;
		std::vector<uint8_t> data_;
		bool failed_ = false;
	};
}
