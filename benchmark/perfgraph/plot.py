#!/usr/bin/env python3
"""Draw the performance graphs from run.py's JSONL rows.

x = compression ratio (original / compressed); y = combined speed = original bytes /
(encode s + decode s), decimal MB/s, log scale. Each point is annotated with its encode
and decode MB/s. One chart per data group and one overall chart (all inputs of the set).

Per input: encode and decode times are the medians of the REPS runs; the combined time
is the median of the per-round encode+decode sums (Codex's tip-benchmark definition).
Per group: ratio = sum(original) / sum(compressed); speeds = sum(original) / sum(times).

  python3 plot.py rows/dev.jsonl [more.jsonl ...] --out charts/dev
"""
import argparse
import csv
import json
import os
import statistics
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.ticker import FuncFormatter  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
GROUPS = json.load(open(os.path.join(HERE, "groups.json")))

INK = "#1f1f1f"
MUTED = "#6b6a66"
GRID = "#e4e3df"
# One colour per tool (validated categorical palette, fixed order). xz and 7z LZMA2
# are the same codec: one colour, two markers.
TOOLS = [  # tool, colour, marker, legend name
    ("lz4", "#eda100", "o", "lz4"),
    ("zstd", "#2a78d6", "o", "zstd"),
    ("brotli", "#eb6834", "o", "brotli"),
    ("xz", "#6250d6", "o", "xz"),
    ("7z-lzma2", "#6250d6", "D", "7z LZMA2 -mx9"),
    ("7z-ppmd", "#e87ba4", "D", "7z PPMd -mx9"),
    ("bzip3", "#1baf7a", "o", "bzip3"),
    ("bsc", "#008300", "o", "bsc"),
    ("zpaq", "#e34948", "o", "zpaq"),
]
TOOL_STYLE = {t: (c, m, n) for t, c, m, n in TOOLS}
SNAP_MARKER = {"A": ("o", "white"), "B": ("s", INK), "C": ("^", "#2a78d6")}
LEVEL_ORDER = {str(n): n for n in range(1, 10)}
LEVEL_ORDER.update({"ultra": 10, "9e": 9.5, "-": 0})


def med(xs):
    return statistics.median(xs)


def load(paths, size_only=False):
    """Rows of one kind: timed rows (default) or the untimed size-only pass."""
    cells = {}
    for p in paths:
        for line in open(p):
            r = json.loads(line)
            if r.get("error") or not r["enc_ns"] or bool(r.get("size_only")) != size_only:
                continue
            if len(set(r["comp_sha"])) != 1:
                raise SystemExit(f"non-deterministic output: {r['config']} {r['input']}")
            # Later rows for the same cell (a re-run) replace earlier ones.
            cells[(r["config"], r["input"], r.get("snapshot"))] = r
    return list(cells.values())


def per_input(r):
    comb = [e + d for e, d in zip(r["enc_ns"], r["dec_ns"])]
    return dict(bytes=r["input_bytes"], comp=r["comp_bytes"][-1], enc=med(r["enc_ns"]) / 1e9,
                dec=med(r["dec_ns"]) / 1e9, comb=med(comb) / 1e9, rss=max(max(r["enc_rss"]), max(r["dec_rss"])))


def aggregate(rows, inputs):
    """Points keyed by config, summed over `inputs`; a config missing any input is left out."""
    by = defaultdict(dict)
    meta = {}
    for r in rows:
        if r["input"] in inputs:
            by[r["config"]][r["input"]] = per_input(r)
            meta[r["config"]] = r
    pts = []
    for cfg, d in by.items():
        if set(d) != set(inputs):
            continue
        b = sum(x["bytes"] for x in d.values())
        m = meta[cfg]
        pts.append(dict(config=cfg, tool=m["tool"], label=m["label"], level=m["level"], snapshot=m.get("snapshot"),
                        snapshot_label=m.get("snapshot_label"), bytes=b, comp=sum(x["comp"] for x in d.values()),
                        ratio=b / sum(x["comp"] for x in d.values()),
                        enc_mbs=b / 1e6 / sum(x["enc"] for x in d.values()),
                        dec_mbs=b / 1e6 / sum(x["dec"] for x in d.values()),
                        comb_mbs=b / 1e6 / sum(x["comb"] for x in d.values()),
                        peak_rss_mb=max(x["rss"] for x in d.values()) / 1e6, n_inputs=len(d)))
    return pts


def fmt(v):
    return f"{v:.0f}" if v >= 10 else (f"{v:.1f}" if v >= 1 else f"{v:.2f}")


def draw(pts, title, subtitle, path_base):
    fig, ax = plt.subplots(figsize=(15, 9.5), dpi=150)
    ax.set_yscale("log")
    ax.grid(True, which="major", color=GRID, linewidth=0.8)
    ax.grid(True, which="minor", color=GRID, linewidth=0.4, alpha=0.6)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelsize=9)
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))

    def note(p, color, dy=-9):
        ax.annotate(f"{p['label']}  e{fmt(p['enc_mbs'])} d{fmt(p['dec_mbs'])}", (p["ratio"], p["comb_mbs"]),
                    xytext=(5, dy), textcoords="offset points", fontsize=5.6, color=color)

    handles = []
    # Competitors: one line per tool through its levels.
    for tool, (color, marker, name) in TOOL_STYLE.items():
        tp = sorted([p for p in pts if p["tool"] == tool], key=lambda p: LEVEL_ORDER.get(p["level"], 0))
        if not tp:
            continue
        ax.plot([p["ratio"] for p in tp], [p["comb_mbs"] for p in tp], color=color, linewidth=1.4, alpha=0.75,
                zorder=2)
        ax.scatter([p["ratio"] for p in tp], [p["comb_mbs"] for p in tp], s=34, marker=marker, color=color,
                   edgecolors="white", linewidths=1.2, zorder=3)
        for p in tp:
            note(p, MUTED)
        handles.append(Line2D([], [], color=color, marker=marker, markersize=6, linewidth=1.4, label=name))

    # Kolmo: one curve per snapshot, and a dashed connector per level across snapshots.
    kp = [p for p in pts if p["tool"] == "kolmo"]
    snaps = sorted({p["snapshot"] for p in kp})
    by_level = defaultdict(dict)
    for p in kp:
        by_level[p["level"]][p["snapshot"]] = p
    for lv, d in by_level.items():
        seq = [d[s] for s in snaps if s in d]
        if len(seq) > 1:
            ax.plot([p["ratio"] for p in seq], [p["comb_mbs"] for p in seq], color=MUTED, linewidth=0.9,
                    linestyle=(0, (2, 2)), zorder=4)
            ax.annotate("", xy=(seq[-1]["ratio"], seq[-1]["comb_mbs"]),
                        xytext=(seq[-2]["ratio"], seq[-2]["comb_mbs"]),
                        arrowprops=dict(arrowstyle="-|>", color=MUTED, lw=0.9, shrinkA=4, shrinkB=5), zorder=4)
    for i, s in enumerate(snaps):
        sp = sorted([p for p in kp if p["snapshot"] == s], key=lambda p: LEVEL_ORDER[p["level"]])
        marker, face = SNAP_MARKER.get(s, ("P", INK))
        edge = "#2a78d6" if s == "C" else INK
        ax.plot([p["ratio"] for p in sp], [p["comb_mbs"] for p in sp], color=INK, linewidth=1.1 if s != "A" else 0.8,
                alpha=0.35 if s == "A" else 0.6, zorder=5)
        ax.scatter([p["ratio"] for p in sp], [p["comb_mbs"] for p in sp], s=58, marker=marker, facecolor=face,
                   edgecolors=edge, linewidths=1.4, zorder=6)
        for p in sp:
            note(dict(p, label=("K" + p["label"])), INK, dy=5 if s == snaps[-1] else -10)
        handles.append(Line2D([], [], color=INK, marker=marker, markerfacecolor=face, markeredgecolor=edge,
                              markersize=7, linewidth=1.0, label=f"Kolmo {s}: {sp[0]['snapshot_label']}"))

    ax.set_xlabel("Compression ratio (original / compressed)  →  smaller output", fontsize=10, color=INK)
    ax.set_ylabel("Combined speed, MB/s = original / (encode s + decode s)  (log)", fontsize=10, color=INK)
    ax.set_title(title, loc="left", fontsize=14, color=INK, pad=22)
    ax.text(0, 1.012, subtitle, transform=ax.transAxes, fontsize=8.5, color=MUTED, va="bottom")
    ax.legend(handles=handles, loc="upper right", fontsize=8, frameon=True, framealpha=0.92, edgecolor=GRID)
    xs = [p["ratio"] for p in pts]
    ax.set_xlim(min(xs) * 0.97 - 0.05, max(xs) * 1.06)
    fig.text(0.01, 0.005, "Point label: tool/level, e = encode MB/s, d = decode MB/s (decimal MB, wall time incl. "
             "process start, median of 3, one thread). K = Kolmo level. Dashed arrows join the same Kolmo level "
             "across snapshots.", fontsize=7, color=MUTED)
    fig.tight_layout(rect=(0, 0.015, 1, 1))
    fig.savefig(path_base + ".png")
    fig.savefig(path_base + ".pdf")
    plt.close(fig)


def check_sizes(timed, sized):
    """A timed cell must produce the same compressed bytes as the size-only pass of the same binary."""
    ref = {(r["config"], r["input"], r["binary_sha256"]): r["comp_sha"][0] for r in sized}
    for r in timed:
        k = (r["config"], r["input"], r["binary_sha256"])
        if k in ref and ref[k] != r["comp_sha"][0]:
            raise SystemExit(f"compressed output differs from the size-only pass: {k}")


def write_sizes(rows, which, out):
    """Ratio table (rows = configurations, columns = groups + overall) from the untimed pass."""
    groups = [g for g in GROUPS["order"] if set(GROUPS[which][g]) <= {r["input"] for r in rows}]
    every = set().union(*[set(GROUPS[which][g]) for g in groups]) if groups else set()
    cols = groups + (["overall"] if groups else [])
    res = defaultdict(dict)
    meta = {}
    for g in cols:
        for p in aggregate(rows, every if g == "overall" else set(GROUPS[which][g])):
            res[p["config"]][g] = p
            meta[p["config"]] = p

    def key(c):
        m = meta[c]
        tools = ["kolmo"] + [t for t, *_ in TOOLS]
        return (tools.index(m["tool"]), m.get("snapshot") or "", LEVEL_ORDER.get(m["level"], 0))

    order = sorted(res, key=key)
    with open(os.path.join(out, "sizes.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["config", "tool", "snapshot", "level"] + [f"{g}_ratio" for g in cols] +
                   [f"{g}_compressed_bytes" for g in cols])
        for c in order:
            m = meta[c]
            w.writerow([c, m["tool"], m.get("snapshot") or "", m["level"]] +
                       [f"{res[c][g]['ratio']:.4f}" if g in res[c] else "" for g in cols] +
                       [res[c][g]["comp"] if g in res[c] else "" for g in cols])
    with open(os.path.join(out, "sizes.md"), "w") as f:
        f.write(f"# Compression ratio by group ({which} set, untimed size pass)\n\n")
        f.write("Ratio = sum of original bytes / sum of compressed bytes over the group's inputs. "
                "Every output was decoded and matched the input's SHA-256. Timings: pending, quiet window.\n\n")
        f.write("| Config | " + " | ".join(GROUPS["titles"].get(g, "Overall") for g in cols) + " |\n")
        f.write("|---|" + "---:|" * len(cols) + "\n")
        for c in order:
            m = meta[c]
            name = f"Kolmo {m['snapshot']} L{m['label']}" if m["tool"] == "kolmo" else m["label"]
            f.write(f"| {name} | " + " | ".join(f"{res[c][g]['ratio']:.3f}" if g in res[c] else ""
                                                 for g in cols) + " |\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rows", nargs="+")
    ap.add_argument("--out", required=True)
    ap.add_argument("--set", default="dev")
    ap.add_argument("--note", default="Apple M-series Mac, 1 thread per tool, warm cache")
    ap.add_argument("--sizes", action="store_true", help="also write sizes.md/sizes.csv from the size-only rows")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if a.sizes:
        write_sizes([r for r in load(a.rows, size_only=True) if r["set"] == a.set], a.set, a.out)
    rows = [r for r in load(a.rows) if r["set"] == a.set]
    check_sizes(rows, [r for r in load(a.rows, size_only=True) if r["set"] == a.set])
    table = []
    every = []
    for g in GROUPS["order"]:
        inputs = GROUPS[a.set][g]
        every += inputs
        have = {r["input"] for r in rows}
        if not set(inputs) <= have:
            continue
        pts = aggregate(rows, set(inputs))
        if not pts:
            continue
        mb = pts[0]["bytes"] / 1e6
        draw(pts, f"{GROUPS['titles'][g]} — ratio vs combined speed",
             f"{a.set} set: {', '.join(inputs)} ({mb:.1f} MB). {a.note}.", os.path.join(a.out, g))
        table += [dict(group=g, **p) for p in pts]
    if set(every) <= {r["input"] for r in rows}:
        pts = aggregate(rows, set(every))
        if pts:
            draw(pts, "All data groups — ratio vs combined speed",
                 f"{a.set} set, {len(every)} inputs, {pts[0]['bytes'] / 1e6:.1f} MB in all. {a.note}.",
                 os.path.join(a.out, "overall"))
            table += [dict(group="overall", **p) for p in pts]
    with open(os.path.join(a.out, "points.csv"), "w", newline="") as f:
        cols = ["group", "config", "tool", "snapshot", "snapshot_label", "label", "level", "n_inputs", "bytes", "comp",
                "ratio", "enc_mbs", "dec_mbs", "comb_mbs", "peak_rss_mb"]
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        for t in table:
            w.writerow({k: (round(v, 4) if isinstance(v, float) else v) for k, v in t.items()})
    print(f"{len(table)} points -> {a.out}")


if __name__ == "__main__":
    main()
