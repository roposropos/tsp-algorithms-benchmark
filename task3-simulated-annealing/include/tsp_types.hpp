#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class GraphType {
    Symmetric,
    Asymmetric
};

struct TSPInstance {
    std::string name;
    GraphType graphType = GraphType::Symmetric;
    std::string sourceKind = "custom";
    std::filesystem::path sourcePath;
    std::vector<std::vector<int>> matrix;
    std::optional<long long> referenceCost;

    int size() const {
        return static_cast<int>(matrix.size());
    }
};

struct SARunResult {
    std::string instanceName;
    std::string sourceKind;
    std::string sourcePath;
    GraphType graphType = GraphType::Symmetric;
    int size = 0;
    int repetition = 0;
    std::uint64_t seed = 0;
    long long initialCost = 0;
    long long bestCost = 0;
    std::optional<long long> referenceCost;
    std::optional<double> relativeErrorPercent;
    double timeMs = 0.0;
    int iterations = 0;
    int acceptedMoves = 0;
    int improvedMoves = 0;
    std::vector<int> bestTour;
};

inline std::string graphTypeToString(GraphType type) {
    return type == GraphType::Symmetric ? "symmetric" : "asymmetric";
}
