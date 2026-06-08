#pragma once

#include "tsp_types.hpp"

#include <random>
#include <vector>

long long computeTourCost(const std::vector<std::vector<int>>& matrix, const std::vector<int>& tour);
std::vector<int> buildRandomTour(int n, std::mt19937_64& rng);
std::vector<int> buildBestNearestNeighborTour(const std::vector<std::vector<int>>& matrix);
std::vector<int> applyTwoOptMove(const std::vector<int>& tour, int left, int right);
std::vector<int> applySwapMove(const std::vector<int>& tour, int left, int right);
