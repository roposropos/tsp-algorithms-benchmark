#!/usr/bin/env python3
"""
Wykresy do zadania 2 na podstawie plików:
- batch_results_by_n.csv
- batch_results_summary.csv
- batch_results_detailed.csv

Tworzy:
- chart_time_vs_n.png
- chart_expanded_vs_n.png
- chart_max_open_vs_n.png
- chart_summary_time.png
- chart_summary_expanded.png
"""

from __future__ import annotations

import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


def read_csv(path: str):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def to_float(x):
    return float(x)


def to_int(x):
    return int(float(x))


def line_chart_by_n(rows, value_key, ylabel, out_name, title):
    grouped = defaultdict(list)
    for row in rows:
        mode = row["mode"]
        grouped[mode].append((to_int(row["n"]), to_float(row[value_key])))

    plt.figure(figsize=(9, 5))
    for mode in ["bfs", "dfs", "lc"]:
        if mode not in grouped:
            continue
        pairs = sorted(grouped[mode], key=lambda x: x[0])
        xs = [x for x, _ in pairs]
        ys = [y for _, y in pairs]
        plt.plot(xs, ys, marker="o", label=mode.upper())

    plt.xlabel("Rozmiar instancji n")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_name, dpi=200)
    plt.close()


def bar_chart_summary(rows, value_key, ylabel, out_name, title):
    rows = sorted(rows, key=lambda r: r["mode"])
    modes = [row["mode"].upper() for row in rows]
    values = [to_float(row[value_key]) for row in rows]

    plt.figure(figsize=(8, 5))
    plt.bar(modes, values)
    plt.xlabel("Tryb")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, axis="y")
    plt.tight_layout()
    plt.savefig(out_name, dpi=200)
    plt.close()


def main():
    by_n_path = "batch_results_by_n.csv"
    summary_path = "batch_results_summary.csv"

    by_n_rows = read_csv(by_n_path)
    summary_rows = read_csv(summary_path)

    line_chart_by_n(
        by_n_rows,
        value_key="avg_time_ms",
        ylabel="Średni czas [ms]",
        out_name="chart_time_vs_n.png",
        title="Średni czas działania w funkcji rozmiaru instancji",
    )

    line_chart_by_n(
        by_n_rows,
        value_key="avg_expanded_nodes",
        ylabel="Średnia liczba rozwiniętych węzłów",
        out_name="chart_expanded_vs_n.png",
        title="Średnia liczba rozwiniętych węzłów w funkcji rozmiaru instancji",
    )

    line_chart_by_n(
        by_n_rows,
        value_key="avg_max_open_size",
        ylabel="Średni maksymalny rozmiar OPEN",
        out_name="chart_max_open_vs_n.png",
        title="Średni maksymalny rozmiar OPEN w funkcji rozmiaru instancji",
    )

    bar_chart_summary(
        summary_rows,
        value_key="avg_time_ms",
        ylabel="Średni czas [ms]",
        out_name="chart_summary_time.png",
        title="Średni czas działania - porównanie BFS / DFS / LC",
    )

    bar_chart_summary(
        summary_rows,
        value_key="avg_expanded_nodes",
        ylabel="Średnia liczba rozwiniętych węzłów",
        out_name="chart_summary_expanded.png",
        title="Średnia liczba rozwiniętych węzłów - porównanie BFS / DFS / LC",
    )

    print("Wygenerowano pliki:")
    for name in [
        "chart_time_vs_n.png",
        "chart_expanded_vs_n.png",
        "chart_max_open_vs_n.png",
        "chart_summary_time.png",
        "chart_summary_expanded.png",
    ]:
        print(name)


if __name__ == "__main__":
    main()
