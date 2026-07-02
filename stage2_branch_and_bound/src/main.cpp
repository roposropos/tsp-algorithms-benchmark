#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr long long kInf = std::numeric_limits<long long>::max() / 4;

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

std::unordered_map<std::string, std::string> parse_config(const std::string& path) {
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

bool get_bool(const std::unordered_map<std::string, std::string>& cfg,
              const std::string& key,
              bool fallback) {
    const std::string value = upper(get_value(cfg, key, fallback ? "true" : "false"));
    return value == "1" || value == "TRUE" || value == "YES" || value == "Y" || value == "ON";
}

std::string bool_word(bool value) {
    return value ? "True" : "False";
}

std::string bool_lower(bool value) {
    return value ? "true" : "false";
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
    if (type == "ATT") {
        const double rij = std::sqrt((dx * dx + dy * dy) / 10.0);
        const int tij = static_cast<int>(std::floor(rij + 0.5));
        return tij < rij ? tij + 1 : tij;
    }
    if (type == "GEO") {
        return geo_distance(x1, y1, x2, y2);
    }
    throw std::runtime_error("Unsupported coordinate EDGE_WEIGHT_TYPE: " + type);
}

std::vector<std::vector<int>> build_explicit_matrix(int n, const std::vector<int>& values,
                                                    const std::string& format) {
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    size_t k = 0;
    if (format == "FULL_MATRIX" || format.empty()) {
        if (values.size() < static_cast<size_t>(n * n)) throw std::runtime_error("Too few matrix values");
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) matrix[i][j] = values[k++];
        }
        return matrix;
    }
    if (format == "UPPER_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) matrix[i][j] = matrix[j][i] = values[k++];
        }
        return matrix;
    }
    if (format == "LOWER_ROW") {
        for (int i = 1; i < n; ++i) {
            for (int j = 0; j < i; ++j) matrix[i][j] = matrix[j][i] = values[k++];
        }
        return matrix;
    }
    if (format == "UPPER_DIAG_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = i; j < n; ++j) matrix[i][j] = matrix[j][i] = values[k++];
        }
        return matrix;
    }
    if (format == "LOWER_DIAG_ROW") {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j <= i; ++j) matrix[i][j] = matrix[j][i] = values[k++];
        }
        return matrix;
    }
    throw std::runtime_error("Unsupported EDGE_WEIGHT_FORMAT: " + format);
}

std::pair<std::string, std::vector<std::vector<int>>> parse_tsplib(const std::filesystem::path& path) {
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
    const std::string ew_type = meta.count("EDGE_WEIGHT_TYPE") ? upper(meta["EDGE_WEIGHT_TYPE"]) : "EUC_2D";
    const std::string ew_format = meta.count("EDGE_WEIGHT_FORMAT") ? upper(meta["EDGE_WEIGHT_FORMAT"]) : "";

    if (!coords.empty()) {
        std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                matrix[i][j] = i == j ? 0 : coord_distance(coords[i].first, coords[i].second,
                                                           coords[j].first, coords[j].second, ew_type);
            }
        }
        return {name, matrix};
    }
    return {name, build_explicit_matrix(n, explicit_weights, ew_format)};
}

std::pair<std::string, std::vector<std::vector<int>>> load_instance(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open instance: " + path.string());
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (text.find("TYPE") != std::string::npos && text.find("TSP") != std::string::npos) {
        return parse_tsplib(path);
    }
    std::stringstream ss(text);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line)) {
        line = trim(line);
        if (!line.empty() && line[0] != '#') lines.push_back(line);
    }
    if (lines.empty()) throw std::runtime_error("Empty matrix file");
    const int n = std::stoi(lines[0]);
    std::vector<std::vector<int>> matrix(n, std::vector<int>(n, 0));
    if (static_cast<int>(lines.size()) - 1 < n) throw std::runtime_error("Matrix has too few rows");
    for (int i = 0; i < n; ++i) {
        std::stringstream row(lines[i + 1]);
        for (int j = 0; j < n; ++j) {
            double value = 0.0;
            if (!(row >> value)) throw std::runtime_error("Invalid matrix row");
            matrix[i][j] = static_cast<int>(value);
        }
    }
    return {path.stem().string(), matrix};
}

struct Node {
    std::vector<int> path;
    uint64_t visited_mask = 0;
    int last = 0;
    long long cost_so_far = 0;
    int level = 0;
    long long lower_bound = kInf;
};

struct Result {
    std::string instance_name;
    std::string instance_path;
    int n = 0;
    std::string mode;
    std::optional<long long> best_cost;
    std::vector<int> best_path;
    double time_ms = 0.0;
    long long expanded_nodes = 0;
    long long generated_nodes = 0;
    long long pruned_by_bound = 0;
    long long pruned_infeasible = 0;
    long long completed_tours = 0;
    long long max_open_size = 0;
    bool timed_out = false;
    bool optimality_proven = false;
    std::optional<long long> initial_upper_bound;
    std::string initial_upper_bound_method = "infinity";
    std::string lower_bound_method = "simple_admissible_min_outgoing";
};

long long full_tour_cost(const std::vector<std::vector<int>>& matrix, const std::vector<int>& path) {
    long long total = 0;
    for (size_t i = 0; i + 1 < path.size(); ++i) total += matrix[path[i]][path[i + 1]];
    total += matrix[path.back()][path.front()];
    return total;
}

std::pair<std::vector<int>, long long> nearest_neighbor_ub(const std::vector<std::vector<int>>& matrix, int start) {
    const int n = static_cast<int>(matrix.size());
    std::vector<unsigned char> used(n, 0);
    std::vector<int> path;
    path.reserve(n);
    int current = start;
    for (int step = 0; step < n; ++step) {
        path.push_back(current);
        used[current] = 1;
        int best = -1;
        int best_distance = std::numeric_limits<int>::max();
        for (int v = 0; v < n; ++v) {
            if (!used[v] && matrix[current][v] < best_distance) {
                best_distance = matrix[current][v];
                best = v;
            }
        }
        if (best == -1) break;
        current = best;
    }
    return {path, full_tour_cost(matrix, path)};
}

std::pair<std::vector<int>, long long> best_of_all_starts_nn(const std::vector<std::vector<int>>& matrix) {
    std::vector<int> best_path;
    long long best_cost = kInf;
    for (int start = 0; start < static_cast<int>(matrix.size()); ++start) {
        auto [path, cost] = nearest_neighbor_ub(matrix, start);
        if (cost < best_cost) {
            best_path = std::move(path);
            best_cost = cost;
        }
    }
    return {best_path, best_cost};
}

std::vector<int> compute_min_outgoing(const std::vector<std::vector<int>>& matrix) {
    const int n = static_cast<int>(matrix.size());
    std::vector<int> mins(n, std::numeric_limits<int>::max());
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i != j) mins[i] = std::min(mins[i], matrix[i][j]);
        }
    }
    return mins;
}

long long lower_bound_simple(const std::vector<std::vector<int>>& matrix,
                             const Node& node,
                             const std::vector<int>& min_outgoing,
                             int start) {
    const int n = static_cast<int>(matrix.size());
    if (node.level == n) return node.cost_so_far + matrix[node.last][start];

    long long bound = node.cost_so_far;
    int leave_current = std::numeric_limits<int>::max();
    for (int v = 0; v < n; ++v) {
        const bool unvisited = (node.visited_mask & (uint64_t{1} << v)) == 0;
        if (v != node.last && (unvisited || v == start)) {
            leave_current = std::min(leave_current, matrix[node.last][v]);
        }
    }
    bound += leave_current;
    for (int v = 0; v < n; ++v) {
        if ((node.visited_mask & (uint64_t{1} << v)) == 0) bound += min_outgoing[v];
    }
    return bound;
}

struct QueueItem {
    long long priority = 0;
    long long order = 0;
    Node node;
};

struct QueueCompare {
    bool operator()(const QueueItem& a, const QueueItem& b) const {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.order > b.order;
    }
};

class OpenSet {
public:
    explicit OpenSet(std::string mode) : mode_(std::move(mode)) {}

    void push(Node node) {
        if (mode_ == "bfs") {
            deque_.push_back(std::move(node));
        } else if (mode_ == "dfs") {
            stack_.push_back(std::move(node));
        } else {
            heap_.push({node.lower_bound, order_++, std::move(node)});
        }
    }

    Node pop() {
        if (mode_ == "bfs") {
            Node node = std::move(deque_.front());
            deque_.pop_front();
            return node;
        }
        if (mode_ == "dfs") {
            Node node = std::move(stack_.back());
            stack_.pop_back();
            return node;
        }
        Node node = std::move(heap_.top().node);
        heap_.pop();
        return node;
    }

    size_t size() const {
        if (mode_ == "bfs") return deque_.size();
        if (mode_ == "dfs") return stack_.size();
        return heap_.size();
    }

private:
    std::string mode_;
    long long order_ = 0;
    std::deque<Node> deque_;
    std::vector<Node> stack_;
    std::priority_queue<QueueItem, std::vector<QueueItem>, QueueCompare> heap_;
};

Result branch_and_bound(const std::vector<std::vector<int>>& matrix,
                        const std::string& instance_name,
                        const std::string& instance_path,
                        const std::string& mode,
                        int start,
                        double time_limit_s,
                        bool use_nn_ub) {
    const int n = static_cast<int>(matrix.size());
    if (n > 63) throw std::runtime_error("This exact implementation supports up to 63 cities");
    const auto min_outgoing = compute_min_outgoing(matrix);
    std::vector<int> best_path;
    long long best_cost = kInf;
    std::optional<long long> initial_ub;
    std::string initial_ub_method = "infinity";
    if (use_nn_ub) {
        auto [path, cost] = best_of_all_starts_nn(matrix);
        best_path = std::move(path);
        best_cost = cost;
        initial_ub = cost;
        initial_ub_method = "nearest_neighbor_best_start";
    }

    Node root;
    root.path = {start};
    root.visited_mask = uint64_t{1} << start;
    root.last = start;
    root.cost_so_far = 0;
    root.level = 1;
    root.lower_bound = lower_bound_simple(matrix, root, min_outgoing, start);

    OpenSet open(mode);
    open.push(root);

    Result result;
    result.instance_name = instance_name;
    result.instance_path = instance_path;
    result.n = n;
    result.mode = mode;
    result.generated_nodes = 1;
    result.max_open_size = 1;
    result.initial_upper_bound = initial_ub;
    result.initial_upper_bound_method = initial_ub_method;

    const auto start_time = Clock::now();
    while (open.size() > 0) {
        if (std::chrono::duration<double>(Clock::now() - start_time).count() >= time_limit_s) {
            result.timed_out = true;
            break;
        }
        result.max_open_size = std::max<long long>(result.max_open_size, static_cast<long long>(open.size()));
        Node node = open.pop();
        if (node.lower_bound >= best_cost) {
            ++result.pruned_by_bound;
            continue;
        }
        ++result.expanded_nodes;

        if (node.level == n) {
            ++result.completed_tours;
            const long long total = node.cost_so_far + matrix[node.last][start];
            if (total < best_cost) {
                best_cost = total;
                best_path = node.path;
            }
            continue;
        }

        for (int next = 0; next < n; ++next) {
            if (node.visited_mask & (uint64_t{1} << next)) continue;
            Node child;
            child.path = node.path;
            child.path.push_back(next);
            child.visited_mask = node.visited_mask | (uint64_t{1} << next);
            child.last = next;
            child.cost_so_far = node.cost_so_far + matrix[node.last][next];
            child.level = node.level + 1;
            child.lower_bound = lower_bound_simple(matrix, child, min_outgoing, start);
            ++result.generated_nodes;
            if (child.lower_bound >= best_cost) {
                ++result.pruned_by_bound;
                continue;
            }
            open.push(std::move(child));
        }
    }

    result.time_ms = std::chrono::duration<double, std::milli>(Clock::now() - start_time).count();
    if (best_cost != kInf) {
        result.best_cost = best_cost;
        result.best_path = best_path;
    }
    result.optimality_proven = !result.timed_out && result.best_cost.has_value();
    return result;
}

std::string format_path(const std::vector<int>& path) {
    if (path.empty()) return "-";
    std::ostringstream out;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i) out << " -> ";
        out << path[i];
    }
    out << " -> " << path.front();
    return out.str();
}

void print_result(const Result& r) {
    std::cout << "========================================================================\n";
    std::cout << "TASK 2 - TSP Branch and Bound (C++)\n";
    std::cout << "========================================================================\n";
    std::cout << "Instance name           : " << r.instance_name << '\n';
    std::cout << "Number of cities        : " << r.n << '\n';
    std::cout << "Mode                    : " << r.mode << '\n';
    std::cout << "Initial UB method       : " << r.initial_upper_bound_method << '\n';
    std::cout << "Initial UB              : " << (r.initial_upper_bound ? std::to_string(*r.initial_upper_bound) : "None") << '\n';
    std::cout << "Lower bound method      : " << r.lower_bound_method << '\n';
    std::cout << "Timed out               : " << bool_lower(r.timed_out) << '\n';
    std::cout << "Optimality proven       : " << bool_lower(r.optimality_proven) << '\n';
    std::cout << "Best cost               : " << (r.best_cost ? std::to_string(*r.best_cost) : "None") << '\n';
    std::cout << "Best path               : " << format_path(r.best_path) << '\n';
    std::cout << "------------------------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Time [ms]               : " << r.time_ms << '\n';
    std::cout << "Expanded nodes          : " << r.expanded_nodes << '\n';
    std::cout << "Generated nodes         : " << r.generated_nodes << '\n';
    std::cout << "Pruned by bound         : " << r.pruned_by_bound << '\n';
    std::cout << "Pruned infeasible       : " << r.pruned_infeasible << '\n';
    std::cout << "Completed tours         : " << r.completed_tours << '\n';
    std::cout << "Max OPEN size           : " << r.max_open_size << '\n';
    std::cout << "========================================================================\n";
}

std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

std::string result_csv_row(const Result& r) {
    std::ostringstream out;
    out << csv_escape(r.instance_name) << ','
        << csv_escape(r.instance_path) << ','
        << r.n << ','
        << r.mode << ','
        << (r.best_cost ? std::to_string(*r.best_cost) : "") << ','
        << std::fixed << std::setprecision(3) << r.time_ms << ','
        << r.expanded_nodes << ','
        << r.generated_nodes << ','
        << r.pruned_by_bound << ','
        << r.pruned_infeasible << ','
        << r.completed_tours << ','
        << r.max_open_size << ','
        << bool_word(r.timed_out) << ','
        << bool_word(r.optimality_proven) << ','
        << (r.initial_upper_bound ? std::to_string(*r.initial_upper_bound) : "") << ','
        << r.initial_upper_bound_method << ','
        << r.lower_bound_method << ','
        << csv_escape(format_path(r.best_path));
    return out.str();
}

void write_single_csv(const Result& r, const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write CSV: " + path);
    out << "instance_name,instance_path,n,mode,best_cost,time_ms,expanded_nodes,generated_nodes,pruned_by_bound,pruned_infeasible,completed_tours,max_open_size,timed_out,optimality_proven,initial_upper_bound,initial_upper_bound_method,lower_bound_method,best_path\n";
    out << result_csv_row(r) << '\n';
}

std::vector<std::filesystem::path> list_instances(const std::string& dir, const std::string& pattern) {
    std::vector<std::filesystem::path> paths;
    const std::string suffix = pattern.find('*') == std::string::npos
                                   ? pattern
                                   : pattern.substr(pattern.find('*') + 1);
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

double avg(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / static_cast<double>(values.size());
}

double avg_ll(const std::vector<long long>& values) {
    if (values.empty()) return 0.0;
    long double sum = 0.0;
    for (long long v : values) sum += static_cast<long double>(v);
    return static_cast<double>(sum / values.size());
}

void write_batch_outputs(const std::vector<Result>& results,
                         const std::string& detailed_csv,
                         const std::string& summary_csv,
                         const std::string& by_n_csv) {
    {
        std::ofstream out(detailed_csv);
        out << "instance_name,instance_path,n,mode,best_cost,time_ms,expanded_nodes,generated_nodes,pruned_by_bound,pruned_infeasible,completed_tours,max_open_size,timed_out,optimality_proven,initial_upper_bound,initial_upper_bound_method,lower_bound_method,best_path\n";
        for (const auto& r : results) out << result_csv_row(r) << '\n';
    }

    std::map<std::string, std::vector<Result>> by_mode;
    std::map<std::pair<int, std::string>, std::vector<Result>> by_size_mode;
    for (const auto& r : results) {
        by_mode[r.mode].push_back(r);
        by_size_mode[{r.n, r.mode}].push_back(r);
    }

    auto write_group = [](std::ostream& out, const std::string& prefix,
                          const auto& key, const std::vector<Result>& rows) {
        std::vector<double> times;
        std::vector<long long> expanded;
        std::vector<long long> generated;
        std::vector<long long> pruned;
        std::vector<long long> completed;
        std::vector<long long> open_sizes;
        int timeouts = 0;
        int proven = 0;
        long long min_cost = kInf;
        long long max_cost = 0;
        for (const auto& r : rows) {
            times.push_back(r.time_ms);
            expanded.push_back(r.expanded_nodes);
            generated.push_back(r.generated_nodes);
            pruned.push_back(r.pruned_by_bound);
            completed.push_back(r.completed_tours);
            open_sizes.push_back(r.max_open_size);
            if (r.timed_out) ++timeouts;
            if (r.optimality_proven) ++proven;
            if (r.best_cost) {
                min_cost = std::min(min_cost, *r.best_cost);
                max_cost = std::max(max_cost, *r.best_cost);
            }
        }
        out << prefix << key << ','
            << rows.size() << ','
            << std::fixed << std::setprecision(3) << avg(times) << ','
            << avg_ll(expanded) << ','
            << avg_ll(generated) << ','
            << avg_ll(pruned) << ','
            << avg_ll(completed) << ','
            << avg_ll(open_sizes) << ','
            << timeouts << ','
            << proven;
        if (min_cost != kInf) out << ',' << min_cost << ',' << max_cost;
        out << '\n';
    };

    {
        std::ofstream out(summary_csv);
        out << "mode,runs,avg_time_ms,avg_expanded_nodes,avg_generated_nodes,avg_pruned_by_bound,avg_completed_tours,avg_max_open_size,timeouts,optimality_proven_count,best_cost_min,best_cost_max\n";
        for (const auto& [mode, rows] : by_mode) write_group(out, "", mode, rows);
    }

    {
        std::ofstream out(by_n_csv);
        out << "n,mode,runs,avg_time_ms,avg_expanded_nodes,avg_generated_nodes,avg_pruned_by_bound,avg_completed_tours,avg_max_open_size,timeouts,optimality_proven_count\n";
        for (const auto& [key, rows] : by_size_mode) {
            std::vector<double> times;
            std::vector<long long> expanded;
            std::vector<long long> generated;
            std::vector<long long> pruned;
            std::vector<long long> completed;
            std::vector<long long> open_sizes;
            int timeouts = 0;
            int proven = 0;
            for (const auto& r : rows) {
                times.push_back(r.time_ms);
                expanded.push_back(r.expanded_nodes);
                generated.push_back(r.generated_nodes);
                pruned.push_back(r.pruned_by_bound);
                completed.push_back(r.completed_tours);
                open_sizes.push_back(r.max_open_size);
                if (r.timed_out) ++timeouts;
                if (r.optimality_proven) ++proven;
            }
            out << key.first << ',' << key.second << ','
                << rows.size() << ','
                << std::fixed << std::setprecision(3) << avg(times) << ','
                << avg_ll(expanded) << ','
                << avg_ll(generated) << ','
                << avg_ll(pruned) << ','
                << avg_ll(completed) << ','
                << avg_ll(open_sizes) << ','
                << timeouts << ','
                << proven << '\n';
        }
    }
}

void run_single(const std::unordered_map<std::string, std::string>& cfg) {
    const std::string instance_path = get_value(cfg, "instance");
    auto [name, matrix] = load_instance(instance_path);
    const std::string mode = get_value(cfg, "mode", "lc");
    const int start_city = std::stoi(get_value(cfg, "start_city", "0"));
    const double time_limit_s = std::stod(get_value(cfg, "time_limit_s", "60"));
    const bool use_nn_ub = get_bool(cfg, "use_nn_ub", true);
    const bool save_csv = get_bool(cfg, "save_csv", false);
    const std::string csv_path = get_value(cfg, "csv_path", "result_task2.csv");
    Result r = branch_and_bound(matrix, name, instance_path, mode, start_city, time_limit_s, use_nn_ub);
    print_result(r);
    if (save_csv) {
        write_single_csv(r, csv_path);
        std::cout << "CSV saved to            : " << csv_path << '\n';
    }
}

void run_batch(const std::unordered_map<std::string, std::string>& cfg) {
    const std::string instances_dir = get_value(cfg, "instances_dir");
    const std::string pattern = get_value(cfg, "pattern", "*.txt");
    const auto modes = split_list(get_value(cfg, "modes", "bfs,dfs,lc"));
    const int start_city = std::stoi(get_value(cfg, "start_city", "0"));
    const double time_limit_s = std::stod(get_value(cfg, "time_limit_s", "5"));
    const bool use_nn_ub = get_bool(cfg, "use_nn_ub", false);
    const std::string detailed_csv = get_value(cfg, "detailed_csv", "batch_results_detailed.csv");
    const std::string summary_csv = get_value(cfg, "summary_csv", "batch_results_summary.csv");
    const std::string by_n_csv = get_value(cfg, "by_n_csv", "batch_results_by_n.csv");

    std::vector<Result> results;
    for (const auto& path : list_instances(instances_dir, pattern)) {
        auto [name, matrix] = load_instance(path);
        for (const auto& mode : modes) {
            std::cout << "[run] " << name << " n=" << matrix.size() << " mode=" << mode << '\n';
            results.push_back(branch_and_bound(matrix, name, path.string(), mode, start_city, time_limit_s, use_nn_ub));
        }
    }
    write_batch_outputs(results, detailed_csv, summary_csv, by_n_csv);
    std::cout << "Saved: " << detailed_csv << ", " << summary_csv << ", " << by_n_csv << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./tsp_bnb <config_path>\n";
        return 1;
    }
    try {
        const auto cfg = parse_config(argv[1]);
        if (cfg.count("instance")) run_single(cfg);
        else if (cfg.count("instances_dir")) run_batch(cfg);
        else throw std::runtime_error("Config must contain instance or instances_dir");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}

