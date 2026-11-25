"""Verify complete repetitions and each round trip, then emit median paired-run results."""
import csv
import json
from collections import defaultdict
from pathlib import Path
from statistics import median

root = Path(__file__).resolve().parent
inputs = [
    "zstd-regular", "zstd-ultra-and-time", "openzl-csv", "openzl-trained-holdout"
]
columns = [
    "suite", "file", "codec", "level", "runs", "bytes", "input_bytes",
    "encode_MBps", "decode_MBps", "combined_MBps", "encode_CPU_ms", "decode_CPU_ms",
    "encode_RSS_bytes", "decode_RSS_bytes", "input_sha256", "frame_sha256"
]


def summarize(rows):
    first = rows[0]
    n = first["n"]
    assert sorted(r["rep"] for r in rows) == list(range(8))
    for k in ("frame_hash", "bytes", "n"):
        assert len({r[k] for r in rows}) == 1
    scored = [r for r in rows if r["rep"] > 0]
    def middle(fn, digits=3):
        return round(median(fn(r) for r in scored), digits)
    return [
        len(scored), first["bytes"], n,
        middle(lambda r: n / r["enc"] / 1e6), middle(lambda r: n / r["dec"] / 1e6),
        middle(lambda r: n / (r["enc"] + r["dec"]) / 1e6),
        middle(lambda r: r["encode_cpu"] * 1000), middle(lambda r: r["decode_cpu"] * 1000),
        middle(lambda r: r["encode_rss"], 0), middle(lambda r: r["decode_rss"], 0),
        first.get("source_hash", first.get("raw_hash")), first["frame_hash"]
    ]


with (root / "competitor-results.csv").open("w", newline="") as output:
    writer = csv.writer(output)
    writer.writerow(columns)
    for suite in inputs:
        groups = defaultdict(list)
        for line in (root / (suite + ".jsonl")).read_text().splitlines():
            r = json.loads(line)
            assert r.get("source_hash", r.get("raw_hash")) == r.get("restored_hash", r.get("decoded_hash"))
            if r.get("stage") != "screen":
                groups[(r["file"], r["codec"], r["level"])].append(r)
        for (name, codec, level), rows in sorted(groups.items()):
            writer.writerow([suite, name, codec, level, *summarize(rows)])
