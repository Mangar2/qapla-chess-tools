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

#include "game-file-to-binpack.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

#include "game-file.h"
#include <nnue-pytorch/nnue_training_data_formats.h>

namespace GameFile {

	namespace {

		// The constants of the loss of nnue-pytorch (model/config.py, LossParams), out_offset and
		// out_scaling: the ones that turn the score of a training entry into its target.
		constexpr double OUT_OFFSET = 270.0;
		constexpr double OUT_SCALING = 380.0;
		constexpr double SCORE_LIMIT = 32000.0;

		double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }

		double targetOfScore(double score) {
			return 0.5 * (1.0 + sigmoid((score - OUT_OFFSET) / OUT_SCALING)
				- sigmoid((-score - OUT_OFFSET) / OUT_SCALING));
		}

		/** The score whose target is the given probability. The target rises with the score. */
		int16_t scoreOfProbability(double probability) {
			double low = -SCORE_LIMIT, high = SCORE_LIMIT;
			for (int step = 0; step < 100; ++step) {
				const double middle = 0.5 * (low + high);
				(targetOfScore(middle) < probability ? low : high) = middle;
			}
			return int16_t(std::lround(0.5 * (low + high)));
		}

		/**
		 * The score of every value code. Code 1 is a probability of 0 and code 2047 one of 1, whose
		 * scores are infinite: both ends are moved in by half a code.
		 */
		std::array<int16_t, MAX_VALUE_CODE + 1> scoreTable() {
			std::array<int16_t, MAX_VALUE_CODE + 1> table{};
			const double span = double(MAX_VALUE_CODE - MIN_VALUE_CODE);
			for (uint32_t code = MIN_VALUE_CODE; code <= MAX_VALUE_CODE; ++code) {
				double probability = double(code - MIN_VALUE_CODE) / span;
				probability = std::clamp(probability, 0.5 / span, 1.0 - 0.5 / span);
				table[code] = scoreOfProbability(probability);
			}
			return table;
		}

		chess::PieceType pieceTypeOf(int piece) {
			switch (piece & ~1) {
			case KNIGHT: return chess::PieceType::Knight;
			case BISHOP: return chess::PieceType::Bishop;
			case ROOK: return chess::PieceType::Rook;
			default: return chess::PieceType::Queen;
			}
		}

		/** The move of the game file as chess::Move: castling as the king taking its rook. */
		chess::Move chessMove(const Move& move, const chess::Position& position) {
			const chess::Square from(move.from);
			const chess::Square to(move.to);
			const chess::Piece piece = position.pieceAt(from);
			if (piece.type() == chess::PieceType::King && std::abs(move.to - move.from) == 2) {
				const chess::Square rook(move.to > move.from ? move.from + 3 : move.from - 4);
				return chess::Move{ from, rook, chess::MoveType::Castle, chess::Piece::none() };
			}
			if (piece.type() == chess::PieceType::Pawn && (move.from & 7) != (move.to & 7)
				&& position.pieceAt(to) == chess::Piece::none()) {
				return chess::Move{ from, to, chess::MoveType::EnPassant, chess::Piece::none() };
			}
			if (move.promotion != NO_PIECE) {
				return chess::Move{ from, to, chess::MoveType::Promotion,
					chess::Piece(pieceTypeOf(move.promotion), position.sideToMove()) };
			}
			return chess::Move{ from, to, chess::MoveType::Normal, chess::Piece::none() };
		}

		int16_t resultOf(uint32_t result) {
			switch (result) {
			case RESULT_WIN: return 1;
			case RESULT_LOSS: return -1;
			default: return 0;
			}
		}
	}

	uint64_t GameFileToBinpack::convert(const std::string& gamePath, const std::string& binpackPath) const {
		const auto scores = scoreTable();
		Reader reader(gamePath);
		std::filesystem::remove(binpackPath);
		uint64_t games = 0, entries = 0, withoutResult = 0;
		{
			binpack::CompressedTrainingDataEntryWriter writer(binpackPath, std::ios_base::app);
			Game game;
			while (reader.next(game)) {
				Board board;
				chess::Position position = chess::Position::startPosition();
				for (size_t ply = 0; ply < game.size(); ++ply) {
					const Record& record = game[ply];
					const Move move = unpackMove(record.move, board);
					const chess::Move played = chessMove(move, position);
					if (!position.isMoveLegal(played)) {
						throw std::runtime_error("game " + std::to_string(games + 1) + " ply "
							+ std::to_string(ply + 1) + ": the move is not legal in " + position.fen());
					}
					if (record.value != NO_GAME_VALUE) {
						if (record.result == RESULT_NONE) ++withoutResult;
						writer.addTrainingDataEntry(binpack::TrainingDataEntry{ .pos = position,
							.move = played, .score = scores[record.value], .ply = uint16_t(ply),
							.result = resultOf(record.result) });
						++entries;
					}
					position.doMove(played);
					board.apply(move);
				}
				++games;
				if (maxGames != 0 && games >= maxGames) break;
			}
		}
		std::printf("%llu games, %llu entries written to %s", (unsigned long long)games,
			(unsigned long long)entries, binpackPath.c_str());
		if (withoutResult != 0) {
			std::printf(" - %llu of them without a game result, written as a draw",
				(unsigned long long)withoutResult);
		}
		std::printf("\n");
		return entries;
	}
}
