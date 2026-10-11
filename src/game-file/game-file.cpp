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
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

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

		constexpr int sign(int value) { return (value > 0) - (value < 0); }

		// --- the nine bit move of src/book9/packed-move.h: IIII CCCC C, the index of the moving
		// piece among the pieces of the side to move and a code that depends on its type -----------
		constexpr uint32_t CODE_MASK = 0x1F;
		constexpr uint32_t PIECE_SHIFT = 5;
		constexpr uint32_t PIECE_INDEX_MASK = 0xF;
		constexpr uint32_t MOVE9_MASK = 0x1FF;
		constexpr uint32_t MAX_PIECES_PER_COLOR = 16;
		// Queen, rook, bishop and king: the line of the move, two bits, and the position of the
		// destination on it, three bits. The lines are absolute: rank, file, diagonal, anti diagonal.
		constexpr uint32_t LINE_SHIFT = 3;
		constexpr uint32_t LINE_POSITION_MASK = 0x7;
		constexpr uint32_t RANK_LINE = 0, FILE_LINE = 1, DIAGONAL_LINE = 2, ANTI_DIAGONAL_LINE = 3;
		// A pawn: forward, capture towards a, capture towards h, double step, then the promotions,
		// 4 + 4 * direction + piece.
		constexpr uint32_t PAWN_PUSH = 0, PAWN_CAPTURE_TOWARDS_A = 1, PAWN_DOUBLE_PUSH = 3;
		constexpr uint32_t PAWN_PROMOTION = 4, PAWN_CODE_COUNT = 16;
		constexpr int PROMOTION_TYPES[4] = { QUEEN, ROOK, BISHOP, KNIGHT };
		/** The eight knight directions as file and rank offsets, indexed by the knight code. */
		constexpr int KNIGHT_OFFSETS[8][2] = {
			{ -1, +2 }, { +1, +2 }, { +2, +1 }, { +2, -1 },
			{ -2, +1 }, { -2, -1 }, { -1, -2 }, { +1, -2 },
		};

		constexpr int fileOf(int square) { return square & 7; }
		constexpr int rankOf(int square) { return square >> 3; }
		constexpr bool isOnBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }
		constexpr int squareOf(int file, int rank) { return rank * 8 + file; }

		int colorToMove(const Board& board) { return board.whiteToMove ? WHITE : BLACK; }

		bool isPieceOf(const Board& board, int square, int color) {
			const int piece = board.squares[square];
			return piece != NO_PIECE && (piece & 1) == color;
		}

		std::optional<uint32_t> lineCode(int from, int to) {
			const int fileDelta = fileOf(to) - fileOf(from);
			const int rankDelta = rankOf(to) - rankOf(from);
			if (fileDelta == 0 && rankDelta == 0) return std::nullopt;
			if (rankDelta == 0) return (RANK_LINE << LINE_SHIFT) | uint32_t(fileOf(to));
			if (fileDelta == 0) return (FILE_LINE << LINE_SHIFT) | uint32_t(rankOf(to));
			if (fileDelta == rankDelta) return (DIAGONAL_LINE << LINE_SHIFT) | uint32_t(fileOf(to));
			if (fileDelta == -rankDelta) return (ANTI_DIAGONAL_LINE << LINE_SHIFT) | uint32_t(fileOf(to));
			return std::nullopt;
		}

		/** The destination of a line code, or -1 if it points off the board. */
		int lineTarget(int from, uint32_t code) {
			const int position = int(code & LINE_POSITION_MASK);
			const int file = fileOf(from);
			const int rank = rankOf(from);
			int targetFile = position;
			int targetRank = rank;
			switch (code >> LINE_SHIFT) {
			case RANK_LINE: break;
			case FILE_LINE: targetFile = file; targetRank = position; break;
			case DIAGONAL_LINE: targetRank = rank + (position - file); break;
			case ANTI_DIAGONAL_LINE: targetRank = rank - (position - file); break;
			}
			return isOnBoard(targetFile, targetRank) ? squareOf(targetFile, targetRank) : -1;
		}

		/**
		 * The square a king move is packed with: castling is the king moving onto its own rook. A king
		 * move to the c or g file that covers more than one file, or none, is castling; the rook is
		 * the first piece beyond the king in that direction. Every other king move stays as it is.
		 */
		std::optional<int> kingDestination(const Move& move, const Board& board, int color) {
			const int fileDelta = fileOf(move.to) - fileOf(move.from);
			const bool sameRank = rankOf(move.to) == rankOf(move.from);
			const bool isStep = fileDelta >= -1 && fileDelta <= 1 && fileDelta != 0;
			const bool ontoOwnPiece = move.to != move.from && isPieceOf(board, move.to, color);
			if (!sameRank || isStep || ontoOwnPiece) return move.to;
			const int targetFile = fileOf(move.to);
			if (targetFile != 2 && targetFile != 6) return std::nullopt;
			const int step = targetFile == 6 ? 1 : -1;
			const int rank = rankOf(move.from);
			for (int file = fileOf(move.from) + step; file >= 0 && file < 8; file += step) {
				const int piece = board.squares[squareOf(file, rank)];
				if (piece == NO_PIECE) continue;
				if (piece != ROOK + color) return std::nullopt;
				return squareOf(file, rank);
			}
			return std::nullopt;
		}

		std::optional<uint32_t> pawnCode(const Move& move, int color) {
			const int forward = color == WHITE ? 1 : -1;
			const int fileDelta = fileOf(move.to) - fileOf(move.from);
			const int steps = (rankOf(move.to) - rankOf(move.from)) * forward;
			if (steps == 2 && fileDelta == 0 && move.promotion == NO_PIECE) return PAWN_DOUBLE_PUSH;
			if (steps != 1 || fileDelta < -1 || fileDelta > 1) return std::nullopt;
			const uint32_t direction = fileDelta == 0 ? PAWN_PUSH
				: (fileDelta < 0 ? PAWN_CAPTURE_TOWARDS_A : PAWN_CAPTURE_TOWARDS_A + 1);
			if (move.promotion == NO_PIECE) return direction;
			for (uint32_t index = 0; index < 4; ++index) {
				if (PROMOTION_TYPES[index] == (move.promotion & ~1)) {
					return PAWN_PROMOTION + direction * 4 + index;
				}
			}
			return std::nullopt;
		}

		[[noreturn]] void brokenMove(uint32_t packed) {
			throw std::runtime_error("the nine bit move " + std::to_string(packed)
				+ " does not fit its position - the game file is broken");
		}

		void writeLittleEndian(uint8_t* bytes, uint32_t value, int count) {
			for (int byte = 0; byte < count; ++byte) bytes[byte] = uint8_t(value >> (8 * byte));
		}

		uint32_t readLittleEndian(const uint8_t* bytes, int count) {
			uint32_t value = 0;
			for (int byte = 0; byte < count; ++byte) value |= uint32_t(bytes[byte]) << (8 * byte);
			return value;
		}

		/** Bytes per ply in a file of this format. */
		size_t recordSize(const Format& format) {
			return format.version == VERSION_MOVE11 ? 3 : 2 + size_t(format.payload);
		}
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

	int valueOfCode(uint32_t code) {
		constexpr int MATE_VALUE = 30000;
		const double probability = double(code - MIN_VALUE_CODE) / double(MAX_VALUE_CODE - MIN_VALUE_CODE);
		if (probability <= 0.0) return -MATE_VALUE;
		if (probability >= 1.0) return MATE_VALUE;
		const double exact = -VALUE_SCALE * std::log(1.0 / probability - 1.0);
		const int nearest = int(std::lround(std::fmax(-MATE_VALUE, std::fmin(MATE_VALUE, exact))));
		for (const int offset : { 0, -1, 1, -2, 2 }) {
			if (codeOfValue(nearest + offset) == code) return nearest + offset;
		}
		return nearest;
	}

	void appendValueComment(std::string& text, uint32_t code) {
		const int value = valueOfCode(code);
		char buffer[32];
		std::snprintf(buffer, sizeof(buffer), " {%c%d.%02d}", value < 0 ? '-' : '+',
			std::abs(value) / 100, std::abs(value) % 100);
		text += buffer;
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

	std::optional<uint32_t> packMove9(const Move& move, const Board& board) {
		if (move.from < 0 || move.from >= 64 || move.to < 0 || move.to >= 64) return std::nullopt;
		const int color = colorToMove(board);
		if (!isPieceOf(board, move.from, color)) return std::nullopt;
		uint32_t index = 0;
		for (int below = 0; below < move.from; ++below) {
			if (isPieceOf(board, below, color)) ++index;
		}
		if (index >= MAX_PIECES_PER_COLOR) return std::nullopt;

		const int kind = board.squares[move.from] & ~1;
		std::optional<uint32_t> code;
		switch (kind) {
		case PAWN:
			code = pawnCode(move, color);
			break;
		case KNIGHT: {
			const int fileDelta = fileOf(move.to) - fileOf(move.from);
			const int rankDelta = rankOf(move.to) - rankOf(move.from);
			for (uint32_t direction = 0; direction < 8; ++direction) {
				if (KNIGHT_OFFSETS[direction][0] == fileDelta && KNIGHT_OFFSETS[direction][1] == rankDelta) {
					code = direction;
				}
			}
			break;
		}
		case KING: {
			const std::optional<int> destination = kingDestination(move, board, color);
			if (destination) code = lineCode(move.from, *destination);
			break;
		}
		default:
			code = lineCode(move.from, move.to);
			break;
		}
		if (!code) return std::nullopt;
		if (move.promotion != NO_PIECE && kind != PAWN) return std::nullopt;
		return (index << PIECE_SHIFT) | *code;
	}

	Move unpackMove9(uint32_t packed, const Board& board) {
		const int color = colorToMove(board);
		uint32_t index = (packed >> PIECE_SHIFT) & PIECE_INDEX_MASK;
		const uint32_t code = packed & CODE_MASK;
		int from = 0;
		for (; from < 64; ++from) {
			if (!isPieceOf(board, from, color)) continue;
			if (index == 0) break;
			--index;
		}
		if (from == 64) brokenMove(packed);
		const int file = fileOf(from);
		const int rank = rankOf(from);

		switch (board.squares[from] & ~1) {
		case PAWN: {
			if (code >= PAWN_CODE_COUNT) brokenMove(packed);
			const int forward = color == WHITE ? 1 : -1;
			if (code == PAWN_DOUBLE_PUSH) {
				if (!isOnBoard(file, rank + 2 * forward)) brokenMove(packed);
				return Move{ .from = from, .to = squareOf(file, rank + 2 * forward), .promotion = NO_PIECE };
			}
			int promotion = NO_PIECE;
			uint32_t direction = code;
			if (code >= PAWN_PROMOTION) {
				direction = (code - PAWN_PROMOTION) / 4;
				promotion = PROMOTION_TYPES[(code - PAWN_PROMOTION) % 4] + color;
			}
			const int fileDelta = direction == PAWN_PUSH ? 0 : (direction == PAWN_CAPTURE_TOWARDS_A ? -1 : 1);
			if (!isOnBoard(file + fileDelta, rank + forward)) brokenMove(packed);
			return Move{ .from = from, .to = squareOf(file + fileDelta, rank + forward), .promotion = promotion };
		}
		case KNIGHT: {
			if (code >= 8) brokenMove(packed);
			const int targetFile = file + KNIGHT_OFFSETS[code][0];
			const int targetRank = rank + KNIGHT_OFFSETS[code][1];
			if (!isOnBoard(targetFile, targetRank)) brokenMove(packed);
			return Move{ .from = from, .to = squareOf(targetFile, targetRank), .promotion = NO_PIECE };
		}
		default: {
			const int to = lineTarget(from, code);
			if (to < 0) brokenMove(packed);
			if ((board.squares[from] & ~1) == KING && isPieceOf(board, to, color)) {
				// Castling, the king onto its own rook: Qapla writes it as the king to the c or g file.
				const bool kingSide = fileOf(to) > file;
				return Move{ .from = from, .to = squareOf(kingSide ? 6 : 2, rank), .promotion = NO_PIECE };
			}
			return Move{ .from = from, .to = to, .promotion = NO_PIECE };
		}
		}
	}

	std::optional<uint32_t> packMove(const Move& move, const Board& board, uint32_t version) {
		if (version == VERSION_MOVE9) return packMove9(move, board);
		const std::optional<int> packed = packMove(move.from, move.to, move.promotion);
		if (!packed) return std::nullopt;
		return uint32_t(*packed);
	}

	Move unpackMove(uint32_t packed, const Board& board, uint32_t version) {
		return version == VERSION_MOVE9 ? unpackMove9(packed, board) : unpackMove(packed, board);
	}

	Reader::Reader(const std::string& path) : file_(std::fopen(path.c_str(), "rb")) {
		if (file_ == nullptr) throw std::runtime_error("cannot open " + path);
		uint8_t header[sizeof(MAGIC) + 4];
		bool valid = std::fread(header, 1, sizeof(header), file_) == sizeof(header)
			&& std::memcmp(header, MAGIC, sizeof(MAGIC)) == 0;
		if (valid) {
			format_.version = readLittleEndian(header + sizeof(MAGIC), 4);
			if (format_.version == VERSION_MOVE9) {
				uint8_t payload[4];
				valid = std::fread(payload, 1, sizeof(payload), file_) == sizeof(payload);
				format_.payload = readLittleEndian(payload, 4);
				valid = valid && format_.payload <= MAX_PAYLOAD;
			}
			else {
				valid = format_.version == VERSION_MOVE11;
			}
		}
		if (!valid) {
			std::fclose(file_);
			file_ = nullptr;
			throw std::runtime_error(path + " is not a game file of version 2 or 3 - a file of the "
				"older format holds pawns where this expects probabilities");
		}
	}

	Reader::~Reader() {
		if (file_ != nullptr) std::fclose(file_);
	}

	bool Reader::next(Game& game) {
		game.clear();
		const int count = std::fgetc(file_);
		if (count == EOF || count == 0) return false;
		const size_t size = recordSize(format_);
		data_.resize(size * size_t(count));
		if (std::fread(data_.data(), 1, data_.size(), file_) != data_.size()) return false;
		for (int ply = 0; ply < count; ++ply) {
			const uint8_t* bytes = data_.data() + size * size_t(ply);
			if (format_.version == VERSION_MOVE11) {
				game.push_back(unpackRecord(readLittleEndian(bytes, 3)));
				continue;
			}
			const uint32_t bits = readLittleEndian(bytes, 2);
			const uint32_t value = format_.payload >= VALUE_PAYLOAD
				? readLittleEndian(bytes + 2, 2) & MAX_VALUE_CODE : NO_GAME_VALUE;
			game.push_back(Record{ .move = bits & MOVE9_MASK, .value = value, .result = (bits >> 9) & 0x3 });
		}
		return true;
	}

	Writer::Writer(const std::string& path, const Format& format) : file_(nullptr), format_(format) {
		const bool known = format.version == VERSION_MOVE11 ? format.payload == 0
			: format.version == VERSION_MOVE9 && format.payload <= MAX_PAYLOAD;
		if (!known) {
			throw std::runtime_error("no game file of version " + std::to_string(format.version)
				+ " with a payload of " + std::to_string(format.payload) + " bytes");
		}
		file_ = std::fopen(path.c_str(), "wb");
		if (file_ == nullptr) throw std::runtime_error("cannot create " + path);
		uint8_t header[sizeof(MAGIC) + 8];
		std::memcpy(header, MAGIC, sizeof(MAGIC));
		writeLittleEndian(header + sizeof(MAGIC), format.version, 4);
		writeLittleEndian(header + sizeof(MAGIC) + 4, format.payload, 4);
		const size_t length = format.version == VERSION_MOVE9 ? sizeof(header) : sizeof(MAGIC) + 4;
		failed_ = std::fwrite(header, 1, length, file_) != length;
	}

	Writer::~Writer() {
		if (file_ != nullptr) std::fclose(file_);
	}

	void Writer::write(const Game& game) {
		if (game.empty() || game.size() > MAX_PLIES) return;
		const size_t size = recordSize(format_);
		data_.assign(1 + size * game.size(), 0);
		data_[0] = uint8_t(game.size());
		uint8_t* bytes = data_.data() + 1;
		for (const Record& record : game) {
			if (format_.version == VERSION_MOVE11) {
				writeLittleEndian(bytes, packRecord(record), 3);
			}
			else {
				writeLittleEndian(bytes, (record.move & MOVE9_MASK) | ((record.result & 0x3) << 9), 2);
				if (format_.payload >= VALUE_PAYLOAD) {
					writeLittleEndian(bytes + 2, record.value & MAX_VALUE_CODE, 2);
				}
			}
			bytes += size;
		}
		if (std::fwrite(data_.data(), 1, data_.size(), file_) != data_.size()) failed_ = true;
	}

	void Writer::close() {
		if (file_ == nullptr) return;
		if (std::fclose(file_) != 0) failed_ = true;
		file_ = nullptr;
		if (failed_) throw std::runtime_error("writing the game file failed");
	}
}
