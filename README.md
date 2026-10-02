# qapla-chess-tools

Tools around the data of the [Qapla](https://github.com/Mangar2/Qapla) chess engine.

## Game files: pgn to gam and back

The nnue training of Qapla reads its positions from a packed game file (`QAPLAGM2`): three bytes
per ply - the move, the result, the value as a win probability. These two commands convert between
that file and a pgn, with exactly the output of the python scripts in the engine's repository
(`src/trainer/convert.py` and `src/trainer/export-pgn.py`), byte for byte, only much faster.

    qapla-chess-tools --pgn2gam in=<pgn> out=<game file> [wdl=result|none] [maxgames=N]
    qapla-chess-tools --gam2pgn in=<game file> out=<pgn> [maxgames=N]

`--pgn2gam` reads a pgn as qapla-engine-tester writes it with `notation=lan` and `eval=true`: long
algebraic notation, the value of each move in its comment, in pawns, from the side to move. A file
whose values look like they are seen from white is refused. `wdl=none` stores no result with the
positions - right for games between players of different strength, where the result says something
about the players and not about the positions.

`--gam2pgn` writes the games back out in long notation without values. That is what a relabelling
needs: the analysis of qapla-engine-tester takes a pgn, searches every position and writes the
values back into it.

## Building

    cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
    cmake --build build/release

The binary is `build/release/qapla-chess-tools`. On Windows the Visual Studio project
`qapla-chess-tools.sln` builds the same sources.
