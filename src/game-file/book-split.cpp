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

#include "book-split.h"

#include <cstdio>
#include <filesystem>
#include <stdexcept>

#include "book-file.h"

namespace GameFile {

	std::string BookSplit::partPath(const std::string& outPath, uint32_t part, uint32_t parts) {
		const std::filesystem::path path(outPath);
		const std::string extension = path.has_extension() ? path.extension().string() : ".bok";
		const int digits = parts < 100 ? 2 : (parts < 1000 ? 3 : 4);
		char number[16];
		std::snprintf(number, sizeof(number), "-%0*u", digits, part);
		return (path.parent_path() / (path.stem().string() + number + extension)).string();
	}

	void BookSplit::convert(const std::string& bookPath, const std::string& outPath) const {
		if (parts < 2) throw std::runtime_error("a book is split into two parts at least");
		Book book;
		book.read(bookPath);

		std::vector<Book> pieces(parts);
		for (uint32_t part = 0; part < parts; ++part) {
			Book& piece = pieces[part];
			piece.header = book.header;
			piece.header.name = book.header.name + " " + std::to_string(part + 1) + "/" + std::to_string(parts);
			piece.rootScore = book.rootScore;
			if (!book.wdl.empty()) piece.wdl.assign(1, WdlCount{});
			if (!book.rates.empty()) piece.rates.assign(1, WdlRate{});
		}

		// The line last written to each part, as pairs of a node of the book and its node in the
		// part: a part gets every tenth leaf, whose lines mostly begin like the last one it got, and
		// only the rest of the line has to be added.
		struct Step {
			Book::NodeIndex node;
			Book::NodeIndex pieceNode;
		};
		std::vector<std::vector<Step>> lastLine(parts);
		std::vector<uint64_t> lines(parts, 0);
		std::vector<Book::NodeIndex> line;

		const auto addLine = [&](uint32_t part) {
			Book& piece = pieces[part];
			std::vector<Step>& last = lastLine[part];
			size_t common = 0;
			while (common < last.size() && common < line.size() && last[common].node == line[common]) ++common;
			last.resize(common);
			Book::NodeIndex parent = common == 0 ? Book::ROOT : last.back().pieceNode;
			for (size_t ply = common; ply < line.size(); ++ply) {
				const Book::Node& node = book.node(line[ply]);
				const Book::NodeIndex added = piece.addMove(parent, node.move);
				Book::Node& copy = piece.node(added);
				copy.priority = node.priority;
				copy.value = node.value;
				copy.score = node.score;
				if (!book.wdl.empty()) {
					if (piece.wdl.size() <= added) piece.wdl.resize(added + 1);
					piece.wdl[added] = book.wdl[line[ply]];
				}
				if (!book.rates.empty()) {
					if (piece.rates.size() <= added) piece.rates.resize(added + 1);
					piece.rates[added] = book.rates[line[ply]];
				}
				last.push_back(Step{ .node = line[ply], .pieceNode = added });
				parent = added;
			}
			++lines[part];
		};

		// The leaves in the order of the book: a depth first walk.
		uint64_t leaf = 0;
		Book::NodeIndex next = book.node(Book::ROOT).firstChild;
		if (next != Book::NO_NODE) line.push_back(next);
		while (!line.empty()) {
			const Book::Node& current = book.node(line.back());
			if (current.firstChild != Book::NO_NODE) {
				line.push_back(current.firstChild);
				continue;
			}
			addLine(uint32_t(leaf % parts));
			++leaf;
			while (!line.empty() && book.node(line.back()).nextSibling == Book::NO_NODE) line.pop_back();
			if (line.empty()) break;
			line.back() = book.node(line.back()).nextSibling;
		}

		uint64_t nodes = 0;
		for (uint32_t part = 0; part < parts; ++part) {
			const std::string path = partPath(outPath, part + 1, parts);
			const std::string partFile = path + ".part";
			pieces[part].write(partFile);
			std::filesystem::rename(partFile, path);
			nodes += pieces[part].size();
			std::printf("  %s: %llu lines, %zu nodes\n", path.c_str(), (unsigned long long)lines[part],
				pieces[part].size());
		}
		std::printf("%llu lines of %s split into %u parts, %llu nodes in all of them for %zu in the book\n",
			(unsigned long long)leaf, bookPath.c_str(), parts, (unsigned long long)nodes, book.size());
	}
}
