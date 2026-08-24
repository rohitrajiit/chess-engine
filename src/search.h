#pragma once

#include "board.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

constexpr int MAX_PLY = 64;
constexpr int MATE = 30000;
constexpr int MATE_BOUND = MATE - MAX_PLY;
constexpr int SCORE_INF = 31001;

struct SearchLimits {
  int depth = MAX_PLY;
  int movetime = 0; // ms
  int wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 0;
  int nodes = 0;
  bool infinite = false;
};

enum { TF_EXACT = 1, TF_LOWER = 2, TF_UPPER = 3 };

struct TTEntry {
  uint64_t key = 0;
  uint32_t move = 0;
  int16_t score = 0;
  int8_t depth = 0;
  uint8_t flag = 0;
};

class Searcher {
public:
  Searcher();

  void clearTT();

  // Blocking: runs iterative deepening, prints info lines and "bestmove".
  void think(Board& pos, const SearchLimits& lim, const std::atomic<bool>& externalStop);

private:
  static constexpr size_t TT_BITS = 20; // 1M entries = 16 MB
  static constexpr size_t TT_SIZE = size_t(1) << TT_BITS;
  static constexpr uint64_t TT_MASK = TT_SIZE - 1;

  int search(int depth, int alpha, int beta, int ply);
  int qsearch(int alpha, int beta, int ply);

  void scoreMoves(struct MoveList& ml, Move ttMove, int ply);
  void scoreCaptureMoves(struct MoveList& ml);
  static void pickBest(struct MoveList& ml, int i);

  bool checkStop();
  long long elapsedMs() const;

  std::vector<Move> extractPV(int maxLen);
  std::string scoreStr(int sc) const;
  void report(int depth, int sc);

  Board* pos_ = nullptr;
  SearchLimits limits_;
  const std::atomic<bool>* extStop_ = nullptr;
  bool stopped_ = false;
  uint64_t nodes_ = 0;
  int seldepth_ = 0;
  std::chrono::steady_clock::time_point start_;
  int timeAllocMs_ = INT32_MAX;

  Move killers_[MAX_PLY][2]{};
  int history_[2][128][128]{};
  std::vector<TTEntry> tt_;
};
