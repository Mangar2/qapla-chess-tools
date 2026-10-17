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
#pragma once

#include <string>

namespace GameFile {

	class Book;

	/**
	 * Writes a book trained by --bookwdl with the two byte payload qapla-wdl-rate-v1 instead of the
	 * counts - win rate and significance, see WdlRate in book-file.h - and prints how the nodes are
	 * spread over the significances: per significance the nodes, those always lost (rate 0) and those
	 * always won (rate 255). The tree and the moves stay as they are. The book of counts stays the
	 * one to train further; the rates cannot be. A book of counts with scores (--bookwdl of an
	 * evaluated book) gives a book of scores and rates, qapla-score-wdl-rate-v1.
	 */
	struct WdlToRate {
		/**
		 * @throws std::runtime_error if the book cannot be read or written or holds no counts.
		 */
		void convert(const std::string& countPath, const std::string& ratePath) const;
	};

	/**
	 * Writes a book with counts or rates as a book of rates - with the scores of the book as well, if
	 * withScores - and prints the statistics; the book's own payload is left as it was. Rates are
	 * made from the counts when the book has them.
	 */
	void writeRateBook(Book& book, const std::string& ratePath, bool withScores);
}
