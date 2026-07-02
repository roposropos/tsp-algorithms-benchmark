CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

BIN := tsp_stage1
SRC := src/main.cpp

.PHONY: all run-bruteforce run-heuristics smoke clean

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

run-bruteforce: $(BIN)
	./$(BIN) config_bruteforce.txt

run-heuristics: $(BIN)
	./$(BIN) config_heuristics.txt

smoke: $(BIN)
	./$(BIN) config_smoke.txt

clean:
	rm -f $(BIN) smoke_results.csv
