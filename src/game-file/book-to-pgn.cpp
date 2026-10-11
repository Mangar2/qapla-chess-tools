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

#include "book-to-pgn.h"

#include <cstdio>
#include <stdexcept>
#include <vector>

#include "book-file.h"

namespace GameFile {

	uint64_t BookToPgn::convert(const std::string& bookPath, const std::string& pgnPath) const {
		Book book;
		book.read(bookPath);
		std::FILE* out = std::fopen(pgnPath.c_str(), "wb");
		if (out == nullptr) throw std::runtime_error("cannot create " + pgnPath);

		// A depth first walk, one frame per ply of the line: the node and the board it is played on.
		struct Frame {
			Book::NodeIndex node;
			Board board;
			std::string move;
		};
		std::vector<Frame> line;
		uint64_t written = 0;
		std::string text;
		bool failed = false;

		const auto writeLine = [&]() {
			text = "[Event \"Qapla position library\"]\n[Result \"*\"]\n\n";
			for (size_t ply = 0; ply < line.size(); ++ply) {
				if (ply % 2 == 0) text += std::to_string(ply / 2 + 1) + ". ";
				text += line[ply].move;
				text += ' ';
			}
			text += "*\n\n";
			if (std::fwrite(text.data(), 1, text.size(), out) != text.size()) failed = true;
			++written;
		};

		// Enters a node: unpacks its move on the board of its parent.
		const auto enter = [&](Book::NodeIndex node, const Board& parent) {
			const Book::Node& entry = book.node(node);
			const Move move = unpackMove9(entry.move, parent);
			Frame frame{ .node = node, .board = parent, .move = {} };
			frame.move += char('a' + move.from % 8);
			frame.move += char('1' + move.from / 8);
			frame.move += char('a' + move.to % 8);
			frame.move += char('1' + move.to / 8);
			// A piece code halved is 2 for a knight, 3 bishop, 4 rook, 5 queen.
			if (move.promotion != NO_PIECE) frame.move += "nbrq"[(move.promotion >> 1) - 2];
			if (withValues && entry.value != NO_GAME_VALUE) appendValueComment(frame.move, entry.value);
			frame.board.apply(move);
			line.push_back(std::move(frame));
		};

		try {
			const Board start;
			if (book.node(Book::ROOT).firstChild != Book::NO_NODE) enter(book.node(Book::ROOT).firstChild, start);
			while (!line.empty() && !failed && (maxLines == 0 || written < maxLines)) {
				const Book::Node& current = book.node(line.back().node);
				if (current.firstChild != Book::NO_NODE) {
					enter(current.firstChild, line.back().board);
					continue;
				}
				writeLine();
				// Back up to the first node with a right sibling, and go on there.
				while (!line.empty() && book.node(line.back().node).nextSibling == Book::NO_NODE) line.pop_back();
				if (line.empty()) break;
				const Book::NodeIndex sibling = book.node(line.back().node).nextSibling;
				line.pop_back();
				enter(sibling, line.empty() ? start : line.back().board);
			}
		}
		catch (...) {
			std::fclose(out);
			throw;
		}
		if (std::fclose(out) != 0 || failed) throw std::runtime_error("writing " + pgnPath + " failed");
		std::printf("%llu lines written to %s\n", (unsigned long long)written, pgnPath.c_str());
		return written;
	}
}
