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

#include "pgn-to-book.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <vector>

#include "book-file.h"
#include "opening-reader.h"

namespace GameFile {

	bool PgnToBook::convert(const std::string& pgnPath, const std::string& bookPath) const {
		const bool withValues = payload >= VALUE_PAYLOAD;
		Book book;
		book.header.author = "Qapla";
		book.header.name = std::filesystem::path(pgnPath).stem().string();
		book.header.payload = payload;
		if (withValues) book.header.payloadId = VALUE_PAYLOAD_ID;

		OpeningCounters counters;
		uint64_t lines = 0, truncated = 0, skippedUnpackable = 0, sameSign = 0, oppositeSign = 0;
		std::vector<uint32_t> packed;
		readOpenings(pgnPath, counters, [&](const std::vector<PgnMove>& game) {
			const bool more = maxGames == 0 || counters.games < maxGames;
			std::vector<PgnMove> moves = game;
			if (maxPly != 0 && moves.size() > maxPly) {
				moves.resize(maxPly);
				++truncated;
			}
			// Packed in full first: a game with a move the format cannot express leaves no trace.
			packed.clear();
			Board board;
			for (const PgnMove& move : moves) {
				const Move played{ .from = move.from, .to = move.to, .promotion = move.promotion };
				const std::optional<uint32_t> code = packMove9(played, board);
				if (!code) {
					++skippedUnpackable;
					return more;
				}
				packed.push_back(*code);
				board.apply(played);
			}
			Book::NodeIndex node = Book::ROOT;
			std::optional<int> previous;
			for (size_t ply = 0; ply < moves.size(); ++ply) {
				node = book.addMove(node, packed[ply]);
				const std::optional<int> value = moves[ply].value;
				if (!value) continue;
				if (previous) ((*value < 0) != (*previous < 0) ? oppositeSign : sameSign)++;
				previous = value;
				book.node(node).value = codeOfValue(*value);
			}
			++lines;
			return more;
		});

		if (withValues) {
			printPerspective(sameSign, oppositeSign);
			if (const char* refusal = perspectiveRefusal(sameSign, oppositeSign)) {
				std::printf("%s\n", refusal);
				return false;
			}
		}
		book.write(bookPath);
		std::printf("games %llu, lines %llu, moves %llu, leaves %llu\n", (unsigned long long)counters.games,
			(unsigned long long)lines, (unsigned long long)book.size(), (unsigned long long)book.leafCount());
		std::printf("skipped: from a fen %llu, illegal move %llu, moves not leading to the fen %llu, "
			"unpackable move %llu; truncated %llu\n",
			(unsigned long long)counters.skippedFen, (unsigned long long)counters.skippedIllegal,
			(unsigned long long)counters.skippedMismatch, (unsigned long long)skippedUnpackable,
			(unsigned long long)truncated);
		return true;
	}
}
