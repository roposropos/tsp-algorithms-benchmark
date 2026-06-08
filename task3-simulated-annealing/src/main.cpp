#include "benchmark.hpp"
#include "config.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./pea_sa <config_path>\n";
        return 1;
    }

    try {
        const BenchmarkConfig config = loadBenchmarkConfig(argv[1]);
        return runBenchmark(config);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
