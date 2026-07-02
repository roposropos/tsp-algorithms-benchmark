#include "heuristics.hpp"

#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>

long long computeTourCost(const std::vector<std::vector<int>>& matrix, const std::vector<int>& tour) {
    if (tour.empty()) {
        return 0;
    }

    long long cost = 0;
    for (std::size_t i = 0; i + 1 < tour.size(); ++i) {
        cost += matrix[tour[i]][tour[i + 1]];
    }
    cost += matrix[tour.back()][tour.front()];
    return cost;
}

std::vector<int> buildRandomTour(int n, std::mt19937_64& rng) {
    if (n <= 0) {
        return {};
    }

    std::vector<int> tour(n);
    std::iota(tour.begin(), tour.end(), 0);
    std::shuffle(tour.begin(), tour.end(), rng);
    return tour;
}

namespace {

std::vector<int> buildNearestNeighborTourFromStart(const std::vector<std::vector<int>>& matrix, int start) {
    const int n = static_cast<int>(matrix.size());
    std::vector<int> tour;
    tour.reserve(n);

    std::vector<bool> visited(n, false);
    int current = start;
    visited[current] = true;
    tour.push_back(current);

    for (int step = 1; step < n; ++step) {
        int nextCity = -1;
        int bestWeight = std::numeric_limits<int>::max();
        for (int candidate = 0; candidate < n; ++candidate) {
            if (visited[candidate] || candidate == current) {
                continue;
            }
            if (matrix[current][candidate] < bestWeight) {
                bestWeight = matrix[current][candidate];
                nextCity = candidate;
            }
        }

        if (nextCity < 0) {
            throw std::runtime_error("Nearest neighbor failed to build a complete tour");
        }

        visited[nextCity] = true;
        tour.push_back(nextCity);
        current = nextCity;
    }

    return tour;
}

}  // namespace

std::vector<int> buildBestNearestNeighborTour(const std::vector<std::vector<int>>& matrix) {
    const int n = static_cast<int>(matrix.size());
    if (n <= 0) {
        return {};
    }

    long long bestCost = std::numeric_limits<long long>::max();
    std::vector<int> bestTour;

    for (int start = 0; start < n; ++start) {
        auto candidateTour = buildNearestNeighborTourFromStart(matrix, start);
        const long long candidateCost = computeTourCost(matrix, candidateTour);
        if (candidateCost < bestCost) {
            bestCost = candidateCost;
            bestTour = std::move(candidateTour);
        }
    }

    return bestTour;
}

std::vector<int> applyTwoOptMove(const std::vector<int>& tour, int left, int right) {
    std::vector<int> candidate = tour;
    std::reverse(candidate.begin() + left, candidate.begin() + right + 1);
    return candidate;
}

std::vector<int> applySwapMove(const std::vector<int>& tour, int left, int right) {
    std::vector<int> candidate = tour;
    std::swap(candidate[left], candidate[right]);
    return candidate;
}
