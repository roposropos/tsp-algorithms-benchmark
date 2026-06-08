from __future__ import annotations

import argparse
import csv
import glob
import itertools
import math
import random
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Sequence

Route = List[int]


# =========================
# Model instancji i utils
# =========================

@dataclass(slots=True)
class TSPInstance:
    name: str
    matrix: list[list[int]]
    best_known: int | None = None
    source: str = "unknown"
    family: str = "generic"
    symmetric: bool | None = None

    @property
    def n(self) -> int:
        return len(self.matrix)

    def infer_symmetry(self) -> bool:
        n = self.n
        for i in range(n):
            for j in range(n):
                if self.matrix[i][j] != self.matrix[j][i]:
                    return False
        return True


def route_cost(instance: TSPInstance, route: Sequence[int]) -> int:
    n = instance.n
    if len(route) != n:
        raise ValueError(f"Route must contain exactly {n} vertices")
    if len(set(route)) != n:
        raise ValueError("Route must be a permutation of vertices")

    total = 0
    for i in range(n - 1):
        total += instance.matrix[route[i]][route[i + 1]]
    total += instance.matrix[route[-1]][route[0]]
    return total


def relative_error_percent(cost: int | None, best_known: int | None) -> float | None:
    if cost is None or best_known in (None, 0):
        return None
    return ((cost - best_known) / best_known) * 100.0


# =========================
# Parser TSPLIB
# =========================

def _euclidean_distance(a: tuple[float, float], b: tuple[float, float]) -> int:
    return int(round(math.dist(a, b)))


def _ceil_2d_distance(a: tuple[float, float], b: tuple[float, float]) -> int:
    return int(math.ceil(math.dist(a, b)))


def _att_distance(a: tuple[float, float], b: tuple[float, float]) -> int:
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    rij = math.sqrt((dx * dx + dy * dy) / 10.0)
    tij = int(round(rij))
    return tij + 1 if tij < rij else tij


def _parse_header_value(line: str) -> tuple[str, str] | None:
    if ':' in line:
        key, value = line.split(':', 1)
    else:
        parts = line.split(maxsplit=1)
        if len(parts) != 2:
            return None
        key, value = parts
    return key.strip().upper(), value.strip()


def _parse_numbers(lines: Iterable[str]) -> list[float]:
    values: list[float] = []
    for line in lines:
        if line.strip().upper() == 'EOF':
            break
        values.extend(float(x) for x in line.split())
    return values


def load_tsplib(path: str | Path, best_known: int | None = None) -> TSPInstance:
    path = Path(path)
    raw_lines = [line.rstrip() for line in path.read_text(encoding='utf-8', errors='ignore').splitlines()]

    header: dict[str, str] = {}
    section_idx: dict[str, int] = {}
    for idx, line in enumerate(raw_lines):
        stripped = line.strip()
        upper = stripped.upper()
        if not stripped:
            continue
        if upper.endswith('_SECTION'):
            section_idx[upper] = idx
            continue
        parsed = _parse_header_value(stripped)
        if parsed is not None:
            header[parsed[0]] = parsed[1]

    name = header.get('NAME', path.stem)
    dimension = int(header['DIMENSION'])
    edge_weight_type = header.get('EDGE_WEIGHT_TYPE', 'EUC_2D').upper()
    edge_weight_format = header.get('EDGE_WEIGHT_FORMAT', '').upper()

    if 'NODE_COORD_SECTION' in section_idx:
        start = section_idx['NODE_COORD_SECTION'] + 1
        coords: list[tuple[float, float]] = []
        for line in raw_lines[start:]:
            stripped = line.strip()
            if not stripped or stripped.upper() == 'EOF':
                break
            parts = stripped.split()
            if len(parts) < 3:
                continue
            _, x, y = parts[:3]
            coords.append((float(x), float(y)))
        if len(coords) != dimension:
            raise ValueError(f"Expected {dimension} coordinates, got {len(coords)} in {path}")

        matrix = [[0] * dimension for _ in range(dimension)]
        for i in range(dimension):
            for j in range(dimension):
                if i == j:
                    continue
                if edge_weight_type == 'EUC_2D':
                    matrix[i][j] = _euclidean_distance(coords[i], coords[j])
                elif edge_weight_type == 'CEIL_2D':
                    matrix[i][j] = _ceil_2d_distance(coords[i], coords[j])
                elif edge_weight_type == 'ATT':
                    matrix[i][j] = _att_distance(coords[i], coords[j])
                else:
                    raise NotImplementedError(f"Unsupported coordinate EDGE_WEIGHT_TYPE={edge_weight_type} in {path}")
    elif 'EDGE_WEIGHT_SECTION' in section_idx:
        start = section_idx['EDGE_WEIGHT_SECTION'] + 1
        numbers = _parse_numbers(raw_lines[start:])
        if edge_weight_format != 'FULL_MATRIX':
            raise NotImplementedError("Supported explicit format: FULL_MATRIX")
        if len(numbers) != dimension * dimension:
            raise ValueError(f"Expected {dimension * dimension} weights, got {len(numbers)} in {path}")
        matrix = []
        idx = 0
        for _ in range(dimension):
            row = [int(numbers[idx + j]) for j in range(dimension)]
            matrix.append(row)
            idx += dimension
    else:
        raise NotImplementedError("Missing supported TSPLIB section")

    inst = TSPInstance(name=name, matrix=matrix, best_known=best_known, source='tsplib', family=edge_weight_type)
    inst.symmetric = inst.infer_symmetry()
    return inst


# =========================
# Generator grafów pełnych
# =========================

def generate_complete_graph(
    n: int,
    symmetric: bool,
    low: int = 1,
    high: int = 100,
    seed: int | None = None,
    name: str | None = None,
) -> TSPInstance:
    if n < 2:
        raise ValueError("n must be >= 2")
    if low > high:
        raise ValueError("low must be <= high")

    rng = random.Random(seed)
    matrix = [[0] * n for _ in range(n)]

    if symmetric:
        for i in range(n):
            for j in range(i + 1, n):
                w = rng.randint(low, high)
                matrix[i][j] = w
                matrix[j][i] = w
    else:
        for i in range(n):
            for j in range(n):
                if i != j:
                    matrix[i][j] = rng.randint(low, high)

    return TSPInstance(
        name=name or f"gen_{'sym' if symmetric else 'asym'}_{n}_{seed}",
        matrix=matrix,
        best_known=None,
        source='generated',
        family='generated_complete',
        symmetric=symmetric,
    )


# =========================
# Algorytmy
# =========================

@dataclass(slots=True)
class TSPResult:
    algorithm: str
    route: Route | None
    cost: int | None
    time_s: float
    completed: bool
    timed_out: bool = False
    extra: dict | None = None


def rand_tsp(instance: TSPInstance, trials: int = 1000, seed: int | None = None) -> TSPResult:
    rng = random.Random(seed)
    nodes = list(range(instance.n))
    best_route: Route | None = None
    best_cost: int | None = None
    start_time = time.perf_counter()
    for _ in range(trials):
        route = nodes[:]
        rng.shuffle(route)
        cost = route_cost(instance, route)
        if best_cost is None or cost < best_cost:
            best_cost = cost
            best_route = route[:]
    return TSPResult('rand', best_route, best_cost, time.perf_counter() - start_time, True, extra={'trials': trials})


def nearest_neighbor(instance: TSPInstance, start: int = 0) -> TSPResult:
    unvisited = set(range(instance.n))
    unvisited.remove(start)
    route = [start]
    current = start
    start_time = time.perf_counter()
    while unvisited:
        nxt = min(unvisited, key=lambda v: instance.matrix[current][v])
        route.append(nxt)
        unvisited.remove(nxt)
        current = nxt
    return TSPResult('nn', route, route_cost(instance, route), time.perf_counter() - start_time, True, extra={'start': start})


def _rnn_branch(
    instance: TSPInstance,
    route: Route,
    unvisited: set[int],
    current: int,
    start_clock: float,
    timeout_seconds: float | None,
) -> tuple[Route | None, int | None, bool]:
    if timeout_seconds is not None and (time.perf_counter() - start_clock) > timeout_seconds:
        return None, None, True
    if not unvisited:
        return route[:], route_cost(instance, route), False

    min_dist = min(instance.matrix[current][v] for v in unvisited)
    candidates = [v for v in unvisited if instance.matrix[current][v] == min_dist]
    best_route: Route | None = None
    best_cost: int | None = None

    for nxt in candidates:
        new_unvisited = set(unvisited)
        new_unvisited.remove(nxt)
        route.append(nxt)
        cand_route, cand_cost, timed_out = _rnn_branch(instance, route, new_unvisited, nxt, start_clock, timeout_seconds)
        route.pop()
        if timed_out:
            return None, None, True
        if cand_cost is not None and (best_cost is None or cand_cost < best_cost):
            best_cost = cand_cost
            best_route = cand_route
    return best_route, best_cost, False


def repetitive_nearest_neighbor(instance: TSPInstance, timeout_seconds: float | None = None) -> TSPResult:
    best_route: Route | None = None
    best_cost: int | None = None
    start_time = time.perf_counter()
    for start in range(instance.n):
        route, cost, timed_out = _rnn_branch(instance, [start], set(range(instance.n)) - {start}, start, start_time, timeout_seconds)
        if timed_out:
            return TSPResult('rnn', None, None, time.perf_counter() - start_time, False, timed_out=True, extra={'reason': 'timeout'})
        if cost is not None and (best_cost is None or cost < best_cost):
            best_cost = cost
            best_route = route
    return TSPResult('rnn', best_route, best_cost, time.perf_counter() - start_time, True)


def brute_force_tsp(instance: TSPInstance, timeout_seconds: float | None = None) -> TSPResult:
    if instance.n == 0:
        return TSPResult('bruteforce', [], 0, 0.0, True)
    fixed_start = 0
    nodes = list(range(1, instance.n))
    best_route: Route | None = None
    best_cost: int | None = None
    checked = 0
    start_time = time.perf_counter()
    for perm in itertools.permutations(nodes):
        if timeout_seconds is not None and (time.perf_counter() - start_time) > timeout_seconds:
            return TSPResult('bruteforce', best_route, best_cost, time.perf_counter() - start_time, False, True, extra={'checked_permutations': checked})
        route = [fixed_start, *perm]
        cost = route_cost(instance, route)
        checked += 1
        if best_cost is None or cost < best_cost:
            best_cost = cost
            best_route = route
    return TSPResult('bruteforce', best_route, best_cost, time.perf_counter() - start_time, True, extra={'checked_permutations': checked})


def run_algorithm(
    instance: TSPInstance,
    algorithm: str,
    rand_trials: int = 1000,
    seed: int | None = None,
    timeout_seconds: float | None = None,
) -> TSPResult:
    algorithm = algorithm.lower()
    if algorithm == 'rand':
        return rand_tsp(instance, trials=rand_trials, seed=seed)
    if algorithm == 'nn':
        return nearest_neighbor(instance, start=0)
    if algorithm == 'rnn':
        return repetitive_nearest_neighbor(instance, timeout_seconds=timeout_seconds)
    if algorithm in {'bruteforce', 'bf', 'brute_force'}:
        return brute_force_tsp(instance, timeout_seconds=timeout_seconds)
    raise ValueError(f"Unsupported algorithm: {algorithm}")


# =========================
# Benchmark i konfiguracja
# =========================

def parse_value(v: str):
    v = v.strip()
    if ',' in v and not v.startswith('"'):
        return [x.strip() for x in v.split(',') if x.strip()]
    return v


def load_config(path: str | Path) -> dict:
    cfg: dict = {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if '=' not in line:
            raise ValueError(f'Niepoprawna linia w konfiguracji: {line}')
        k, v = line.split('=', 1)
        cfg[k.strip()] = parse_value(v)
    return cfg


def load_best_known_csv(path: str | Path | None) -> dict[str, int]:
    if path is None:
        return {}
    result: dict[str, int] = {}
    with open(path, 'r', encoding='utf-8', newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            result[row['name'].strip()] = int(float(row['best_known']))
    return result


def result_row(instance: TSPInstance, algorithm: str, result: TSPResult, seed: int | None) -> dict:
    return {
        'instance_name': instance.name,
        'source': instance.source,
        'family': instance.family,
        'n': instance.n,
        'symmetric': instance.symmetric,
        'algorithm': algorithm,
        'seed': seed,
        'cost': result.cost,
        'best_known': instance.best_known,
        'rel_error_pct': relative_error_percent(result.cost, instance.best_known),
        'time_s': result.time_s,
        'completed': result.completed,
        'timed_out': result.timed_out,
        'route': ' '.join(map(str, result.route)) if result.route is not None else None,
        'extra': repr(result.extra) if result.extra is not None else None,
    }


def write_rows(path: str | Path, rows: list[dict]) -> None:
    if not rows:
        raise ValueError('No rows to save.')
    with open(path, 'w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def run_on_instances(instances: Iterable[TSPInstance], algorithms: list[str], rand_trials: int, timeout_seconds: float | None, base_seed: int) -> list[dict]:
    rows: list[dict] = []
    for idx, instance in enumerate(instances):
        print(f"\n=== {instance.name} (n={instance.n}, symmetric={instance.symmetric}) ===")
        for a_idx, algorithm in enumerate(algorithms):
            seed = base_seed + idx * 100 + a_idx
            print(f"  -> {algorithm} ...", end='', flush=True)
            result = run_algorithm(instance, algorithm, rand_trials=rand_trials, seed=seed, timeout_seconds=timeout_seconds)
            print(f" cost={result.cost} time={result.time_s:.6f}s completed={result.completed} timeout={result.timed_out}")
            rows.append(result_row(instance, algorithm, result, seed))
    return rows


def run_from_config(config_path: str | Path) -> None:
    cfg = load_config(config_path)
    mode = cfg.get('mode')
    seed = int(cfg.get('seed', 12345))

    if mode == 'tsplib':
        best_known_map = load_best_known_csv(cfg.get('best_known_csv'))
        paths = sorted(glob.glob(cfg['input_glob']))
        if not paths:
            raise SystemExit(f"No files matched: {cfg['input_glob']}")
        instances = []
        for path in paths:
            stem = Path(path).stem
            instances.append(load_tsplib(path, best_known=best_known_map.get(stem)))
        rows = run_on_instances(
            instances=instances,
            algorithms=list(cfg['algorithms']),
            rand_trials=int(cfg.get('rand_trials', 1000)),
            timeout_seconds=float(cfg.get('timeout_seconds')) if cfg.get('timeout_seconds') is not None else None,
            base_seed=seed,
        )
        write_rows(cfg['output'], rows)
        print(f"\nSaved: {cfg['output']}")
        return

    if mode == 'generated':
        instances: list[TSPInstance] = []
        counter = 0
        for n in [int(x) for x in cfg['sizes']]:
            for symmetric in (True, False):
                for k in range(int(cfg['instances_per_size'])):
                    inst_seed = seed + counter
                    instances.append(generate_complete_graph(
                        n=n,
                        symmetric=symmetric,
                        low=int(cfg.get('weight_low', 1)),
                        high=int(cfg.get('weight_high', 100)),
                        seed=inst_seed,
                        name=f"gen_{'sym' if symmetric else 'asym'}_n{n}_{k:02d}",
                    ))
                    counter += 1
        rows = run_on_instances(
            instances=instances,
            algorithms=list(cfg['algorithms']),
            rand_trials=int(cfg.get('rand_trials', 1000)),
            timeout_seconds=float(cfg.get('timeout_seconds')) if cfg.get('timeout_seconds') is not None else None,
            base_seed=seed,
        )
        write_rows(cfg['output'], rows)
        print(f"\nSaved: {cfg['output']}")
        return

    raise ValueError('mode musi mieć wartość generated albo tsplib')


def main() -> None:
    parser = argparse.ArgumentParser(description='Zadanie 1 - TSP (RAND, NN, RNN, brute-force) sterowane plikiem konfiguracyjnym.')
    parser.add_argument('config', help='Ścieżka do pliku tekstowego z konfiguracją')
    args = parser.parse_args()
    run_from_config(args.config)


if __name__ == '__main__':
    main()
