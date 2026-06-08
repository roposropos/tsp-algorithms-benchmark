#include "config.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parseBool(const std::string& value) {
    const std::string lowered = trim(value);
    return lowered == "1" || lowered == "true" || lowered == "TRUE" ||
           lowered == "yes" || lowered == "YES" || lowered == "on" ||
           lowered == "tak" || lowered == "TAK";
}

std::vector<std::string> parseList(const std::string& value) {
    std::vector<std::string> items;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            items.push_back(item);
        }
    }
    return items;
}

std::string getRequired(const std::unordered_map<std::string, std::string>& raw, const std::string& key) {
    const auto it = raw.find(key);
    if (it == raw.end() || trim(it->second).empty()) {
        throw std::runtime_error("Missing required config key: " + key);
    }
    return trim(it->second);
}

template <typename T>
T getNumeric(const std::unordered_map<std::string, std::string>& raw, const std::string& key, const T& defaultValue) {
    const auto it = raw.find(key);
    if (it == raw.end()) {
        return defaultValue;
    }

    std::stringstream stream(trim(it->second));
    T parsed{};
    stream >> parsed;
    if (stream.fail()) {
        throw std::runtime_error("Invalid numeric value for config key: " + key);
    }
    return parsed;
}

std::string getString(const std::unordered_map<std::string, std::string>& raw, const std::string& key, const std::string& defaultValue) {
    const auto it = raw.find(key);
    if (it == raw.end()) {
        return defaultValue;
    }
    return trim(it->second);
}

}  // namespace

std::unordered_map<std::string, std::string> parseConfigFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    std::unordered_map<std::string, std::string> raw;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, equalPos));
        const std::string value = trim(line.substr(equalPos + 1));
        raw[key] = value;
    }
    return raw;
}

BenchmarkConfig loadBenchmarkConfig(const std::string& path) {
    const auto raw = parseConfigFile(path);

    BenchmarkConfig config;
    config.inputPaths = parseList(getRequired(raw, "input_paths"));
    config.referenceCsv = getString(raw, "reference_csv", "");
    config.outputCsv = getString(raw, "output_csv", config.outputCsv);
    config.summaryCsv = getString(raw, "summary_csv", config.summaryCsv);
    config.seed = getNumeric<std::uint64_t>(raw, "seed", config.seed);
    config.repetitions = getNumeric<int>(raw, "repetitions", config.repetitions);
    config.maxIterations = getNumeric<int>(raw, "max_iterations", config.maxIterations);
    config.timeLimitMs = getNumeric<double>(raw, "time_limit_ms", config.timeLimitMs);
    config.initialTemperature = getNumeric<double>(raw, "initial_temperature", config.initialTemperature);
    config.minimumTemperature = getNumeric<double>(raw, "minimum_temperature", config.minimumTemperature);
    config.coolingRate = getNumeric<double>(raw, "cooling_rate", config.coolingRate);
    config.iterationsPerTemperature = getNumeric<int>(raw, "iterations_per_temperature", config.iterationsPerTemperature);
    config.initialSolution = getString(raw, "initial_solution", config.initialSolution);
    config.neighborhood = getString(raw, "neighborhood", config.neighborhood);
    config.useHeldKarpReference = parseBool(getString(raw, "use_held_karp_reference", config.useHeldKarpReference ? "true" : "false"));
    config.heldKarpMaxN = getNumeric<int>(raw, "held_karp_max_n", config.heldKarpMaxN);
    config.verbose = parseBool(getString(raw, "verbose", config.verbose ? "true" : "false"));

    return config;
}
