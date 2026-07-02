#pragma once

#include "config.hpp"
#include "tsp_types.hpp"

SARunResult runSimulatedAnnealing(const TSPInstance& instance, const BenchmarkConfig& config, int repetitionIndex);
