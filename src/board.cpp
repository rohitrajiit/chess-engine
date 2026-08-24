#include "board.h"

#include <cassert>
#include <cctype>
#include <sstream>

namespace {

// ---------------------------------------------------------------------------
// Zobrist hashing
// ---------------------------------------------------------------------------
uint64_t rngState = 0x9E3779B97F4A7C15ULL;

uint64_t rand64() {
  rngState += 0x9E3779B97F4A7C15ULL;
  uint64_t z = rngState;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

struct Zobrist {
  uint64_t piece[12][64];
  uint64_t side;
  uint64_t castle[16];
  uint64_t epFile[8];

  Zobrist() {
    for (auto& row : piece)
      for (auto& v : row) v = rand64();
    side = rand64();
    for (auto& v : castle) v = rand64();
    for (auto& v : epFile) v = rand64();
  }
};

const Zobrist z;

// white pieces 1..6 -> index 0..5, black pieces -1..-6 -> index 6..11
inline int pieceIndex(int p) { return p > 0 ? p - 1 : 5 - p; }

// castling rights kept after a move touching the given square
struct CastleMaskInit {
  int mask[128];
  CastleMaskInit() {
    for (int i = 0; i < 128; ++i) mask[i] = 15;
    // bits: 1=WK 2=WQ 4=BK 8=BQ
    mask[SQ(4, 0)] &= ~(1 | 2); // e1
    mask[SQ(0, 0)] &= ~2;       // a1
    mask[SQ(7, 0)] &= ~1;       // h1
    mask[SQ(4, 7)] &= ~(4 | 8); // e8
    mask[SQ(0, 7)] &= ~8;       // a8
    mask[SQ(7, 7)] &= ~4;       // h8
  }
};
const CastleMaskInit castleMask;

inline int pieceTypeFromChar(char c) {
  switch (std::tolower(static_cast<unsigned char>(c))) {
    case 'p': return PAWN;
    case 'n': return KNIGHT;
    case 'b': return BISHOP;
    case 'r': return ROOK;
    case 'q': return QUEEN;
    case 'k': return KING;
  }
  return 0;
}

inline char pieceChar(int p) {
  static const char w[] = " pnbrqk";
  static const char b[] = " PNBRQK";
  return p > 0 ? w[p] : b[-p];
}

} // namespace

// ---------------------------------------------------------------------------
// Position setup
// ---------------------------------------------------------------------------

void Board::setStartPos() {
  setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Board::setFen(const std::string& fen) {
  std::istringstream ss(fen);
  std::string placement, stm, cast, epStr;
  int hm = 0, fm = 1;
  ss >> placement >> stm >> cast >> epStr >> hm >> fm;

  for (int i = 0; i < 128; ++i) board[i] = 0;
  kingSq[WHITE] = kingSq[BLACK] = -1;
  castling = 0;
  ep = -1;
  halfmove = hm;
  fullmove = fm;
  histLen = 0;

  int rank = 7, file = 0;
  for (char c : placement) {
    if (c == '/') {
      --rank;
      file = 0;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      file += c - '0';
    } else {
      int t = pieceTypeFromChar(c);
      if (t == 0) continue;
      int color = std::isupper(static_cast<unsigned char>(c)) ? WHITE : BLACK;
      int sq = SQ(file, rank);
      board[sq] = color == WHITE ? t : -t;
      if (t == KING) kingSq[color] = sq;
      ++file;
    }
  }

  side = stm == "b" ? BLACK : WHITE;
  for (char c : cast) {
    if (c == 'K') castling |= 1;
    else if (c == 'Q') castling |= 2;
    else if (c == 'k') castling |= 4;
    else if (c == 'q') castling |= 8;
  }
  if (!epStr.empty() && epStr != "-") ep = SQ(epStr[0] - 'a', epStr[1] - '1');

  hash = computeHash();
  hist[histLen++] = hash;
}

std::string Board::fen() const {
  std::string out;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      int p = board[SQ(file, rank)];
      if (p == 0) {
        ++empty;
      } else {
        if (empty) { out += char('0' + empty); empty = 0; }
        out += pieceChar(p);
      }
    }
    if (empty) out += char('0' + empty);
    if (rank) out += '/';
  }
  out += side == WHITE ? " w " : " b ";
  std::string c;
  if (castling & 1) c += 'K';
  if (castling & 2) c += 'Q';
  if (castling & 4) c += 'k';
  if (castling & 8) c += 'q';
  out += c.empty() ? "-" : c;
  out += ep >= 0 ? " " + squareName(ep) : " -";
  out += " " + std::to_string(halfmove) + " " + std::to_string(fullmove);
  return out;
}

uint64_t Board::computeHash() const {
  uint64_t h = 0;
  for (int r = 0; r < 8; ++r)
    for (int f = 0; f < 8; ++f) {
      int p = board[SQ(f, r)];
      if (p) h ^= z.piece[pieceIndex(p)][sq64(SQ(f, r))];
    }
  if (side == BLACK) h ^= z.side;
  h ^= z.castle[castling];
  if (ep >= 0) h ^= z.epFile[fileOf(ep)];
  return h;
}

// ---------------------------------------------------------------------------
// Attack detection
// ---------------------------------------------------------------------------

bool Board::isAttacked(int sq, int byColor) const {
  const bool byWhite = byColor == WHITE;

  // pawns
  if (byWhite) {
    int f = sq - 15; if (validSquare(f) && board[f] == PAWN) return true;
    f = sq - 17;     if (validSquare(f) && board[f] == PAWN) return true;
  } else {
    int f = sq + 15; if (validSquare(f) && board[f] == -PAWN) return true;
    f = sq + 17;     if (validSquare(f) && board[f] == -PAWN) return true;
  }

  // knights
  static const int knightD[8] = {-33, -31, -18, -14, 14, 18, 31, 33};
  const int kn = byWhite ? KNIGHT : -KNIGHT;
  for (int d : knightD) {
    int f = sq + d;
    if (validSquare(f) && board[f] == kn) return true;
  }

  // king
  static const int kingD[8] = {-17, -16, -15, -1, 1, 15, 16, 17};
  const int kg = byWhite ? KING : -KING;
  for (int d : kingD) {
    int f = sq + d;
    if (validSquare(f) && board[f] == kg) return true;
  }

  // diagonal sliders
  static const int diagD[4] = {-17, -15, 15, 17};
  for (int d : diagD) {
    for (int f = sq + d; validSquare(f); f += d) {
      int p = board[f];
      if (p == 0) continue;
      if ((p > 0) == byWhite && (p == BISHOP || p == -BISHOP || p == QUEEN || p == -QUEEN))
        return true;
      break;
    }
  }

  // orthogonal sliders
  static const int orthD[4] = {-16, -1, 1, 16};
  for (int d : orthD) {
    for (int f = sq + d; validSquare(f); f += d) {
      int p = board[f];
      if (p == 0) continue;
      if ((p > 0) == byWhite && (p == ROOK || p == -ROOK || p == QUEEN || p == -QUEEN))
        return true;
      break;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Make / unmake
// ---------------------------------------------------------------------------

bool Board::makeMove(Move m) {
  assert(histLen < MAX_HIST);
  const int from = moveFrom(m), to = moveTo(m), flag = moveFlag(m);
  const int promo = movePromo(m), capturedType = moveCaptured(m);
  const int us = side, them = other(us);
  const int piece = board[from];

  Undo& u = undos[histLen];
  u.move = m;
  u.castling = castling;
  u.ep = ep;
  u.halfmove = halfmove;
  u.hash = hash;

  if (ep >= 0) hash ^= z.epFile[fileOf(ep)];
  hash ^= z.castle[castling];

  halfmove++;

  if (capturedType) {
    int capSq = to;
    if (flag == FLAG_EP) capSq = us == WHITE ? to - 16 : to + 16;
    board[capSq] = 0;
    hash ^= z.piece[pieceIndex(them == WHITE ? capturedType : -capturedType)][sq64(capSq)];
    halfmove = 0;
  }

  hash ^= z.piece[pieceIndex(piece)][sq64(from)];
  board[from] = 0;
  const int placed = promo ? (us == WHITE ? promo : -promo) : piece;
  board[to] = placed;
  hash ^= z.piece[pieceIndex(placed)][sq64(to)];

  if (piece == KING || piece == -KING) kingSq[us] = to;

  if (flag == FLAG_CASTLE_K) {
    const int rf = us == WHITE ? SQ(7, 0) : SQ(7, 7);
    const int rt = rf - 2;
    const int rook = board[rf];
    board[rf] = 0;
    board[rt] = rook;
    hash ^= z.piece[pieceIndex(rook)][sq64(rf)] ^ z.piece[pieceIndex(rook)][sq64(rt)];
  } else if (flag == FLAG_CASTLE_Q) {
    const int rf = us == WHITE ? SQ(0, 0) : SQ(0, 7);
    const int rt = rf + 3;
    const int rook = board[rf];
    board[rf] = 0;
    board[rt] = rook;
    hash ^= z.piece[pieceIndex(rook)][sq64(rf)] ^ z.piece[pieceIndex(rook)][sq64(rt)];
  }

  castling &= castleMask.mask[from] & castleMask.mask[to];
  hash ^= z.castle[castling];

  ep = flag == FLAG_DOUBLE ? (from + to) / 2 : -1;
  if (ep >= 0) hash ^= z.epFile[fileOf(ep)];

  if (piece == PAWN || piece == -PAWN) halfmove = 0;

  hash ^= z.side;
  side = them;
  if (us == BLACK) ++fullmove;

  hist[histLen++] = hash;

#ifndef NDEBUG
  assert(hash == computeHash());
#endif

  if (inCheck(us)) {
    unmakeMove(m);
    return false;
  }
  return true;
}

void Board::unmakeMove(Move m) {
  --histLen;
  const Undo& u = undos[histLen];
  const int from = moveFrom(m), to = moveTo(m), flag = moveFlag(m);
  side = other(side);
  const int us = side, them = other(us);

  int piece = board[to];
  if (movePromo(m)) piece = us == WHITE ? PAWN : -PAWN;
  board[from] = piece;
  board[to] = 0;

  if (flag == FLAG_EP) {
    const int capSq = us == WHITE ? to - 16 : to + 16;
    board[capSq] = them == WHITE ? PAWN : -PAWN;
  } else if (int ct = moveCaptured(m)) {
    board[to] = them == WHITE ? ct : -ct;
  }

  if (flag == FLAG_CASTLE_K) {
    const int rt = us == WHITE ? SQ(5, 0) : SQ(5, 7);
    const int rf = rt + 2;
    board[rf] = board[rt];
    board[rt] = 0;
  } else if (flag == FLAG_CASTLE_Q) {
    const int rt = us == WHITE ? SQ(3, 0) : SQ(3, 7);
    const int rf = rt - 3;
    board[rf] = board[rt];
    board[rt] = 0;
  }

  if (piece == KING || piece == -KING) kingSq[us] = from;

  castling = u.castling;
  ep = u.ep;
  halfmove = u.halfmove;
  hash = u.hash;
  if (us == BLACK) --fullmove;
}

void Board::makeNull() {
  assert(histLen < MAX_HIST);
  Undo& u = undos[histLen];
  u.move = MOVE_NONE;
  u.castling = castling;
  u.ep = ep;
  u.halfmove = halfmove;
  u.hash = hash;

  if (ep >= 0) hash ^= z.epFile[fileOf(ep)];
  ep = -1;
  hash ^= z.side;
  side = other(side);

  hist[histLen++] = hash;
}

void Board::unmakeNull() {
  --histLen;
  const Undo& u = undos[histLen];
  side = other(side);
  castling = u.castling;
  ep = u.ep;
  halfmove = u.halfmove;
  hash = u.hash;
}

// ---------------------------------------------------------------------------
// Draw detection
// ---------------------------------------------------------------------------

bool Board::drawByRep() const {
  int start = histLen - 1 - halfmove;
  if (start < 0) start = 0;
  for (int i = histLen - 3; i >= start; i -= 2)
    if (hist[i] == hash) return true;
  return false;
}

bool Board::insufficientMaterial() const {
  int minors[2] = {0, 0};
  for (int r = 0; r < 8; ++r)
    for (int f = 0; f < 8; ++f) {
      int p = board[SQ(f, r)];
      if (!p) continue;
      int t = p > 0 ? p : -p;
      if (t == PAWN || t == ROOK || t == QUEEN) return false;
      if (t == KNIGHT || t == BISHOP) ++minors[p > 0 ? WHITE : BLACK];
    }
  return minors[WHITE] + minors[BLACK] <= 1;
}

bool Board::hasNonPawnMaterial(int color) const {
  const bool white = color == WHITE;
  for (int r = 0; r < 8; ++r)
    for (int f = 0; f < 8; ++f) {
      int p = board[SQ(f, r)];
      if (p == 0 || (p > 0) != white) continue;
      int t = p > 0 ? p : -p;
      if (t != PAWN && t != KING) return true;
    }
  return false;
}
