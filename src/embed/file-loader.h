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

#pragma once

#include <string>
#include <vector>

 /**
  * Loads binary data from a file into a byte vector.
  */
class FileLoader {
public:
    /**
     * Loads the file at the given path as binary data.
     * @param filePath Path to the binary input file
     * @return Byte vector containing file contents
     * @throws std::runtime_error if reading fails
     */
    static std::vector<uint8_t> loadBinary(const std::string& filePath);
};