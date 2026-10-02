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
 * @copyright Copyright (c) 2025 Volker Böhm
 */

#include "cpp-array-writer.h"
#include <fstream>
#include <iomanip>
#include <stdexcept>

void CppArrayWriter::write(const std::string& varName,
    const std::string& headerPath,
    const std::string& sourcePath,
    const std::vector<uint8_t>& data) {
    std::ofstream header(headerPath);
    if (!header) throw std::runtime_error("Cannot open header file: " + headerPath);

    header << "#pragma once\n\n";
    header << "#include <cstdint>\n\n";
    header << "extern const uint32_t " << varName << "Size;\n";
    header << "extern const uint32_t " << varName << "[];\n";
    header.close();

    std::ofstream source(sourcePath);
    if (!source) throw std::runtime_error("Cannot open source file: " + sourcePath);

    source << "#include \"" << headerPath.substr(headerPath.find_last_of("/\\") + 1) << "\"\n\n";
    source << "const uint32_t " << varName << "Size = " << data.size() << ";\n";
    source << "const uint32_t " << varName << "[] = {";

    size_t numWords = (data.size() + 3) / 4;

    auto read32 = [&](size_t index) -> uint32_t {
        uint32_t val = 0;
        for (size_t i = 0; i < 4; ++i) {
            size_t pos = index * 4 + i;
            val |= (pos < data.size() ? data[pos] : 0) << (i * 8);
        }
        return val;
        };

    for (size_t i = 0; i < numWords; ++i) {
        if (i % 8 == 0) source << "\n    ";
        source << "0x" << std::hex << std::setw(8) << std::setfill('0') << read32(i);
        if (i + 1 < numWords) source << ",";
    }

    source << "\n};\n";
    source.close();
}
