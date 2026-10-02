# qapla-chess-tools

Tools around the data of the [Qapla](https://github.com/Mangar2/Qapla) chess engine.

## Game files: pgn to gam and back

The nnue training of Qapla reads its positions from a packed game file (`QAPLAGM2`): three bytes
per ply - the move, the result, the value as a win probability. These two commands convert between
that file and a pgn, with exactly the output of the python scripts in the engine's repository
(`src/trainer/convert.py` and `src/trainer/export-pgn.py`), byte for byte, only much faster.

    pgn2gam --in=<pgn> --out=<game file> [--wdl=result|none] [--maxgames=N]
    gam2pgn --in=<game file> --out=<pgn> [--maxgames=N]

`pgn2gam` reads a pgn as qapla-engine-tester writes it with `notation=lan` and `eval=true`: long
algebraic notation, the value of each move in its comment, in pawns, from the side to move. A file
whose values look like they are seen from white is refused. `--wdl=none` stores no result with the
positions - right for games between players of different strength, where the result says something
about the players and not about the positions.

`gam2pgn` writes the games back out in long notation without values. That is what a relabelling
needs: the analysis of qapla-engine-tester takes a pgn, searches every position and writes the
values back into it.

## Building

Every tool is an executable of its own and can be built alone. CMake presets, Clang and Ninja on
every system:

    cmake --preset release
    cmake --build --preset release                  every tool
    cmake --build --preset release-pgn2gam          pgn2gam only
    cmake --build --preset release-gam2pgn          gam2pgn only

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
