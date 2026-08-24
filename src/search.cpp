#include "search.h"

#include "evaluate.h"
#include "movegen.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

namespace {

constexpr int VAL[7] = {0, 100, 320, 330, 500, 900, 10000};

inline int matedScore(int ply) { return -MATE + ply; }

} // namespace

Searcher::Searcher() { tt_.resize(TT_SIZE); }

void Searcher::clearTT() { std::memset(tt_.data(), 0, sizeof(TTEntry) * tt_.size()); }

// ---------------------------------------------------------------------------
// Time control
// ---------------------------------------------------------------------------

long long Searcher::elapsedMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - start_)
      .count();
}

bool Searcher::checkStop() {
  if (stopped_) return true;
  if ((nodes_ & 2047) == 0) {
    if (extStop_->load()) stopped_ = true;
    else if (limits_.nodes > 0 && nodes_ >= static_cast<uint64_t>(limits_.nodes))
      stopped_ = true;
    else if (timeAllocMs_ < INT32_MAX && elapsedMs() >= timeAllocMs_)
      stopped_ = true;
  }
  return stopped_;
}

// ---------------------------------------------------------------------------
// Move ordering
// ---------------------------------------------------------------------------

void Searcher::scoreMoves(MoveList& ml, Move ttMove, int ply) {
  const int stm = pos_->side;
  for (int i = 0; i < ml.count; ++i) {
    const Move m = ml.v[i];
    if (m == ttMove) {
      ml.s[i] = 1 << 28;
      continue;
    }
    const int captured = moveCaptured(m);
    const int promo = movePromo(m);
    if (captured) {
      const int attacker = pos_->board[moveFrom(m)];
      ml.s[i] = 1000000 + VAL[captured] * 16 - VAL[attacker > 0 ? attacker : -attacker];
    } else if (promo == QUEEN) {
      ml.s[i] = 900000;
    } else if (m == killers_[ply][0]) {
      ml.s[i] = 800000;
    } else if (m == killers_[ply][1]) {
      ml.s[i] = 799999;
    } else {
      ml.s[i] = history_[stm][moveFrom(m)][moveTo(m)];
    }
  }
}

void Searcher::scoreCaptureMoves(MoveList& ml) {
  for (int i = 0; i < ml.count; ++i) {
    const Move m = ml.v[i];
    const int captured = moveCaptured(m);
    const int promo = movePromo(m);
    int sc;
    if (captured) {
      const int attacker = pos_->board[moveFrom(m)];
      sc = 1000000 + VAL[captured] * 16 - VAL[attacker > 0 ? attacker : -attacker];
    } else {
      sc = 900000; // quiet promotion in qsearch
    }
    if (promo == QUEEN) sc += 500000;
    ml.s[i] = sc;
  }
}

void Searcher::pickBest(MoveList& ml, int i) {
  int bi = i, bs = ml.s[i];
  for (int j = i + 1; j < ml.count; ++j)
    if (ml.s[j] > bs) {
      bs = ml.s[j];
      bi = j;
    }
  std::swap(ml.v[i], ml.v[bi]);
  std::swap(ml.s[i], ml.s[bi]);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

int Searcher::qsearch(int alpha, int beta, int ply) {
  ++nodes_;
  if (ply > seldepth_) seldepth_ = ply;
  if (checkStop()) return 0;
  if (ply >= MAX_PLY - 1) return evaluate(*pos_);

  const int stand = evaluate(*pos_);
  if (stand >= beta) return stand;
  if (stand > alpha) alpha = stand;

  MoveList ml;
  generateMoves(*pos_, ml, GEN_CAPTURES);
  scoreCaptureMoves(ml);

  for (int i = 0; i < ml.count; ++i) {
    pickBest(ml, i);
    const Move m = ml.v[i];
    if (!pos_->makeMove(m)) continue;
    const int sc = -qsearch(-beta, -alpha, ply + 1);
    pos_->unmakeMove(m);
    if (stopped_) return 0;
    if (sc >= beta) return sc;
    if (sc > alpha) alpha = sc;
  }
  return alpha;
}

int Searcher::search(int depth, int alpha, int beta, int ply) {
  if (checkStop()) return 0;

  if (ply > 0 &&
      (pos_->drawByRep() || pos_->halfmove >= 100 || pos_->insufficientMaterial()))
    return 0;

  const bool inChk = pos_->inCheck(pos_->side);
  if (inChk) ++depth; // check extension

  if (depth <= 0 || ply >= MAX_PLY - 1) return qsearch(alpha, beta, ply);

  ++nodes_;

  // mate distance pruning
  alpha = std::max(alpha, matedScore(ply));
  beta = std::min(beta, MATE - (ply + 1));
  if (alpha >= beta) return alpha;

  // transposition table probe
  Move ttMove = MOVE_NONE;
  TTEntry& e = tt_[pos_->hash & TT_MASK];
  if (e.key == pos_->hash && e.move) ttMove = e.move;
  if (e.key == pos_->hash && e.depth >= depth) {
    const int sc = e.score > MATE_BOUND   ? e.score - ply
                   : e.score < -MATE_BOUND ? e.score + ply
                                           : e.score;
    if (e.flag == TF_EXACT ||
        (e.flag == TF_LOWER && sc >= beta) ||
        (e.flag == TF_UPPER && sc <= alpha))
      return sc;
  }

  // null-move pruning
  if (!inChk && ply > 0 && depth >= 3 && beta < MATE_BOUND &&
      pos_->hasNonPawnMaterial(pos_->side)) {
    pos_->makeNull();
    const int sc = -search(depth - 3, -beta, -beta + 1, ply + 1);
    pos_->unmakeNull();
    if (stopped_) return 0;
    if (sc >= beta) return beta;
  }

  MoveList ml;
  generateMoves(*pos_, ml, GEN_ALL);
  scoreMoves(ml, ttMove, ply);

  Move best = MOVE_NONE;
  int bestSc = -SCORE_INF;
  const int oldAlpha = alpha;
  int legal = 0;

  for (int i = 0; i < ml.count; ++i) {
    pickBest(ml, i);
    const Move m = ml.v[i];
    const bool tactical = moveCaptured(m) || movePromo(m);

    if (!pos_->makeMove(m)) continue;
    ++legal;

    int sc;
    if (legal == 1) {
      sc = -search(depth - 1, -beta, -alpha, ply + 1);
    } else {
      int reduce = 0;
      if (depth >= 3 && legal > 3 && !tactical && !inChk) reduce = 1;
      sc = -search(depth - 1 - reduce, -alpha - 1, -alpha, ply + 1);
      if (sc > alpha && reduce > 0) sc = -search(depth - 1, -alpha - 1, -alpha, ply + 1);
      if (sc > alpha && sc < beta) sc = -search(depth - 1, -beta, -alpha, ply + 1);
    }
    pos_->unmakeMove(m);

    if (stopped_) return 0;

    if (sc > bestSc) {
      bestSc = sc;
      best = m;
      if (sc > alpha) {
        alpha = sc;
        if (alpha >= beta) {
          if (!tactical) {
            if (killers_[ply][0] != m) {
              killers_[ply][1] = killers_[ply][0];
              killers_[ply][0] = m;
            }
            history_[pos_->side][moveFrom(m)][moveTo(m)] += depth * depth;
          }
          break;
        }
      }
    }
  }

  if (!legal) return inChk ? matedScore(ply) : 0;

  const uint8_t flag = bestSc >= beta       ? TF_LOWER
                       : bestSc > oldAlpha ? TF_EXACT
                                           : TF_UPPER;
  if (e.key == 0 || e.key == pos_->hash || e.depth <= depth) {
    int stored = bestSc;
    if (stored > MATE_BOUND) stored += ply;
    else if (stored < -MATE_BOUND) stored -= ply;
    if (stored > INT16_MAX) stored = INT16_MAX;
    if (stored < INT16_MIN) stored = INT16_MIN;
    e.key = pos_->hash;
    e.move = best;
    e.score = static_cast<int16_t>(stored);
    e.depth = static_cast<int8_t>(std::min(depth, 127));
    e.flag = flag;
  }

  return bestSc;
}

// ---------------------------------------------------------------------------
// PV extraction and reporting
// ---------------------------------------------------------------------------

std::vector<Move> Searcher::extractPV(int maxLen) {
  std::vector<Move> pv;
  uint64_t seen[MAX_PLY];
  int nSeen = 0;

  while (static_cast<int>(pv.size()) < maxLen) {
    bool cycle = false;
    for (int k = 0; k < nSeen; ++k)
      if (seen[k] == pos_->hash) { cycle = true; break; }
    if (cycle) break;

    const TTEntry& e = tt_[pos_->hash & TT_MASK];
    if (e.key != pos_->hash || !e.move) break;
    const Move m = e.move;

    MoveList ml;
    generateMoves(*pos_, ml, GEN_ALL);
    bool found = false;
    for (int i = 0; i < ml.count; ++i)
      if (ml.v[i] == m) { found = true; break; }
    if (!found) break;

    seen[nSeen++] = pos_->hash;
    pos_->makeMove(m); // legal by construction above
    pv.push_back(m);
  }
  for (size_t k = pv.size(); k > 0; --k) pos_->unmakeMove(pv[k - 1]);
  return pv;
}

std::string Searcher::scoreStr(int sc) const {
  std::ostringstream oss;
  if (sc > MATE_BOUND) oss << "mate " << (MATE - sc + 1) / 2;
  else if (sc < -MATE_BOUND) oss << "mate " << -(MATE + sc + 1) / 2;
  else oss << "cp " << sc;
  return oss.str();
}

void Searcher::report(int depth, int sc) {
  const long long ms = elapsedMs();
  const uint64_t nps = ms > 0 ? nodes_ * 1000ULL / static_cast<uint64_t>(ms) : nodes_;
  std::ostringstream oss;
  oss << "info depth " << depth << " seldepth " << seldepth_ << " score "
      << scoreStr(sc) << " nodes " << nodes_ << " nps " << nps << " time " << ms
      << " pv";
  for (Move m : extractPV(depth)) oss << ' ' << moveToUci(m);
  std::cout << oss.str() << std::endl;
}

// ---------------------------------------------------------------------------
// Iterative deepening driver
// ---------------------------------------------------------------------------

void Searcher::think(Board& pos, const SearchLimits& lim,
                     const std::atomic<bool>& externalStop) {
  pos_ = &pos;
  limits_ = lim;
  extStop_ = &externalStop;
  stopped_ = false;
  nodes_ = 0;
  seldepth_ = 0;
  start_ = std::chrono::steady_clock::now();
  std::memset(killers_, 0, sizeof(killers_));
  timeAllocMs_ = INT32_MAX;

  if (lim.movetime > 0) {
    timeAllocMs_ = lim.movetime;
  } else if (lim.wtime > 0 || lim.btime > 0) {
    const int t = pos.side == WHITE ? lim.wtime : lim.btime;
    const int inc = pos.side == WHITE ? lim.winc : lim.binc;
    const int mtg = lim.movestogo > 0 ? lim.movestogo : 30;
    long long alloc = t / mtg + inc / 2;
    alloc = std::max(5LL, std::min(alloc, static_cast<long long>(std::max(5, t - 50))));
    timeAllocMs_ = static_cast<int>(alloc);
  }

  Move best = MOVE_NONE;

  for (int d = 1; d <= lim.depth; ++d) {
    const int sc = search(d, -SCORE_INF, SCORE_INF, 0);
    if (stopped_) break;

    best = MOVE_NONE;
    const TTEntry& e = tt_[pos.hash & TT_MASK];
    if (e.key == pos.hash && e.move) {
      MoveList ml;
      generateMoves(pos, ml, GEN_ALL);
      for (int i = 0; i < ml.count; ++i)
        if (ml.v[i] == e.move) { best = e.move; break; }
    }
    report(d, sc);

    if (std::abs(sc) > MATE_BOUND) break; // forced mate found
    const long long el = elapsedMs();
    if (el >= timeAllocMs_ || el * 2 >= timeAllocMs_) break;
    if (extStop_->load()) break;
  }

  if (best == MOVE_NONE) {
    MoveList ml;
    generateMoves(pos, ml, GEN_ALL);
    for (int i = 0; i < ml.count; ++i)
      if (pos.makeMove(ml.v[i])) {
        pos.unmakeMove(ml.v[i]);
        best = ml.v[i];
        break;
      }
  }

  std::cout << "bestmove " << moveToUci(best) << std::endl;
}
