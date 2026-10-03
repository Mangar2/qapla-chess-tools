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
 * gamefile - converts between a pgn and the packed game file the nnue training of Qapla reads.
 *
 *   gamefile --pgn2gam in=<pgn> out=<game file> [wdl=result|none] [maxgames=N]
 *   gamefile --gam2pgn in=<game file> out=<pgn> [maxgames=N]
 *   gamefile --gam2binpack in=<game file> out=<binpack> [maxgames=N]
 *
 * Several may be given in one call; they run in this order. See GameFile::PgnToGameFile and
 * GameFile::GameFileToPgn for what is read, written and refused.
 * Exit code 0 on success, 1 when pgn2gam refused its file, 2 on an error.
 */

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "app-error.h"
#include "cli-settings-manager.h"
#include "game-file/game-file-to-binpack.h"
#include "game-file/game-file-to-pgn.h"
#include "game-file/pgn-to-game-file.h"
#include "string-helper.h"

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
		Manager::registerGroup("gam2binpack",
			"Writes the valued positions of a game file as Stockfish training data (.binpack) for "
			"nnue-pytorch, the values turned into the scores whose training target is the stored "
			"probability.",
			true, {
				{ "in", { "The game file to read", true, std::nullopt, ValueType::PathExists } },
				{ "out", { "The .binpack to write", true, std::nullopt, ValueType::PathParentExists } },
				{ "maxgames", { "Stops after this many games, 0 for all", false, 0, ValueType::Int } },
			});
	}

	uint64_t maxGamesOf(int value) { return uint64_t(value < 0 ? 0 : value); }

	int run() {
		using CliSettings::Manager;
		bool any = false;
		if (const auto group = Manager::getGroupInstance("pgn2gam")) {
			any = true;
			const std::string wdl = to_lowercase(group->get<std::string>("wdl"));
			if (wdl != "result" && wdl != "none") {
				throw AppError::makeInvalidParameters("wdl takes result or none, not \"" + wdl + "\"");
			}
			const GameFile::PgnToGameFile converter{ .useResult = wdl == "result",
				.maxGames = maxGamesOf(group->get<int>("maxgames")) };
			if (!converter.convert(group->get<std::string>("in"), group->get<std::string>("out"))) {
				return 1;
			}
		}
		if (const auto group = Manager::getGroupInstance("gam2pgn")) {
			any = true;
			const GameFile::GameFileToPgn converter{ .maxGames = maxGamesOf(group->get<int>("maxgames")) };
			converter.convert(group->get<std::string>("in"), group->get<std::string>("out"));
		}
		if (const auto group = Manager::getGroupInstance("gam2binpack")) {
			any = true;
			const GameFile::GameFileToBinpack converter{ .maxGames = maxGamesOf(group->get<int>("maxgames")) };
			converter.convert(group->get<std::string>("in"), group->get<std::string>("out"));
		}
		if (!any) {
			Manager::showHelp();
			return 2;
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
