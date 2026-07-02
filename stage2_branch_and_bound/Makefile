CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

BIN := tsp_bnb
SRC := src/main.cpp

.PHONY: all run batch smoke clean

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN) config.txt

batch: $(BIN)
	./$(BIN) batch_config.txt

smoke: $(BIN)
	./$(BIN) config_smoke.txt

clean:
	rm -f $(BIN) smoke_result_task2.csv
