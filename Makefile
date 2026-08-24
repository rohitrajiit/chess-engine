CXX ?= c++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -DNDEBUG

SRC := $(wildcard src/*.cpp)
HDR := $(wildcard src/*.h)

chessengine: $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# debug build with assertions + hash self-checks enabled
debug: $(SRC) $(HDR)
	$(CXX) -O1 -g -std=c++17 -Wall -Wextra -o chessengine-debug $(SRC)

test: chessengine
	./chessengine perft

clean:
	rm -f chessengine chessengine-debug

.PHONY: test clean debug
