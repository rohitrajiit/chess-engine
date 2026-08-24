#include "evaluate.h"

namespace {

constexpr int MATERIAL[7] = {0, 100, 320, 330, 500, 900, 0};

// Piece-square tables, white's point of view, index 0 = a8 ... 63 = h1.
// For a black piece the table is mirrored vertically.
constexpr int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};
constexpr int PST_KNIGHT[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50,
};
constexpr int PST_BISHOP[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20,
};
constexpr int PST_ROOK[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0,
};
constexpr int PST_QUEEN[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20,
};
constexpr int PST_KING_MG[64] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20,
};
constexpr int PST_KING_EG[64] = {
   -50,-40,-30,-20,-20,-30,-40,-50,
   -30,-20,-10,  0,  0,-10,-20,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 30, 40, 40, 30,-10,-30,
   -30,-10, 20, 30, 30, 20,-10,-30,
   -30,-30,  0,  0,  0,  0,-30,-30,
   -50,-30,-30,-30,-30,-30,-30,-50,
};

constexpr const int* PST[7] = {
    nullptr, PST_PAWN, PST_KNIGHT, PST_BISHOP, PST_ROOK, PST_QUEEN, nullptr,
};

} // namespace

int evaluate(const Board& b) {
  long long mg = 0, eg = 0;
  int phase = 0;
  int bishops[2] = {0, 0};

  for (int r = 0; r < 8; ++r)
    for (int f = 0; f < 8; ++f) {
      const int sq = SQ(f, r);
      const int p = b.board[sq];
      if (!p) continue;
      const int t = p > 0 ? p : -p;
      const int sign = p > 0 ? 1 : -1;
      // white reads table with rank flipped (tables start at rank 8);
      // black reads it mirrored.
      const int i = p > 0 ? (7 - r) * 8 + f : r * 8 + f;

      if (t == KING) {
        mg += sign * PST_KING_MG[i];
        eg += sign * PST_KING_EG[i];
      } else {
        mg += sign * (MATERIAL[t] + PST[t][i]);
        eg += sign * (MATERIAL[t] + PST[t][i]);
      }

      switch (t) {
        case KNIGHT:
        case BISHOP: phase += 1; break;
        case ROOK: phase += 2; break;
        case QUEEN: phase += 4; break;
      }
      if (t == BISHOP) ++bishops[p > 0 ? WHITE : BLACK];
    }

  if (bishops[WHITE] >= 2) { mg += 30; eg += 30; }
  if (bishops[BLACK] >= 2) { mg -= 30; eg -= 30; }

  if (phase > 24) phase = 24;
  const int score = static_cast<int>((mg * phase + eg * (24 - phase)) / 24);
  return (b.side == WHITE ? score : -score) + 10; // + tempo
}
