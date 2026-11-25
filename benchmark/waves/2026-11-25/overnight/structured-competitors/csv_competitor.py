"""Compare OpenZL's built-in or trained CSV graph with the typed Kolmo presets."""
import argparse
import json
import random
import tempfile
from pathlib import Path

from competitor_support import digest, observe, tables


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("corpus", type=Path)
parser.add_argument("main")
parser.add_argument("candidate")
parser.add_argument("zli")
parser.add_argument("out", type=Path)
group = parser.add_mutually_exclusive_group()
group.add_argument("--ghcn", action="store_true", help="Use the graph trained on four disjoint GHCN stations")
group.add_argument("--owid-trained", action="store_true", help="Use country-disjoint validation data from train_owid.py")
parser.add_argument("--trained-fallback", choices=tables, action="append", default=[],
                    help="Use OpenZL default fallback when a trained graph fails on this dataset")
args = parser.parse_args()

files = [(name, args.corpus / location) for name, location in tables.items()]
settings = [("typed", 1), ("typed", 2), ("csv", 1), ("csv", 6), ("csv", 12)]
if args.ghcn:
    folder = args.corpus / "bench-page/data/holdout/ghcn-csv/test"
    files = [(path.stem, path) for path in sorted(folder.glob("*.csv"))]
    if len(files) != 4:
        parser.error(f"Expected four held-out station CSV files in {folder}")
    settings = [("typed", 1), ("typed", 2), ("csv", 1), ("trained-csv", 6)]

if args.owid_trained:
    files = [(name, args.corpus / name / "test.csv") for name in tables]
    settings = [("main", 1), ("main", 2), ("typed", 1), ("typed", 2), ("csv", 1),
                ("trained-csv", 1), ("trained-csv", 6), ("trained-csv", 12)]

rng = random.Random(247511)
with tempfile.TemporaryDirectory() as tmp, args.out.open("x") as output:
    frame = Path(tmp) / "f"
    readback = Path(tmp) / "r"
    for name, source in files:
        expected = digest(source)
        for rep in range(8):
            arms = settings.copy()
            rng.shuffle(arms)
            for codec, level in arms:
                if codec in ("typed", "main"):
                    binary = args.main if codec == "main" else args.candidate
                    encode = [binary, f"-{level}", "-T", 1, "-f", "-o", frame, source]
                    decode = [args.main, "-d", "-T", 1, "-f", "-o", readback, frame]
                else:
                    use_fallback = codec == "trained-csv" and name in args.trained_fallback
                    model = args.corpus / "bench-page/results/work-panel/ghcn-csv-p_csv.zlc"
                    if args.owid_trained:
                        model = args.corpus / name / "graph.zlc"
                    graph = ["-c", model] if codec == "trained-csv" else ["--profile", "csv"]
                    strict = [] if use_fallback else ["--strict"]
                    encode = [args.zli, "compress", *graph, *strict, "--level", level, "-f", source, "-o", frame]
                    if use_fallback:
                        codec = "trained-csv-fallback"
                    decode = [args.zli, "decompress", "--profile", "csv", "-f", frame, "-o", readback]
                # Kolmo preserves source mode. Give both CLIs fresh output paths.
                frame.unlink(missing_ok=True)
                readback.unlink(missing_ok=True)
                row = observe(source, frame, readback, expected, encode, decode, quiet=False)
                row.update(file=name, codec=codec, level=level, rep=rep,
                           source_hash=expected, restored_hash=expected)
                output.write(json.dumps(row) + "\n")
                output.flush()
