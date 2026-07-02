#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

std::vector<std::string> split_list(const std::string& value) {
    std::vector<std::string> out;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::unordered_map<std::string, std::string> load_config(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open config: " + path);
    std::unordered_map<std::string, std::string> cfg;
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const auto comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("Invalid config line " + std::to_string(line_no));
        }
        cfg[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return cfg;
}

std::string get_value(const std::unordered_map<std::string, std::string>& cfg,
                      const std::string& key,
                      const std::string& fallback = "") {
    const auto it = cfg.find(key);
    return it == cfg.end() ? fallback : it->second;
}

struct Instance {
    std::string name;
    std::string source = "unknown";
    std::string family = "generic";
    std::vector<std::vector<int>> matrix;
    std::optional<long long> best_known;
    bool symmetric = false;

    int n() const {
        return static_cast<int>(matrix.size());
    }
};

bool is_symmetric(const std::vector<std::vector<int>>& matrix) {
    const int n = static_cast<int>(matrix.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (matrix[i][j] != matrix[j][i]) return false;
        }
    }
    return true;
}

long long route_cost(const Instance& instance, const std::vector<int>& route) {
    const int n = instance.n();
    long long total = 0;
    for (int i = 0; i + 1 < n; ++i) {
        total += instance.matrix[route[i]][route[i + 1]];
    }
    total += instance.matrix[route.back()][route.front()];
    return total;
}

std::string route_to_string(const std::vector<int>& route) {
    std::ostringstream out;
    for (size_t i = 0; i < route.size(); ++i) {
        if (i) out << ' ';
        out << route[i];
    }
    return out.str();
}

std::optional<double> relative_error(long long cost, const std::optional<long long>& best_known) {
    if (!best_known || *best_known == 0) return std::nullopt;
    return (static_cast<double>(cost - *best_known) / static_cast<double>(*best_known)) * 100.0;
}

std::pair<std::string, std::string> parse_header_line(const std::string& line) {
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        return {upper(trim(line.substr(0, colon))), trim(line.substr(colon + 1))};
    }
    std::istringstream iss(line);
    std::string key;
    iss >> key;
    std::string value;
    std::getline(iss, value);
    return {upper(trim(key)), trim(value)};
}

double geo_to_rad(double value) {
    const int deg = static_cast<int>(value);
    const double minutes = value - deg;
    return kPi * (deg + 5.0 * minutes / 3.0) / 180.0;
}

int geo_distance(double x1, double y1, double x2, double y2) {
    const double rrr = 6378.388;
    const double lat_i = geo_to_rad(x1);
    const double lon_i = geo_to_rad(y1);
    const double lat_j = geo_to_rad(x2);
    const double lon_j = geo_to_rad(y2);
    const double q1 = std::cos(lon_i - lon_j);
    const double q2 = std::cos(lat_i - lat_j);
    const double q3 = std::cos(lat_i + lat_j);
    return static_cast<int>(rrr * std::acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
}

int coord_distance(double x1, double y1, double x2, double y2, const std::string& type) {
    const double dx = x1 - x2;
    const double dy = y1 - y2;
    if (type == "EUC_2D") {
        return static_cast<int>(std::floor(std::sqrt(dx * dx + dy * dy) + 0.5));
    }
    if (type == "CEIL_2D") {
        return static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy)));
    }
    if (type == "ATT") {
        const double rij = std::sqrt((dx * dx + dy * dy) / 10.0);
        const int tij = static_cast<int>(std::floor(rij + 0.5));
        return tij < rij ? tij + 1 : tij;
    }
    if (type == "GEO") {
        return geo_distance(x1, y1, x2, y2);
    }
    throw std::runtime_error("Unsupported EDGE_WEIGHT_TYPE: " + type);
}

std::vector<std::vector<int>> build_explicit_matrix(int n, const std::vector<int>& values,
                                                    const std::string& format) {
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    size_t k = 0;
    if (format == "FULL_MATRIX" || format.empty()) {
        if (values.size() < static_cast<size_t>(n * n)) {
            throw std::runtime_error("FULL_MATRIX has too few values");
        }
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) matrix[i][j] = values[k++];
        }
        return matrix;
    }
    if (format == "UPPER_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                matrix[i][j] = matrix[j][i] = values[k++];
            }
        }
        return matrix;
    }
    if (format == "LOWER_ROW") {
        for (int i = 1; i < n; ++i) {
            for (int j = 0; j < i; ++j) {
                matrix[i][j] = matrix[j][i] = values[k++];
            }
        }
        return matrix;
    }
    if (format == "UPPER_DIAG_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = i; j < n; ++j) {
                matrix[i][j] = matrix[j][i] = values[k++];
            }
        }
        return matrix;
    }
    if (format == "LOWER_DIAG_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) {
                matrix[i][j] = matrix[j][i] = values[k++];
            }
        }
        return matrix;
    }
    throw std::runtime_error("Unsupported EDGE_WEIGHT_FORMAT: " + format);
}

Instance load_tsplib(const std::filesystem::path& path, std::optional<long long> best_known) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open instance: " + path.string());

    std::map<std::string, std::string> meta;
    std::vector<std::pair<double, double>> coords;
    std::vector<int> explicit_weights;
    enum class Section { Header, NodeCoord, EdgeWeight };
    Section section = Section::Header;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        const std::string u = upper(line);
        if (u == "EOF") break;
        if (u == "NODE_COORD_SECTION") {
            section = Section::NodeCoord;
            continue;
        }
        if (u == "EDGE_WEIGHT_SECTION") {
            section = Section::EdgeWeight;
            continue;
        }
        if (section == Section::Header) {
            auto [key, value] = parse_header_line(line);
            meta[key] = value;
        } else if (section == Section::NodeCoord) {
            std::istringstream iss(line);
            int id = 0;
            double x = 0.0;
            double y = 0.0;
            if (!(iss >> id >> x >> y)) throw std::runtime_error("Invalid NODE_COORD_SECTION");
            coords.push_back({x, y});
        } else {
            std::istringstream iss(line);
            double value = 0.0;
            while (iss >> value) explicit_weights.push_back(static_cast<int>(value));
        }
    }

    const int n = std::stoi(meta.at("DIMENSION"));
    const std::string name = meta.count("NAME") ? meta["NAME"] : path.stem().string();
    const std::string type = meta.count("EDGE_WEIGHT_TYPE") ? upper(meta["EDGE_WEIGHT_TYPE"]) : "EUC_2D";
    const std::string format = meta.count("EDGE_WEIGHT_FORMAT") ? upper(meta["EDGE_WEIGHT_FORMAT"]) : "";

    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    if (!coords.empty()) {
        if (static_cast<int>(coords.size()) != n) throw std::runtime_error("Coordinate count mismatch");
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                matrix[i][j] = i == j ? 0 : coord_distance(coords[i].first, coords[i].second,
                                                           coords[j].first, coords[j].second, type);
            }
        }
    } else {
        matrix = build_explicit_matrix(n, explicit_weights, format);
    }

    Instance instance;
    instance.name = name;
    instance.source = "tsplib";
    instance.family = type;
    instance.matrix = std::move(matrix);
    instance.best_known = best_known;
    instance.symmetric = is_symmetric(instance.matrix);
    return instance;
}

std::unordered_map<std::string, long long> load_best_known_csv(const std::string& path) {
    std::unordered_map<std::string, long long> out;
    if (path.empty()) return out;
    std::ifstream in(path);
    if (!in) return out;
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        if (trim(line).empty()) continue;
        std::stringstream ss(line);
        std::string name;
        std::string value;
        std::getline(ss, name, ',');
        std::getline(ss, value, ',');
        out[trim(name)] = static_cast<long long>(std::stod(value));
    }
    return out;
}

std::vector<std::filesystem::path> expand_glob(const std::string& pattern) {
    std::vector<std::filesystem::path> paths;
    const auto star = pattern.find('*');
    if (star == std::string::npos) {
        paths.push_back(pattern);
        return paths;
    }
    const std::filesystem::path p(pattern);
    const auto dir = p.parent_path().empty() ? std::filesystem::path(".") : p.parent_path();
    const std::string filename_pattern = p.filename().string();
    const std::string suffix = filename_pattern.substr(filename_pattern.find('*') + 1);
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (suffix.empty() || (name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

Instance generate_complete_graph(int n, bool symmetric, int low, int high, uint64_t seed,
                                 const std::string& name) {
    std::mt19937 rng(static_cast<unsigned int>(seed));
    std::uniform_int_distribution<int> dist(low, high);
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    if (symmetric) {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                const int w = dist(rng);
                matrix[i][j] = matrix[j][i] = w;
            }
        }
    } else {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i != j) matrix[i][j] = dist(rng);
            }
        }
    }
    return {name, "generated", "generated_complete", std::move(matrix), std::nullopt, symmetric};
}

struct AlgorithmResult {
    std::string algorithm;
    std::vector<int> route;
    long long cost = -1;
    double time_s = 0.0;
    bool completed = true;
    bool timed_out = false;
    std::string extra;
};

AlgorithmResult random_search(const Instance& instance, int trials, uint64_t seed) {
    const auto start = Clock::now();
    std::mt19937 rng(static_cast<unsigned int>(seed));
    std::vector<int> route(instance.n());
    std::iota(route.begin(), route.end(), 0);
    std::vector<int> best_route;
    long long best_cost = std::numeric_limits<long long>::max();
    for (int t = 0; t < trials; ++t) {
        std::shuffle(route.begin(), route.end(), rng);
        const long long cost = route_cost(instance, route);
        if (cost < best_cost) {
            best_cost = cost;
            best_route = route;
        }
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return {"rand", best_route, best_cost, elapsed, true, false, "trials=" + std::to_string(trials)};
}

AlgorithmResult nearest_neighbor(const Instance& instance, int start_city) {
    const auto start = Clock::now();
    const int n = instance.n();
    std::vector<unsigned char> used(n, 0);
    std::vector<int> route;
    route.reserve(n);
    int current = start_city;
    for (int step = 0; step < n; ++step) {
        route.push_back(current);
        used[current] = 1;
        int best = -1;
        int best_distance = std::numeric_limits<int>::max();
        for (int v = 0; v < n; ++v) {
            if (!used[v] && instance.matrix[current][v] < best_distance) {
                best_distance = instance.matrix[current][v];
                best = v;
            }
        }
        if (best == -1) break;
        current = best;
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return {"nn", route, route_cost(instance, route), elapsed, true, false,
            "start=" + std::to_string(start_city)};
}

AlgorithmResult repetitive_nearest_neighbor(const Instance& instance, double timeout_s) {
    const auto start = Clock::now();
    std::vector<int> best_route;
    long long best_cost = std::numeric_limits<long long>::max();
    for (int s = 0; s < instance.n(); ++s) {
        if (timeout_s > 0.0 &&
            std::chrono::duration<double>(Clock::now() - start).count() >= timeout_s) {
            return {"rnn", best_route, best_cost == std::numeric_limits<long long>::max() ? -1 : best_cost,
                    std::chrono::duration<double>(Clock::now() - start).count(), false, true, "timeout"};
        }
        auto result = nearest_neighbor(instance, s);
        if (result.cost < best_cost) {
            best_cost = result.cost;
            best_route = result.route;
        }
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return {"rnn", best_route, best_cost, elapsed, true, false, "starts=" + std::to_string(instance.n())};
}

AlgorithmResult brute_force(const Instance& instance, double timeout_s) {
    const auto start = Clock::now();
    const int n = instance.n();
    std::vector<int> tail;
    for (int i = 1; i < n; ++i) tail.push_back(i);
    std::vector<int> best_route;
    long long best_cost = std::numeric_limits<long long>::max();
    long long checked = 0;
    do {
        if (timeout_s > 0.0 &&
            std::chrono::duration<double>(Clock::now() - start).count() >= timeout_s) {
            return {"bruteforce", best_route,
                    best_cost == std::numeric_limits<long long>::max() ? -1 : best_cost,
                    std::chrono::duration<double>(Clock::now() - start).count(), false, true,
                    "checked_permutations=" + std::to_string(checked)};
        }
        std::vector<int> route;
        route.reserve(n);
        route.push_back(0);
        route.insert(route.end(), tail.begin(), tail.end());
        const long long cost = route_cost(instance, route);
        ++checked;
        if (cost < best_cost) {
            best_cost = cost;
            best_route = std::move(route);
        }
    } while (std::next_permutation(tail.begin(), tail.end()));

    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return {"bruteforce", best_route, best_cost, elapsed, true, false,
            "checked_permutations=" + std::to_string(checked)};
}

AlgorithmResult run_algorithm(const Instance& instance, const std::string& algorithm,
                              int rand_trials, uint64_t seed, double timeout_s) {
    const std::string a = upper(algorithm);
    if (a == "RAND") return random_search(instance, rand_trials, seed);
    if (a == "NN") return nearest_neighbor(instance, 0);
    if (a == "RNN") return repetitive_nearest_neighbor(instance, timeout_s);
    if (a == "BRUTEFORCE" || a == "BF" || a == "BRUTE_FORCE") return brute_force(instance, timeout_s);
    throw std::runtime_error("Unsupported algorithm: " + algorithm);
}

std::string csv_bool(bool value) {
    return value ? "true" : "false";
}

void write_rows(const std::string& path, const std::vector<std::vector<std::string>>& rows) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write CSV: " + path);
    out << "instance_name,source,family,n,symmetric,algorithm,seed,cost,best_known,rel_error_pct,time_s,completed,timed_out,route,extra\n";
    out << std::fixed << std::setprecision(6);
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i) out << ',';
            const bool needs_quote = row[i].find_first_of(",\"") != std::string::npos;
            if (needs_quote) out << '"';
            for (char c : row[i]) {
                if (c == '"') out << "\"\"";
                else out << c;
            }
            if (needs_quote) out << '"';
        }
        out << '\n';
    }
}

std::vector<std::string> make_row(const Instance& instance, const std::string& algorithm,
                                  const AlgorithmResult& result, uint64_t seed) {
    std::ostringstream time_s;
    time_s << std::fixed << std::setprecision(6) << result.time_s;
    std::string best_known;
    std::string rel_error_s;
    if (instance.best_known) {
        best_known = std::to_string(*instance.best_known);
        const auto err = relative_error(result.cost, instance.best_known);
        if (err) {
            std::ostringstream os;
            os << std::fixed << std::setprecision(6) << *err;
            rel_error_s = os.str();
        }
    }
    return {
        instance.name,
        instance.source,
        instance.family,
        std::to_string(instance.n()),
        csv_bool(instance.symmetric),
        algorithm,
        std::to_string(seed),
        result.cost >= 0 ? std::to_string(result.cost) : "",
        best_known,
        rel_error_s,
        time_s.str(),
        csv_bool(result.completed),
        csv_bool(result.timed_out),
        route_to_string(result.route),
        result.extra,
    };
}

std::vector<std::vector<std::string>> run_on_instances(const std::vector<Instance>& instances,
                                                       const std::vector<std::string>& algorithms,
                                                       int rand_trials,
                                                       double timeout_s,
                                                       uint64_t base_seed) {
    std::vector<std::vector<std::string>> rows;
    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& instance = instances[i];
        std::cout << "\n=== " << instance.name << " (n=" << instance.n()
                  << ", symmetric=" << csv_bool(instance.symmetric) << ") ===\n";
        for (size_t a = 0; a < algorithms.size(); ++a) {
            const uint64_t seed = base_seed + i * 100 + a;
            std::cout << "  -> " << algorithms[a] << " ..." << std::flush;
            auto result = run_algorithm(instance, algorithms[a], rand_trials, seed, timeout_s);
            std::cout << " cost=" << result.cost << " time=" << std::fixed << std::setprecision(6)
                      << result.time_s << "s completed=" << csv_bool(result.completed)
                      << " timeout=" << csv_bool(result.timed_out) << '\n';
            rows.push_back(make_row(instance, algorithms[a], result, seed));
        }
    }
    return rows;
}

void run_from_config(const std::string& config_path) {
    const auto cfg = load_config(config_path);
    const std::string mode = get_value(cfg, "mode");
    const uint64_t seed = static_cast<uint64_t>(std::stoull(get_value(cfg, "seed", "12345")));
    const auto algorithms = split_list(get_value(cfg, "algorithms"));
    const int rand_trials = std::stoi(get_value(cfg, "rand_trials", "1000"));
    const double timeout_s = std::stod(get_value(cfg, "timeout_seconds", "0"));
    const std::string output = get_value(cfg, "output", "results.csv");

    std::vector<Instance> instances;
    if (mode == "generated") {
        int counter = 0;
        const auto sizes = split_list(get_value(cfg, "sizes"));
        const int per_size = std::stoi(get_value(cfg, "instances_per_size", "1"));
        const int low = std::stoi(get_value(cfg, "weight_low", "1"));
        const int high = std::stoi(get_value(cfg, "weight_high", "100"));
        for (const auto& size_s : sizes) {
            const int n = std::stoi(size_s);
            for (bool symmetric : {true, false}) {
                for (int k = 0; k < per_size; ++k) {
                    const uint64_t inst_seed = seed + static_cast<uint64_t>(counter++);
                    std::ostringstream name;
                    name << "gen_" << (symmetric ? "sym" : "asym") << "_n" << n << '_'
                         << std::setw(2) << std::setfill('0') << k;
                    instances.push_back(generate_complete_graph(n, symmetric, low, high, inst_seed, name.str()));
                }
            }
        }
    } else if (mode == "tsplib") {
        const auto best_known = load_best_known_csv(get_value(cfg, "best_known_csv"));
        for (const auto& path : expand_glob(get_value(cfg, "input_glob"))) {
            const std::string stem = path.stem().string();
            const auto it = best_known.find(stem);
            instances.push_back(load_tsplib(path, it == best_known.end()
                                                  ? std::optional<long long>{}
                                                  : std::optional<long long>{it->second}));
        }
    } else {
        throw std::runtime_error("mode must be generated or tsplib");
    }

    const auto rows = run_on_instances(instances, algorithms, rand_trials, timeout_s, seed);
    write_rows(output, rows);
    std::cout << "\nSaved: " << output << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./tsp_stage1 <config_path>\n";
        return 1;
    }
    try {
        run_from_config(argv[1]);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}

