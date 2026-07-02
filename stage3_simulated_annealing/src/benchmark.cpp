#include "benchmark.hpp"

#include "instance_loader.hpp"
#include "simulated_annealing.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace {

struct SummaryStats {
    int instanceCount = 0;
    int runCount = 0;
    double totalTimeMs = 0.0;
    double totalErrorPercent = 0.0;
    int errorSamples = 0;
    double totalImprovementPercent = 0.0;
    int improvementSamples = 0;
};

std::string escapeCsv(const std::string& value) {
    if (value.find(',') == std::string::npos && value.find('"') == std::string::npos) {
        return value;
    }
    std::string escaped = "\"";
    for (char ch : value) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
}

std::string joinTour(const std::vector<int>& tour) {
    std::ostringstream output;
    for (std::size_t i = 0; i < tour.size(); ++i) {
        if (i > 0) {
            output << ' ';
        }
        output << tour[i];
    }
    return output.str();
}

void ensureParentDirectory(const std::string& filePath) {
    const std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
}

void attachReferences(
    std::vector<TSPInstance>& instances,
    const std::unordered_map<std::string, long long>& referenceMap,
    bool useHeldKarpReference,
    int heldKarpMaxN,
    bool verbose) {

    for (auto& instance : instances) {
        if (!instance.referenceCost.has_value()) {
            const auto it = referenceMap.find(instance.name);
            if (it != referenceMap.end()) {
                instance.referenceCost = it->second;
            }
        }

        if (!instance.referenceCost.has_value() && useHeldKarpReference) {
            instance.referenceCost = computeHeldKarpReference(instance, heldKarpMaxN);
            if (verbose && instance.referenceCost.has_value()) {
                std::cout << "[reference] computed exact value for " << instance.name
                          << " = " << *instance.referenceCost << '\n';
            }
        }
    }
}

void writeResultsCsv(const std::string& outputPath, const std::vector<SARunResult>& results) {
    ensureParentDirectory(outputPath);
    std::ofstream output(outputPath);
    if (!output) {
        throw std::runtime_error("Cannot open output CSV: " + outputPath);
    }

    output << "instance_name,source_kind,source_path,graph_type,n,repetition,seed,initial_cost,best_cost,reference_cost,relative_error_percent,time_ms,iterations,accepted_moves,improved_moves,best_tour\n";
    output << std::fixed << std::setprecision(6);

    for (const auto& result : results) {
        output << escapeCsv(result.instanceName) << ','
               << escapeCsv(result.sourceKind) << ','
               << escapeCsv(result.sourcePath) << ','
               << graphTypeToString(result.graphType) << ','
               << result.size << ','
               << result.repetition << ','
               << result.seed << ','
               << result.initialCost << ','
               << result.bestCost << ',';

        if (result.referenceCost.has_value()) {
            output << *result.referenceCost;
        }
        output << ',';

        if (result.relativeErrorPercent.has_value()) {
            output << *result.relativeErrorPercent;
        }
        output << ','
               << result.timeMs << ','
               << result.iterations << ','
               << result.acceptedMoves << ','
               << result.improvedMoves << ','
               << escapeCsv(joinTour(result.bestTour)) << '\n';
    }
}

void writeSummaryCsv(const std::string& summaryPath, const std::vector<SARunResult>& results) {
    ensureParentDirectory(summaryPath);
    std::ofstream output(summaryPath);
    if (!output) {
        throw std::runtime_error("Cannot open summary CSV: " + summaryPath);
    }

    std::map<std::tuple<int, std::string, std::string>, SummaryStats> grouped;
    std::map<std::tuple<int, std::string, std::string>, std::map<std::string, bool>> uniqueInstances;

    for (const auto& result : results) {
        const auto key = std::make_tuple(result.size, graphTypeToString(result.graphType), result.sourceKind);
        auto& stats = grouped[key];
        stats.runCount += 1;
        stats.totalTimeMs += result.timeMs;

        if (result.relativeErrorPercent.has_value()) {
            stats.totalErrorPercent += *result.relativeErrorPercent;
            stats.errorSamples += 1;
        }

        if (result.initialCost > 0) {
            const double improvement = (static_cast<double>(result.initialCost - result.bestCost) / static_cast<double>(result.initialCost)) * 100.0;
            stats.totalImprovementPercent += improvement;
            stats.improvementSamples += 1;
        }

        uniqueInstances[key][result.instanceName] = true;
    }

    for (auto& [key, seen] : uniqueInstances) {
        grouped[key].instanceCount = static_cast<int>(seen.size());
    }

    output << "n,graph_type,source_kind,instance_count,run_count,avg_time_ms,avg_relative_error_percent,avg_improvement_percent\n";
    output << std::fixed << std::setprecision(6);

    for (const auto& [key, stats] : grouped) {
        const auto& [size, graphType, sourceKind] = key;
        output << size << ','
               << graphType << ','
               << sourceKind << ','
               << stats.instanceCount << ','
               << stats.runCount << ','
               << (stats.runCount > 0 ? stats.totalTimeMs / static_cast<double>(stats.runCount) : 0.0) << ',';

        if (stats.errorSamples > 0) {
            output << (stats.totalErrorPercent / static_cast<double>(stats.errorSamples));
        }
        output << ',';

        if (stats.improvementSamples > 0) {
            output << (stats.totalImprovementPercent / static_cast<double>(stats.improvementSamples));
        }
        output << '\n';
    }
}

void printConsoleSummary(const std::vector<SARunResult>& results) {
    std::map<std::tuple<int, std::string>, SummaryStats> grouped;
    std::map<std::tuple<int, std::string>, std::map<std::string, bool>> uniqueInstances;

    for (const auto& result : results) {
        const auto key = std::make_tuple(result.size, graphTypeToString(result.graphType));
        auto& stats = grouped[key];
        stats.runCount += 1;
        stats.totalTimeMs += result.timeMs;
        if (result.relativeErrorPercent.has_value()) {
            stats.totalErrorPercent += *result.relativeErrorPercent;
            stats.errorSamples += 1;
        }
        uniqueInstances[key][result.instanceName] = true;
    }

    for (auto& [key, seen] : uniqueInstances) {
        grouped[key].instanceCount = static_cast<int>(seen.size());
    }

    std::cout << "\nSummary by size and graph type:\n";
    std::cout << "n,type,instances,avg_time_ms,avg_error_pct\n";
    for (const auto& [key, stats] : grouped) {
        const auto& [size, graphType] = key;
        std::cout << size << ','
                  << graphType << ','
                  << stats.instanceCount << ','
                  << std::fixed << std::setprecision(3)
                  << (stats.runCount > 0 ? stats.totalTimeMs / static_cast<double>(stats.runCount) : 0.0)
                  << ',';

        if (stats.errorSamples > 0) {
            std::cout << (stats.totalErrorPercent / static_cast<double>(stats.errorSamples));
        } else {
            std::cout << "n/a";
        }
        std::cout << '\n';
    }
}

}  // namespace

int runBenchmark(const BenchmarkConfig& config) {
    auto instances = loadInstances(config.inputPaths);
    const auto referenceMap = loadReferenceCsv(config.referenceCsv);

    attachReferences(
        instances,
        referenceMap,
        config.useHeldKarpReference,
        config.heldKarpMaxN,
        config.verbose);

    if (instances.empty()) {
        throw std::runtime_error("No input instances found");
    }

    std::vector<SARunResult> results;
    results.reserve(static_cast<std::size_t>(instances.size()) * static_cast<std::size_t>(config.repetitions));

    for (const auto& instance : instances) {
        if (config.verbose) {
            std::cout << "[run] " << instance.name
                      << " n=" << instance.size()
                      << " type=" << graphTypeToString(instance.graphType)
                      << " source=" << instance.sourceKind << '\n';
        }

        for (int repetition = 0; repetition < config.repetitions; ++repetition) {
            results.push_back(runSimulatedAnnealing(instance, config, repetition));
        }
    }

    writeResultsCsv(config.outputCsv, results);
    writeSummaryCsv(config.summaryCsv, results);
    printConsoleSummary(results);

    std::cout << "\nSaved detailed results to: " << config.outputCsv << '\n';
    std::cout << "Saved summary to: " << config.summaryCsv << '\n';
    return 0;
}
