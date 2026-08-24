#pragma once

#include "types.h"

constexpr int MAX_HIST = 2048;

struct Undo {
  Move move;
  int castling;
  int ep;
  int halfmove;
  uint64_t hash;
};

class Board {
public:
  // Signed piece: positive = white, negative = black, 0 = empty.
  int8_t board[128]{};
  int side = WHITE;     // color to move
  int castling = 0;     // bits: 1=WK 2=WQ 4=BK 8=BQ
  int ep = -1;          // en passant target square (0x88) or -1
  int halfmove = 0;     // 50-move rule clock
  int fullmove = 1;
  int kingSq[2] = {-1, -1};
  uint64_t hash = 0;    // zobrist hash of current position

  // Position history (hash after each played move). undos is indexed in lockstep.
  uint64_t hist[MAX_HIST]{};
  Undo undos[MAX_HIST];
  int histLen = 0;

  Board() { setStartPos(); }

  void setStartPos();
  void setFen(const std::string& fen);
  std::string fen() const;

  // makeMove returns false (and restores the position) if the move leaves the
  // mover's king in check.
  bool makeMove(Move m);
  void unmakeMove(Move m);
  void makeNull();
  void unmakeNull();

  bool isAttacked(int sq, int byColor) const;
  bool inCheck(int color) const { return isAttacked(kingSq[color], other(color)); }

  bool drawByRep() const;
  bool insufficientMaterial() const;
  bool hasNonPawnMaterial(int color) const;

  uint64_t computeHash() const;
};
