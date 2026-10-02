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
 * pgn2gam - turns a pgn of played games into the packed game file the nnue training of Qapla reads.
 *
 *   pgn2gam --in=<pgn> --out=<game file> [--wdl=result|none] [--maxgames=N]
 *
 * See GameFile::PgnToGameFile for what it reads and what it refuses.
 * Exit code 0 when the file was written, 1 when it was refused, 2 on an error.
 */

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "app-error.h"
#include "cli-settings-manager.h"
#include "game-file/pgn-to-game-file.h"

int main(int argc, char* argv[]) {
	using CliSettings::Manager;
	using CliSettings::ValueType;
	try {
		Manager::registerSetting("in", "The pgn to read: long notation, the values in the comments",
			true, std::nullopt, ValueType::PathExists);
		Manager::registerSetting("out", "The game file to write", true, std::nullopt,
			ValueType::PathParentExists);
		Manager::registerSetting("wdl", "result: the result of a game is stored with its positions; "
			"none: it is not, for games between players of different strength", false,
			std::string("result"), ValueType::String);
		Manager::registerSetting("maxgames", "Stops after this many games, 0 for all", false, 0,
			ValueType::Int);
		const std::vector<std::string> args(argv, argv + argc);
		Manager::parseCommandLine(Manager::mergeWithSettingsFile(args));

		const std::string wdl = Manager::get<std::string>("wdl");
		if (wdl != "result" && wdl != "none") {
			throw AppError::makeInvalidParameters("--wdl takes result or none, not \"" + wdl + "\"");
		}
		const int maxGames = Manager::get<int>("maxgames");
		const GameFile::PgnToGameFile converter{ .useResult = wdl == "result",
			.maxGames = uint64_t(maxGames < 0 ? 0 : maxGames) };
		return converter.convert(Manager::get<std::string>("in"), Manager::get<std::string>("out"))
			? 0 : 1;
	}
	catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << std::endl;
		return 2;
	}
}
