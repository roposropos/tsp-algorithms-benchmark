#!/usr/bin/env python3
import csv
import math
import os
import statistics
from collections import defaultdict

try:
    from PIL import Image, ImageDraw, ImageFont
except Exception:
    Image = None
    ImageDraw = None
    ImageFont = None


INPUT = "results/experiment_results.csv"
INSTANCE_SUMMARY = "results/summary_by_instance.csv"
GROUP_SUMMARY = "results/summary_by_group.csv"
MARKDOWN_TABLES = "docs/results_tables.md"
PLOT_GAP = "plots/gap_by_instance.png"
PLOT_TIME = "plots/time_by_dimension.png"
PLOT_LIMIT = "plots/limit_usage.png"


def threshold_for_n(n):
    if n < 25:
        return 0.0
    if n < 74:
        return 50.0
    if n < 449:
        return 100.0
    if n < 2500:
        return 150.0
    return None


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    for row in rows:
        for key in ["dimension", "repetition", "seed", "best_length", "opt_length", "generations", "improvements"]:
            row[key] = int(row[key])
        for key in ["gap_percent", "elapsed_ms", "crossover_rate", "mutation_rate", "local_search_rate"]:
            row[key] = float(row[key])
    return rows


def aggregate_by_instance(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["instance"]].append(row)

    summaries = []
    for label, values in grouped.items():
        values = sorted(values, key=lambda r: r["repetition"])
        gaps = [r["gap_percent"] for r in values]
        times = [r["elapsed_ms"] for r in values]
        best_row = min(values, key=lambda r: r["best_length"])
        n = best_row["dimension"]
        threshold = threshold_for_n(n)
        summaries.append({
            "instance": label,
            "kind": best_row["kind"],
            "source": best_row["source"],
            "type": best_row["type"],
            "edge_weight_type": best_row["edge_weight_type"],
            "dimension": n,
            "opt_length": best_row["opt_length"],
            "best_length": best_row["best_length"],
            "min_gap_percent": min(gaps),
            "mean_gap_percent": statistics.mean(gaps),
            "max_gap_percent": max(gaps),
            "mean_elapsed_ms": statistics.mean(times),
            "min_elapsed_ms": min(times),
            "max_elapsed_ms": max(times),
            "threshold_percent": threshold if threshold is not None else "",
            "threshold_ok": "" if threshold is None else max(gaps) <= threshold + 1e-9,
            "repetitions": len(values),
        })

    return sorted(summaries, key=lambda r: (r["kind"], r["dimension"], r["instance"]))


def aggregate_by_group(summaries):
    grouped = defaultdict(list)
    for row in summaries:
        grouped[row["kind"]].append(row)

    out = []
    for kind, values in sorted(grouped.items()):
        out.append({
            "kind": kind,
            "instances": len(values),
            "min_n": min(r["dimension"] for r in values),
            "max_n": max(r["dimension"] for r in values),
            "mean_gap_percent": statistics.mean(r["mean_gap_percent"] for r in values),
            "max_gap_percent": max(r["max_gap_percent"] for r in values),
            "mean_elapsed_ms": statistics.mean(r["mean_elapsed_ms"] for r in values),
            "worst_instance": max(values, key=lambda r: r["max_gap_percent"])["instance"],
            "all_thresholds_ok": all(r["threshold_ok"] in (True, "") for r in values),
        })
    return out


def write_csv(path, rows, fieldnames):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def fmt(value, places=2):
    if isinstance(value, bool):
        return "tak" if value else "nie"
    if value == "":
        return ""
    if isinstance(value, float):
        return f"{value:.{places}f}"
    return str(value)


def write_markdown(summaries, groups):
    os.makedirs(os.path.dirname(MARKDOWN_TABLES), exist_ok=True)
    with open(MARKDOWN_TABLES, "w", encoding="utf-8") as f:
        f.write("# Tabele wynikow\n\n")
        f.write("## Agregacja po grupach\n\n")
        f.write("| grupa | liczba | zakres n | sredni blad [%] | maks. blad [%] | sredni czas [ms] | najgorsza instancja | progi OK |\n")
        f.write("|---|---:|---:|---:|---:|---:|---|---|\n")
        for row in groups:
            f.write(
                f"| {row['kind']} | {row['instances']} | {row['min_n']}-{row['max_n']} | "
                f"{fmt(row['mean_gap_percent'])} | {fmt(row['max_gap_percent'])} | "
                f"{fmt(row['mean_elapsed_ms'], 1)} | {row['worst_instance']} | {fmt(row['all_thresholds_ok'])} |\n"
            )
        f.write("\n## Agregacja po instancjach\n\n")
        f.write("| instancja | grupa | n | OPT | najlepszy | sr. blad [%] | maks. blad [%] | sr. czas [ms] | limit [%] | OK |\n")
        f.write("|---|---|---:|---:|---:|---:|---:|---:|---:|---|\n")
        for row in sorted(summaries, key=lambda r: r["dimension"]):
            f.write(
                f"| {row['instance']} | {row['kind']} | {row['dimension']} | {row['opt_length']} | "
                f"{row['best_length']} | {fmt(row['mean_gap_percent'])} | {fmt(row['max_gap_percent'])} | "
                f"{fmt(row['mean_elapsed_ms'], 1)} | {fmt(row['threshold_percent'])} | {fmt(row['threshold_ok'])} |\n"
            )


def palette(kind):
    return {
        "SYM": (42, 116, 204),
        "ASYM": (217, 95, 2),
        "VLSI": (45, 145, 87),
    }.get(kind, (80, 80, 80))


def load_font(size=18):
    if ImageFont is None:
        return None
    for path in [
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Times New Roman.ttf",
        "/Library/Fonts/Arial.ttf",
    ]:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def draw_axes(draw, x0, y0, x1, y1, title, x_label, y_label, font, small_font):
    draw.rectangle([x0, y0, x1, y1], outline=(80, 80, 80), width=1)
    draw.text((x0, 22), title, fill=(25, 25, 25), font=font)
    if x_label:
        draw.text(((x0 + x1) // 2 - 70, y1 + 42), x_label, fill=(25, 25, 25), font=small_font)
    if y_label:
        draw.text((18, (y0 + y1) // 2), y_label, fill=(25, 25, 25), font=small_font)


def make_gap_plot(summaries):
    if Image is None:
        return
    os.makedirs(os.path.dirname(PLOT_GAP), exist_ok=True)
    rows = sorted(summaries, key=lambda r: r["dimension"])
    width, height = 1300, 980
    margin_l, margin_r, margin_t, margin_b = 160, 80, 78, 64
    x0, y0 = margin_l, margin_t
    x1, y1 = width - margin_r, height - margin_b
    max_gap = max(max(r["max_gap_percent"] for r in rows), 12.0)
    max_gap = math.ceil(max_gap / 5.0) * 5.0
    img = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(img)
    font = load_font(24)
    small = load_font(16)
    draw_axes(draw, x0, y0, x1, y1, "Błąd względem OPT dla instancji", "błąd [%]", "", font, small)
    for tick in range(0, int(max_gap) + 1, 5):
        x = x0 + (tick / max_gap) * (x1 - x0)
        draw.line([(x, y0), (x, y1)], fill=(228, 228, 228))
        draw.text((x - 8, y1 + 10), str(tick), fill=(60, 60, 60), font=small)
    row_h = (y1 - y0) / len(rows)
    bar_h = max(10, int(row_h * 0.62))
    for idx, row in enumerate(rows):
        cy = y0 + row_h * idx + row_h / 2
        w = (row["mean_gap_percent"] / max_gap) * (x1 - x0)
        color = palette(row["kind"])
        draw.text((18, cy - 9), row["instance"], fill=(45, 45, 45), font=small)
        draw.rectangle([x0, cy - bar_h / 2, x0 + w, cy + bar_h / 2], fill=color)
        draw.text((x0 + w + 6, cy - 9), f"{row['mean_gap_percent']:.2f}", fill=(45, 45, 45), font=small)
    legend_x = x1 - 245
    for i, kind in enumerate(["SYM", "ASYM", "VLSI"]):
        y = 25 + i * 24
        draw.rectangle([legend_x, y, legend_x + 16, y + 16], fill=palette(kind))
        draw.text((legend_x + 24, y - 2), kind, fill=(30, 30, 30), font=small)
    img.save(PLOT_GAP)


def make_time_plot(summaries):
    if Image is None:
        return
    os.makedirs(os.path.dirname(PLOT_TIME), exist_ok=True)
    rows = sorted(summaries, key=lambda r: r["dimension"])
    width, height = 1200, 720
    margin_l, margin_r, margin_t, margin_b = 92, 36, 78, 92
    x0, y0 = margin_l, margin_t
    x1, y1 = width - margin_r, height - margin_b
    max_n = max(r["dimension"] for r in rows) * 1.05
    max_time = max(r["mean_elapsed_ms"] for r in rows)
    max_time = math.ceil(max_time / 500.0) * 500.0
    img = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(img)
    font = load_font(24)
    small = load_font(16)
    draw_axes(draw, x0, y0, x1, y1, "Średni czas wykonania", "liczba miast n", "czas [ms]", font, small)
    for tick in range(0, int(max_time) + 1, 500):
        y = y1 - (tick / max_time) * (y1 - y0)
        draw.line([(x0, y), (x1, y)], fill=(228, 228, 228))
        draw.text((22, y - 9), str(tick), fill=(60, 60, 60), font=small)
    for tick in [0, 500, 1000, 1500, 2000, 2500]:
        x = x0 + (tick / max_n) * (x1 - x0)
        draw.line([(x, y0), (x, y1)], fill=(242, 242, 242))
        draw.text((x - 18, y1 + 10), str(tick), fill=(60, 60, 60), font=small)
    for row in rows:
        x = x0 + (row["dimension"] / max_n) * (x1 - x0)
        y = y1 - (row["mean_elapsed_ms"] / max_time) * (y1 - y0)
        color = palette(row["kind"])
        draw.ellipse([x - 6, y - 6, x + 6, y + 6], fill=color, outline=(30, 30, 30))
    legend_x = x1 - 245
    for i, kind in enumerate(["SYM", "ASYM", "VLSI"]):
        y = 25 + i * 24
        draw.rectangle([legend_x, y, legend_x + 16, y + 16], fill=palette(kind))
        draw.text((legend_x + 24, y - 2), kind, fill=(30, 30, 30), font=small)
    img.save(PLOT_TIME)


def make_limit_plot(summaries):
    if Image is None:
        return
    os.makedirs(os.path.dirname(PLOT_LIMIT), exist_ok=True)
    rows = sorted(summaries, key=lambda r: r["dimension"])
    width, height = 1300, 980
    margin_l, margin_r, margin_t, margin_b = 160, 80, 78, 64
    x0, y0 = margin_l, margin_t
    x1, y1 = width - margin_r, height - margin_b
    values = []
    for row in rows:
        threshold = row["threshold_percent"]
        if threshold == "" or float(threshold) == 0:
            usage = 0.0
        else:
            usage = 100.0 * row["max_gap_percent"] / float(threshold)
        values.append(usage)
    max_usage = max(max(values), 20.0)
    max_usage = math.ceil(max_usage / 5.0) * 5.0
    img = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(img)
    font = load_font(24)
    small = load_font(16)
    draw_axes(draw, x0, y0, x1, y1, "Wykorzystanie dopuszczalnego limitu błędu", "część limitu [%]", "", font, small)
    for tick in range(0, int(max_usage) + 1, 5):
        x = x0 + (tick / max_usage) * (x1 - x0)
        draw.line([(x, y0), (x, y1)], fill=(228, 228, 228))
        draw.text((x - 8, y1 + 10), str(tick), fill=(60, 60, 60), font=small)
    row_h = (y1 - y0) / len(rows)
    bar_h = max(10, int(row_h * 0.62))
    for idx, row in enumerate(rows):
        cy = y0 + row_h * idx + row_h / 2
        usage = values[idx]
        w = (usage / max_usage) * (x1 - x0)
        color = palette(row["kind"])
        draw.text((18, cy - 9), row["instance"], fill=(45, 45, 45), font=small)
        draw.rectangle([x0, cy - bar_h / 2, x0 + w, cy + bar_h / 2], fill=color)
        draw.text((x0 + w + 6, cy - 9), f"{usage:.1f}", fill=(45, 45, 45), font=small)
    legend_x = x1 - 245
    for i, kind in enumerate(["SYM", "ASYM", "VLSI"]):
        y = 25 + i * 24
        draw.rectangle([legend_x, y, legend_x + 16, y + 16], fill=palette(kind))
        draw.text((legend_x + 24, y - 2), kind, fill=(30, 30, 30), font=small)
    img.save(PLOT_LIMIT)


def main():
    rows = read_rows(INPUT)
    summaries = aggregate_by_instance(rows)
    groups = aggregate_by_group(summaries)
    write_csv(INSTANCE_SUMMARY, summaries, list(summaries[0].keys()))
    write_csv(GROUP_SUMMARY, groups, list(groups[0].keys()))
    write_markdown(summaries, groups)
    make_gap_plot(summaries)
    make_time_plot(summaries)
    make_limit_plot(summaries)
    print(f"rows={len(rows)} instances={len(summaries)} groups={len(groups)}")
    print(f"wrote {INSTANCE_SUMMARY}, {GROUP_SUMMARY}, {MARKDOWN_TABLES}")
    if Image is not None:
        print(f"wrote {PLOT_GAP}, {PLOT_TIME}, {PLOT_LIMIT}")


if __name__ == "__main__":
    main()
