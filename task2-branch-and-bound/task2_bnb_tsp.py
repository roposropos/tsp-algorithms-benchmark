#!/usr/bin/env python3
"""
Task 2 - TSP Branch and Bound in three search variants:
- bfs  : breadth-first search
- dfs  : depth-first search
- lc   : least-cost / best-first search


Supported input formats:
1) Simple matrix file:
   First non-comment line: integer n
   Next n lines: n numbers each

2) Basic TSPLIB (.tsp):
   - EDGE_WEIGHT_TYPE = EUC_2D
   - EDGE_WEIGHT_TYPE = ATT
   - EDGE_WEIGHT_TYPE = GEO
   - EDGE_WEIGHT_TYPE = EXPLICIT
     with FULL_MATRIX / UPPER_ROW / LOWER_ROW / UPPER_DIAG_ROW / LOWER_DIAG_ROW

Example:
    python task2_bnb_tsp.py config.txt
"""

from __future__ import annotations

import heapq
import math
import sys
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


INF = float("inf")


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

def parse_config(path: str) -> dict[str, str]:
    """
    Parse a simple key=value config file.
    Lines starting with '#' are comments.
    """
    cfg: dict[str, str] = {}
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                raise ValueError(f"Invalid config line: {raw.rstrip()}")
            key, value = line.split("=", 1)
            cfg[key.strip().lower()] = value.strip()
    return cfg


def get_required(cfg: dict[str, str], key: str) -> str:
    if key not in cfg:
        raise ValueError(f"Missing required config key: {key}")
    return cfg[key]


def get_int(cfg: dict[str, str], key: str, default: int) -> int:
    return int(cfg.get(key, str(default)))


def get_bool(cfg: dict[str, str], key: str, default: bool) -> bool:
    value = cfg.get(key, str(default)).strip().lower()
    return value in {"1", "true", "yes", "y", "on"}


# ---------------------------------------------------------------------------
# Input loading
# ---------------------------------------------------------------------------

def load_instance(path: str) -> tuple[str, list[list[int]]]:
    p = Path(path)
    text = p.read_text(encoding="utf-8", errors="replace")

    if "TYPE" in text and "TSP" in text:
        name, matrix = parse_tsplib(text, fallback_name=p.stem)
        return name, matrix

    matrix = parse_simple_matrix(text)
    return p.stem, matrix


def parse_simple_matrix(text: str) -> list[list[int]]:
    lines = [
        line.strip()
        for line in text.splitlines()
        if line.strip() and not line.strip().startswith("#")
    ]
    if not lines:
        raise ValueError("Empty matrix file")

    n = int(lines[0])
    rows = []
    for line in lines[1:]:
        rows.append([int(float(x)) for x in line.split()])

    if len(rows) != n:
        raise ValueError(f"Matrix file declares n={n}, but has {len(rows)} rows")

    for i, row in enumerate(rows):
        if len(row) != n:
            raise ValueError(f"Row {i} has length {len(row)}, expected {n}")

    return rows


def parse_tsplib(text: str, fallback_name: str) -> tuple[str, list[list[int]]]:
    meta: dict[str, str] = {}
    lines = [line.rstrip() for line in text.splitlines()]
    idx = 0

    def norm_key(k: str) -> str:
        return k.strip().upper()

    while idx < len(lines):
        line = lines[idx].strip()
        if not line:
            idx += 1
            continue
        if line.upper() in {"NODE_COORD_SECTION", "EDGE_WEIGHT_SECTION", "DISPLAY_DATA_SECTION"}:
            break
        if ":" in line:
            k, v = line.split(":", 1)
            meta[norm_key(k)] = v.strip()
        else:
            parts = line.split(maxsplit=1)
            if len(parts) == 2:
                meta[norm_key(parts[0])] = parts[1].strip()
        idx += 1

    name = meta.get("NAME", fallback_name)
    dimension = int(meta["DIMENSION"])
    ew_type = meta.get("EDGE_WEIGHT_TYPE", "EXPLICIT").upper()
    ew_format = meta.get("EDGE_WEIGHT_FORMAT", "").upper()

    current = lines[idx].strip().upper() if idx < len(lines) else ""

    if current == "NODE_COORD_SECTION":
        coords = []
        idx += 1
        while idx < len(lines):
            line = lines[idx].strip()
            if not line or line.upper() == "EOF":
                break
            parts = line.split()
            if len(parts) < 3:
                raise ValueError(f"Invalid NODE_COORD_SECTION line: {line}")
            _, x, y = parts[:3]
            coords.append((float(x), float(y)))
            idx += 1

        if len(coords) != dimension:
            raise ValueError(f"Expected {dimension} coordinates, got {len(coords)}")

        matrix = build_matrix_from_coords(coords, ew_type)
        return name, matrix

    if current == "EDGE_WEIGHT_SECTION":
        idx += 1
        values: list[int] = []
        while idx < len(lines):
            line = lines[idx].strip()
            if not line or line.upper() == "EOF":
                break
            values.extend(int(float(x)) for x in line.split())
            idx += 1

        matrix = build_explicit_matrix(dimension, values, ew_format)
        return name, matrix

    raise ValueError("Unsupported TSPLIB structure in file")


def build_matrix_from_coords(coords: list[tuple[float, float]], ew_type: str) -> list[list[int]]:
    n = len(coords)
    matrix = [[0] * n for _ in range(n)]

    for i in range(n):
        x1, y1 = coords[i]
        for j in range(n):
            if i == j:
                matrix[i][j] = 0
                continue
            x2, y2 = coords[j]

            if ew_type == "EUC_2D":
                dij = math.sqrt((x1 - x2) ** 2 + (y1 - y2) ** 2)
                matrix[i][j] = int(round(dij))
            elif ew_type == "ATT":
                rij = math.sqrt(((x1 - x2) ** 2 + (y1 - y2) ** 2) / 10.0)
                tij = int(round(rij))
                matrix[i][j] = tij if tij >= rij else tij + 1
            elif ew_type == "GEO":
                matrix[i][j] = geo_distance((x1, y1), (x2, y2))
            else:
                raise ValueError(f"Unsupported EDGE_WEIGHT_TYPE for coords: {ew_type}")

    return matrix


def geo_to_radians(value: float) -> float:
    deg = int(value)
    minutes = value - deg
    return math.pi * (deg + 5.0 * minutes / 3.0) / 180.0


def geo_distance(a: tuple[float, float], b: tuple[float, float]) -> int:
    lat_i = geo_to_radians(a[0])
    lon_i = geo_to_radians(a[1])
    lat_j = geo_to_radians(b[0])
    lon_j = geo_to_radians(b[1])

    RRR = 6378.388
    q1 = math.cos(lon_i - lon_j)
    q2 = math.cos(lat_i - lat_j)
    q3 = math.cos(lat_i + lat_j)
    return int(RRR * math.acos(0.5 * ((1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0)


def build_explicit_matrix(n: int, values: list[int], fmt: str) -> list[list[int]]:
    matrix = [[0] * n for _ in range(n)]

    if fmt == "FULL_MATRIX":
        if len(values) != n * n:
            raise ValueError(f"FULL_MATRIX expects {n*n} values, got {len(values)}")
        k = 0
        for i in range(n):
            for j in range(n):
                matrix[i][j] = values[k]
                k += 1
        return matrix

    if fmt == "UPPER_ROW":
        expected = n * (n - 1) // 2
        if len(values) != expected:
            raise ValueError(f"UPPER_ROW expects {expected} values, got {len(values)}")
        k = 0
        for i in range(n):
            for j in range(i + 1, n):
                matrix[i][j] = values[k]
                matrix[j][i] = values[k]
                k += 1
        return matrix

    if fmt == "LOWER_ROW":
        expected = n * (n - 1) // 2
        if len(values) != expected:
            raise ValueError(f"LOWER_ROW expects {expected} values, got {len(values)}")
        k = 0
        for i in range(1, n):
            for j in range(i):
                matrix[i][j] = values[k]
                matrix[j][i] = values[k]
                k += 1
        return matrix

    if fmt == "UPPER_DIAG_ROW":
        expected = n * (n + 1) // 2
        if len(values) != expected:
            raise ValueError(f"UPPER_DIAG_ROW expects {expected} values, got {len(values)}")
        k = 0
        for i in range(n):
            for j in range(i, n):
                matrix[i][j] = values[k]
                matrix[j][i] = values[k]
                k += 1
        return matrix

    if fmt == "LOWER_DIAG_ROW":
        expected = n * (n + 1) // 2
        if len(values) != expected:
            raise ValueError(f"LOWER_DIAG_ROW expects {expected} values, got {len(values)}")
        k = 0
        for i in range(n):
            for j in range(i + 1):
                matrix[i][j] = values[k]
                matrix[j][i] = values[k]
                k += 1
        return matrix

    raise ValueError(f"Unsupported EDGE_WEIGHT_FORMAT: {fmt}")


# ---------------------------------------------------------------------------
# Core structures
# ---------------------------------------------------------------------------

@dataclass(order=True)
class PrioritizedNode:
    priority: float
    order: int
    node: "Node" = field(compare=False)


@dataclass
class Node:
    path: tuple[int, ...]
    visited_mask: int
    last: int
    cost_so_far: int
    level: int
    lower_bound: float = INF


@dataclass
class Result:
    instance_name: str
    n: int
    mode: str
    best_cost: int | None
    best_path: list[int] | None
    time_ms: float
    expanded_nodes: int
    generated_nodes: int
    pruned_by_bound: int
    pruned_infeasible: int
    completed_tours: int
    max_open_size: int
    timed_out: bool
    optimality_proven: bool
    initial_upper_bound: int | None
    initial_upper_bound_method: str
    lower_bound_method: str


# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

def validate_matrix(matrix: list[list[int]]) -> None:
    n = len(matrix)
    if n == 0:
        raise ValueError("Empty matrix")
    for i, row in enumerate(matrix):
        if len(row) != n:
            raise ValueError(f"Row {i} length is {len(row)}, expected {n}")
    for i in range(n):
        if matrix[i][i] != 0:
            # diagonal should be 0 in standard TSP
            pass


def path_cost(matrix: list[list[int]], path: Iterable[int]) -> int:
    path = list(path)
    total = 0
    for i in range(len(path) - 1):
        total += matrix[path[i]][path[i + 1]]
    return total


def full_tour_cost(matrix: list[list[int]], path: list[int]) -> int:
    if len(path) < 2:
        return 0
    return path_cost(matrix, path) + matrix[path[-1]][path[0]]


def nearest_neighbor_ub(matrix: list[list[int]], start: int) -> tuple[list[int], int]:
    n = len(matrix)
    unvisited = set(range(n))
    unvisited.remove(start)
    path = [start]
    current = start

    while unvisited:
        nxt = min(unvisited, key=lambda v: matrix[current][v])
        path.append(nxt)
        unvisited.remove(nxt)
        current = nxt

    return path, full_tour_cost(matrix, path)


def best_of_all_starts_nn(matrix: list[list[int]]) -> tuple[list[int], int]:
    best_path = None
    best_cost = INF
    for s in range(len(matrix)):
        p, c = nearest_neighbor_ub(matrix, s)
        if c < best_cost:
            best_path, best_cost = p, c
    return list(best_path), int(best_cost)


def compute_min_outgoing(matrix: list[list[int]]) -> list[int]:
    n = len(matrix)
    mins = []
    for i in range(n):
        best = INF
        for j in range(n):
            if i != j and matrix[i][j] < best:
                best = matrix[i][j]
        if best == INF:
            raise ValueError(f"Vertex {i} has no outgoing edge")
        mins.append(int(best))
    return mins


def lower_bound_simple(
    matrix: list[list[int]],
    node: Node,
    min_outgoing: list[int],
    start: int,
) -> float:
    """
    A simple admissible lower bound:
    cost_so_far
    + minimal outgoing from current last city
    + for each unvisited city: minimal outgoing from that city
    + minimal cost to return to start from any remaining possibility

    This bound is intentionally simple and safe for a complete graph TSP.
    It is not the strongest possible bound, but it is easy to explain in a report.
    """
    n = len(matrix)
    if node.level == n:
        return node.cost_so_far + matrix[node.last][start]

    unvisited = [v for v in range(n) if not (node.visited_mask & (1 << v))]

    bound = float(node.cost_so_far)

    # we still need to leave the current city somehow
    bound += min(
        matrix[node.last][v] for v in range(n)
        if v != node.last and (v in unvisited or v == start)
    )

    # each unvisited city must have at least one outgoing edge in the final tour
    for v in unvisited:
        bound += min_outgoing[v]

    return bound


# ---------------------------------------------------------------------------
# Branch and Bound
# ---------------------------------------------------------------------------

class OpenSet:
    def __init__(self, mode: str):
        self.mode = mode
        self.counter = 0
        if mode == "bfs":
            self.data = deque()
        elif mode == "dfs":
            self.data = []
        elif mode == "lc":
            self.data = []
        else:
            raise ValueError(f"Unsupported mode: {mode}")

    def push(self, node: Node) -> None:
        if self.mode == "bfs":
            self.data.append(node)
        elif self.mode == "dfs":
            self.data.append(node)
        else:
            heapq.heappush(self.data, PrioritizedNode(node.lower_bound, self.counter, node))
            self.counter += 1

    def pop(self) -> Node:
        if self.mode == "bfs":
            return self.data.popleft()
        if self.mode == "dfs":
            return self.data.pop()
        return heapq.heappop(self.data).node

    def __len__(self) -> int:
        return len(self.data)


def branch_and_bound(
    matrix: list[list[int]],
    instance_name: str,
    mode: str,
    start: int,
    time_limit_s: float,
    use_nn_ub: bool,
) -> Result:
    validate_matrix(matrix)
    n = len(matrix)
    min_outgoing = compute_min_outgoing(matrix)

    if use_nn_ub:
        best_path, best_cost = best_of_all_starts_nn(matrix)
        initial_ub = best_cost
        initial_ub_method = "nearest_neighbor_best_start"
    else:
        best_path, best_cost = None, INF
        initial_ub = None
        initial_ub_method = "infinity"

    root = Node(
        path=(start,),
        visited_mask=(1 << start),
        last=start,
        cost_so_far=0,
        level=1,
    )
    root.lower_bound = lower_bound_simple(matrix, root, min_outgoing, start)

    open_set = OpenSet(mode)
    open_set.push(root)

    expanded_nodes = 0
    generated_nodes = 1
    pruned_by_bound = 0
    pruned_infeasible = 0
    completed_tours = 0
    max_open_size = 1
    timed_out = False

    time_start = time.perf_counter()

    while len(open_set) > 0:
        if time.perf_counter() - time_start >= time_limit_s:
            timed_out = True
            break

        max_open_size = max(max_open_size, len(open_set))
        node = open_set.pop()

        if node.lower_bound >= best_cost:
            pruned_by_bound += 1
            continue

        expanded_nodes += 1

        if node.level == n:
            completed_tours += 1
            total = node.cost_so_far + matrix[node.last][start]
            if total < best_cost:
                best_cost = total
                best_path = list(node.path)
            continue

        for nxt in range(n):
            if node.visited_mask & (1 << nxt):
                continue
            if nxt == node.last:
                continue

            edge_cost = matrix[node.last][nxt]
            child = Node(
                path=node.path + (nxt,),
                visited_mask=node.visited_mask | (1 << nxt),
                last=nxt,
                cost_so_far=node.cost_so_far + edge_cost,
                level=node.level + 1,
            )
            child.lower_bound = lower_bound_simple(matrix, child, min_outgoing, start)
            generated_nodes += 1

            if child.lower_bound >= best_cost:
                pruned_by_bound += 1
                continue

            open_set.push(child)

    elapsed_ms = (time.perf_counter() - time_start) * 1000.0

    optimality_proven = (not timed_out) and (best_path is not None)

    return Result(
        instance_name=instance_name,
        n=n,
        mode=mode,
        best_cost=(None if best_cost == INF else int(best_cost)),
        best_path=best_path,
        time_ms=elapsed_ms,
        expanded_nodes=expanded_nodes,
        generated_nodes=generated_nodes,
        pruned_by_bound=pruned_by_bound,
        pruned_infeasible=pruned_infeasible,
        completed_tours=completed_tours,
        max_open_size=max_open_size,
        timed_out=timed_out,
        optimality_proven=optimality_proven,
        initial_upper_bound=initial_ub,
        initial_upper_bound_method=initial_ub_method,
        lower_bound_method="simple_admissible_min_outgoing",
    )


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

def format_path(path: list[int] | None) -> str:
    if not path:
        return "-"
    return " -> ".join(map(str, path)) + f" -> {path[0]}"


def print_result(result: Result) -> None:
    print("=" * 72)
    print("TASK 2 - TSP Branch and Bound")
    print("=" * 72)
    print(f"Instance name           : {result.instance_name}")
    print(f"Number of cities        : {result.n}")
    print(f"Mode                    : {result.mode}")
    print(f"Initial UB method       : {result.initial_upper_bound_method}")
    print(f"Initial UB              : {result.initial_upper_bound}")
    print(f"Lower bound method      : {result.lower_bound_method}")
    print(f"Timed out               : {result.timed_out}")
    print(f"Optimality proven       : {result.optimality_proven}")
    print(f"Best cost               : {result.best_cost}")
    print(f"Best path               : {format_path(result.best_path)}")
    print("-" * 72)
    print(f"Time [ms]               : {result.time_ms:.3f}")
    print(f"Expanded nodes          : {result.expanded_nodes}")
    print(f"Generated nodes         : {result.generated_nodes}")
    print(f"Pruned by bound         : {result.pruned_by_bound}")
    print(f"Pruned infeasible       : {result.pruned_infeasible}")
    print(f"Completed tours         : {result.completed_tours}")
    print(f"Max OPEN size           : {result.max_open_size}")
    print("=" * 72)


def save_result_csv(result: Result, path: str) -> None:
    header = (
        "instance_name,n,mode,best_cost,time_ms,expanded_nodes,generated_nodes,"
        "pruned_by_bound,pruned_infeasible,completed_tours,max_open_size,"
        "timed_out,optimality_proven,initial_upper_bound,initial_upper_bound_method,"
        "lower_bound_method,best_path\n"
    )
    row = (
        f"{result.instance_name},{result.n},{result.mode},{result.best_cost},"
        f"{result.time_ms:.3f},{result.expanded_nodes},{result.generated_nodes},"
        f"{result.pruned_by_bound},{result.pruned_infeasible},{result.completed_tours},"
        f"{result.max_open_size},{result.timed_out},{result.optimality_proven},"
        f"{result.initial_upper_bound},{result.initial_upper_bound_method},"
        f"{result.lower_bound_method},\"{format_path(result.best_path)}\"\n"
    )
    with open(path, "w", encoding="utf-8") as f:
        f.write(header)
        f.write(row)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python task2_bnb_tsp.py config.txt")
        return 1

    cfg_path = sys.argv[1]
    cfg = parse_config(cfg_path)

    instance_path = get_required(cfg, "instance")
    mode = cfg.get("mode", "lc").strip().lower()
    start_city = get_int(cfg, "start_city", 0)
    time_limit_s = float(cfg.get("time_limit_s", "60"))
    use_nn_ub = get_bool(cfg, "use_nn_ub", True)
    save_csv = get_bool(cfg, "save_csv", False)
    csv_path = cfg.get("csv_path", "result_task2.csv")

    instance_name, matrix = load_instance(instance_path)

    if not (0 <= start_city < len(matrix)):
        raise ValueError(f"start_city must be in [0, {len(matrix)-1}]")

    result = branch_and_bound(
        matrix=matrix,
        instance_name=instance_name,
        mode=mode,
        start=start_city,
        time_limit_s=time_limit_s,
        use_nn_ub=use_nn_ub,
    )

    print_result(result)

    if save_csv:
        save_result_csv(result, csv_path)
        print(f"CSV saved to            : {csv_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
