import csv
import sys

from task2_bnb_tsp import (
    parse_config,
    get_required,
    get_int,
    get_bool,
    load_instance,
    branch_and_bound,
)


def fmt(value):
    return "-" if value is None else str(value)


def main():
    config_path = sys.argv[1] if len(sys.argv) > 1 else "config.txt"
    cfg = parse_config(config_path)

    instance_path = get_required(cfg, "instance")
    start_city = get_int(cfg, "start_city", 0)
    time_limit_s = float(cfg.get("time_limit_s", "60"))
    use_nn_ub = get_bool(cfg, "use_nn_ub", True)

    instance_name, matrix = load_instance(instance_path)

    modes = ["bfs", "dfs", "lc"]
    results = []

    for mode in modes:
        result = branch_and_bound(
            matrix=matrix,
            instance_name=instance_name,
            mode=mode,
            start=start_city,
            time_limit_s=time_limit_s,
            use_nn_ub=use_nn_ub,
        )
        results.append(result)

    print("=" * 120)
    print(f"PORÓWNANIE TRYBÓW B&B DLA INSTANCJI: {instance_name}")
    print("=" * 120)
    print(
        f"{'mode':<8}"
        f"{'best_cost':<12}"
        f"{'time_ms':<12}"
        f"{'expanded':<12}"
        f"{'generated':<12}"
        f"{'pruned':<12}"
        f"{'max_open':<12}"
        f"{'timed_out':<12}"
        f"{'optimal':<12}"
    )
    print("-" * 120)

    for r in results:
        print(
            f"{r.mode:<8}"
            f"{fmt(r.best_cost):<12}"
            f"{r.time_ms:<12.3f}"
            f"{r.expanded_nodes:<12}"
            f"{r.generated_nodes:<12}"
            f"{r.pruned_by_bound:<12}"
            f"{r.max_open_size:<12}"
            f"{str(r.timed_out):<12}"
            f"{str(r.optimality_proven):<12}"
        )

    with open("comparison_task2.csv", "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "instance_name",
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
        ])
        for r in results:
            writer.writerow([
                r.instance_name,
                r.mode,
                r.best_cost,
                f"{r.time_ms:.3f}",
                r.expanded_nodes,
                r.generated_nodes,
                r.pruned_by_bound,
                r.pruned_infeasible,
                r.completed_tours,
                r.max_open_size,
                r.timed_out,
                r.optimality_proven,
                r.initial_upper_bound,
                r.initial_upper_bound_method,
                r.lower_bound_method,
                " -> ".join(map(str, r.best_path)) + (f" -> {r.best_path[0]}" if r.best_path else ""),
            ])

    print("-" * 120)
    print("Zapisano plik: comparison_task2.csv")


if __name__ == "__main__":
    main()