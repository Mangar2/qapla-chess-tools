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

#include "game-file.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace GameFile {

	namespace {
		constexpr int DIRECTIONS[16] = {
			+2 * 8 - 1, +2 * 8 + 1, +1 * 8 + 2, -1 * 8 + 2,      // knight
			+1 * 8 - 2, -1 * 8 - 2, -2 * 8 - 1, -2 * 8 + 1,
			+0 * 8 + 1, +0 * 8 - 1, +1 * 8 + 0, -1 * 8 + 0,      // rook
			+1 * 8 + 1, -1 * 8 + 1, +1 * 8 - 1, -1 * 8 - 1,      // bishop
		};
		constexpr int KNIGHT_DIRECTIONS = 8;
		constexpr int DIRECTION_COUNT = 16;

		// Index 0 and 7 both mean queen: those are the values the rank bits of the destination
		// square have anyway, so a queen promotion leaves them alone.
		constexpr int PROMOTION_PIECES[8] = {
			QUEEN + BLACK, ROOK + BLACK, BISHOP + BLACK, KNIGHT + BLACK,
			KNIGHT + WHITE, BISHOP + WHITE, ROOK + WHITE, QUEEN + WHITE,
		};

		constexpr int TO_MASK = 0x3F;
		constexpr int TO_RANK_MASK = 0x38;
		constexpr int DIRECTION_SHIFT = 6;
		constexpr int DIRECTION_MASK = 0x3C0;
		constexpr int PROMOTION_FLAG = 0x400;

		constexpr char MAGIC[8] = { 'Q', 'A', 'P', 'L', 'A', 'G', 'M', '2' };
		constexpr uint32_t VERSION = 2;

		constexpr int sign(int value) { return (value > 0) - (value < 0); }
	}

	uint32_t codeOfProbability(double probability) {
		const double span = double(MAX_VALUE_CODE - MIN_VALUE_CODE);
		const double clamped = probability < 0.0 ? 0.0 : (probability > 1.0 ? 1.0 : probability);
		// floor(x + 0.5): the engine rounds with lround(), and format.py does the same with int().
		return MIN_VALUE_CODE + uint32_t(clamped * span + 0.5);
	}

	uint32_t codeOfValue(int value) {
		return codeOfProbability(1.0 / (1.0 + std::exp(-double(value) / VALUE_SCALE)));
	}

	std::optional<int> packMove(int departure, int destination, int promotion) {
		const int fileDelta = (destination & 7) - (departure & 7);
		const int rankDelta = (destination >> 3) - (departure >> 3);
		if (fileDelta == 0 && rankDelta == 0) return std::nullopt;

		std::optional<int> packed;
		if (std::abs(fileDelta) * std::abs(rankDelta) == 2) {
			// A knight, whose direction points back at the departure square.
			const int backwards = departure - destination;
			for (int index = 0; index < KNIGHT_DIRECTIONS; ++index) {
				if (DIRECTIONS[index] == backwards) {
					packed = destination | (index << DIRECTION_SHIFT);
					break;
				}
			}
		}
		else if (fileDelta == 0 || rankDelta == 0 || std::abs(fileDelta) == std::abs(rankDelta)) {
			const int step = sign(rankDelta) * 8 + sign(fileDelta);
			for (int index = KNIGHT_DIRECTIONS; index < DIRECTION_COUNT; ++index) {
				if (DIRECTIONS[index] == step) {
					packed = destination | (index << DIRECTION_SHIFT);
					break;
				}
			}
		}
		if (!packed) return std::nullopt;
		if (promotion == NO_PIECE) return packed;

		// The rank bits of the destination are redundant for a promotion and carry the piece
		// instead; a queen leaves them as they are.
		const int rankBits = *packed & TO_RANK_MASK;
		const bool lastRank = rankBits == TO_RANK_MASK;
		if (!lastRank && rankBits != 0) return std::nullopt;
		int code;
		switch (promotion & ~1) {
		case QUEEN: code = lastRank ? 7 : 0; break;
		case ROOK: code = lastRank ? 6 : 1; break;
		case BISHOP: code = lastRank ? 5 : 2; break;
		case KNIGHT: code = lastRank ? 4 : 3; break;
		default: return std::nullopt;
		}
		return (*packed & ~TO_RANK_MASK) | (code << 3) | PROMOTION_FLAG;
	}

	Board::Board() {
		const int back[8] = { ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK };
		for (int file = 0; file < 8; ++file) {
			squares[file] = int8_t(back[file] + WHITE);
			squares[8 + file] = int8_t(PAWN + WHITE);
			squares[48 + file] = int8_t(PAWN + BLACK);
			squares[56 + file] = int8_t(back[file] + BLACK);
		}
	}

	void Board::apply(const Move& move) {
		const int departure = move.from;
		const int destination = move.to;
		const int piece = squares[departure];
		const int kind = piece & ~1;
		if (kind == KING) {
			kings[piece & 1] = destination;
			const int step = destination - departure;
			if (step == 2) {            // castling short, the rook follows
				squares[destination - 1] = squares[destination + 1];
				squares[destination + 1] = NO_PIECE;
			}
			else if (step == -2) {      // castling long
				squares[destination + 1] = squares[destination - 2];
				squares[destination - 2] = NO_PIECE;
			}
		}
		else if (kind == PAWN && (departure & 7) != (destination & 7)
			&& squares[destination] == NO_PIECE) {
			// A pawn moving sideways to an empty square captures en passant.
			squares[(departure & ~7) | (destination & 7)] = NO_PIECE;
		}
		squares[destination] = int8_t(move.promotion != NO_PIECE ? move.promotion : piece);
		squares[departure] = NO_PIECE;
		whiteToMove = !whiteToMove;
	}

	Move unpackMove(uint32_t packed, const Board& board) {
		int bits = int(packed);
		int promotion = NO_PIECE;
		if (bits & PROMOTION_FLAG) {
			const int code = (bits & TO_RANK_MASK) >> 3;
			promotion = PROMOTION_PIECES[code];
			// Restore the rank of the destination, which carried the promotion piece.
			bits = code > 3 ? (bits | TO_RANK_MASK) : (bits & ~TO_RANK_MASK);
		}
		const int to = bits & TO_MASK;
		const int direction = (bits & DIRECTION_MASK) >> DIRECTION_SHIFT;
		if (direction < KNIGHT_DIRECTIONS) {
			return Move{ .from = to + DIRECTIONS[direction], .to = to, .promotion = promotion };
		}
		const int step = DIRECTIONS[direction];
		int square = to - step;
		while (square >= 0 && square < 64 && board.squares[square] == NO_PIECE) {
			square -= step;
		}
		return Move{ .from = square, .to = to, .promotion = promotion };
	}

	Reader::Reader(const std::string& path) : file_(std::fopen(path.c_str(), "rb")) {
		if (file_ == nullptr) throw std::runtime_error("cannot open " + path);
		char header[sizeof(MAGIC) + 4];
		const bool complete = std::fread(header, 1, sizeof(header), file_) == sizeof(header);
		uint32_t version = 0;
		if (complete) {
			for (int byte = 0; byte < 4; ++byte) {
				version |= uint32_t(uint8_t(header[sizeof(MAGIC) + byte])) << (8 * byte);
			}
		}
		if (!complete || std::memcmp(header, MAGIC, sizeof(MAGIC)) != 0 || version != VERSION) {
			std::fclose(file_);
			file_ = nullptr;
			throw std::runtime_error(path + " is not a game file of version 2 - a file of the older "
				"format holds pawns where this expects probabilities");
		}
	}

	Reader::~Reader() {
		if (file_ != nullptr) std::fclose(file_);
	}

	bool Reader::next(Game& game) {
		game.clear();
		const int count = std::fgetc(file_);
		if (count == EOF || count == 0) return false;
		uint8_t data[3 * MAX_PLIES];
		if (std::fread(data, 1, size_t(3 * count), file_) != size_t(3 * count)) return false;
		for (int ply = 0; ply < count; ++ply) {
			const uint8_t* bytes = data + 3 * ply;
			game.push_back(unpackRecord(uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8)
				| (uint32_t(bytes[2]) << 16)));
		}
		return true;
	}

	Writer::Writer(const std::string& path) : file_(std::fopen(path.c_str(), "wb")) {
		if (file_ == nullptr) throw std::runtime_error("cannot create " + path);
		const uint8_t version[4] = { uint8_t(VERSION), 0, 0, 0 };
		failed_ = std::fwrite(MAGIC, 1, sizeof(MAGIC), file_) != sizeof(MAGIC)
			|| std::fwrite(version, 1, sizeof(version), file_) != sizeof(version);
	}

	Writer::~Writer() {
		if (file_ != nullptr) std::fclose(file_);
	}

	void Writer::write(const Game& game) {
		if (game.empty() || game.size() > MAX_PLIES) return;
		uint8_t data[1 + 3 * MAX_PLIES];
		data[0] = uint8_t(game.size());
		size_t length = 1;
		for (const Record& record : game) {
			const uint32_t bits = packRecord(record);
			data[length++] = uint8_t(bits & 0xFF);
			data[length++] = uint8_t((bits >> 8) & 0xFF);
			data[length++] = uint8_t((bits >> 16) & 0xFF);
		}
		if (std::fwrite(data, 1, length, file_) != length) failed_ = true;
	}

	void Writer::close() {
		if (file_ == nullptr) return;
		if (std::fclose(file_) != 0) failed_ = true;
		file_ = nullptr;
		if (failed_) throw std::runtime_error("writing the game file failed");
	}
}
