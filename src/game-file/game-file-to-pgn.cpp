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

#include "game-file-to-pgn.h"

#include <cstdio>
#include <stdexcept>
#include <vector>

#include "game-file.h"

namespace GameFile {

	namespace {

		/**
		 * The result of the game as a pgn tag, seen from white. The result in a record is seen from
		 * the side to move, and the side to move of the first position is white, so the first record
		 * decides. A game whose records say nothing gets a star.
		 */
		const char* resultOf(const Game& game) {
			if (game.empty()) return "*";
			switch (game[0].result) {
			case RESULT_DRAW: return "1/2-1/2";
			case RESULT_WIN: return "1-0";
			case RESULT_LOSS: return "0-1";
			default: return "*";
			}
		}

		void appendSquare(std::string& text, int square) {
			text += char('a' + square % 8);
			text += char('1' + square / 8);
		}
	}

	uint64_t GameFileToPgn::convert(const std::string& gamePath, const std::string& pgnPath) const {
		Reader reader(gamePath);
		std::FILE* out = std::fopen(pgnPath.c_str(), "wb");
		if (out == nullptr) throw std::runtime_error("cannot create " + pgnPath);

		uint64_t written = 0;
		Game game;
		std::vector<std::string> moves;
		std::string text;
		while (reader.next(game)) {
			Board board;
			moves.clear();
			for (const Record& record : game) {
				const Move move = unpackMove(record.move, board);
				std::string name;
				appendSquare(name, move.from);
				appendSquare(name, move.to);
				if (move.promotion != NO_PIECE) {
					// A piece code halved is 2 for a knight, 3 bishop, 4 rook, 5 queen.
					name += "nbrq"[(move.promotion >> 1) - 2];
				}
				moves.push_back(name);
				board.apply(move);
			}
			const char* result = resultOf(game);
			text = "[Event \"Qapla rebuilt from a game file\"]\n[White \"A\"]\n[Black \"B\"]\n";
			text += std::string("[Result \"") + result + "\"]\n[SetUp \"0\"]\n\n";
			// Twelve tokens a line, move numbers counted as tokens, as export-pgn.py breaks them.
			std::vector<std::string> line;
			auto writeLine = [&]() {
				for (size_t index = 0; index < line.size(); ++index) {
					if (index > 0) text += ' ';
					text += line[index];
				}
				text += '\n';
				line.clear();
			};
			for (size_t ply = 0; ply < moves.size(); ++ply) {
				if (ply % 2 == 0) line.push_back(std::to_string(ply / 2 + 1) + ".");
				line.push_back(moves[ply]);
				if (line.size() >= 12) writeLine();
			}
			line.push_back(result);
			writeLine();
			text += '\n';
			if (std::fwrite(text.data(), 1, text.size(), out) != text.size()) {
				std::fclose(out);
				throw std::runtime_error("writing " + pgnPath + " failed");
			}
			++written;
			if (maxGames != 0 && written >= maxGames) break;
		}
		if (std::fclose(out) != 0) throw std::runtime_error("writing " + pgnPath + " failed");
		std::printf("%llu games written to %s\n", (unsigned long long)written, pgnPath.c_str());
		return written;
	}
}
