#include "movegen.h"

#include <cctype>

namespace {

const int knightD[8] = {-33, -31, -18, -14, 14, 18, 31, 33};
const int kingD[8] = {-17, -16, -15, -1, 1, 15, 16, 17};
const int bishopD[4] = {-17, -15, 15, 17};
const int rookD[4] = {-16, -1, 1, 16};

void genPawns(const Board& b, MoveList& ml, GenType type) {
  const int sgn = b.side == WHITE ? 1 : -1;
  const int dir = 16 * sgn;
  const int startRank = sgn > 0 ? 1 : 6;

  for (int r = 0; r < 8; ++r)
    for (int f = 0; f < 8; ++f) {
      const int from = SQ(f, r);
      const int p = b.board[from];
      if (p != (sgn > 0 ? PAWN : -PAWN)) continue;

      // pushes
      const int fwd = from + dir;
      if (validSquare(fwd) && b.board[fwd] == 0) {
        if (rankOf(fwd) == (sgn > 0 ? 7 : 0)) {
          for (int pr : {QUEEN, ROOK, BISHOP, KNIGHT})
            ml.add(makeMove(from, fwd, FLAG_QUIET, pr, 0));
        } else {
          if (type == GEN_ALL) {
            ml.add(makeMove(from, fwd, FLAG_QUIET, 0, 0));
            if (rankOf(from) == startRank && b.board[from + 2 * dir] == 0)
              ml.add(makeMove(from, from + 2 * dir, FLAG_DOUBLE, 0, 0));
          }
        }
      }

      // captures / en passant
      for (int dd = dir - 1; dd <= dir + 1; dd += 2) {
        const int to = from + dd;
        if (!validSquare(to)) continue;
        const int tp = b.board[to];
        if (tp && (tp > 0) == (sgn > 0)) continue; // own piece
        if (tp) {
          const int cap = tp > 0 ? tp : -tp;
          if (rankOf(to) == (sgn > 0 ? 7 : 0)) {
            for (int pr : {QUEEN, ROOK, BISHOP, KNIGHT})
              ml.add(makeMove(from, to, FLAG_QUIET, pr, cap));
          } else {
            ml.add(makeMove(from, to, FLAG_QUIET, 0, cap));
          }
        } else if (to == b.ep) {
          ml.add(makeMove(from, to, FLAG_EP, 0, PAWN));
        }
      }
    }
}

} // namespace

namespace {

void genPieces(const Board& b, MoveList& ml, GenType type) {
  const bool white = b.side == WHITE;

  for (int r = 0; r < 8; ++r)
    for (int f = 0; f < 8; ++f) {
      const int from = SQ(f, r);
      const int p = b.board[from];
      if (p == 0 || (p > 0) != white || (p == PAWN || p == -PAWN)) continue;

      const int t = p > 0 ? p : -p;

      if (t == KNIGHT || t == KING) {
        const int* dirs = t == KNIGHT ? knightD : kingD;
        for (int i = 0; i < 8; ++i) {
          const int to = from + dirs[i];
          if (!validSquare(to)) continue;
          const int tp = b.board[to];
          if (tp && (tp > 0) == white) continue;
          if (tp || type == GEN_ALL)
            ml.add(makeMove(from, to, FLAG_QUIET, 0, tp > 0 ? tp : -tp));
        }
      } else {
        if (t == BISHOP || t == QUEEN) {
          for (int i = 0; i < 4; ++i) {
            for (int to = from + bishopD[i]; validSquare(to); to += bishopD[i]) {
              const int tp = b.board[to];
              if (tp) {
                if ((tp > 0) == white) break;
                ml.add(makeMove(from, to, FLAG_QUIET, 0, tp > 0 ? tp : -tp));
                break;
              }
              if (type == GEN_ALL) ml.add(makeMove(from, to, FLAG_QUIET, 0, 0));
            }
          }
        }
        if (t == ROOK || t == QUEEN) {
          for (int i = 0; i < 4; ++i) {
            for (int to = from + rookD[i]; validSquare(to); to += rookD[i]) {
              const int tp = b.board[to];
              if (tp) {
                if ((tp > 0) == white) break;
                ml.add(makeMove(from, to, FLAG_QUIET, 0, tp > 0 ? tp : -tp));
                break;
              }
              if (type == GEN_ALL) ml.add(makeMove(from, to, FLAG_QUIET, 0, 0));
            }
          }
        }
      }
    }
}

void genCastles(const Board& b, MoveList& ml) {
  if (b.inCheck(b.side)) return;

  if (b.side == WHITE) {
    if ((b.castling & 1) && !b.board[SQ(5, 0)] && !b.board[SQ(6, 0)] &&
        !b.isAttacked(SQ(4, 0), BLACK) && !b.isAttacked(SQ(5, 0), BLACK))
      ml.add(makeMove(SQ(4, 0), SQ(6, 0), FLAG_CASTLE_K, 0, 0));
    if ((b.castling & 2) && !b.board[SQ(3, 0)] && !b.board[SQ(2, 0)] &&
        !b.board[SQ(1, 0)] && !b.isAttacked(SQ(4, 0), BLACK) &&
        !b.isAttacked(SQ(3, 0), BLACK))
      ml.add(makeMove(SQ(4, 0), SQ(2, 0), FLAG_CASTLE_Q, 0, 0));
  } else {
    if ((b.castling & 4) && !b.board[SQ(5, 7)] && !b.board[SQ(6, 7)] &&
        !b.isAttacked(SQ(4, 7), WHITE) && !b.isAttacked(SQ(5, 7), WHITE))
      ml.add(makeMove(SQ(4, 7), SQ(6, 7), FLAG_CASTLE_K, 0, 0));
    if ((b.castling & 8) && !b.board[SQ(3, 7)] && !b.board[SQ(2, 7)] &&
        !b.board[SQ(1, 7)] && !b.isAttacked(SQ(4, 7), WHITE) &&
        !b.isAttacked(SQ(3, 7), WHITE))
      ml.add(makeMove(SQ(4, 7), SQ(2, 7), FLAG_CASTLE_Q, 0, 0));
  }
}

} // namespace

void generateMoves(const Board& b, MoveList& out, GenType type) {
  genPawns(b, out, type);
  genPieces(b, out, type);
  if (type == GEN_ALL) genCastles(b, out);
}

uint64_t perft(Board& b, int depth) {
  if (depth == 0) return 1;
  MoveList ml;
  generateMoves(b, ml, GEN_ALL);
  uint64_t nodes = 0;
  for (int i = 0; i < ml.count; ++i)
    if (b.makeMove(ml.v[i])) {
      nodes += perft(b, depth - 1);
      b.unmakeMove(ml.v[i]);
    }
  return nodes;
}

Move uciToMove(Board& b, const std::string& s) {
  if (s.size() < 4) return MOVE_NONE;
  MoveList ml;
  generateMoves(b, ml, GEN_ALL);
  for (int i = 0; i < ml.count; ++i) {
    const Move m = ml.v[i];
    if (squareName(moveFrom(m)) != s.substr(0, 2)) continue;
    if (squareName(moveTo(m)) != s.substr(2, 2)) continue;
    if (movePromo(m)) {
      if (s.size() < 5) continue;
      char c = std::tolower(static_cast<unsigned char>(s[4]));
      const int want = c == 'n'   ? KNIGHT
                       : c == 'b' ? BISHOP
                       : c == 'r' ? ROOK
                       : c == 'q' ? QUEEN
                                  : 0;
      if (want != movePromo(m)) continue;
    }
    return m;
  }
  return MOVE_NONE;
}
