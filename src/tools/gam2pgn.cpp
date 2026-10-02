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
 * gam2pgn - writes the games of a packed game file of Qapla back out as a pgn in long notation.
 *
 *   gam2pgn --in=<game file> --out=<pgn> [--maxgames=N]
 *
 * See GameFile::GameFileToPgn for what it writes. Exit code 0 on success, 2 on an error.
 */

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "cli-settings-manager.h"
#include "game-file/game-file-to-pgn.h"

int main(int argc, char* argv[]) {
	using CliSettings::Manager;
	using CliSettings::ValueType;
	try {
		Manager::registerSetting("in", "The game file to read", true, std::nullopt,
			ValueType::PathExists);
		Manager::registerSetting("out", "The pgn to write", true, std::nullopt,
			ValueType::PathParentExists);
		Manager::registerSetting("maxgames", "Stops after this many games, 0 for all", false, 0,
			ValueType::Int);
		const std::vector<std::string> args(argv, argv + argc);
		Manager::parseCommandLine(Manager::mergeWithSettingsFile(args));

		const int maxGames = Manager::get<int>("maxgames");
		const GameFile::GameFileToPgn converter{ .maxGames = uint64_t(maxGames < 0 ? 0 : maxGames) };
		converter.convert(Manager::get<std::string>("in"), Manager::get<std::string>("out"));
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << std::endl;
		return 2;
	}
}
