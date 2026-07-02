#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct BenchmarkConfig {
    std::vector<std::string> inputPaths;
    std::string referenceCsv;
    std::string outputCsv = "results/sa_results.csv";
    std::string summaryCsv = "results/sa_summary.csv";
    std::uint64_t seed = 12345;
    int repetitions = 3;
    int maxIterations = 200000;
    double timeLimitMs = 1000.0;
    double initialTemperature = 1000.0;
    double minimumTemperature = 0.001;
    double coolingRate = 0.995;
    int iterationsPerTemperature = 100;
    std::string initialSolution = "nearest_neighbor";
    std::string neighborhood = "two_opt";
    bool useHeldKarpReference = true;
    int heldKarpMaxN = 16;
    bool verbose = true;
};

std::unordered_map<std::string, std::string> parseConfigFile(const std::string& path);
BenchmarkConfig loadBenchmarkConfig(const std::string& path);
