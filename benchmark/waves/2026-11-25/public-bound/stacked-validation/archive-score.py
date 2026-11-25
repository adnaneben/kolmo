"""Summarize archive-speed.py timings in decimal MB/s and matched-pair gains."""
import argparse
import csv
import statistics
from pathlib import Path


def phase(row, field):
    if field == "combined":
        return row["encode_ns"] + row["decode_ns"]
    return row[field + "_ns"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    opts = parser.parse_args()
    with opts.csv.open() as f:
        data = list(csv.DictReader(f))
    arms = ("base", "base-AA", "stack")
    keyed = {arm: {} for arm in arms}
    assert len(data) == 3 * 22
    assert len({r["frame_sha256"] for r in data}) == 1
    assert len({r["input_sha256"] for r in data}) == 1
    n = int(data[0]["input_bytes"])
    for row in data:
        rep = int(row["repeat"])
        assert rep not in keyed[row["arm"]]
        keyed[row["arm"]][rep] = {k: int(row[k]) for k in ("encode_ns", "decode_ns")}
    for rounds in keyed.values():
        assert set(rounds) == set(range(22))
    print("arm,encode_MBps,decode_MBps,combined_MBps")
    for arm in arms:
        rates = []
        for field in ("encode", "decode", "combined"):
            times = [phase(keyed[arm][i], field) for i in range(1, 22)]
            rates.append(n * 1000 / statistics.median(times))
        print(arm, *(f"{rate:.2f}" for rate in rates), sep=",")
    print("control,compared,paired_encode_pct,paired_decode_pct,paired_combined_pct,combined_wins")
    for a, b in (("base", "stack"), ("base-AA", "stack"), ("base", "base-AA")):
        gains = []
        for field in ("encode", "decode", "combined"):
            changes = [100 * (phase(keyed[a][i], field) / phase(keyed[b][i], field) - 1)
                       for i in range(1, 22)]
            gains.append(statistics.median(changes))
        print(a, b, *(f"{gain:+.2f}" for gain in gains), sum(v > 0 for v in changes), sep=",")


if __name__ == "__main__":
    main()
