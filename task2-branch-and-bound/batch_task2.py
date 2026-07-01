#!/usr/bin/env python3
"""
Masowe testy dla zadania 2 - TSP Branch and Bound (BFS / DFS / LC).

Skrypt:
- wczytuje listę instancji z pliku tekstowego albo z katalogu,
- uruchamia dla każdej instancji tryby bfs / dfs / lc,
- zapisuje:
    1) szczegółowe wyniki per instancja i tryb,
    2) średnie per tryb,
    3) średnie per rozmiar instancji i tryb.

Uruchomienie:
    python3 batch_task2.py batch_config.txt
"""

from __future__ import annotations

import csv
import glob
import os
import statistics
import sys
from collections import defaultdict
from pathlib import Path

from task2_bnb_tsp import (
    parse_config,
    get_int,
    get_bool,
    load_instance,
    branch_and_bound,
    format_path,
)


def parse_batch_config(path: str) -> dict[str, str]:
    return parse_config(path)


def parse_instances_from_config(cfg: dict[str, str]) -> list[str]:
    """
    Obsługiwane warianty:
    1) instances_file=instances.txt
       gdzie każda niepusta linia to ścieżka do instancji
    2) instances_dir=instances
       pattern=*.txt
    3) instances=plik1.txt,plik2.txt,plik3.tsp
    """
    if "instances_file" in cfg:
        file_path = cfg["instances_file"]
        items = []
        with open(file_path, "r", encoding="utf-8") as f:
            for raw in f:
                line = raw.strip()
                if not line or line.startswith("#"):
                    continue
                items.append(line)
        return items

    if "instances_dir" in cfg:
        instances_dir = cfg["instances_dir"]
        pattern = cfg.get("pattern", "*")
        paths = sorted(glob.glob(os.path.join(instances_dir, pattern)))
        return paths

    if "instances" in cfg:
        return [x.strip() for x in cfg["instances"].split(",") if x.strip()]

    raise ValueError(
        "Brak definicji instancji. Użyj: instances_file=..., instances_dir=... albo instances=..."
    )


def parse_modes(cfg: dict[str, str]) -> list[str]:
    raw = cfg.get("modes", "bfs,dfs,lc")
    modes = [x.strip().lower() for x in raw.split(",") if x.strip()]
    valid = {"bfs", "dfs", "lc"}
    for mode in modes:
        if mode not in valid:
            raise ValueError(f"Niepoprawny tryb: {mode}")
    if not modes:
        raise ValueError("Brak trybów w polu modes")
    return modes


def to_float_str(x: float) -> str:
    return f"{x:.3f}"


def mean_or_zero(values: list[float]) -> float:
    return statistics.mean(values) if values else 0.0


def write_detailed_csv(rows: list[dict], path: str) -> None:
    fieldnames = [
        "instance_name",
        "instance_path",
        "n",
        "mode",
        "best_cost",
        "time_ms",
        "expanded_nodes",
        "generated_nodes",
        "pruned_by_bound",
        "pruned_infeasible",
        "completed_tours",
        "max_open_size",
        "timed_out",
        "optimality_proven",
        "initial_upper_bound",
        "initial_upper_bound_method",
        "lower_bound_method",
        "best_path",
    ]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_summary_csv(rows: list[dict], path: str) -> None:
    fieldnames = [
        "mode",
        "runs",
        "avg_time_ms",
        "avg_expanded_nodes",
        "avg_generated_nodes",
        "avg_pruned_by_bound",
        "avg_completed_tours",
        "avg_max_open_size",
        "timeouts",
        "optimality_proven_count",
        "best_cost_min",
        "best_cost_max",
    ]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_by_n_csv(rows: list[dict], path: str) -> None:
    fieldnames = [
        "n",
        "mode",
        "runs",
        "avg_time_ms",
        "avg_expanded_nodes",
        "avg_generated_nodes",
        "avg_pruned_by_bound",
        "avg_completed_tours",
        "avg_max_open_size",
        "timeouts",
        "optimality_proven_count",
    ]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def print_summary(summary_rows: list[dict]) -> None:
    print("=" * 116)
    print("ŚREDNIE WYNIKI DLA WSZYSTKICH INSTANCJI")
    print("=" * 116)
    print(
        f"{'mode':<8}"
        f"{'runs':<8}"
        f"{'avg_time_ms':<14}"
        f"{'avg_expanded':<16}"
        f"{'avg_generated':<16}"
        f"{'avg_pruned':<14}"
        f"{'avg_max_open':<14}"
        f"{'timeouts':<12}"
        f"{'optimal':<12}"
    )
    print("-" * 116)
    for row in summary_rows:
        print(
            f"{row['mode']:<8}"
            f"{row['runs']:<8}"
            f"{row['avg_time_ms']:<14}"
            f"{row['avg_expanded_nodes']:<16}"
            f"{row['avg_generated_nodes']:<16}"
            f"{row['avg_pruned_by_bound']:<14}"
            f"{row['avg_max_open_size']:<14}"
            f"{row['timeouts']:<12}"
            f"{row['optimality_proven_count']:<12}"
        )
    print("=" * 116)


def print_instance_results(rows: list[dict]) -> None:
    print("=" * 140)
    print("WYNIKI SZCZEGÓŁOWE")
    print("=" * 140)
    print(
        f"{'instance':<24}"
        f"{'n':<6}"
        f"{'mode':<8}"
        f"{'best_cost':<12}"
        f"{'time_ms':<12}"
        f"{'expanded':<12}"
        f"{'generated':<12}"
        f"{'pruned':<12}"
        f"{'max_open':<12}"
        f"{'timeout':<10}"
        f"{'optimal':<10}"
    )
    print("-" * 140)
    for row in rows:
        print(
            f"{row['instance_name']:<24}"
            f"{row['n']:<6}"
            f"{row['mode']:<8}"
            f"{str(row['best_cost']):<12}"
            f"{row['time_ms']:<12}"
            f"{row['expanded_nodes']:<12}"
            f"{row['generated_nodes']:<12}"
            f"{row['pruned_by_bound']:<12}"
            f"{row['max_open_size']:<12}"
            f"{str(row['timed_out']):<10}"
            f"{str(row['optimality_proven']):<10}"
        )
    print("=" * 140)


def main() -> int:
    config_path = sys.argv[1] if len(sys.argv) > 1 else "batch_config.txt"
    cfg = parse_batch_config(config_path)

    instance_paths = parse_instances_from_config(cfg)
    modes = parse_modes(cfg)
    start_city = get_int(cfg, "start_city", 0)
    time_limit_s = float(cfg.get("time_limit_s", "60"))
    use_nn_ub = get_bool(cfg, "use_nn_ub", True)

    detailed_csv = cfg.get("detailed_csv", "batch_results_detailed.csv")
    summary_csv = cfg.get("summary_csv", "batch_results_summary.csv")
    by_n_csv = cfg.get("by_n_csv", "batch_results_by_n.csv")

    if not instance_paths:
        raise ValueError("Lista instancji jest pusta")

    detailed_rows: list[dict] = []

    for instance_path in instance_paths:
        instance_name, matrix = load_instance(instance_path)
        n = len(matrix)

        for mode in modes:
            result = branch_and_bound(
                matrix=matrix,
                instance_name=instance_name,
                mode=mode,
                start=start_city,
                time_limit_s=time_limit_s,
                use_nn_ub=use_nn_ub,
            )

            detailed_rows.append(
                {
                    "instance_name": result.instance_name,
                    "instance_path": instance_path,
                    "n": result.n,
                    "mode": result.mode,
                    "best_cost": result.best_cost,
                    "time_ms": to_float_str(result.time_ms),
                    "expanded_nodes": result.expanded_nodes,
                    "generated_nodes": result.generated_nodes,
                    "pruned_by_bound": result.pruned_by_bound,
                    "pruned_infeasible": result.pruned_infeasible,
                    "completed_tours": result.completed_tours,
                    "max_open_size": result.max_open_size,
                    "timed_out": result.timed_out,
                    "optimality_proven": result.optimality_proven,
                    "initial_upper_bound": result.initial_upper_bound,
                    "initial_upper_bound_method": result.initial_upper_bound_method,
                    "lower_bound_method": result.lower_bound_method,
                    "best_path": format_path(result.best_path),
                }
            )

    detailed_rows.sort(key=lambda r: (int(r["n"]), r["instance_name"], r["mode"]))

    grouped_mode: dict[str, list[dict]] = defaultdict(list)
    grouped_n_mode: dict[tuple[int, str], list[dict]] = defaultdict(list)

    for row in detailed_rows:
        grouped_mode[row["mode"]].append(row)
        grouped_n_mode[(int(row["n"]), row["mode"])].append(row)

    summary_rows: list[dict] = []
    for mode in modes:
        rows = grouped_mode[mode]
        times = [float(r["time_ms"]) for r in rows]
        expanded = [int(r["expanded_nodes"]) for r in rows]
        generated = [int(r["generated_nodes"]) for r in rows]
        pruned = [int(r["pruned_by_bound"]) for r in rows]
        tours = [int(r["completed_tours"]) for r in rows]
        max_open = [int(r["max_open_size"]) for r in rows]
        costs = [int(r["best_cost"]) for r in rows if r["best_cost"] not in (None, "", "None")]
        timeouts = sum(str(r["timed_out"]).lower() == "true" for r in rows)
        proven = sum(str(r["optimality_proven"]).lower() == "true" for r in rows)

        summary_rows.append(
            {
                "mode": mode,
                "runs": len(rows),
                "avg_time_ms": to_float_str(mean_or_zero(times)),
                "avg_expanded_nodes": to_float_str(mean_or_zero(expanded)),
                "avg_generated_nodes": to_float_str(mean_or_zero(generated)),
                "avg_pruned_by_bound": to_float_str(mean_or_zero(pruned)),
                "avg_completed_tours": to_float_str(mean_or_zero(tours)),
                "avg_max_open_size": to_float_str(mean_or_zero(max_open)),
                "timeouts": timeouts,
                "optimality_proven_count": proven,
                "best_cost_min": min(costs) if costs else "",
                "best_cost_max": max(costs) if costs else "",
            }
        )

    by_n_rows: list[dict] = []
    for (n, mode), rows in sorted(grouped_n_mode.items(), key=lambda x: (x[0][0], x[0][1])):
        times = [float(r["time_ms"]) for r in rows]
        expanded = [int(r["expanded_nodes"]) for r in rows]
        generated = [int(r["generated_nodes"]) for r in rows]
        pruned = [int(r["pruned_by_bound"]) for r in rows]
        tours = [int(r["completed_tours"]) for r in rows]
        max_open = [int(r["max_open_size"]) for r in rows]
        timeouts = sum(str(r["timed_out"]).lower() == "true" for r in rows)
        proven = sum(str(r["optimality_proven"]).lower() == "true" for r in rows)

        by_n_rows.append(
            {
                "n": n,
                "mode": mode,
                "runs": len(rows),
                "avg_time_ms": to_float_str(mean_or_zero(times)),
                "avg_expanded_nodes": to_float_str(mean_or_zero(expanded)),
                "avg_generated_nodes": to_float_str(mean_or_zero(generated)),
                "avg_pruned_by_bound": to_float_str(mean_or_zero(pruned)),
                "avg_completed_tours": to_float_str(mean_or_zero(tours)),
                "avg_max_open_size": to_float_str(mean_or_zero(max_open)),
                "timeouts": timeouts,
                "optimality_proven_count": proven,
            }
        )

    write_detailed_csv(detailed_rows, detailed_csv)
    write_summary_csv(summary_rows, summary_csv)
    write_by_n_csv(by_n_rows, by_n_csv)

    print_instance_results(detailed_rows)
    print_summary(summary_rows)
    print(f"Zapisano: {detailed_csv}")
    print(f"Zapisano: {summary_csv}")
    print(f"Zapisano: {by_n_csv}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
