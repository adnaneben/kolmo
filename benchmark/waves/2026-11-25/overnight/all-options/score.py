"""Verify and score the paired all-options file/archive replay."""
import argparse
import csv
import hashlib
import json
import random
import re
import statistics
from collections import defaultdict
from pathlib import Path

ARMS = ("pre", "main", "main-AA", "combined")
PHASES = ("encode", "decode", "combined")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def phase(row, name):
    if name == "combined":
        return int(row["encode_ns"]) + int(row["decode_ns"])
    return int(row[name + "_ns"])


def verify(root, manifest):
    path = root / "raw.csv"
    expected_hash = manifest.get("build_provenance_evidence", {}).get("raw_csv_sha256")
    if expected_hash:
        require(hashlib.sha256(path.read_bytes()).hexdigest() == expected_hash, "raw.csv changed")
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    settings = [tuple(c) for c in manifest["settings"]]
    rounds = manifest["rounds"]
    require(rounds >= 1 and len(set(settings)) == len(settings), "invalid round count or duplicate settings")
    require(set(manifest["binaries"]) == set(ARMS), "wrong executable set")
    require(manifest["binaries"]["main"] == manifest["binaries"]["main-AA"], "A/A binary mismatch")
    rng = random.Random(manifest["randomization_seed"])
    expected = []
    for repeat in range(rounds + 1):
        cells = list(settings)
        rng.shuffle(cells)
        for cell in cells:
            labels = list(ARMS)
            rng.shuffle(labels)
            expected.extend((cell, repeat, order, arm) for order, arm in enumerate(labels))
    require(len(rows) == len(expected), "incomplete or oversized timing matrix")
    groups = defaultdict(dict)
    for row, (cell, repeat, order, arm) in zip(rows, expected):
        key = (row["source"], row["level"], int(row["threads"]))
        require((key, int(row["repeat"]), int(row["order"]), row["arm"]) == (cell, repeat, order, arm),
                "row does not match the preregistered seeded order")
        source = manifest["sources"][cell[0]]
        require(row["sha256"] == source["input_sha256"] and int(row["input_bytes"]) == source["input_bytes"],
                "raw source hash or size differs from the manifest")
        require(re.fullmatch(r"[0-9a-f]{64}", row["frame_sha256"]) and int(row["stored_bytes"]) > 0,
                "invalid frame digest or size")
        require(all(phase(row, p) > 0 for p in PHASES), "invalid timing")
        groups[cell][arm, repeat] = row
    for (source, _, _), group in groups.items():
        frames = {}
        for arm in ARMS:
            samples = [group[arm, i] for i in range(rounds + 1)]
            frames[arm] = {(r["frame_sha256"], r["stored_bytes"], r["codecs"]) for r in samples}
            require(len(frames[arm]) == 1, "stored frame or codec counts changed between repetitions")
        require(frames["pre"] == frames["main"] == frames["main-AA"], "baseline/control frames differ")
        if source != "owid-co2.csv":
            require(frames["combined"] == frames["main"], "unexpected non-table frame change")
    return groups


def score(groups, reps):
    records, paired = [], []
    for (source, level, threads), group in sorted(groups.items()):
        fields = dict(source=source, level=level, threads=threads)
        for arm in ARMS:
            cells = [group[arm, i] for i in range(1, reps + 1)]
            first = cells[0]
            size, n = int(first["stored_bytes"]), int(first["input_bytes"])
            entry = dict(**fields, arm=arm, input_bytes=n, stored_bytes=size, ratio=n/size,
                         frame_sha256=first["frame_sha256"], frame_codec_counts=first["codecs"])
            for p in PHASES:
                entry[p + "_MBps"] = n*1000/statistics.median(phase(row, p) for row in cells)
                if p != "combined":
                    entry[p + "_peak_rss_median_B"] = statistics.median(int(row[p+"_rss_B"]) for row in cells)
            records.append(entry)
        for left, right in (("pre", "main"), ("pre", "combined"), ("main", "combined"),
                            ("main-AA", "combined"), ("main", "main-AA")):
            result = dict(**fields, baseline=left, candidate=right, paired_reps=reps,
                          size_delta_bytes=int(group[right, 0]["stored_bytes"])-int(group[left, 0]["stored_bytes"]))
            for p in PHASES:
                gains = [phase(group[left, i], p)/phase(group[right, i], p)-1 for i in range(1, reps+1)]
                result[p+"_pct"] = 100*statistics.median(gains)
                result[p+"_wins"] = sum(x > 0 for x in gains)
            paired.append(result)
    return records, paired


def write_report(root, manifest, groups, records, pairs):
    for name, rows in (("score.csv", records), ("paired.csv", pairs)):
        with (root/name).open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=rows[0])
            writer.writeheader()
            writer.writerows(rows)
    reps = manifest["rounds"]
    cross = len(groups)*len(ARMS)
    total = cross*(reps+1)
    builds = manifest.get("build_provenance", {})
    def commit(arm):
        value = builds.get(arm, {}).get("source_commit")
        return f"`{value}`" if value else "source revision unavailable"
    lines = ["# All-options build against historical and qualified main", "",
             f"The four arms are historical main {commit('pre')}, qualified main {commit('main')}, "
             f"a repeat of the same qualified-main executable (A/A), and the opt-in integrated build {commit('combined')}. "
             "Source revisions and build options were supplied from build records. The harness measured the "
             "actual executable digests and the source and input byte hashes, which are in manifest.json.", "",
             f"Each of the {len(groups)} workload, level and thread settings had one warmup and {reps} scored "
             f"rounds per arm. raw.csv contains {total} timed encode/restore rows ({cross} warmup and {cross*reps} scored). "
             f"The runner wrote each warmup row after an additional byte-exact readback with qualified main, "
             f"giving {cross} additional successful checks. These additional readbacks have no separate timing rows. "
             "Archive trials also required the saved file mode and modification time on timed restores.", "",
             "The archive workload is the complete enwik8 ZIP as one member. Ordinary file inputs are the full "
             "OWID CO₂ table, the first 65,536 bytes of the supplied prose file, and the whole Pydub WAV. "
             "Only the listed levels and requested thread counts were tested.", "",
             "| Input | Level | Threads | Stored B pre / main / combined | Encode MB/s pre / main / combined | Decode MB/s pre / main / combined | Combined MB/s pre / main / combined | Paired combined gain vs main / AA | A/A change |",
             "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    flags = builds.get("combined", {}).get("effective_codec_flags", [])
    if flags:
        lines[4:4] = [
            "The recorded combined build uses " + " ".join(f"`{f}`" for f in flags) +
            ". The runner set `KOLMO_AUDIO_SCREEN=1`. The learning tool was outside the timed CLI path.", ""
        ]
    for cell in sorted(groups):
        subset = [r for r in records if (r["source"], r["level"], r["threads"]) == cell]
        by_arm = {r["arm"]:r for r in subset}
        by_pair = {(r["baseline"], r["candidate"]):r for r in pairs
                   if (r["source"], r["level"], r["threads"]) == cell}
        def values(field, fmt):
            return " / ".join(format(by_arm[a][field], fmt) for a in ("pre", "main", "combined"))
        def pct(a, b):
            return f"{by_pair[a, b]['combined_pct']:+.1f}%"
        s, level, threads = cell
        lines.append(f"| {s} | {level} | {threads} | {values('stored_bytes', ',')} | "
                     f"{values('encode_MBps', '.2f')} | {values('decode_MBps', '.2f')} | "
                     f"{values('combined_MBps', '.2f')} | {pct('main','combined')} / {pct('main-AA','combined')} | "
                     f"{pct('main','main-AA')} |")
    lines += ["", "Decimal MB/s uses original bytes and wall time including CLI startup and file I/O. Combined speed uses "
              "the median of each observed encode-plus-decode duration. paired.csv reports median within-round gains "
              "and win counts. score.csv retains per-arm phase rates and peak RSS medians. These are warm-cache measurements "
              "on a shared host. Worker occupancy, p95 latency and native x86 performance were outside this run.", "",
              "Use the shared [replay](../all-options/replay.py) with --reference pointing at this manifest, --verify-reference, the three binary paths, "
              "--archive, --csv, --prose, --audio and a fresh --out directory. Run the shared [scorer](../all-options/score.py) with --data-dir on that directory."]
    verification = manifest.get("post_run_verification")
    if verification:
        lines += ["", f"A separate hash-pinned replay reproduced all {verification['reproduced_frame_sizes_hashes_and_codec_counts']} "
                  f"arm/setting frame hashes, stored sizes and codec counts. Its {verification['timed_restores']} timed restores and "
                  f"{verification['additional_stock_main_readbacks']} additional qualified-main readbacks passed. "
                  "The table above uses only the original seven-round measurements."]
    (root/"README.md").write_text("\n".join(lines)+"\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=Path(__file__).resolve().parent)
    directory = parser.parse_args().data_dir
    manifest = json.loads((directory/"manifest.json").read_text())
    groups = verify(directory, manifest)
    write_report(directory, manifest, groups, *score(groups, manifest["rounds"]))
