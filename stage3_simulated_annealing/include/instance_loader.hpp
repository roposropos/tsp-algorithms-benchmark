#pragma once

#include "tsp_types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

std::vector<TSPInstance> loadInstances(const std::vector<std::string>& inputPaths);
std::unordered_map<std::string, long long> loadReferenceCsv(const std::string& csvPath);
std::optional<long long> computeHeldKarpReference(const TSPInstance& instance, int maxN);
