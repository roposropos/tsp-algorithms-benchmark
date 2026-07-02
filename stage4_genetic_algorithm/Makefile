CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic

BIN := tsp_ga
SRC := src/main.cpp

.PHONY: all clean run run-smoke

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN) config/experiment.cfg

run-smoke: $(BIN)
	./$(BIN) config/smoke.cfg

clean:
	rm -f $(BIN)
	rm -f results/smoke_ga_results.csv
	rm -rf results/smoke_tours
