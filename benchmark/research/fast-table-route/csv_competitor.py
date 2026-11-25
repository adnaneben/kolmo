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
parser.add_argument("--ghcn", action="store_true", help="Use the graph trained on four disjoint GHCN stations")
args = parser.parse_args()

files = [(name, args.corpus / location) for name, location in tables.items()]
settings = [("typed", 1), ("typed", 2), ("csv", 1), ("csv", 6), ("csv", 12)]
if args.ghcn:
    folder = args.corpus / "bench-page/data/holdout/ghcn-csv/test"
    files = [(path.stem, path) for path in sorted(folder.glob("*.csv"))]
    if len(files) != 4:
        parser.error(f"Expected four held-out station CSV files in {folder}")
    settings = [("typed", 1), ("typed", 2), ("csv", 1), ("trained-csv", 6)]

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
                if codec == "typed":
                    encode = [args.candidate, f"-{level}", "-T", 1, "-f", "-o", frame, source]
                    decode = [args.main, "-d", "-T", 1, "-f", "-o", readback, frame]
                else:
                    model = args.corpus / "bench-page/results/work-panel/ghcn-csv-p_csv.zlc"
                    graph = ["-c", model] if codec == "trained-csv" else ["--profile", "csv"]
                    encode = [args.zli, "compress", *graph, "--strict", "--level", level, "-f", source, "-o", frame]
                    decode = [args.zli, "decompress", "--profile", "csv", "-f", frame, "-o", readback]
                # Kolmo preserves source mode. Give both CLIs fresh output paths.
                frame.unlink(missing_ok=True)
                readback.unlink(missing_ok=True)
                row = observe(source, frame, readback, expected, encode, decode, quiet=False)
                row.update(file=name, codec=codec, level=level, rep=rep,
                           source_hash=expected, restored_hash=expected)
                output.write(json.dumps(row) + "\n")
                output.flush()
