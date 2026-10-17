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

#include "book-to-game-file.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "book-file.h"
#include "game-file.h"

namespace GameFile {

	namespace {

		/** Three counts of up to 2^32 - 1 make at most 34 bits of games. */
		constexpr size_t SIGNIFICANCES = 35;

		/** What the mixing did, per number of games as floor(log2 N) + 1. */
		struct MixLevel {
			uint64_t positions = 0;
			double shift = 0.0;
		};

		struct MixStatistics {
			std::array<MixLevel, SIGNIFICANCES> levels{};
			uint64_t withoutGames = 0;
			uint64_t mates = 0;
		};

		/** The results of the games through a position, for the side to move in it. */
		WdlCount countsOf(const Book& book, Book::NodeIndex from, const WdlCount& rootCounts) {
			if (from == Book::ROOT) return rootCounts;
			// Counted for the side that played the node's move: the other side moves in its position.
			const WdlCount& node = book.wdl[from];
			return WdlCount{ .wins = node.losses, .draws = node.draws, .losses = node.wins };
		}

		uint32_t mixedCode(int16_t score, const WdlCount& counts, BookToGameFile::WdlWeight weight, double value,
			MixStatistics& statistics) {
			const uint64_t games = uint64_t(counts.wins) + counts.draws + counts.losses;
			if (score > MAX_SCORE_CENTIPAWNS || score < -MAX_SCORE_CENTIPAWNS) {
				++statistics.mates;
				return codeOfValue(valueOfScore(score));
			}
			if (games == 0) {
				++statistics.withoutGames;
				return codeOfValue(valueOfScore(score));
			}
			// The same expression codeOfValue turns a score into a probability with.
			const double p = 1.0 / (1.0 + std::exp(-double(score) / VALUE_SCALE));
			const double r = (double(counts.wins) + double(counts.draws) / 2.0) / double(games);
			const double w = weight == BookToGameFile::WdlWeight::Fixed ? value
				: double(games) / (double(games) + value);
			const double target = (1.0 - w) * p + w * r;
			size_t significance = 0;
			for (uint64_t rest = games; rest != 0; rest >>= 1) ++significance;
			MixLevel& level = statistics.levels[significance];
			++level.positions;
			level.shift += std::fabs(target - p);
			return codeOfProbability(target);
		}

		void printMixStatistics(const MixStatistics& statistics, BookToGameFile::WdlWeight weight, double value) {
			uint64_t adjusted = 0;
			double shift = 0.0;
			for (const MixLevel& level : statistics.levels) {
				adjusted += level.positions;
				shift += level.shift;
			}
			std::printf("wdl, %s %g: %llu positions adjusted, mean |target - p| %.4f; unchanged: %llu "
				"without games, %llu mate scores\n",
				weight == BookToGameFile::WdlWeight::Fixed ? "fixed w =" : "w = N / (N + k), k =", value,
				(unsigned long long)adjusted,
				adjusted == 0 ? 0.0 : shift / double(adjusted), (unsigned long long)statistics.withoutGames,
				(unsigned long long)statistics.mates);
			std::printf("  sig  games                      positions  mean |target - p|\n");
			for (size_t significance = 1; significance < SIGNIFICANCES; ++significance) {
				const MixLevel& level = statistics.levels[significance];
				if (level.positions == 0) continue;
				const uint64_t from = uint64_t(1) << (significance - 1);
				const uint64_t to = (uint64_t(1) << significance) - 1;
				std::printf("  %2zu  %10llu - %-10llu %12llu  %.4f\n", significance, (unsigned long long)from,
					(unsigned long long)to, (unsigned long long)level.positions,
					level.shift / double(level.positions));
			}
		}
	}

	uint64_t BookToGameFile::convert(const std::string& bookPath, const std::string& gamePath) const {
		Book book;
		book.read(bookPath);
		if (!hasScores(book.header.payloadId)) {
			throw std::runtime_error(bookPath + " holds no search scores; give a book evaluated by "
				"qapla-engine-tester --bookeval");
		}

		// Whether the position a node's children are played from has had its value written, by the
		// index of that node; the root stands for the start position.
		std::vector<bool> written(book.size() + 1, false);
		uint64_t evaluated = 0;
		// A position a move is played from, with a score - the ones a game file can carry.
		for (Book::NodeIndex node = 0; node <= book.size(); ++node) {
			const Book::NodeIndex child = book.node(node).firstChild;
			if (child != Book::NO_NODE && book.scoreBefore(child, node) != NO_SCORE) ++evaluated;
		}
		const Book::NodeIndex firstMove = book.node(Book::ROOT).firstChild;

		const bool mixing = wdl != WdlWeight::None;
		if (mixing && book.wdl.empty()) {
			throw std::runtime_error(bookPath + " holds no counts; give a book of scores and counts, "
				"written by --bookwdl of an evaluated book");
		}
		// The start position has no node: its games are those of the moves from it, counted for white.
		WdlCount rootCounts;
		if (mixing) {
			for (Book::NodeIndex child = firstMove; child != Book::NO_NODE; child = book.node(child).nextSibling) {
				const WdlCount& count = book.wdl[child];
				rootCounts.wins += count.wins;
				rootCounts.draws += count.draws;
				rootCounts.losses += count.losses;
			}
		}
		MixStatistics mixStatistics;

		// Written to a file of its own first, renamed when it is complete.
		const std::string partPath = gamePath + ".part";
		Writer writer(partPath, Format{});

		// A depth first walk, one frame per ply of the line: the node and the board it is played on.
		struct Frame {
			Book::NodeIndex node;
			Board board;
			Move move;
		};
		std::vector<Frame> line;
		Game game;
		uint64_t games = 0, plies = 0, withValue = 0, cut = 0, lostValues = 0;

		const auto writeLine = [&]() {
			game.clear();
			for (size_t ply = 0; ply < line.size(); ++ply) {
				const Book::NodeIndex from = ply == 0 ? Book::ROOT : line[ply - 1].node;
				const int16_t score = book.scoreBefore(line[ply].node, from);
				const bool first = score != NO_SCORE && !written[from];
				if (ply >= MAX_PLIES) {
					// The game file has one byte for the length of a game.
					if (first) ++lostValues;
					continue;
				}
				const Move& move = line[ply].move;
				const std::optional<int> packed = packMove(move.from, move.to, move.promotion);
				if (!packed) throw std::runtime_error("a move of the book has no eleven bit code");
				uint32_t value = NO_GAME_VALUE;
				if (first) {
					value = mixing ? mixedCode(score, countsOf(book, from, rootCounts), wdl, wdlValue, mixStatistics)
						: codeOfValue(valueOfScore(score));
					written[from] = true;
					++withValue;
				}
				game.push_back(Record{ .move = uint32_t(*packed), .value = value, .result = RESULT_NONE });
			}
			if (line.size() > MAX_PLIES) ++cut;
			if (compact) {
				while (!game.empty() && game.back().value == NO_GAME_VALUE) game.pop_back();
				if (game.empty()) return;
			}
			writer.write(game);
			++games;
			plies += game.size();
		};

		const auto enter = [&](Book::NodeIndex node, const Board& parent) {
			const Move move = unpackMove9(book.node(node).move, parent);
			Frame frame{ .node = node, .board = parent, .move = move };
			frame.board.apply(move);
			line.push_back(frame);
		};

		const Board start;
		if (firstMove != Book::NO_NODE) enter(firstMove, start);
		while (!line.empty()) {
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
		writer.close();
		std::filesystem::rename(partPath, gamePath);

		std::printf("games %llu, plies %llu, with a value %llu; evaluated positions of the book %llu\n",
			(unsigned long long)games, (unsigned long long)plies, (unsigned long long)withValue,
			(unsigned long long)evaluated);
		if (mixing) printMixStatistics(mixStatistics, wdl, wdlValue);
		if (cut != 0) {
			std::printf("%llu lines longer than %zu plies were cut, %llu values lost with them\n",
				(unsigned long long)cut, MAX_PLIES, (unsigned long long)lostValues);
		}
		return games;
	}
}
