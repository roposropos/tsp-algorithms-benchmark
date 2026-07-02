#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr int kInfCost = 1000000000;

std::string trim(const std::string &s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

bool parse_bool(const std::string &value) {
    std::string v = upper(trim(value));
    return v == "1" || v == "TRUE" || v == "YES" || v == "TAK";
}

std::vector<std::string> split_csv_line(const std::string &line) {
    std::vector<std::string> parts;
    std::string current;
    bool quoted = false;
    for (char c : line) {
        if (c == '"') {
            quoted = !quoted;
        } else if (c == ',' && !quoted) {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(trim(current));
    return parts;
}

struct Config {
    std::string instances_file = "config/instances.csv";
    std::string output_csv = "results/results.csv";
    std::string tour_output_dir = "results/tours";
    int repetitions = 3;
    uint64_t seed = 1;
    int population_size = 80;
    int generations = 500;
    double time_limit_seconds = 2.0;
    int elite_count = 2;
    int tournament_size = 4;
    double crossover_rate = 0.9;
    double mutation_rate = 0.3;
    double local_search_rate = 0.3;
    int local_search_max_moves = 40;
    int candidate_list_size = 20;
    int nearest_neighbor_seeds = 20;
    int stagnation_generations = 180;
    bool write_tours = true;
};

Config load_config(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nie mozna otworzyc konfiguracji: " + path);
    Config cfg;
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error("Blad konfiguracji w linii " + std::to_string(lineno));
        }
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key == "instances_file") cfg.instances_file = value;
        else if (key == "output_csv") cfg.output_csv = value;
        else if (key == "tour_output_dir") cfg.tour_output_dir = value;
        else if (key == "repetitions") cfg.repetitions = std::stoi(value);
        else if (key == "seed") cfg.seed = static_cast<uint64_t>(std::stoull(value));
        else if (key == "population_size") cfg.population_size = std::stoi(value);
        else if (key == "generations") cfg.generations = std::stoi(value);
        else if (key == "time_limit_seconds") cfg.time_limit_seconds = std::stod(value);
        else if (key == "elite_count") cfg.elite_count = std::stoi(value);
        else if (key == "tournament_size") cfg.tournament_size = std::stoi(value);
        else if (key == "crossover_rate") cfg.crossover_rate = std::stod(value);
        else if (key == "mutation_rate") cfg.mutation_rate = std::stod(value);
        else if (key == "local_search_rate") cfg.local_search_rate = std::stod(value);
        else if (key == "local_search_max_moves") cfg.local_search_max_moves = std::stoi(value);
        else if (key == "candidate_list_size") cfg.candidate_list_size = std::stoi(value);
        else if (key == "nearest_neighbor_seeds") cfg.nearest_neighbor_seeds = std::stoi(value);
        else if (key == "stagnation_generations") cfg.stagnation_generations = std::stoi(value);
        else if (key == "write_tours") cfg.write_tours = parse_bool(value);
        else throw std::runtime_error("Nieznany klucz konfiguracji: " + key);
    }
    if (cfg.population_size < 4) throw std::runtime_error("population_size musi byc >= 4");
    if (cfg.elite_count < 0 || cfg.elite_count >= cfg.population_size) {
        throw std::runtime_error("elite_count musi byc z zakresu [0, population_size)");
    }
    return cfg;
}

struct InstanceEntry {
    std::string label;
    std::string path;
    long long optimum = 0;
    std::string kind;
    std::string source;
};

std::vector<InstanceEntry> load_manifest(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nie mozna otworzyc manifestu: " + path);
    std::vector<InstanceEntry> entries;
    std::string line;
    int lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        size_t comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        auto parts = split_csv_line(line);
        if (parts.size() < 5) {
            throw std::runtime_error("Manifest: za malo kolumn w linii " + std::to_string(lineno));
        }
        entries.push_back({parts[0], parts[1], std::stoll(parts[2]), parts[3], parts[4]});
    }
    return entries;
}

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct TspInstance {
    std::string name;
    std::string type;
    std::string edge_weight_type;
    std::string edge_weight_format;
    int n = 0;
    bool asymmetric = false;
    std::vector<Point> coords;
    std::vector<int> dist;

    int d(int i, int j) const {
        return dist[static_cast<size_t>(i) * n + j];
    }
};

std::pair<std::string, std::string> parse_header_line(const std::string &line) {
    size_t colon = line.find(':');
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

double geo_to_rad(double x) {
    int deg = static_cast<int>(x);
    double minutes = x - deg;
    return kPi * (deg + 5.0 * minutes / 3.0) / 180.0;
}

int geo_distance(const Point &a, const Point &b) {
    const double rrr = 6378.388;
    double lat_i = geo_to_rad(a.x);
    double lon_i = geo_to_rad(a.y);
    double lat_j = geo_to_rad(b.x);
    double lon_j = geo_to_rad(b.y);
    double q1 = std::cos(lon_i - lon_j);
    double q2 = std::cos(lat_i - lat_j);
    double q3 = std::cos(lat_i + lat_j);
    return static_cast<int>(rrr * std::acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0);
}

int coord_distance(const Point &a, const Point &b, const std::string &type) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    if (type == "EUC_2D") {
        return static_cast<int>(std::floor(std::sqrt(dx * dx + dy * dy) + 0.5));
    }
    if (type == "CEIL_2D") {
        return static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy)));
    }
    if (type == "ATT") {
        double rij = std::sqrt((dx * dx + dy * dy) / 10.0);
        int tij = static_cast<int>(std::floor(rij + 0.5));
        return tij < rij ? tij + 1 : tij;
    }
    if (type == "GEO") {
        return geo_distance(a, b);
    }
    throw std::runtime_error("Nieobslugiwany EDGE_WEIGHT_TYPE: " + type);
}

TspInstance load_tsplib(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nie mozna otworzyc instancji: " + path);

    TspInstance tsp;
    std::vector<int> explicit_weights;
    std::string line;
    enum class Section { Header, NodeCoord, EdgeWeights };
    Section section = Section::Header;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        std::string uline = upper(line);
        if (uline == "EOF") break;
        if (uline == "NODE_COORD_SECTION") {
            section = Section::NodeCoord;
            continue;
        }
        if (uline == "EDGE_WEIGHT_SECTION") {
            section = Section::EdgeWeights;
            continue;
        }
        if (section == Section::Header) {
            auto [key, value] = parse_header_line(line);
            if (key == "NAME") tsp.name = value;
            else if (key == "TYPE") tsp.type = upper(value);
            else if (key == "DIMENSION") tsp.n = std::stoi(value);
            else if (key == "EDGE_WEIGHT_TYPE") tsp.edge_weight_type = upper(value);
            else if (key == "EDGE_WEIGHT_FORMAT") tsp.edge_weight_format = upper(value);
        } else if (section == Section::NodeCoord) {
            std::istringstream iss(line);
            int id = 0;
            Point p;
            if (!(iss >> id >> p.x >> p.y)) {
                throw std::runtime_error("Niepoprawny NODE_COORD_SECTION w " + path);
            }
            tsp.coords.push_back(p);
        } else if (section == Section::EdgeWeights) {
            std::istringstream iss(line);
            int w = 0;
            while (iss >> w) explicit_weights.push_back(w);
        }
    }

    if (tsp.n <= 0) throw std::runtime_error("Brak DIMENSION w " + path);
    if (tsp.name.empty()) tsp.name = std::filesystem::path(path).stem().string();
    tsp.asymmetric = tsp.type == "ATSP";
    tsp.dist.assign(static_cast<size_t>(tsp.n) * tsp.n, 0);

    if (tsp.edge_weight_type == "EXPLICIT") {
        if (tsp.edge_weight_format != "FULL_MATRIX") {
            throw std::runtime_error("Obslugiwany jest tylko FULL_MATRIX dla EXPLICIT: " + path);
        }
        if (static_cast<int>(explicit_weights.size()) < tsp.n * tsp.n) {
            throw std::runtime_error("Za malo wag w macierzy FULL_MATRIX: " + path);
        }
        for (int i = 0; i < tsp.n; ++i) {
            for (int j = 0; j < tsp.n; ++j) {
                tsp.dist[static_cast<size_t>(i) * tsp.n + j] = explicit_weights[static_cast<size_t>(i) * tsp.n + j];
            }
        }
    } else {
        if (static_cast<int>(tsp.coords.size()) != tsp.n) {
            throw std::runtime_error("Liczba wspolrzednych nie zgadza sie z DIMENSION: " + path);
        }
        for (int i = 0; i < tsp.n; ++i) {
            for (int j = 0; j < tsp.n; ++j) {
                tsp.dist[static_cast<size_t>(i) * tsp.n + j] = (i == j) ? 0 : coord_distance(tsp.coords[i], tsp.coords[j], tsp.edge_weight_type);
            }
        }
    }

    if (!tsp.asymmetric) {
        for (int i = 0; i < tsp.n && !tsp.asymmetric; ++i) {
            for (int j = i + 1; j < tsp.n; ++j) {
                if (tsp.d(i, j) != tsp.d(j, i)) {
                    tsp.asymmetric = true;
                    break;
                }
            }
        }
    }

    return tsp;
}

struct Individual {
    std::vector<int> tour;
    long long cost = std::numeric_limits<long long>::max();
};

long long evaluate(const TspInstance &tsp, const std::vector<int> &tour) {
    long long sum = 0;
    int n = tsp.n;
    for (int i = 0; i < n; ++i) {
        int a = tour[i];
        int b = tour[(i + 1) % n];
        sum += tsp.d(a, b);
    }
    return sum;
}

std::vector<std::vector<int>> build_candidate_lists(const TspInstance &tsp, int k) {
    int n = tsp.n;
    k = std::max(0, std::min(k, n - 1));
    std::vector<std::vector<int>> lists(n);
    for (int i = 0; i < n; ++i) {
        std::vector<int> nodes;
        nodes.reserve(n - 1);
        for (int j = 0; j < n; ++j) {
            if (i != j) nodes.push_back(j);
        }
        std::partial_sort(nodes.begin(), nodes.begin() + k, nodes.end(), [&](int a, int b) {
            return tsp.d(i, a) < tsp.d(i, b);
        });
        nodes.resize(k);
        lists[i] = std::move(nodes);
    }
    return lists;
}

std::vector<int> nearest_neighbor_tour(const TspInstance &tsp, int start) {
    int n = tsp.n;
    std::vector<int> tour;
    tour.reserve(n);
    std::vector<unsigned char> used(n, 0);
    int current = start % n;
    for (int step = 0; step < n; ++step) {
        tour.push_back(current);
        used[current] = 1;
        int best = -1;
        int best_d = kInfCost;
        for (int j = 0; j < n; ++j) {
            if (!used[j] && tsp.d(current, j) < best_d) {
                best_d = tsp.d(current, j);
                best = j;
            }
        }
        if (best == -1) break;
        current = best;
    }
    return tour;
}

bool two_opt_symmetric(const TspInstance &tsp, std::vector<int> &tour,
                       const std::vector<std::vector<int>> &candidates, int max_moves) {
    int n = tsp.n;
    if (n < 4 || max_moves <= 0) return false;
    bool changed = false;
    std::vector<int> pos(n);
    for (int i = 0; i < n; ++i) pos[tour[i]] = i;

    int moves = 0;
    bool improved = true;
    while (improved && moves < max_moves) {
        improved = false;
        for (int i = 0; i < n && moves < max_moves; ++i) {
            int a = tour[i];
            int b = tour[(i + 1) % n];
            const auto &cand = candidates.empty() ? std::vector<int>() : candidates[a];
            if (cand.empty()) {
                for (int k = i + 2; k < n + (i > 0 ? 0 : -1); ++k) {
                    int kk = k % n;
                    int c = tour[kk];
                    int d = tour[(kk + 1) % n];
                    long long delta = static_cast<long long>(tsp.d(a, c)) + tsp.d(b, d) - tsp.d(a, b) - tsp.d(c, d);
                    if (delta < 0) {
                        int l = (i + 1) % n;
                        int r = kk;
                        if (l <= r) {
                            std::reverse(tour.begin() + l, tour.begin() + r + 1);
                        }
                        for (int p = 0; p < n; ++p) pos[tour[p]] = p;
                        improved = true;
                        changed = true;
                        ++moves;
                        break;
                    }
                }
            } else {
                for (int c : cand) {
                    int k = pos[c];
                    if (k <= i + 1) continue;
                    if (i == 0 && k == n - 1) continue;
                    int d = tour[(k + 1) % n];
                    long long delta = static_cast<long long>(tsp.d(a, c)) + tsp.d(b, d) - tsp.d(a, b) - tsp.d(c, d);
                    if (delta < 0) {
                        int l = i + 1;
                        int r = k;
                        std::reverse(tour.begin() + l, tour.begin() + r + 1);
                        for (int p = l; p <= r; ++p) pos[tour[p]] = p;
                        improved = true;
                        changed = true;
                        ++moves;
                        break;
                    }
                }
            }
            if (improved) break;
        }
    }
    return changed;
}

bool local_search_asymmetric_random(const TspInstance &tsp, std::vector<int> &tour,
                                    std::mt19937_64 &rng, int max_moves) {
    int n = tsp.n;
    if (n < 4 || max_moves <= 0) return false;
    bool changed = false;
    long long current = evaluate(tsp, tour);
    std::uniform_int_distribution<int> pos_dist(0, n - 1);
    int accepted = 0;
    int attempts_limit = std::max(80, 5 * n);

    while (accepted < max_moves) {
        bool improved = false;
        for (int attempt = 0; attempt < attempts_limit; ++attempt) {
            int i = pos_dist(rng);
            int j = pos_dist(rng);
            if (i == j) continue;
            std::vector<int> candidate = tour;
            if ((attempt & 1) == 0) {
                std::swap(candidate[i], candidate[j]);
            } else {
                int value = candidate[i];
                candidate.erase(candidate.begin() + i);
                if (j > i) --j;
                candidate.insert(candidate.begin() + ((j + 1) % n), value);
            }
            long long cand_cost = evaluate(tsp, candidate);
            if (cand_cost < current) {
                tour.swap(candidate);
                current = cand_cost;
                changed = true;
                improved = true;
                ++accepted;
                break;
            }
        }
        if (!improved) break;
    }
    return changed;
}

std::vector<int> order_crossover(const std::vector<int> &p1, const std::vector<int> &p2,
                                 std::mt19937_64 &rng) {
    int n = static_cast<int>(p1.size());
    std::uniform_int_distribution<int> dist(0, n - 1);
    int a = dist(rng);
    int b = dist(rng);
    if (a > b) std::swap(a, b);
    std::vector<int> child(n, -1);
    std::vector<unsigned char> used(n, 0);
    for (int i = a; i <= b; ++i) {
        child[i] = p1[i];
        used[p1[i]] = 1;
    }
    int write = (b + 1) % n;
    for (int step = 0; step < n; ++step) {
        int gene = p2[(b + 1 + step) % n];
        if (!used[gene]) {
            child[write] = gene;
            used[gene] = 1;
            write = (write + 1) % n;
        }
    }
    return child;
}

void mutate(std::vector<int> &tour, std::mt19937_64 &rng) {
    int n = static_cast<int>(tour.size());
    if (n < 2) return;
    std::uniform_int_distribution<int> pos_dist(0, n - 1);
    std::uniform_int_distribution<int> op_dist(0, 2);
    int i = pos_dist(rng);
    int j = pos_dist(rng);
    if (i == j) j = (j + 1) % n;
    if (i > j) std::swap(i, j);

    int op = op_dist(rng);
    if (op == 0) {
        std::swap(tour[i], tour[j]);
    } else if (op == 1) {
        if (i > j) std::swap(i, j);
        std::reverse(tour.begin() + i, tour.begin() + j + 1);
    } else {
        int value = tour[i];
        tour.erase(tour.begin() + i);
        if (j > i) --j;
        tour.insert(tour.begin() + ((j + 1) % n), value);
    }
}

int tournament_select(const std::vector<Individual> &pop, std::mt19937_64 &rng, int tournament_size) {
    std::uniform_int_distribution<int> dist(0, static_cast<int>(pop.size()) - 1);
    int best = dist(rng);
    for (int i = 1; i < tournament_size; ++i) {
        int idx = dist(rng);
        if (pop[idx].cost < pop[best].cost) best = idx;
    }
    return best;
}

struct RunResult {
    long long best_cost = 0;
    std::vector<int> best_tour;
    int generations_done = 0;
    int improvements = 0;
    double elapsed_ms = 0.0;
};

RunResult run_ga(const TspInstance &tsp, const Config &cfg, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> real_dist(0.0, 1.0);
    std::vector<std::vector<int>> candidates;
    if (!tsp.asymmetric) {
        candidates = build_candidate_lists(tsp, cfg.candidate_list_size);
    }

    auto start_time = Clock::now();
    auto time_exceeded = [&]() {
        double elapsed = std::chrono::duration<double>(Clock::now() - start_time).count();
        return elapsed >= cfg.time_limit_seconds;
    };

    std::vector<Individual> pop;
    pop.reserve(cfg.population_size);
    int nn_count = std::min({cfg.nearest_neighbor_seeds, cfg.population_size, tsp.n});
    for (int i = 0; i < nn_count; ++i) {
        int start = (static_cast<long long>(i) * tsp.n) / nn_count;
        Individual ind;
        ind.tour = nearest_neighbor_tour(tsp, start);
        if (!tsp.asymmetric) {
            two_opt_symmetric(tsp, ind.tour, candidates, cfg.local_search_max_moves * 3);
        } else {
            local_search_asymmetric_random(tsp, ind.tour, rng, std::max(8, cfg.local_search_max_moves / 2));
        }
        ind.cost = evaluate(tsp, ind.tour);
        pop.push_back(std::move(ind));
    }

    std::vector<int> base(tsp.n);
    std::iota(base.begin(), base.end(), 0);
    while (static_cast<int>(pop.size()) < cfg.population_size) {
        Individual ind;
        ind.tour = base;
        std::shuffle(ind.tour.begin(), ind.tour.end(), rng);
        if (real_dist(rng) < 0.20 && !tsp.asymmetric) {
            two_opt_symmetric(tsp, ind.tour, candidates, std::max(8, cfg.local_search_max_moves / 2));
        }
        ind.cost = evaluate(tsp, ind.tour);
        pop.push_back(std::move(ind));
    }

    auto by_cost = [](const Individual &a, const Individual &b) {
        return a.cost < b.cost;
    };
    std::sort(pop.begin(), pop.end(), by_cost);
    Individual global_best = pop.front();
    int stagnation = 0;
    int improvements = 0;
    int gen_done = 0;

    for (int gen = 0; gen < cfg.generations; ++gen) {
        if (time_exceeded()) break;
        std::vector<Individual> next;
        next.reserve(cfg.population_size);
        int elites = std::min(cfg.elite_count, static_cast<int>(pop.size()));
        for (int i = 0; i < elites; ++i) next.push_back(pop[i]);

        while (static_cast<int>(next.size()) < cfg.population_size) {
            int i1 = tournament_select(pop, rng, cfg.tournament_size);
            int i2 = tournament_select(pop, rng, cfg.tournament_size);
            Individual child;
            if (real_dist(rng) < cfg.crossover_rate) {
                child.tour = order_crossover(pop[i1].tour, pop[i2].tour, rng);
            } else {
                child.tour = pop[i1].tour;
            }
            if (real_dist(rng) < cfg.mutation_rate) {
                mutate(child.tour, rng);
            }
            if (real_dist(rng) < cfg.local_search_rate) {
                if (!tsp.asymmetric) {
                    two_opt_symmetric(tsp, child.tour, candidates, cfg.local_search_max_moves);
                } else {
                    local_search_asymmetric_random(tsp, child.tour, rng, std::max(4, cfg.local_search_max_moves / 3));
                }
            }
            child.cost = evaluate(tsp, child.tour);
            next.push_back(std::move(child));
        }

        pop.swap(next);
        std::sort(pop.begin(), pop.end(), by_cost);
        if (pop.front().cost < global_best.cost) {
            global_best = pop.front();
            stagnation = 0;
            ++improvements;
        } else {
            ++stagnation;
        }
        gen_done = gen + 1;
        if (cfg.stagnation_generations > 0 && stagnation >= cfg.stagnation_generations) break;
    }

    auto end_time = Clock::now();
    RunResult result;
    result.best_cost = global_best.cost;
    result.best_tour = std::move(global_best.tour);
    result.generations_done = gen_done;
    result.improvements = improvements;
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return result;
}

std::string csv_escape(const std::string &s) {
    if (s.find_first_of(",\"") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

void write_tour(const std::string &dir, const InstanceEntry &entry, int repetition,
                const RunResult &result) {
    std::filesystem::create_directories(dir);
    std::filesystem::path path = std::filesystem::path(dir) / (entry.label + "_r" + std::to_string(repetition) + ".tour");
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Nie mozna zapisac trasy: " + path.string());
    out << "NAME : " << entry.label << "_r" << repetition << "\n";
    out << "COMMENT : best tour length " << result.best_cost << "\n";
    out << "TYPE : TOUR\n";
    out << "DIMENSION : " << result.best_tour.size() << "\n";
    out << "TOUR_SECTION\n";
    for (int city : result.best_tour) out << (city + 1) << "\n";
    out << "-1\nEOF\n";
}

uint64_t mix_seed(uint64_t base, const std::string &label, int repetition) {
    uint64_t h = std::hash<std::string>{}(label);
    return base ^ (h + 0x9e3779b97f4a7c15ULL + (base << 6) + (base >> 2)) ^
           (static_cast<uint64_t>(repetition + 1) * 1000003ULL);
}

}  // namespace

int main(int argc, char **argv) {
    try {
        std::string config_path = argc >= 2 ? argv[1] : "config/experiment.cfg";
        Config cfg = load_config(config_path);
        auto entries = load_manifest(cfg.instances_file);
        std::filesystem::create_directories(std::filesystem::path(cfg.output_csv).parent_path());
        if (cfg.write_tours) std::filesystem::create_directories(cfg.tour_output_dir);

        std::ofstream csv(cfg.output_csv);
        if (!csv) throw std::runtime_error("Nie mozna zapisac CSV: " + cfg.output_csv);
        csv << "instance,kind,source,type,edge_weight_type,dimension,repetition,seed,best_length,opt_length,gap_percent,elapsed_ms,generations,improvements,population_size,crossover_rate,mutation_rate,local_search_rate\n";

        for (const auto &entry : entries) {
            TspInstance tsp = load_tsplib(entry.path);
            std::cerr << "Instancja " << entry.label << " n=" << tsp.n
                      << " type=" << tsp.type << " opt=" << entry.optimum << "\n";
            for (int rep = 0; rep < cfg.repetitions; ++rep) {
                uint64_t run_seed = mix_seed(cfg.seed, entry.label, rep);
                RunResult result = run_ga(tsp, cfg, run_seed);
                double gap = entry.optimum > 0
                                 ? 100.0 * (static_cast<double>(result.best_cost) / entry.optimum - 1.0)
                                 : 0.0;
                csv << csv_escape(entry.label) << ','
                    << csv_escape(entry.kind) << ','
                    << csv_escape(entry.source) << ','
                    << csv_escape(tsp.type) << ','
                    << csv_escape(tsp.edge_weight_type) << ','
                    << tsp.n << ','
                    << rep << ','
                    << run_seed << ','
                    << result.best_cost << ','
                    << entry.optimum << ','
                    << std::fixed << std::setprecision(6) << gap << ','
                    << std::fixed << std::setprecision(3) << result.elapsed_ms << ','
                    << result.generations_done << ','
                    << result.improvements << ','
                    << cfg.population_size << ','
                    << std::fixed << std::setprecision(3) << cfg.crossover_rate << ','
                    << std::fixed << std::setprecision(3) << cfg.mutation_rate << ','
                    << std::fixed << std::setprecision(3) << cfg.local_search_rate << '\n';
                csv.flush();
                if (cfg.write_tours) write_tour(cfg.tour_output_dir, entry, rep, result);
                std::cerr << "  rep " << rep
                          << " best=" << result.best_cost
                          << " gap=" << std::fixed << std::setprecision(2) << gap << "%"
                          << " time=" << std::setprecision(2) << result.elapsed_ms / 1000.0 << "s\n";
            }
        }
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "BLAD: " << ex.what() << "\n";
        return 1;
    }
}
