#include "board.h"
#include "evaluate.h"
#include "movegen.h"
#include "search.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

Board board;
Searcher searcher;
std::thread searchThread;
std::atomic<bool> stopFlag{false};

void joinSearch() {
  if (searchThread.joinable()) searchThread.join();
}

// ---------------------------------------------------------------------------
// position command
// ---------------------------------------------------------------------------

void setPosition(const std::vector<std::string>& tok) {
  joinSearch();
  size_t i = 1;
  if (i >= tok.size()) return;

  if (tok[i] == "startpos") {
    board.setStartPos();
    ++i;
  } else if (tok[i] == "fen") {
    std::string fen;
    ++i;
    while (i < tok.size() && tok[i] != "moves") {
      fen += tok[i];
      fen += ' ';
      ++i;
    }
    board.setFen(fen);
  }

  if (i < tok.size() && tok[i] == "moves") {
    ++i;
    for (; i < tok.size(); ++i) {
      Move m = uciToMove(board, tok[i]);
      if (m != MOVE_NONE) board.makeMove(m);
    }
  }
}

// ---------------------------------------------------------------------------
// go command
// ---------------------------------------------------------------------------

void go(const std::vector<std::string>& tok) {
  joinSearch();

  SearchLimits lim;
  for (size_t i = 1; i < tok.size(); ++i) {
    auto val = [&]() -> int {
      if (i + 1 < tok.size()) return std::atoi(tok[++i].c_str());
      return 0;
    };
    if (tok[i] == "depth") lim.depth = val();
    else if (tok[i] == "movetime") lim.movetime = val();
    else if (tok[i] == "wtime") lim.wtime = val();
    else if (tok[i] == "btime") lim.btime = val();
    else if (tok[i] == "winc") lim.winc = val();
    else if (tok[i] == "binc") lim.binc = val();
    else if (tok[i] == "movestogo") lim.movestogo = val();
    else if (tok[i] == "nodes") lim.nodes = val();
    else if (tok[i] == "infinite") lim.infinite = true;
  }

  stopFlag = false;
  SearchLimits captured = lim;
  Board* posPtr = &board;
  searchThread = std::thread([posPtr, captured] { searcher.think(*posPtr, captured, stopFlag); });
}

// ---------------------------------------------------------------------------
// perft test suite
// ---------------------------------------------------------------------------

struct PerftCase {
  const char* name;
  const char* fen;
  int depth;
  uint64_t expected;
};

void runPerftSuite() {
  const PerftCase cases[] = {
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5,
       4865609},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
       4, 4085603},
      {"position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
      {"position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
       4, 422333},
      {"position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4,
       2103487},
      {"position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
       4, 3894594},
  };

  int failures = 0;
  for (const PerftCase& c : cases) {
    Board b;
    b.setFen(c.fen);
    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t got = perft(b, c.depth);
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - t0)
                             .count();
    const bool pass = got == c.expected;
    if (!pass) ++failures;
    std::cout << (pass ? "[PASS]" : "[FAIL]") << ' ' << c.name << " perft(" << c.depth
              << ") = " << got << " (expected " << c.expected << ") in " << ms << " ms"
              << std::endl;
  }
  std::cout << (failures ? "PERFT SUITE FAILED" : "ALL PERFT TESTS PASSED")
            << std::endl;
}

} // namespace

int main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);

  if (argc > 1 && std::string(argv[1]) == "perft") {
    runPerftSuite();
    return 0;
  }

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream ss(line);
    std::vector<std::string> tok;
    std::string w;
    while (ss >> w) tok.push_back(w);
    if (tok.empty()) continue;
    const std::string& cmd = tok[0];

    if (cmd == "uci") {
      std::cout << "id name ox-engine 1.0\n";
      std::cout << "id author opencode\n";
      std::cout << "uciok" << std::endl;
    } else if (cmd == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (cmd == "setoption") {
      // no configurable options yet
    } else if (cmd == "ucinewgame") {
      joinSearch();
      searcher.clearTT();
      board.setStartPos();
    } else if (cmd == "position") {
      setPosition(tok);
    } else if (cmd == "go") {
      go(tok);
    } else if (cmd == "stop") {
      stopFlag = true;
    } else if (cmd == "ponderhit") {
      // pondering not implemented
    } else if (cmd == "quit") {
      stopFlag = true;
      joinSearch();
      break;
    } else if (cmd == "perft") {
      joinSearch();
      if (tok.size() > 1) {
        Board copy = board;
        const int d = std::atoi(tok[1].c_str());
        std::cout << "perft " << d << ": " << perft(copy, d) << std::endl;
      }
    } else if (cmd == "eval") {
      joinSearch();
      std::cout << "eval: " << evaluate(board) << std::endl;
    } else if (cmd == "d") {
      joinSearch();
      std::cout << board.fen() << std::endl;
    }
  }

  joinSearch();
  return 0;
}
