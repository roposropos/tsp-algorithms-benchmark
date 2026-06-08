CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic

BIN := tsp_ga
SRC := src/main.cpp

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN) config/experiment.cfg

clean:
	rm -f $(BIN)
