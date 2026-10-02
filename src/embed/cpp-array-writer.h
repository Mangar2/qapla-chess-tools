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
  * Writes binary data as a packed uint32_t array to .h/.cpp files.
  */
class CppArrayWriter {
public:
    /**
     * Writes the given data as a packed const uint32_t[] array to the specified files.
     * @param varName Name of the generated array
     * @param headerPath Path to the .h output file
     * @param sourcePath Path to the .cpp output file
     * @param data Binary data to embed
     * @throws std::runtime_error on file I/O errors
     */
    static void write(const std::string& varName,
        const std::string& headerPath,
        const std::string& sourcePath,
        const std::vector<uint8_t>& data);
};
