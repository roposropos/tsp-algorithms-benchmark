#include "instance_loader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace {

constexpr long long INF = std::numeric_limits<long long>::max() / 4;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open input file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::vector<int> parseIntRow(const std::string& line) {
    std::vector<int> row;
    std::stringstream stream(line);
    std::string token;
    while (stream >> token) {
        row.push_back(static_cast<int>(std::llround(std::stod(token))));
    }
    return row;
}

std::string toUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

GraphType parseCustomGraphType(const std::string& raw) {
    const std::string normalized = toUpper(trim(raw));
    if (normalized == "ASYMMETRIC" || normalized == "ATSP") {
        return GraphType::Asymmetric;
    }
    return GraphType::Symmetric;
}

TSPInstance parseCustomMatrixFormat(const std::filesystem::path& path, const std::string& text) {
    const auto lines = splitLines(text);

    std::string name = path.stem().string();
    GraphType graphType = GraphType::Symmetric;
    int size = -1;
    std::optional<long long> referenceCost;
    bool inMatrix = false;
    std::vector<std::vector<int>> matrix;

    for (const auto& rawLine : lines) {
        const std::string line = trim(rawLine);
        if (line.empty()) {
            continue;
        }

        if (!inMatrix) {
            if (line == "MATRIX") {
                inMatrix = true;
                continue;
            }
            const auto equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                const std::string key = trim(line.substr(0, equalPos));
                const std::string value = trim(line.substr(equalPos + 1));
                if (key == "TYPE") {
                    graphType = parseCustomGraphType(value);
                } else if (key == "NAME") {
                    name = value;
                } else if (key == "SIZE") {
                    size = std::stoi(value);
                } else if (key == "REFERENCE_COST") {
                    referenceCost = std::stoll(value);
                }
            }
            continue;
        }

        matrix.push_back(parseIntRow(line));
    }

    if (size < 0) {
        throw std::runtime_error("Missing SIZE in file: " + path.string());
    }
    if (static_cast<int>(matrix.size()) != size) {
        throw std::runtime_error("Matrix row count does not match SIZE in file: " + path.string());
    }
    for (const auto& row : matrix) {
        if (static_cast<int>(row.size()) != size) {
            throw std::runtime_error("Matrix column count does not match SIZE in file: " + path.string());
        }
    }

    return TSPInstance{name, graphType, "generated", path, matrix, referenceCost};
}

std::vector<std::vector<int>> parseSimpleMatrixFormat(const std::filesystem::path& path, const std::string& text) {
    std::vector<std::string> filtered;
    for (const auto& rawLine : splitLines(text)) {
        const std::string line = trim(rawLine);
        if (!line.empty() && line[0] != '#') {
            filtered.push_back(line);
        }
    }
    if (filtered.empty()) {
        throw std::runtime_error("Empty matrix file: " + path.string());
    }

    const int n = std::stoi(filtered.front());
    if (static_cast<int>(filtered.size()) != n + 1) {
        throw std::runtime_error("Invalid matrix format in file: " + path.string());
    }

    std::vector<std::vector<int>> matrix;
    matrix.reserve(n);
    for (int row = 0; row < n; ++row) {
        matrix.push_back(parseIntRow(filtered[row + 1]));
        if (static_cast<int>(matrix.back().size()) != n) {
            throw std::runtime_error("Invalid matrix width in file: " + path.string());
        }
    }
    return matrix;
}

int geoDistance(const std::pair<double, double>& a, const std::pair<double, double>& b) {
    auto geoToRadians = [](double value) {
        const int degrees = static_cast<int>(value);
        const double minutes = value - static_cast<double>(degrees);
        return M_PI * (static_cast<double>(degrees) + 5.0 * minutes / 3.0) / 180.0;
    };

    const double latitudeI = geoToRadians(a.first);
    const double longitudeI = geoToRadians(a.second);
    const double latitudeJ = geoToRadians(b.first);
    const double longitudeJ = geoToRadians(b.second);
    const double q1 = std::cos(longitudeI - longitudeJ);
    const double q2 = std::cos(latitudeI - latitudeJ);
    const double q3 = std::cos(latitudeI + latitudeJ);
    return static_cast<int>(6378.388 * std::acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
}

std::vector<std::vector<int>> buildMatrixFromCoordinates(
    const std::vector<std::pair<double, double>>& coords,
    const std::string& edgeWeightType) {

    const int n = static_cast<int>(coords.size());
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                continue;
            }

            const auto [x1, y1] = coords[i];
            const auto [x2, y2] = coords[j];

            if (edgeWeightType == "EUC_2D") {
                const double distance = std::sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
                matrix[i][j] = static_cast<int>(std::llround(distance));
            } else if (edgeWeightType == "ATT") {
                const double rij = std::sqrt(((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)) / 10.0);
                const int tij = static_cast<int>(std::llround(rij));
                matrix[i][j] = (static_cast<double>(tij) >= rij) ? tij : tij + 1;
            } else if (edgeWeightType == "GEO") {
                matrix[i][j] = geoDistance(coords[i], coords[j]);
            } else {
                throw std::runtime_error("Unsupported EDGE_WEIGHT_TYPE for coordinates: " + edgeWeightType);
            }
        }
    }

    return matrix;
}

std::vector<std::vector<int>> buildExplicitMatrix(int n, const std::vector<int>& values, const std::string& format) {
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));

    if (format == "FULL_MATRIX") {
        if (static_cast<int>(values.size()) != n * n) {
            throw std::runtime_error("FULL_MATRIX size mismatch");
        }
        int index = 0;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                matrix[i][j] = values[index++];
            }
        }
        return matrix;
    }

    int index = 0;
    if (format == "UPPER_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                matrix[i][j] = values.at(index);
                matrix[j][i] = values.at(index);
                ++index;
            }
        }
        return matrix;
    }

    if (format == "LOWER_ROW") {
        for (int i = 1; i < n; ++i) {
            for (int j = 0; j < i; ++j) {
                matrix[i][j] = values.at(index);
                matrix[j][i] = values.at(index);
                ++index;
            }
        }
        return matrix;
    }

    if (format == "UPPER_DIAG_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = i; j < n; ++j) {
                matrix[i][j] = values.at(index);
                matrix[j][i] = values.at(index);
                ++index;
            }
        }
        return matrix;
    }

    if (format == "LOWER_DIAG_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) {
                matrix[i][j] = values.at(index);
                matrix[j][i] = values.at(index);
                ++index;
            }
        }
        return matrix;
    }

    throw std::runtime_error("Unsupported EDGE_WEIGHT_FORMAT: " + format);
}

TSPInstance parseTsplibFormat(const std::filesystem::path& path, const std::string& text) {
    const auto lines = splitLines(text);

    std::unordered_map<std::string, std::string> meta;
    std::size_t index = 0;
    for (; index < lines.size(); ++index) {
        const std::string line = trim(lines[index]);
        if (line.empty()) {
            continue;
        }
        const std::string upperLine = toUpper(line);
        if (upperLine == "NODE_COORD_SECTION" || upperLine == "EDGE_WEIGHT_SECTION" || upperLine == "DISPLAY_DATA_SECTION") {
            break;
        }

        const auto colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            meta[toUpper(trim(line.substr(0, colonPos)))] = trim(line.substr(colonPos + 1));
            continue;
        }

        std::stringstream split(line);
        std::string key;
        std::string value;
        if (split >> key && std::getline(split, value)) {
            meta[toUpper(trim(key))] = trim(value);
        }
    }

    if (!meta.count("DIMENSION")) {
        throw std::runtime_error("TSPLIB file has no DIMENSION: " + path.string());
    }

    const std::string name = meta.count("NAME") ? meta["NAME"] : path.stem().string();
    const std::string type = meta.count("TYPE") ? toUpper(meta["TYPE"]) : "TSP";
    const std::string edgeWeightType = meta.count("EDGE_WEIGHT_TYPE") ? toUpper(meta["EDGE_WEIGHT_TYPE"]) : "EXPLICIT";
    const std::string edgeWeightFormat = meta.count("EDGE_WEIGHT_FORMAT") ? toUpper(meta["EDGE_WEIGHT_FORMAT"]) : "FULL_MATRIX";
    const int dimension = std::stoi(meta["DIMENSION"]);
    const GraphType graphType = (type == "ATSP") ? GraphType::Asymmetric : GraphType::Symmetric;

    if (index >= lines.size()) {
        throw std::runtime_error("TSPLIB file has no data section: " + path.string());
    }

    const std::string section = toUpper(trim(lines[index]));
    if (section == "NODE_COORD_SECTION") {
        std::vector<std::pair<double, double>> coords;
        ++index;
        for (; index < lines.size(); ++index) {
            const std::string line = trim(lines[index]);
            if (line.empty() || toUpper(line) == "EOF") {
                break;
            }
            std::stringstream row(line);
            int id = 0;
            double x = 0.0;
            double y = 0.0;
            row >> id >> x >> y;
            if (row.fail()) {
                throw std::runtime_error("Invalid NODE_COORD_SECTION row in: " + path.string());
            }
            coords.emplace_back(x, y);
        }

        if (static_cast<int>(coords.size()) != dimension) {
            throw std::runtime_error("Coordinate count mismatch in: " + path.string());
        }

        return TSPInstance{name, graphType, "tsplib", path, buildMatrixFromCoordinates(coords, edgeWeightType), std::nullopt};
    }

    if (section == "EDGE_WEIGHT_SECTION") {
        std::vector<int> values;
        ++index;
        for (; index < lines.size(); ++index) {
            const std::string line = trim(lines[index]);
            if (line.empty() || toUpper(line) == "EOF") {
                break;
            }
            const auto rowValues = parseIntRow(line);
            values.insert(values.end(), rowValues.begin(), rowValues.end());
        }

        return TSPInstance{name, graphType, "tsplib", path, buildExplicitMatrix(dimension, values, edgeWeightFormat), std::nullopt};
    }

    throw std::runtime_error("Unsupported TSPLIB structure in: " + path.string());
}

TSPInstance loadSingleInstance(const std::filesystem::path& path) {
    const std::string text = readFile(path);
    if (text.find("MATRIX") != std::string::npos && text.find("TYPE=") != std::string::npos) {
        return parseCustomMatrixFormat(path, text);
    }
    if ((text.find("TYPE") != std::string::npos && text.find("DIMENSION") != std::string::npos) ||
        path.extension() == ".tsp" || path.extension() == ".atsp") {
        return parseTsplibFormat(path, text);
    }

    auto matrix = parseSimpleMatrixFormat(path, text);
    return TSPInstance{path.stem().string(), GraphType::Symmetric, "custom", path, std::move(matrix), std::nullopt};
}

}  // namespace

std::vector<TSPInstance> loadInstances(const std::vector<std::string>& inputPaths) {
    std::vector<TSPInstance> instances;

    for (const auto& rawPath : inputPaths) {
        const std::filesystem::path path(rawPath);
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Input path does not exist: " + path.string());
        }

        if (std::filesystem::is_directory(path)) {
            std::vector<std::filesystem::path> files;
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                }
            }
            std::sort(files.begin(), files.end());
            for (const auto& file : files) {
                instances.push_back(loadSingleInstance(file));
            }
            continue;
        }

        instances.push_back(loadSingleInstance(path));
    }

    std::sort(instances.begin(), instances.end(), [](const TSPInstance& left, const TSPInstance& right) {
        if (left.size() != right.size()) {
            return left.size() < right.size();
        }
        return left.name < right.name;
    });

    return instances;
}

std::unordered_map<std::string, long long> loadReferenceCsv(const std::string& csvPath) {
    std::unordered_map<std::string, long long> referenceMap;
    if (csvPath.empty()) {
        return referenceMap;
    }

    std::ifstream input(csvPath);
    if (!input) {
        throw std::runtime_error("Cannot open reference CSV: " + csvPath);
    }

    std::string line;
    bool headerSkipped = false;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }

        std::stringstream row(line);
        std::string name;
        std::string bestKnown;
        std::getline(row, name, ',');
        std::getline(row, bestKnown, ',');
        if (!trim(name).empty() && !trim(bestKnown).empty()) {
            referenceMap[trim(name)] = std::stoll(trim(bestKnown));
        }
    }

    return referenceMap;
}

std::optional<long long> computeHeldKarpReference(const TSPInstance& instance, int maxN) {
    const int n = instance.size();
    if (n <= 1) {
        return 0;
    }
    if (n > maxN) {
        return std::nullopt;
    }
    if (n >= static_cast<int>(sizeof(unsigned int) * 8)) {
        return std::nullopt;
    }

    const unsigned int subsetCount = 1u << (n - 1);
    std::vector<long long> dp(static_cast<std::size_t>(subsetCount) * static_cast<std::size_t>(n), INF);

    auto index = [n](unsigned int mask, int last) {
        return static_cast<std::size_t>(mask) * static_cast<std::size_t>(n) + static_cast<std::size_t>(last);
    };

    for (int city = 1; city < n; ++city) {
        const unsigned int mask = 1u << (city - 1);
        dp[index(mask, city)] = instance.matrix[0][city];
    }

    for (unsigned int mask = 1; mask < subsetCount; ++mask) {
        for (int last = 1; last < n; ++last) {
            if ((mask & (1u << (last - 1))) == 0u) {
                continue;
            }

            const unsigned int previousMask = mask ^ (1u << (last - 1));
            if (previousMask == 0u) {
                continue;
            }

            long long best = INF;
            for (int prev = 1; prev < n; ++prev) {
                if ((previousMask & (1u << (prev - 1))) == 0u) {
                    continue;
                }
                const long long candidate = dp[index(previousMask, prev)] + instance.matrix[prev][last];
                if (candidate < best) {
                    best = candidate;
                }
            }
            dp[index(mask, last)] = std::min(dp[index(mask, last)], best);
        }
    }

    const unsigned int fullMask = subsetCount - 1u;
    long long answer = INF;
    for (int last = 1; last < n; ++last) {
        const long long candidate = dp[index(fullMask, last)] + instance.matrix[last][0];
        if (candidate < answer) {
            answer = candidate;
        }
    }

    if (answer >= INF / 2) {
        return std::nullopt;
    }
    return answer;
}
