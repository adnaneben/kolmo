#!/usr/bin/env python3
"""Validate frozen final inputs/results and regenerate aggregate evidence."""
import collections
import csv
import hashlib
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
HERE = pathlib.Path(__file__).resolve().parent
inputs = {}
for name in ("inputs.json", "validation-inputs.json"):
    for item in json.loads((HERE / name).read_text())["files"]:
        path = ROOT / item["path"]
        assert path.stat().st_size == item["bytes"], path
        assert hashlib.sha256(path.read_bytes()).hexdigest() == item["sha256"], path
        inputs[item["path"]] = item

rows = list(csv.DictReader((HERE / "final-results.csv").open()))
expected = {("krc-cpu", "1"), ("krc-cpu", "10"),
            ("kolmo-fast", "1"), ("kolmo-fast", "10"),
            ("zstd-3", "1"), ("lz4", "1"),
            ("krc-metal", "1"), ("krc-hybrid-50", "10")}
assert len(inputs) == 32 and len(rows) == 32 * len(expected)
assert {r["file"] for r in rows} == set(inputs)
groups = collections.defaultdict(list)
for path, item in inputs.items():
    selected = [r for r in rows if r["file"] == path]
    assert len(selected) == len(expected)
    assert {(r["codec"], r["cpu_threads"]) for r in selected} == expected
    assert len({r["compressed_bytes"] for r in selected
                if r["codec"].startswith("krc-")}) == 1
    for row in selected:
        assert row["verified"] == "1" and row["repeats"] == "5"
        assert int(row["input_bytes"]) == item["bytes"]
        assert int(row["compressed_bytes"]) > 0
        assert float(row["compress_mbps"]) > 0 and float(row["decode_mbps"]) > 0
        groups[(item["family"], row["codec"], row["cpu_threads"])].append(row)

with (HERE / "final-summary.csv").open("w") as out:
    writer = csv.writer(out)
    writer.writerow(["family", "codec", "cpu_threads", "files", "input_bytes",
                     "compressed_bytes", "ratio", "compress_mbps", "decode_mbps"])
    for (family, codec, threads), selected in sorted(groups.items()):
        size = sum(int(r["input_bytes"]) for r in selected)
        packed = sum(int(r["compressed_bytes"]) for r in selected)
        speed = lambda key: size / sum(int(r["input_bytes"]) / float(r[key])
                                      for r in selected)
        writer.writerow([family, codec, threads, len(selected), size, packed,
                         f"{size / packed:.6f}", f"{speed('compress_mbps'):.3f}",
                         f"{speed('decode_mbps'):.3f}"])
print("FINAL EVIDENCE PASSED: 32 frozen hashes, 256 configurations, 1280 verified runs; KRC sizes agree")
