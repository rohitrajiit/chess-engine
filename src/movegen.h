#pragma once

#include "board.h"
#include <string>

struct MoveList {
  Move v[256];
  int s[256];
  int count = 0;

  void add(Move m) { v[count++] = m; }
};

enum GenType { GEN_ALL, GEN_CAPTURES };

void generateMoves(const Board& b, MoveList& out, GenType type);
uint64_t perft(Board& b, int depth);

// Finds the legal move matching a UCI string like "e2e4" or "e7e8q".
Move uciToMove(Board& b, const std::string& s);
