"""Screen stock zstd settings, then repeat the nearest size and wall-time settings."""
import argparse
import json
import random
import shutil
import tempfile
from pathlib import Path

from competitor_support import digest, observe, tables


def neighboring_levels(curve, target, measure):
    """Return one setting on each side where that side contains an observation."""
    value = measure(target)
    below = [level for level, row in curve.items() if measure(row) <= value]
    above = [level for level, row in curve.items() if measure(row) > value]
    return {
        min(side, key=lambda level: abs(measure(curve[level]) - value))
        for side in (below, above) if side
    }


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("corpus", type=Path)
parser.add_argument("main")
parser.add_argument("candidate")
parser.add_argument("output", type=Path)
parser.add_argument("--files", nargs="+", choices=tables, default=list(tables))
parser.add_argument("--extend", type=Path, help="Reuse the ordinary screen and add levels 20–22 and time brackets")
args = parser.parse_args()
prior = []
if args.extend:
    prior = [json.loads(line) for line in args.extend.read_text().splitlines()]
rng = random.Random(95723)
zstd = shutil.which("zstd")
if zstd is None:
    parser.error("zstd must be on PATH")

with tempfile.TemporaryDirectory() as tmp, args.output.open("x") as output:
    frame = Path(tmp) / "frame"
    readback = Path(tmp) / "restored"
    for name in args.files:
        source = args.corpus / tables[name]
        expected = digest(source)

        def sample(codec, level, stage, rep):
            if codec == "zstd":
                ultra = ["--ultra"] if level > 19 else []
                encode = [zstd, "--single-thread", "-q", "-f", *ultra, f"-{level}", source, "-o", frame]
                decode = [zstd, "--single-thread", "-q", "-f", "-d", frame, "-o", readback]
            else:
                encode = [args.candidate, f"-{level}", "-T", 1, "-f", "-o", frame, source]
                decode = [args.main, "-d", "-T", 1, "-f", "-o", readback, frame]
            row = observe(source, frame, readback, expected, encode, decode)
            row.update(file=name, codec=codec, level=level, stage=stage, rep=rep,
                       threads=1, raw_hash=expected, decoded_hash=expected)
            output.write(json.dumps(row) + "\n")
            output.flush()
            return row

        old = [r for r in prior if r["file"] == name and r["raw_hash"] == expected and r["stage"] == "screen"]
        targets = {r["level"]: r for r in old if r["codec"] == "typed"}
        curve = {r["level"]: r for r in old if r["codec"] == "zstd"}
        for level in (1, 2):
            if level not in targets:
                targets[level] = sample("typed", level, "screen", 0)
        remaining = [level for level in range(1, 23 if args.extend else 20) if level not in curve]
        rng.shuffle(remaining)
        for level in remaining:
            curve[level] = sample("zstd", level, "screen", 0)
        selected = set()
        for target in targets.values():
            selected.update(neighboring_levels(curve, target, lambda r: r["bytes"]))
            if args.extend:
                selected.update(neighboring_levels(curve, target, lambda r: r["enc"] + r["dec"]))
        for rep in range(8):
            arms = [("zstd", level) for level in sorted(selected)] + [("typed", 1), ("typed", 2)]
            rng.shuffle(arms)
            for codec, level in arms:
                sample(codec, level, "paired", rep)
