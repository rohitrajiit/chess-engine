#pragma once

#include <cstdint>
#include <string>

enum Color : int { WHITE = 0, BLACK = 1 };

constexpr inline int other(int c) { return c ^ 1; }

enum PieceType : int {
  PAWN = 1,
  KNIGHT = 2,
  BISHOP = 3,
  ROOK = 4,
  QUEEN = 5,
  KING = 6,
};

// 0x88 board indexing: a1 = 0x00, h1 = 0x07, a8 = 0x70, h8 = 0x77.
constexpr inline int SQ(int file, int rank) { return rank * 16 + file; }
constexpr inline int fileOf(int s) { return s & 7; }
constexpr inline int rankOf(int s) { return s >> 4; }
constexpr inline bool validSquare(int s) { return (s & 0x88) == 0; }
// a1 = 0 .. h8 = 63
constexpr inline int sq64(int s) { return rankOf(s) * 8 + fileOf(s); }

inline std::string squareName(int s) {
  return std::string{char('a' + fileOf(s)), char('1' + rankOf(s))};
}

enum MoveFlag : int {
  FLAG_QUIET = 0,
  FLAG_DOUBLE = 1,   // pawn double push
  FLAG_EP = 2,       // en passant capture
  FLAG_CASTLE_K = 3, // king side castle
  FLAG_CASTLE_Q = 4, // queen side castle
};

// Move encoding:
// bits  0-6  from square (0x88)
// bits  7-13 to square   (0x88)
// bits 14-16 flag
// bits 17-19 promotion piece type (0 if none)
// bits 20-23 captured piece type  (0 if none)
using Move = uint32_t;
constexpr Move MOVE_NONE = 0;

constexpr inline Move makeMove(int from, int to, int flag, int promo, int captured) {
  return static_cast<Move>(from | (to << 7) | (flag << 14) | (promo << 17) |
                           (captured << 20));
}
constexpr inline int moveFrom(Move m) { return m & 127; }
constexpr inline int moveTo(Move m) { return (m >> 7) & 127; }
constexpr inline int moveFlag(Move m) { return (m >> 14) & 7; }
constexpr inline int movePromo(Move m) { return (m >> 17) & 7; }
constexpr inline int moveCaptured(Move m) { return (m >> 20) & 15; }

inline std::string moveToUci(Move m) {
  std::string s = squareName(moveFrom(m)) + squareName(moveTo(m));
  if (int p = movePromo(m)) {
    static const char pc[8] = {' ', ' ', 'n', 'b', 'r', 'q', ' ', ' '};
    s += pc[p];
  }
  return s;
}
