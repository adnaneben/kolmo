"""Supplementary in-memory lzbench comparison on the holdout and Silesia."""
import argparse
import csv
import hashlib
import io
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("lzbench", "portugal", "silesia"):
        parser.add_argument(key, type=Path)
    args = parser.parse_args()
    freeze = json.loads((HERE / "freeze.json").read_text())
    reference = json.loads((HERE.parent / "holdout/release-runs.json").read_text())["archive/fast"]
    assert sha(args.portugal) == freeze["inputs"]["test_sha256"]
    assert sha(args.silesia) == reference["input_sha256"]
    expected = {"portugal":json.loads((HERE/"decision.json").read_text())["metrics"]["candidate"]["size"],
                "silesia":reference["frames"]["kolmo"]["bytes"]}
    result = dict(harness_sha256=sha(args.lzbench), shared_library_sha256=sha(ROOT/"build/libkolmo.dylib"),
                  upstream_commit="d68cdbc2309408e82da881c61d23cf70cca8db09", samples={})
    # Same worker counts, whole-file input, five iterations, median, warm page cache.
    for name, source, selection in [("portugal",args.portugal,"kolmo,1/zstd,3"),
                                    ("silesia",args.silesia,"zstd,3/kolmo,1")]:
        flags = ["-e"+selection,"-I1","-T1","-t0,0","-i5,5","-p3","-o4","-q"]
        run = subprocess.run([args.lzbench.resolve(),*flags,source.resolve()], cwd=ROOT,
                             text=True,capture_output=True,check=True)
        rows = list(csv.DictReader(io.StringIO(run.stdout)))
        assert len(rows) == 2 and all(int(r["Original size"]) == source.stat().st_size for r in rows)
        kolmo = next(r for r in rows if r["Compressor name"].startswith("kolmo"))
        assert int(kolmo["Compressed size"]) == expected[name]
        for r in rows:
            assert float(r["Compression speed"]) > 0 and float(r["Decompression speed"]) > 0
            r["Filename"] = source.name
        result["samples"][name] = dict(flags=flags,source_sha256=sha(source),rows=rows)
        print(name,rows,flush=True)
    (HERE/"lzbench.json").write_text(json.dumps(result,indent=2)+"\n")


if __name__ == "__main__":
    main()
