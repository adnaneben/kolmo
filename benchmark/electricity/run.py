"""Freeze and score the OPSD electricity holdout. See protocol.md."""
import argparse
import csv
import json
import platform
import random
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "holdout"))
from common import digest, measure, save

URL = "https://data.open-power-system-data.org/time_series/2020-10-06/time_series_60min_singleindex.csv"
MERGED = "5bea3557ccb98af100f8fb8f24b3720cf44a641e"


def prepare(source, work):
    with source.open("rb") as f:
        rows = sum(1 for _ in f) - 1
    with source.open(encoding="utf-8-sig", newline="") as f:
        if sum(1 for _ in csv.reader(f, strict=True)) != rows + 1:
            raise ValueError("Input must have one complete CSV record per line")
    train, test = work / "train.csv", work / "test.csv"
    train_rows = 0
    with source.open("rb") as f, train.open("wb") as a, test.open("wb") as b:
        header = next(f)
        a.write(header)
        b.write(header)
        for i, line in enumerate(f):
            if i >= rows // 4:
                b.write(line)
            elif i == train_rows and a.tell() + len(line) <= 4 * 1024**2:
                a.write(line)
                train_rows += 1
    return dict(url=URL, rows=rows, source_sha256=digest(source), train_rows=train_rows,
                train_sha256=digest(train), train_bytes=train.stat().st_size,
                test_rows=rows - rows // 4, test_sha256=digest(test), test_bytes=test.stat().st_size)


def commands(codec, exe, source, frame, output, graph):
    if codec == "zstd":
        return ([exe, "-q", "--single-thread", "-3", "-o", frame, source],
                [exe, "-q", "--single-thread", "-d", "-o", output, frame])
    if codec == "openzl":
        return ([exe, "compress", "--compressor", graph, source, "--output", frame],
                [exe, "decompress", frame, "--output", output])
    return ([exe, "-m", "fast", "-T", "1", source, frame], [exe, "-d", "-T", "1", frame, output])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["freeze", "score"])
    parser.add_argument("source", type=Path)
    parser.add_argument("work", type=Path)
    for name in ("candidate", "merged", "zstd", "openzl"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    files = {name: getattr(args, name).resolve() for name in ("candidate", "merged", "zstd", "openzl")}
    hashes = {name: digest(path) for name, path in files.items()}
    protocol_hash = digest(HERE / "protocol.md")
    manifest = HERE / "freeze.json"
    if args.action == "freeze":
        if manifest.exists():
            raise FileExistsError("The candidate and data have already been frozen")
        inputs = prepare(args.source, args.work)
        save(manifest, dict(protocol_sha256=protocol_hash, runner_sha256=digest(Path(__file__)),
                            binaries=hashes, merged_commit=MERGED,
                            candidate_commit=subprocess.check_output(["git", "rev-parse", "HEAD"],
                                                                     cwd=HERE, text=True).strip(),
                            system=platform.platform(), inputs=inputs))
        return
    frozen = json.loads(manifest.read_text())
    assert hashes == frozen["binaries"] and protocol_hash == frozen["protocol_sha256"]
    assert frozen["runner_sha256"] == digest(Path(__file__))
    inputs = frozen["inputs"]
    # A clean replay obtains the same partitions from the original download.
    assert prepare(args.source, args.work) == inputs
    train, source = args.work / "train.csv", args.work / "test.csv"
    expected = inputs["test_sha256"]
    trials = []
    with tempfile.TemporaryDirectory(dir=args.work) as temp:
        temp = Path(temp)
        graph = temp / "opsd.zlc"
        training = measure([files["openzl"], "train", "--profile", "csv", "--threads", "4",
                            "--max-time-secs", "10", train, "--output", graph])
        graph_data = dict(sha256=digest(graph), bytes=graph.stat().st_size, **training)
        # Retain the exact trained encoder graph for independent replay.
        (args.work / "trained.zlc").write_bytes(graph.read_bytes())
        rng = random.Random(42571)
        for repeat in range(8):
            codecs = list(files)
            rng.shuffle(codecs)
            for codec in codecs:
                frame, out = temp / "frame", temp / "decoded"
                enc, dec = commands(codec, files[codec], source, frame, out, graph)
                et, dt = measure(enc), measure(dec)
                restored = digest(out)
                assert restored == expected
                trials.append(dict(codec=codec, repeat=repeat, compressed=frame.stat().st_size,
                                   encoded_sha256=digest(frame), decoded_sha256=restored, encode=et, decode=dt))
                out.unlink()
                frame.unlink()
            print("Verified", "warm-up" if not repeat else f"round {repeat}/7", flush=True)
    for codec in files:
        assert len({t["encoded_sha256"] for t in trials if t["codec"] == codec}) == 1
    save(HERE / "trials.json", dict(training=graph_data, trials=trials))


if __name__ == "__main__":
    main()
