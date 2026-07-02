#include "simulated_annealing.hpp"

#include "heuristics.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <random>

namespace {

std::optional<double> computeRelativeError(long long obtained, const std::optional<long long>& reference) {
    if (!reference.has_value() || *reference == 0) {
        return std::nullopt;
    }
    return (static_cast<double>(obtained - *reference) / static_cast<double>(*reference)) * 100.0;
}

std::vector<int> buildInitialTour(const TSPInstance& instance, const BenchmarkConfig& config, std::mt19937_64& rng) {
    if (config.initialSolution == "random") {
        return buildRandomTour(instance.size(), rng);
    }
    return buildBestNearestNeighborTour(instance.matrix);
}

std::vector<int> buildNeighbor(
    const std::vector<int>& tour,
    const BenchmarkConfig& config,
    std::mt19937_64& rng) {

    std::uniform_int_distribution<int> cityDist(0, static_cast<int>(tour.size()) - 1);
    int left = cityDist(rng);
    int right = cityDist(rng);
    if (left > right) {
        std::swap(left, right);
    }
    while (left == right) {
        right = cityDist(rng);
        if (left > right) {
            std::swap(left, right);
        }
    }

    if (config.neighborhood == "swap") {
        return applySwapMove(tour, left, right);
    }
    return applyTwoOptMove(tour, left, right);
}

}  // namespace

SARunResult runSimulatedAnnealing(const TSPInstance& instance, const BenchmarkConfig& config, int repetitionIndex) {
    const std::uint64_t runSeed = config.seed + static_cast<std::uint64_t>(instance.size() * 1000 + repetitionIndex);
    std::mt19937_64 rng(runSeed);
    std::uniform_real_distribution<double> probability(0.0, 1.0);

    std::vector<int> currentTour = buildInitialTour(instance, config, rng);
    long long currentCost = computeTourCost(instance.matrix, currentTour);
    const long long initialCost = currentCost;
    std::vector<int> bestTour = currentTour;
    long long bestCost = currentCost;

    double temperature = config.initialTemperature;
    int iterations = 0;
    int acceptedMoves = 0;
    int improvedMoves = 0;

    const auto started = std::chrono::steady_clock::now();
    while (iterations < config.maxIterations && temperature > config.minimumTemperature) {
        for (int inner = 0; inner < config.iterationsPerTemperature && iterations < config.maxIterations; ++inner) {
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            if (elapsed >= config.timeLimitMs) {
                temperature = config.minimumTemperature;
                break;
            }

            auto candidateTour = buildNeighbor(currentTour, config, rng);
            const long long candidateCost = computeTourCost(instance.matrix, candidateTour);
            const long long delta = candidateCost - currentCost;

            bool accept = false;
            if (delta <= 0) {
                accept = true;
            } else {
                const double acceptanceProbability = std::exp(-static_cast<double>(delta) / temperature);
                accept = probability(rng) < acceptanceProbability;
            }

            if (accept) {
                currentTour = std::move(candidateTour);
                currentCost = candidateCost;
                ++acceptedMoves;
            }

            if (currentCost < bestCost) {
                bestCost = currentCost;
                bestTour = currentTour;
                ++improvedMoves;
            }

            ++iterations;
        }

        temperature *= config.coolingRate;
    }

    const double elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();

    SARunResult result;
    result.instanceName = instance.name;
    result.sourceKind = instance.sourceKind;
    result.sourcePath = instance.sourcePath.string();
    result.graphType = instance.graphType;
    result.size = instance.size();
    result.repetition = repetitionIndex + 1;
    result.seed = runSeed;
    result.initialCost = initialCost;
    result.bestCost = bestCost;
    result.referenceCost = instance.referenceCost;
    result.relativeErrorPercent = computeRelativeError(bestCost, instance.referenceCost);
    result.timeMs = elapsedMs;
    result.iterations = iterations;
    result.acceptedMoves = acceptedMoves;
    result.improvedMoves = improvedMoves;
    result.bestTour = std::move(bestTour);
    return result;
}
