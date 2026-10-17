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

#include "book-wdl-rate.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

#include "book-file.h"

namespace GameFile {

	namespace {

		/** Three counts of up to 2^32 - 1 make at most 34 bits of games. */
		constexpr size_t SIGNIFICANCES = 35;
		/** The significances printed apart from the rest, from 16 games on. */
		constexpr size_t SIGNIFICANT = 5;

		struct Level {
			uint64_t nodes = 0;
			uint64_t alwaysLost = 0;
			uint64_t alwaysWon = 0;
		};

		void printLevel(size_t significance, const Level& level) {
			const uint64_t from = significance == 0 ? 0 : uint64_t(1) << (significance - 1);
			const uint64_t to = significance == 0 ? 0 : (uint64_t(1) << significance) - 1;
			std::printf("  %2zu  %12llu - %-12llu %12llu %12llu %12llu\n", significance,
				(unsigned long long)from, (unsigned long long)to, (unsigned long long)level.nodes,
				(unsigned long long)level.alwaysLost, (unsigned long long)level.alwaysWon);
		}

		void printSum(const char* label, const Level& level) {
			std::printf("  %-31s %12llu %12llu %12llu\n", label, (unsigned long long)level.nodes,
				(unsigned long long)level.alwaysLost, (unsigned long long)level.alwaysWon);
		}
	}

	void writeRateBook(Book& book, const std::string& ratePath) {
		if (book.header.payloadId != WDL_PAYLOAD_ID || book.wdl.empty()) {
			throw std::runtime_error("the book holds no counts; give a book trained by gamefile --bookwdl");
		}
		std::array<Level, SIGNIFICANCES> levels{};
		book.rates.assign(book.wdl.size(), WdlRate{});
		for (size_t node = 1; node < book.wdl.size(); ++node) {
			const WdlRate rate = rateOf(book.wdl[node]);
			book.rates[node] = rate;
			Level& level = levels[rate.significance];
			++level.nodes;
			if (rate.significance != 0 && rate.rate == 0) ++level.alwaysLost;
			if (rate.rate == 255) ++level.alwaysWon;
		}

		const BookHeader counts = book.header;
		book.header.payloadId = RATE_PAYLOAD_ID;
		book.header.payload = RATE_PAYLOAD;
		const std::string partPath = ratePath + ".part";
		try {
			book.write(partPath);
		}
		catch (...) {
			book.header = counts;
			throw;
		}
		book.header = counts;
		std::filesystem::rename(partPath, ratePath);

		std::printf("rates of %zu nodes written to %s\n", book.wdl.size() - 1, ratePath.c_str());
		std::printf("  sig  games                          nodes  always lost  always won\n");
		Level significant, little;
		for (size_t significance = SIGNIFICANT; significance < SIGNIFICANCES; ++significance) {
			const Level& level = levels[significance];
			if (level.nodes == 0) continue;
			printLevel(significance, level);
			significant.nodes += level.nodes;
			significant.alwaysLost += level.alwaysLost;
			significant.alwaysWon += level.alwaysWon;
		}
		printSum("sum from 16 games on", significant);
		std::printf("  below 16 games:\n");
		for (size_t significance = 1; significance < SIGNIFICANT; ++significance) {
			const Level& level = levels[significance];
			printLevel(significance, level);
			little.nodes += level.nodes;
			little.alwaysLost += level.alwaysLost;
			little.alwaysWon += level.alwaysWon;
		}
		printSum("sum of 1 to 15 games", little);
		std::printf("  nodes no game went through: %llu\n", (unsigned long long)levels[0].nodes);
	}

	void WdlToRate::convert(const std::string& countPath, const std::string& ratePath) const {
		Book book;
		book.read(countPath);
		writeRateBook(book, ratePath);
	}
}
