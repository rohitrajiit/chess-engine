# ox-engine

A UCI-compatible chess engine written in C++17. No dependencies beyond the standard library.

## Features

- **Board**: 0x88 representation, incremental Zobrist hashing, full FEN support,
  make/unmake move with legality filtering.
- **Move generation**: complete rules — castling, en passant, promotions,
  double pushes; verified against standard perft test positions.
- **Search**: iterative deepening negamax with alpha-beta, principal variation
  search, late move reductions, null-move pruning, check extensions,
  mate-distance pruning, quiescence search, and a transposition table (16 MB).
- **Move ordering**: TT move, MVV-LVA for captures, killer moves, history heuristic.
- **Evaluation**: material + piece-square tables with tapered midgame/endgame
  king safety, bishop pair bonus, and tempo bonus.
- **Draw detection**: threefold repetition, 50-move rule, insufficient material.
- **Time management**: `movetime`, fixed depth, or clock-based (`wtime`/`btime`
  with increments), plus `stop` handling via a dedicated search thread.

## Build

```sh
make          # optimized build -> ./chessengine
make debug    # debug build with assertions + hash self-checks
make test     # run the perft validation suite
```

## Run

The engine speaks the [UCI protocol](https://www.w3.org/TR/2017/NOTE-uji-20170822/)
on stdin/stdout, so it works with any UCI GUI (e.g. Arena, Banksia, Cute Chess):

```sh
./chessengine
```

Example session:

```
uci
position startpos moves e2e4 e7e5
go wtime 3000 btime 3000 winc 100 binc 100
```

Nonstandard debug commands: `perft <depth>`, `eval`, `d` (print FEN).

## Testing

```sh
./chessengine perft     # validates move generation on 6 classic positions
python3 scripts/selfplay.py   # quick self-play smoke test through the UCI loop
```

## Layout

```
src/types.h      squares, pieces, move encoding
src/board.*      position state, make/unmake, FEN, zobrist, draw detection
src/movegen.*    pseudo-legal generation + perft
src/evaluate.*   material + PST evaluation
src/search.*     alpha-beta search, TT, time management
src/uci.cpp      UCI protocol, main()
```
