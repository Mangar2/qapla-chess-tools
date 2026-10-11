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

#include "opening-reader.h"

#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "game-file.h"
#include "line-reader.h"
#include "opening/pgn-io.h"
#include "game-manager/game-state.h"

namespace GameFile {

	namespace {

		constexpr int MATE_VALUE = 30000;

		/** A move in long notation as the tester writes it, e2e4 or e7e8q, in Qapla's pieces. */
		std::optional<PgnMove> moveOfLan(const std::string& lan) {
			if (lan.size() < 4) return std::nullopt;
			const auto square = [&](size_t at) -> int {
				if (lan[at] < 'a' || lan[at] > 'h' || lan[at + 1] < '1' || lan[at + 1] > '8') return -1;
				return (lan[at] - 'a') + (lan[at + 1] - '1') * 8;
			};
			const int from = square(0), to = square(2);
			if (from < 0 || to < 0) return std::nullopt;
			int promotion = NO_PIECE;
			if (lan.size() > 4) {
				switch (lan[4]) {
				case 'q': case 'Q': promotion = QUEEN; break;
				case 'r': case 'R': promotion = ROOK; break;
				case 'b': case 'B': promotion = BISHOP; break;
				case 'n': case 'N': promotion = KNIGHT; break;
				default: return std::nullopt;
				}
				promotion += to >= 56 ? WHITE : BLACK;
			}
			return PgnMove{ .from = from, .to = to, .promotion = promotion, .value = std::nullopt };
		}

		/** The first four fields of a fen: placement, side to move, castling, en passant. */
		std::string positionOf(const std::string& fen) {
			std::istringstream stream(fen);
			std::string position, field;
			for (int count = 0; count < 4 && stream >> field; ++count) {
				position += (count == 0 ? "" : " ") + field;
			}
			return position;
		}

		/**
		 * The moves of a game record in long notation, played through by the tester; nothing if one
		 * of them cannot be played. The final position is left in the game state.
		 */
		std::optional<std::vector<PgnMove>> movesOf(const QaplaTester::GameRecord& record,
			QaplaTester::GameState& state) {
			const QaplaTester::GameRecord played = state.setFromGameRecordAndCopy(record, std::nullopt, false);
			if (played.history().size() != record.history().size()) return std::nullopt;
			std::vector<PgnMove> moves;
			for (const QaplaTester::MoveRecord& move : played.history()) {
				std::optional<PgnMove> converted = moveOfLan(move.lan_);
				if (!converted) return std::nullopt;
				if (move.scoreMate) converted->value = *move.scoreMate < 0 ? -MATE_VALUE : MATE_VALUE;
				else if (move.scoreCp) converted->value = *move.scoreCp;
				moves.push_back(*converted);
			}
			return moves;
		}

		void readPgn(const std::string& path, OpeningCounters& counters,
			const std::function<bool(const std::vector<PgnMove>& moves)>& handler) {
			QaplaTester::PgnIO::LoadParams params;
			params.filePath = path;
			params.loadComments = true;
			QaplaTester::PgnGameReader reader(params);
			if (!reader.isOpen()) throw std::runtime_error("cannot open " + path);
			while (const std::optional<QaplaTester::GameRecord> record = reader.next()) {
				if (record->history().empty()) continue;
				++counters.games;
				if (!record->getStartPos()) {
					++counters.skippedFen;
					continue;
				}
				QaplaTester::GameState state;
				const std::optional<std::vector<PgnMove>> moves = movesOf(*record, state);
				if (!moves) {
					++counters.skippedIllegal;
					continue;
				}
				if (!handler(*moves)) return;
			}
		}

		void readRaw(const std::string& path, OpeningCounters& counters,
			const std::function<bool(const std::vector<PgnMove>& moves)>& handler) {
			LineReader reader(path);
			std::string line;
			while (reader.next(line)) {
				const size_t first = line.find(';');
				if (line.find_first_not_of(" \t") == std::string::npos) continue;
				++counters.games;
				const size_t second = first == std::string::npos ? first : line.find(';', first + 1);
				if (second == std::string::npos) {
					++counters.skippedMismatch;
					continue;
				}
				const QaplaTester::GameRecord record = QaplaTester::PgnIO::parseGame(
					line.substr(first + 1, second - first - 1));
				QaplaTester::GameState state;
				const std::optional<std::vector<PgnMove>> moves = movesOf(record, state);
				if (!moves || moves->empty()) {
					++counters.skippedIllegal;
					continue;
				}
				if (positionOf(state.getFen()) != positionOf(line.substr(0, first))) {
					++counters.skippedMismatch;
					continue;
				}
				if (!handler(*moves)) return;
			}
		}
	}

	bool isRawFile(const std::string& path) {
		return std::filesystem::path(path).extension() == ".raw";
	}

	void readOpenings(const std::string& path, OpeningCounters& counters,
		const std::function<bool(const std::vector<PgnMove>& moves)>& handler) {
		if (isRawFile(path)) readRaw(path, counters, handler);
		else readPgn(path, counters, handler);
	}
}
