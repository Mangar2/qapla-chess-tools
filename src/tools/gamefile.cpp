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
 * gamefile - converts between a pgn and the packed game file the nnue training of Qapla reads, and
 * between a pgn and a 9 bit book.
 *
 *   gamefile --pgn2gam in=<pgn> out=<game file> [wdl=result|none] [maxgames=N] [version=2|3]
 *            [payload=N]
 *   gamefile --gam2pgn in=<game file> out=<pgn> [maxgames=N] [values=true|false]
 *   gamefile --gam2binpack in=<game file> out=<binpack> [maxgames=N]
 *   gamefile --pgn2book in=<pgn or raw> out=<book> [payload=N] [maxgames=N] [maxply=N]
 *   gamefile --book2pgn in=<book> out=<pgn> [maxlines=N] [values=true|false]
 *
 * Several may be given in one call; they run in this order. See GameFile::PgnToGameFile and
 * GameFile::GameFileToPgn, GameFile::PgnToBook and GameFile::BookToPgn for what is read, written
 * and refused. The readers take both versions
 * and find out from the header which one a file is.
 * Exit code 0 on success, 1 when pgn2gam or pgn2book refused its file, 2 on an error.
 */

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "app-error.h"
#include "cli-settings-manager.h"
#include "game-file/book-to-pgn.h"
#include "game-file/game-file-to-binpack.h"
#include "game-file/game-file-to-pgn.h"
#include "game-file/pgn-to-book.h"
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
				{ "version", { "2: the eleven bit move, the file the engine's training reads; 3: the nine "
					"bit move of book9 and a payload per ply", false, 2, ValueType::Int } },
				{ "payload", { "Version 3 only: bytes per ply behind the move, 0 to 255; the first two "
					"hold the value, with less there are no values", false, 2, ValueType::Int } },
			});
		Manager::registerGroup("gam2pgn",
			"Writes the games of a packed game file back out as a pgn in long notation, by default "
			"without values.",
			true, {
				{ "in", { "The game file to read", true, std::nullopt, ValueType::PathExists } },
				{ "out", { "The pgn to write", true, std::nullopt, ValueType::PathParentExists } },
				{ "maxgames", { "Stops after this many games, 0 for all", false, 0, ValueType::Int } },
				{ "values", { "Writes the values into the comments, in pawns from the side to move, "
					"as pgn2gam reads them", false, false, ValueType::Bool } },
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
		Manager::registerGroup("pgn2book",
			"Builds a 9 bit book of Qapla from the games of a pgn, in any notation, or the lines of a "
			".raw file of the tester's openings, every game a line from the start position.",
			true, {
				{ "in", { "The pgn or .raw file to read", true, std::nullopt, ValueType::PathExists } },
				{ "out", { "The book to write", true, std::nullopt, ValueType::PathParentExists } },
				{ "payload", { "Bytes per node behind the move, 0 to 255; the first two hold the value "
					"of the move, with less there are no values", false, 0, ValueType::Int } },
				{ "maxgames", { "Stops after this many games, 0 for all", false, 0, ValueType::Int } },
				{ "maxply", { "Cuts every line after this many plies, 0 for the whole game", false, 0,
					ValueType::Int } },
			});
		Manager::registerGroup("book2pgn",
			"Writes the lines of a 9 bit book as a pgn, one game per leaf, in long notation.",
			true, {
				{ "in", { "The book to read", true, std::nullopt, ValueType::PathExists } },
				{ "out", { "The pgn to write", true, std::nullopt, ValueType::PathParentExists } },
				{ "maxlines", { "Stops after this many lines, 0 for all", false, 0, ValueType::Int } },
				{ "values", { "Writes the values into the comments, in pawns from the side to move, "
					"as pgn2gam and pgn2book read them", false, false, ValueType::Bool } },
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
			const int version = group->get<int>("version");
			const int payload = group->get<int>("payload");
			if (version != int(GameFile::VERSION_MOVE11) && version != int(GameFile::VERSION_MOVE9)) {
				throw AppError::makeInvalidParameters("version takes 2 or 3, not " + std::to_string(version));
			}
			if (payload < 0 || payload > int(GameFile::MAX_PAYLOAD)) {
				throw AppError::makeInvalidParameters("payload takes 0 to 255, not " + std::to_string(payload));
			}
			if (version == int(GameFile::VERSION_MOVE11) && payload != int(GameFile::VALUE_PAYLOAD)) {
				throw AppError::makeInvalidParameters("payload needs version=3, version 2 has none");
			}
			const GameFile::Format format{ .version = uint32_t(version),
				.payload = version == int(GameFile::VERSION_MOVE9) ? uint32_t(payload) : 0 };
			const GameFile::PgnToGameFile converter{ .useResult = wdl == "result",
				.maxGames = maxGamesOf(group->get<int>("maxgames")), .format = format };
			if (!converter.convert(group->get<std::string>("in"), group->get<std::string>("out"))) {
				return 1;
			}
		}
		if (const auto group = Manager::getGroupInstance("gam2pgn")) {
			any = true;
			const GameFile::GameFileToPgn converter{ .maxGames = maxGamesOf(group->get<int>("maxgames")),
				.withValues = group->get<bool>("values") };
			converter.convert(group->get<std::string>("in"), group->get<std::string>("out"));
		}
		if (const auto group = Manager::getGroupInstance("gam2binpack")) {
			any = true;
			const GameFile::GameFileToBinpack converter{ .maxGames = maxGamesOf(group->get<int>("maxgames")) };
			converter.convert(group->get<std::string>("in"), group->get<std::string>("out"));
		}
		if (const auto group = Manager::getGroupInstance("pgn2book")) {
			any = true;
			const int payload = group->get<int>("payload");
			const int maxPly = group->get<int>("maxply");
			if (payload < 0 || payload > int(GameFile::MAX_PAYLOAD)) {
				throw AppError::makeInvalidParameters("payload takes 0 to 255, not " + std::to_string(payload));
			}
			if (maxPly < 0) {
				throw AppError::makeInvalidParameters("maxply takes 0 or more, not " + std::to_string(maxPly));
			}
			const GameFile::PgnToBook converter{ .payload = uint32_t(payload),
				.maxGames = maxGamesOf(group->get<int>("maxgames")), .maxPly = uint32_t(maxPly) };
			if (!converter.convert(group->get<std::string>("in"), group->get<std::string>("out"))) {
				return 1;
			}
		}
		if (const auto group = Manager::getGroupInstance("book2pgn")) {
			any = true;
			const GameFile::BookToPgn converter{ .maxLines = maxGamesOf(group->get<int>("maxlines")),
				.withValues = group->get<bool>("values") };
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
