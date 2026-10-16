# qapla-chess-tools

Tools around the data of the [Qapla](https://github.com/Mangar2/Qapla) chess engine.

## Game files: pgn to gam and back

The nnue training of Qapla reads its positions from a packed game file (`QAPLAGM2`): three bytes
per ply - the move, the result, the value as a win probability. The tool `gamefile` converts between
that file and a pgn in both directions, with exactly the output of the python scripts in the engine's repository
(`src/trainer/convert.py` and `src/trainer/export-pgn.py`), byte for byte, only much faster.

    gamefile --pgn2gam in=<pgn> out=<game file> [wdl=result|none] [maxgames=N] [version=2|3] [payload=N]
             [book=<evaluated book>]
    gamefile --gam2pgn in=<game file> out=<pgn> [maxgames=N] [values=true|false]

Both may be given in one call; `--pgn2gam` runs first.

`--pgn2gam` reads a pgn as qapla-engine-tester writes it with `notation=lan` and `eval=true`: long
algebraic notation, the value of each move in its comment, in pawns, from the side to move. A file
whose values look like they are seen from white is refused. `wdl=none` stores no result with the
positions - right for games between players of different strength, where the result says something
about the players and not about the positions.

`--gam2binpack in=<game file> out=<binpack>` writes the valued positions as Stockfish training
data for nnue-pytorch. A value of the game file is a win probability; it is written as the score
whose training target in nnue-pytorch's loss is that probability, so a Stockfish net learns exactly
the targets Qapla's data holds. The binpack code is copied from nnue-pytorch (GPL 3) into
`src/third-party/nnue-pytorch`, `SOURCE-COMMIT` says from which commit.

`--gam2pgn` writes the games back out in long notation without values. That is what a relabelling
needs: the analysis of qapla-engine-tester takes a pgn, searches every position and writes the
values back into it. With `values=true` it writes the values into the comments, as the tester does,
so that `--pgn2gam` reads them back.

### Version 3: the nine bit move and a payload

`version=3` writes the move of the engine's `src/book9`: nine bits, the index of the moving piece
among the pieces of the side to move and a code for where it goes. The header holds, behind the
version, the payload: the number of bytes every ply carries behind its move, set with `payload=N`,
0 to 255, default 2. A ply is two bytes - move and result - and then its payload. The first two
bytes of the payload hold the value; with a payload below two the file holds no values, the bytes
behind them are free for data of one's own and are written as zeros.

    header   "QAPLAGM2", version 3 (4 bytes LE), payload (4 bytes LE)
    game     length (1 byte), then per ply:
    ply      (msb) 00000 RR MMMMMMMMM (lsb), 2 bytes LE - M move, R result
             payload bytes, the value code in the first two (LE)

Version 2 stays the default: it is what the engine's training reads. The readers take both and find
out from the header which one a file is. There is no direct conversion; it goes through a pgn:

    gamefile --gam2pgn in=old.gam out=games.pgn values=true
    gamefile --pgn2gam in=games.pgn out=new.gam version=3

## Books: pgn to book and back

`gamefile` also writes and reads the 9 bit book of the engine (`src/book9/book.h`, file version
`"9"`): a tree of moves, every node with a payload whose size the file states.

    gamefile --pgn2book in=<pgn or .raw> out=<book> [payload=N] [maxgames=N] [maxply=N]
    gamefile --book2pgn in=<book> out=<pgn> [maxlines=N] [values=true|false]
    gamefile --book2gam in=<evaluated book> out=<game file> [compact=true|false]

`--pgn2book` takes every game of the pgn as a line from the start position; lines that begin alike
share their nodes. It reads with the pgn reader of qapla-engine-tester, so the moves may be in short
or long notation. A file ending in `.raw` is read as the tester's openings, such as
`test/opening/book8ply.raw` there: one line per opening, `<fen>; <moves>; <number>`, the moves in
short notation from the start position. The moves have to lead to the fen of their line, or the line
is left out; the number is not read. `maxply` cuts every line, games from a fen are
left out. `payload=N`, 0 to 255, default 0, gives every node N bytes behind its move; with two or more
the first two hold the value of the move from its comment, converted as `--pgn2gam` does, and where
games share a move the last one with a value decides. Then the perspective is checked as in
`--pgn2gam`.

`--book2pgn` writes one game per leaf, the form the engine's `exportOpenings` writes a position
library in, and `values=true` puts the values into the comments. A book of the 11 bit format (file
version 3) is refused.

The file is the one of the engine, which reads it with `QaplaBook9::Book<PAYLOAD>` when its payload
has the size of the file's:

    header   records of tag (4 bytes), length (4 bytes) and a zero terminated string: author,
             name, book version, file version "9", payload id, node size = 2 + payload;
             a tag of 0 ends them, then the number of nodes (4 bytes), all little endian
    nodes    in preorder - the first child right behind its parent, the right sibling behind
             the whole subtree - each:
             (msb) MMMMMMMMM PPPPP S C (lsb), 2 bytes LE - M move, P priority (16),
             S has a right sibling, C has a child; then the payload bytes, the value code in the
             first two (LE) with the payload id "qapla-value-code-v1"

The move is the same nine bit move as in a game file of version 3, so a book and a game file go
into each other through a pgn:

    gamefile --book2pgn in=lines.bok out=lines.pgn values=true
    gamefile --pgn2gam in=lines.pgn out=lines.gam version=3 wdl=none

### Values for the opening: an evaluated book

The moves of the opening a game was started from carry no value in its pgn - the engines never
searched them - and are missing from the training data. `qapla-engine-tester --bookeval` searches
every position of a book once and writes the scores into a new book (payload id
`qapla-search-score-v1`: every node carries, as two signed bytes, the score of the position its
move is played from, from the side to move, in centipawns; see the README of the tester). With
`book=`, `--pgn2gam` gives every ply without a value that lies in that book the book's score, as
if it had stood in the comment, and counts what is still without one:

    gamefile --pgn2book in=start-positions.pgn out=start-positions.bok
    qapla-engine-tester --concurrency=30 --engine name=hce cmd=<engine> proto=uci tc=depth:9 \
        --bookeval book=start-positions.bok out=start-positions-eval.bok
    gamefile --pgn2gam in=games.pgn out=games.gam book=start-positions-eval.bok

A game is followed through the book as long as its moves are there; the file written is the
normal game file of version 2. `--book2pgn values=true` writes the scores of an evaluated book into
the comments, as the tester writes them.

### The evaluated book as training data of its own

`--book2gam` writes an evaluated book as a game file of version 2, so that every evaluated position
of the book is in the training data exactly once - instead of once per game that starts from it,
the start position ten million times:

    gamefile --book2gam in=start-positions-eval.bok out=start-positions.gam [compact=true]

Every line of the book, from the start position to a leaf, is one game. A ply carries the score of
the position it is played from, converted as `--pgn2gam book=` converts it, so both give a position
the same label - but only in the first line through that position, in the order of the book; in the
other lines it has no value (code 0), which the training skips. The leaves were never searched, and
no game has a result. The output says how many plies carry a value; it has to be the number of
evaluated positions of the book, which it prints beside it.

`compact=true` leaves out what carries no value - the lines without a position seen first in them,
and the plies behind the last value of a line - with the same labels in the file. For the book of
the 10 million start positions that is 4.3 million games and 78 million plies instead of 10 million
and 179 million: about 240 MB instead of 550 MB.

### The readers of qapla-engine-tester

`src/engine-tester` holds the pgn and epd readers of qapla-engine-tester (`src/opening` there) and
every file they include, copied unchanged with the directory layout of the tester, so that its
includes stay as they are. `SOURCE-COMMIT` says from which commit. To take a newer version, copy the
same files again.

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
