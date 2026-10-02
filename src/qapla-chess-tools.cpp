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
 * Tools around the data of the Qapla engine.
 *
 *   qapla-chess-tools --pgn2gam in=<pgn> out=<game file> [wdl=result|none] [maxgames=N]
 *   qapla-chess-tools --gam2pgn in=<game file> out=<pgn> [maxgames=N]
 */

#include <iostream>
#include <string>
#include <vector>

#include "app-error.h"
#include "cli-settings-manager.h"
#include "game-file/game-file-to-pgn.h"
#include "game-file/pgn-to-game-file.h"

namespace {

	void registerSettings() {
		using CliSettings::Manager;
		using CliSettings::ValueType;
		Manager::registerGroup("pgn2gam",
			"Converts a pgn of played games in long notation, with the values in the comments, into "
			"the packed game file the nnue training of Qapla reads.",
			true, {
				{ "in", { "The pgn to read", true, std::nullopt, ValueType::PathExists } },
				{ "out", { "The game file to write", true, std::nullopt, ValueType::PathParentExists } },
				{ "wdl", { "result: the result of a game is stored with its positions; none: it is not, "
					"for games between players of different strength", false, std::string("result"),
					ValueType::String } },
				{ "maxgames", { "Stops after this many games, 0 for all", false, 0, ValueType::Int } },
			});
		Manager::registerGroup("gam2pgn",
			"Writes the games of a packed game file back out as a pgn in long notation, without values.",
			true, {
				{ "in", { "The game file to read", true, std::nullopt, ValueType::PathExists } },
				{ "out", { "The pgn to write", true, std::nullopt, ValueType::PathParentExists } },
				{ "maxgames", { "Stops after this many games, 0 for all", false, 0, ValueType::Int } },
			});
	}

	int run() {
		using CliSettings::Manager;
		bool any = false;
		if (const auto group = Manager::getGroupInstance("pgn2gam")) {
			any = true;
			const std::string wdl = group->get<std::string>("wdl");
			if (wdl != "result" && wdl != "none") {
				throw AppError::makeInvalidParameters("wdl takes result or none, not \"" + wdl + "\"");
			}
			const int maxGames = group->get<int>("maxgames");
			GameFile::PgnToGameFile converter{ .useResult = wdl == "result",
				.maxGames = uint64_t(maxGames < 0 ? 0 : maxGames) };
			if (!converter.convert(group->get<std::string>("in"), group->get<std::string>("out"))) {
				return 1;
			}
		}
		if (const auto group = Manager::getGroupInstance("gam2pgn")) {
			any = true;
			const int maxGames = group->get<int>("maxgames");
			GameFile::GameFileToPgn converter{ .maxGames = uint64_t(maxGames < 0 ? 0 : maxGames) };
			converter.convert(group->get<std::string>("in"), group->get<std::string>("out"));
		}
		if (!any) {
			Manager::showHelp();
			return 1;
		}
		return 0;
	}
}

int main(int argc, char* argv[]) {
	try {
		registerSettings();
		const std::vector<std::string> args(argv, argv + argc);
		CliSettings::Manager::parseCommandLine(CliSettings::Manager::mergeWithSettingsFile(args));
		return run();
	}
	catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << std::endl;
		return 2;
	}
}
