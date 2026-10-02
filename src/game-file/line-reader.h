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

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace GameFile {

	/** The characters Python's str.isspace() takes for white space, as far as they are one byte. */
	inline bool isPythonSpace(char c) {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\x0b' || c == '\x0c'
			|| c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
	}

	/**
	 * Reads a text file line by line, ending a line where Python's text mode ends one: at \n, at \r
	 * and at \r\n. A \r\n gives an empty line between the two here, which nothing that reads lines
	 * through this notices - empty lines are skipped anyway. Large reads, because a set of games is
	 * a few gigabytes.
	 */
	class LineReader {
	public:
		explicit LineReader(const std::string& path)
			: file_(std::fopen(path.c_str(), "rb")), buffer_(1 << 22) {
			if (file_ == nullptr) throw std::runtime_error("cannot open " + path);
		}
		~LineReader() { std::fclose(file_); }
		LineReader(const LineReader&) = delete;
		LineReader& operator=(const LineReader&) = delete;

		/** The next line without its end, or false at the end of the file. */
		bool next(std::string& line) {
			line.clear();
			bool any = false;
			while (true) {
				if (position_ == filled_) {
					filled_ = std::fread(buffer_.data(), 1, buffer_.size(), file_);
					position_ = 0;
					if (filled_ == 0) return any;
				}
				any = true;
				const char c = buffer_[position_++];
				if (c == '\n' || c == '\r') return true;
				line += c;
			}
		}

	private:
		std::FILE* file_;
		std::vector<char> buffer_;
		size_t position_ = 0;
		size_t filled_ = 0;
	};
}
