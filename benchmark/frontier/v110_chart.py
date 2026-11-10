"""Render the matched v1.1.0 release replay and preserve its verified measurements."""
import argparse
import hashlib
import json
from pathlib import Path
from statistics import median

import matplotlib
matplotlib.use("Agg")
from matplotlib import pyplot as plt
from matplotlib.patches import Rectangle


DATASETS = [
    ("eia", "US electricity", "EIA-930 balancing authorities"),
    ("grid", "European electricity", "OPSD hourly grid data"),
    ("bsub", "Bacterial genome", "B. subtilis reference FASTA"),
    ("archive", "Mixed archive", "Silesia, complete tar"),
]
OTHER_DATASETS = [
    ("cossmic", "Household electricity", "OPSD CoSSMic meter readings"),
    ("portugal", "Portuguese electricity", "UCI electricity customer readings"),
    ("chr22", "Human genome", "Human chromosome 22 FASTA"),
    ("yeast", "Yeast genome", "S. cerevisiae reference FASTA"),
    ("events", "Event logs", "GH Archive public events JSONL"),
    ("sqlite", "Relational database", "Chinook SQLite database"),
    ("docx", "Office document", "Apache POI example DOCX"),
]
SAMPLING_CASES = {
    "ce11": "Caenorhabditis elegans · chromosome I",
    "pao1": "Pseudomonas aeruginosa PAO1 · reference genome",
    "danrer25": "Zebrafish · chromosome 25",
    "chr22": "Human · chromosome 22",
    "bsub": "Bacillus subtilis · reference genome",
    "yeast": "Saccharomyces cerevisiae · reference genome",
}
SAMPLING_BINARY = "b5e4fb287f051b54df7fdb367d2ba093e4b63b185f8ebd578b96c438eced88fa"
LABELS = {
    "v110": "v1.1.0 · default fast",
    "frontier": "v1.1.0 · opt-in fast",
    "m1": "Nov 7 · M1 fast",
    "v100": "v1.0.0 · fast",
    "zstd": "Zstd −3",
    "openzl": "Trained OpenZL",
    "naf1": "NAF −1",
    "naf19": "NAF −19",
    "guard": "PR #5 · FASTA selector fast",
}
PHASES = ("encode", "decode")


def read_dataset(path, name, title, description, optional=False):
    content = path.read_bytes()
    receipt = json.loads(content)
    if optional and "metrics" not in receipt:
        return None
    rounds = receipt["rounds"]
    size = receipt["source_bytes"]
    source_sha = receipt["source_sha256"]
    if set(receipt.get("metrics", {})) != set(receipt["commands"]) or rounds != 7:
        raise ValueError(f"{name} still needs verified results")
    rows = {}
    for codec, metric in receipt["metrics"].items():
        raw = sorted((t for t in receipt["trials"] if t["codec"] == codec),
                     key=lambda t: t["repeat"])
        if [t["repeat"] for t in raw] != list(range(rounds + 1)):
            raise ValueError(f"Incomplete paired trials for {name}/{codec}")
        if any(not t["exact"] or t["decoded_sha256"] != source_sha or t["size"] != metric["size"] for t in raw):
            raise ValueError(f"Exact restoration or frame size differs for {name}/{codec}")
        commands = receipt["commands"][codec]
        result = {
            "label": LABELS.get(codec, codec),
            "frame_bytes": metric["size"],
            "ratio": size / metric["size"],
            "executables": {
                phase: receipt["binary_hashes"][commands[phase][0]] for phase in PHASES
            },
            "trials": [
                {key: t[key] for key in ("repeat", "frame_sha256", "decoded_sha256", "encode", "decode")}
                for t in raw
            ],
        }
        for phase in PHASES:
            wall_ms = 1000 * median(t[phase]["seconds"] for t in raw[1:])
            peak = max(t[phase]["peak_rss_bytes"] for t in raw[1:])
            if abs(wall_ms - metric[phase + "_ms"]) > 1e-6 or peak != metric[phase + "_rss"]:
                raise ValueError(f"Stored metrics differ from trials for {name}/{codec}/{phase}")
            result[phase + "_ms"] = wall_ms
            result[phase + "_mbps"] = size / (wall_ms * 1000)
            result[phase + "_rss"] = peak
        rows[codec] = result
    result = dict(title=title, description=description, source_bytes=size, source_sha256=source_sha,
                  rounds=rounds, codecs=rows, receipt_sha256=hashlib.sha256(content).hexdigest())
    if "openzl" in rows:
        args = receipt["commands"]["openzl"]["encode"]
        graph = Path(args[args.index("--compressor") + 1])
        result["openzl_graph_sha256_at_report"] = hashlib.sha256(graph.read_bytes()).hexdigest()
    return result


def gate(candidate, target):
    checks = {"at_least_20pct_smaller": 5 * candidate["frame_bytes"] <= 4 * target["frame_bytes"]}
    for phase in PHASES:
        for key in ("_ms", "_rss"):
            checks[phase + key] = candidate[phase + key] <= target[phase + key]
    return {"pass": all(checks.values()), "checks": checks,
            "bytes_saved_pct": 100 * (1 - candidate["frame_bytes"] / target["frame_bytes"])}


def verify_binaries(name, rows, pins):
    for codec, row in rows.items():
        enc = "ennaf" if codec.startswith("naf") else codec
        dec = "unnaf" if codec.startswith("naf") else codec
        if row["executables"] != {"encode": pins[enc], "decode": pins[dec]}:
            raise ValueError(f"Executable pin mismatch for {name}/{codec}")


def read_sampling(directory, freeze):
    cases = {}
    pins = {**freeze["binaries"], "guard": SAMPLING_BINARY}
    for name, description in SAMPLING_CASES.items():
        path = directory/(name+".results.json")
        if not path.exists():
            continue
        dataset = read_dataset(path, name, name+".fa", description, optional=True)
        if dataset is None:
            continue
        info = json.loads((directory/(name+".input.json")).read_text())
        if (info["guard_sha256"] != SAMPLING_BINARY or info["base_revision"] != freeze["source_revision"]
                or info["target_sha256"] != dataset["source_sha256"]
                or info["target_bytes"] != dataset["source_bytes"]):
            raise ValueError(f"FASTA selector input or executable pin mismatch for {name}")
        if set(dataset["codecs"]) != {"guard", "v110", "frontier", "zstd", "naf1", "naf19"}:
            raise ValueError(f"Incomplete selector panel for {name}")
        verify_binaries(name, dataset["codecs"], pins)
        dataset["input_manifest"] = info
        cases[name] = dataset
    return cases


def draw(datasets, output):
    ink, muted = "#173348", "#506771"
    teal, dim, enc, dec = "#00786c", "#708594", "#c86532", "#8fbad2"
    row_h = .33
    gap_h = .57
    top_h = 1.36
    foot_h = 2.15
    group_h = sum(gap_h + row_h * len(datasets[k]["codecs"]) + .16 for k, _, _ in DATASETS)
    height = top_h + group_h + foot_h
    width = 13.2
    fig = plt.figure(figsize=(width, height), facecolor="white")
    ax = fig.add_axes([0, 0, 1, 1], xlim=(0, width), ylim=(height, 0))
    ax.axis("off")

    def text(x, y, value, *, weight="normal", size=9, color=ink, align="left"):
        ax.text(x, y, value, fontsize=size, fontweight=weight, color=color,
                ha=align, va="center", family="DejaVu Sans")

    def rect(x, y, w, h, color):
        ax.add_patch(Rectangle((x, y-h/2), w, h, linewidth=0, facecolor=color))

    start, ratio_x, speed_x = .32, 2.57, 5.56
    rate_col, dec_col, rss_col = 9.95, 10.96, 12.8
    ratio_w, speed_w = 2.07, 3.12
    all_rows = [r for k, _, _ in DATASETS for r in datasets[k]["codecs"].values()]
    ratio_max = max(r["ratio"] for r in all_rows)
    rate_max = max(r["encode_mbps"] + r["decode_mbps"] for r in all_rows)
    text(start, .34, "Kolmo v1.1.0 and the competition", size=22, weight="bold")
    text(start, .72, "Apple M5   ·   One CPU worker   ·   Complete-file CLI measurements   ·   Byte-exact reconstruction",
         size=10, color=muted)
    text(start, 1.12, "Setting", size=9, weight="bold")
    text(ratio_x, 1.12, "Compression ratio", size=9, weight="bold")
    rect(speed_x, 1.12, .12, .12, enc)
    text(speed_x+.19, 1.12, "Encode", size=9)
    rect(speed_x+.98, 1.12, .12, .12, dec)
    text(speed_x+1.17, 1.12, "Decode  ·  MB/s", size=9)
    for x, title in ((rate_col, "Encode"), (dec_col, "Decode"), (rss_col, "Peak RSS · MiB")):
        text(x, 1.12, title, size=9, weight="bold", align="right")
    y = top_h
    for name, title, description in DATASETS:
        info = datasets[name]
        ax.plot([start, rss_col], [y, y], color="#a5b7bd", lw=.65)
        text(start, y+.25, title, weight="bold", size=13)
        text(ratio_x, y+.26, f"{description}  ·  {info['source_bytes']/1e6:.2f} MB", color=muted)
        y += gap_h
        order = list(LABELS) + sorted(set(info["codecs"]) - set(LABELS))
        for k in order:
            if k not in info["codecs"]:
                continue
            r = info["codecs"][k]
            kolmo = k in {"frontier", "v110", "m1", "v100"}
            text(start+.06, y, LABELS.get(k, k), weight="bold" if k == "frontier" else "normal",
                 color=teal if kolmo else ink)
            rw = ratio_w * r["ratio"] / ratio_max
            rect(ratio_x, y, rw, .14, teal if kolmo else dim)
            text(ratio_x + rw + .07, y, f"{r['ratio']:.2f}×", size=8)
            ew = speed_w * r["encode_mbps"] / rate_max
            dw = speed_w * r["decode_mbps"] / rate_max
            rect(speed_x, y, ew, .14, enc)
            rect(speed_x + ew, y, dw, .14, dec)
            text(rate_col, y, f"{r['encode_mbps']:,.1f}", align="right")
            text(dec_col, y, f"{r['decode_mbps']:,.1f}", align="right")
            text(rss_col, y, f"{r['encode_rss']/2**20:,.1f} / {r['decode_rss']/2**20:,.1f}", align="right")
            y += row_h
        y += .16
    ax.plot([start, rss_col], [y-.04, y-.04], color="#a5b7bd", lw=.65)
    checks = [
        ("at_least_20pct_smaller", "≥20% smaller"), ("encode_ms", "Encode time"),
        ("decode_ms", "Decode time"), ("encode_rss", "Encode RSS"), ("decode_rss", "Decode RSS"),
    ]
    columns = (5.9, 7.36, 8.7, 10.05, 11.45)
    text(start, y+.2, "Opt-in fast · electricity targets", size=10, weight="bold")
    for x, (_, label) in zip(columns, checks):
        text(x, y+.2, label, size=8, weight="bold", align="center")
    row = y + .47
    for name, title in (("eia", "US EIA"), ("grid", "European OPSD")):
        for k in ("zstd", "openzl"):
            g = datasets[name]["opt_in_gates"][k]
            delta = g["bytes_saved_pct"]
            size_change = f"{abs(delta):.1f}% {'smaller' if delta >= 0 else 'larger'}"
            text(start, row, f"{title}  vs  {LABELS[k]}", size=9)
            text(3.21, row, size_change, size=9)
            for x, (key, _) in zip(columns, checks):
                ok = g["checks"][key]
                text(x, row, "Pass" if ok else "Miss", size=8, weight="bold",
                     color=teal if ok else "#994c26", align="center")
            row += .26
    text(start, row+.02, "All five checks must pass. Time and memory checks require equal or lower results than the rival for that phase.",
         size=9, color=muted)
    text(start, row+.28, "Median speeds over seven timed pairs after warm-up. Peak RSS shows encode / decode. Each speed segment measures one phase.",
         size=8, color=muted)
    fig.savefig(output, dpi=155, facecolor="white")
    plt.close(fig)


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("receipts", type=Path)
    cli.add_argument("output", type=Path)
    cli.add_argument("--sampling", type=Path, help="FASTA selector receipts (default sibling v110-sampling)")
    args = cli.parse_args()
    if args.receipts.is_file():
        data = json.loads(args.receipts.read_text())["datasets"]
        args.output.mkdir(parents=True, exist_ok=True)
        draw(data, args.output/"comparison.png")
        return
    data = {}
    for name, title, description in DATASETS + OTHER_DATASETS:
        path = args.receipts/(name+".json")
        optional = (name, title, description) in OTHER_DATASETS
        if optional and not path.exists():
            continue
        result = read_dataset(path, name, title, description, optional)
        if result:
            data[name] = result
    freeze = json.loads((args.receipts/"freeze.json").read_text())
    for name, dataset in data.items():
        verify_binaries(name, dataset["codecs"], freeze["binaries"])
        if "openzl" in dataset["codecs"]:
            rows = dataset["codecs"]
            dataset["opt_in_gates"] = {r: gate(rows["frontier"], rows[r]) for r in ("zstd", "openzl")}
    if data["eia"]["openzl_graph_sha256_at_report"] != freeze["graph_sha256"]:
        raise ValueError("Frozen EIA graph changed")
    args.output.mkdir(parents=True, exist_ok=True)
    draw(data, args.output/"comparison.png")
    sampling = read_sampling(args.sampling or args.receipts.parent/"v110-sampling", freeze)
    evidence = {"freeze": freeze, "datasets": data, "sampling": sampling}
    (args.output/"measurements.json").write_text(json.dumps(evidence, indent=2)+"\n")


if __name__ == "__main__":
    main()
