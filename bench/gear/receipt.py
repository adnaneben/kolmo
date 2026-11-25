"""Record executable/corpus identities and reduce the existing archive replay's matched trials."""
import csv
import hashlib
import json
import os
import platform
import statistics
import sys
from pathlib import Path

# The existing native-gear replay has one duplicate baseline and two further program arguments.
ARMS = {"main": "candidate-c", "main-AA": "candidate-c-AA", "asm": "candidate-asm",
        "paired-C": "pinned-main"}
CORPORA = {
    "enwik8.zip": (36445475, "547994d9980ebed1288380d652999f38a14fe291a6247c157c3d33d4932534bc"),
    "mixed.bin.L3.kolmo": (650827, "c7c3fc575fc8f17fb2293a6ac674347327c4a00b86ccf7b2101d2a2a00d0ca8d"),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(work, output, candidate, pin, filename, origin):
    src = Path(filename)
    size, expected = CORPORA[src.name]
    if src.stat().st_size != size or digest(src) != expected:
        raise ValueError(f"Source size or SHA-256 differs for {src.name}")
    versions = {}
    for variant, arm in (("main", "pinned-main"), ("c", "candidate-c"), ("asm", "candidate-asm")):
        versions[arm] = {"revision": pin if variant == "main" else candidate,
                         "binary_sha256": digest(work / variant / "build/kolmo"),
                         "gear_asm": variant == "asm"}
    versions["candidate-c-AA"] = versions["candidate-c"]
    metadata = {
        "programs": versions, "corpus": {"name": src.name, "bytes": size, "sha256": expected, "origin": origin},
        "operation": "archive one whole member; extract and byte-compare all timed arms",
        "level": 1, "threads": 4, "member_mode": "0644", "member_mtime_unix": 1600000000,
        "warmup_rounds": 1, "scored_rounds": 11, "shuffle_seed": 20261125173,
        "combined": "original decimal MB / elapsed (encode + full extract) seconds",
        "hardware": platform.uname()._asdict(), "rss_unit": "bytes" if sys.platform == "darwin" else "KiB",
        "replay_sha256": digest(work / "c/benchmark/waves/2026-11-25/native-gear/replay.py"),
        "runner_image": os.getenv("ImageVersion"), "runner_os": os.getenv("RUNNER_OS"),
        "run_id": os.getenv("GITHUB_RUN_ID"), "run_attempt": os.getenv("GITHUB_RUN_ATTEMPT"),
        "repository": os.getenv("GITHUB_REPOSITORY"), "ref": os.getenv("GITHUB_REF"),
        "compiler": os.getenv("CC", "cc"),
        "comparison": "candidate-c-AA and candidate-c are the same executable; candidate-asm enables only GEAR_ASM",
        "limits": "One host, archive operation, single complete member, L1/T4, shared runner and warmed file cache.",
    }
    (output / "manifest.json").write_text(json.dumps(metadata, indent=2) + "\n")


def phase(row, name):
    if name == "combined":
        return float(row["encode_ms"]) + float(row["decode_ms"])
    return float(row[name + "_ms"])


def score(work, output):
    meta = json.loads((output / "manifest.json").read_text())
    with (work / "legacy-rows.csv").open() as source:
        rows = list(csv.DictReader(source))
    rounds = meta["scored_rounds"]
    assert len(rows) == (rounds + 1) * len(ARMS)
    grouped = {arm: {} for arm in ARMS.values()}
    frames = set()
    for i, row in enumerate(rows):
        arm = row["arm"] = ARMS[row["arm"]]
        rep = int(row["round"])
        assert rep == (i // len(ARMS)) - 1 and rep not in grouped[arm]
        assert row["source"] == meta["corpus"]["name"] and int(row["size"]) == meta["corpus"]["bytes"]
        assert row["sha256"] == meta["corpus"]["sha256"] and int(row["threads"]) == meta["threads"]
        assert min(phase(row, field) for field in ("encode", "decode")) > 0
        row["order"] = i % len(ARMS)
        grouped[arm][rep] = row
        frames.add((int(row["archive_bytes"]), row["archive_sha256"]))
    assert len(frames) == 1 and all(len(samples) == rounds + 1 for samples in grouped.values())
    size, frame = frames.pop()
    with (output / "rounds.csv").open("w", newline="") as dest:
        writer = csv.DictWriter(dest, fieldnames=rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    phases = ("encode", "decode", "combined")
    lines = ["# Native Gear archive comparison", "", f"Corpus: `{meta['corpus']['name']}`"
             f" ({meta['corpus']['bytes']:,} bytes, {meta['corpus']['origin']}). "
             f"All {len(rows)} archives used {size:,} bytes, SHA-256 `{frame}`. "
             "Every timed extract restored the complete original.",
             "", "| Executable | Encode MB/s | Decode MB/s | Combined MB/s |", "|---|---:|---:|---:|"]
    for arm, samples in grouped.items():
        rates = [meta["corpus"]["bytes"] / 1000 /
                 statistics.median(phase(samples[i], field) for i in range(rounds)) for field in phases]
        lines.append(f"| {arm} | " + " | ".join(f"{rate:.2f}" for rate in rates) + " |")
    lines += ["", "Median paired throughput change. Positive values favor the compared executable.",
              "", "| Comparison | Encode | Decode | Combined | Combined wins |", "|---|---:|---:|---:|---:|"]
    for control, arm in (("candidate-c", "candidate-asm"), ("candidate-c-AA", "candidate-asm"),
                         ("pinned-main", "candidate-asm"), ("candidate-c", "candidate-c-AA")):
        gains = []
        for field in phases:
            changes = [phase(grouped[control][i], field) / phase(grouped[arm][i], field) - 1
                       for i in range(rounds)]
            gains.append(statistics.median(changes))
        lines.append(f"| {arm} vs {control} | " + " | ".join(f"{gain:+.2%}" for gain in gains) +
                     f" | {sum(value > 0 for value in changes)}/{rounds} |")
    lines += ["", "The same-binary comparison measures run-to-run variation. Each phase includes process startup "
              "and file I/O. Raw wall times, CPU time and RSS are in rounds.csv. Sources, executables, input, build "
              "options and platform are identified in manifest.json and the build/environment logs."]
    (output / "summary.md").write_text("\n".join(lines) + "\n")
    files = sorted(f for f in output.iterdir() if f.is_file() and f.name != "checksums.sha256")
    (output / "checksums.sha256").write_text("".join(f"{digest(f)}  {f.name}\n" for f in files))


if __name__ == "__main__":
    command, work, output, *rest = sys.argv[1:]
    if command == "prepare":
        prepare(Path(work), Path(output), *rest)
    elif command == "score" and not rest:
        score(Path(work), Path(output))
    else:
        raise SystemExit("expected prepare or score")
