# qapla-chess-tools

Tools around the data of the [Qapla](https://github.com/Mangar2/Qapla) chess engine.

## Game files: pgn to gam and back

The nnue training of Qapla reads its positions from a packed game file (`QAPLAGM2`): three bytes
per ply - the move, the result, the value as a win probability. The tool `gamefile` converts between
that file and a pgn in both directions, with exactly the output of the python scripts in the engine's repository
(`src/trainer/convert.py` and `src/trainer/export-pgn.py`), byte for byte, only much faster.

    gamefile --pgn2gam in=<pgn> out=<game file> [wdl=result|none] [maxgames=N]
    gamefile --gam2pgn in=<game file> out=<pgn> [maxgames=N]

Both may be given in one call; `--pgn2gam` runs first.

`--pgn2gam` reads a pgn as qapla-engine-tester writes it with `notation=lan` and `eval=true`: long
algebraic notation, the value of each move in its comment, in pawns, from the side to move. A file
whose values look like they are seen from white is refused. `wdl=none` stores no result with the
positions - right for games between players of different strength, where the result says something
about the players and not about the positions.

`--gam2pgn` writes the games back out in long notation without values. That is what a relabelling
needs: the analysis of qapla-engine-tester takes a pgn, searches every position and writes the
values back into it.

## Embedding a binary file: embedbin

    embedbin --input=<file> --name=<array name> --header=<.h file> --source=<.cpp file>

Writes the file as a packed `const uint32_t <name>[]` with its length in bytes in `<name>Size`: the
header declares both, the source file defines them, four bytes to a word, little endian, the last word
padded with zeros. The way to compile an opening book or a net into a program.

## Building

Every tool is an executable of its own and can be built alone. CMake presets, Clang and Ninja on
every system:

    cmake --preset release
    cmake --build --preset release                  every tool
    cmake --build --preset release-gamefile         gamefile only
    cmake --build --preset release-embedbin         embedbin only

The binaries land in `build/release/`. `debug` and `debug-<tool>` work the same way. Visual Studio
2022 opens the folder and reads the presets directly.

### Operating systems in CMakeLists.txt

Everything that depends on the operating system lives in exactly one of three sections of
`CMakeLists.txt`: WINDOWS, LINUX, MACOS. There is never a condition over two systems - no
`if(NOT WIN32)`, no `if(UNIX)`, no `else()` that silently means one of them. A setting is either the
same on all three systems and stands in the common part, or it goes into all three sections. This has
to stay that way: a shared condition hides which systems a setting reaches, and the next change to it
changes a system nobody looked at.

A new tool is `src/tools/<name>.cpp` with its `main`, one `qapla_tool(<name> ...)` line in
`CMakeLists.txt` and a build preset `release-<name>` beside the others.
