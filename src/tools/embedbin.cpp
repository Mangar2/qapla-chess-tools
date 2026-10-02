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

/**
 * embedbin - embeds a binary file into a C++ program as a packed uint32_t array.
 *
 *   embedbin --input=<file> --name=<array name> --header=<.h file> --source=<.cpp file>
 *
 * Writes a header that declares <name> and <name>Size and a source file that defines them; see
 * CppArrayWriter for the layout. Exit code 0 on success, 2 on an error.
 */

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "cli-settings-manager.h"
#include "embed/cpp-array-writer.h"
#include "embed/file-loader.h"

int main(int argc, char* argv[]) {
	using CliSettings::Manager;
	using CliSettings::ValueType;
	try {
		Manager::registerSetting("input", "The binary file to embed", true, std::nullopt,
			ValueType::PathExists);
		Manager::registerSetting("name", "Name of the generated C++ array", true, std::nullopt,
			ValueType::String);
		Manager::registerSetting("header", "The .h file to write", true, std::nullopt,
			ValueType::PathParentExists);
		Manager::registerSetting("source", "The .cpp file to write", true, std::nullopt,
			ValueType::PathParentExists);
		const std::vector<std::string> args(argv, argv + argc);
		Manager::parseCommandLine(Manager::mergeWithSettingsFile(args));

		const std::vector<uint8_t> data = FileLoader::loadBinary(Manager::get<std::string>("input"));
		CppArrayWriter::write(Manager::get<std::string>("name"), Manager::get<std::string>("header"),
			Manager::get<std::string>("source"), data);
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << std::endl;
		return 2;
	}
}
