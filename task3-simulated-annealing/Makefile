CXX := c++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude
TARGET := build/pea_sa
SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst src/%.cpp,build/%.o,$(SOURCES))

.PHONY: all clean run-generated run-mixed

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET)

build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $< -o $@

run-generated: $(TARGET)
	./$(TARGET) config/sa_generated.txt

run-mixed: $(TARGET)
	./$(TARGET) config/sa_mixed.txt

clean:
	rm -rf build
