"""Replay the pre-existing fast benchmarks against the frozen merge and candidate."""
import argparse
import json
import random
import tempfile
from pathlib import Path
from run import HERE, commands, digest, measure, save


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("old_workspace", type=Path, help="checkout with the original benchmark data")
    parser.add_argument("work", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("merged", type=Path)
    parser.add_argument("--evidence-dir", type=Path, default=HERE,
                        help="directory holding the candidate and baseline freeze manifest")
    args = parser.parse_args()
    freeze = json.loads((args.evidence_dir / "freeze.json").read_text())
    before = HERE.parent / "holdout"
    release = json.loads((before / "release-runs.json").read_text())
    holdout = json.loads((before / "holdout-runs.json").read_text())
    inputs = json.loads((before / "inputs.json").read_text())
    base = args.old_workspace
    data = base / "build/comparison/data"
    old = base / "build/holdout"
    sources = dict(archive=base / "benchmark/ratio/data/silesia.tar", tables=data / "table.test.csv",
                   numbers=data / "numbers.test", genome=data / "ecoli.fa", image=old / "astronaut.rgb",
                   audio=data / "digits.pcm", video=data / "foreman.yuv")
    expected = {key: (release[key + "/fast"]["input_sha256"], release[key + "/fast"]["frames"]["kolmo"]["sha256"])
                for key in sources}
    for key, row in inputs.items():
        sources[key] = old / (key + ".test")
        trial = next(t for t in holdout[key]["trials"] if t["codec"] == "kolmo")
        expected[key] = (row["test_sha256"], trial["encoded_sha256"])
    baseline = "baseline" if "baseline" in freeze["binaries"] else "merged"
    bins = {"candidate": args.candidate.resolve(), baseline: args.merged.resolve()}
    assert all(digest(exe) == freeze["binaries"][key] for key, exe in bins.items())
    assert all(digest(source) == expected[key][0] for key, source in sources.items())
    rng = random.Random(28937)
    results = {name: [] for name in sources}
    with tempfile.TemporaryDirectory(dir=args.work) as folder:
        frame, out = Path(folder) / "frame", Path(folder) / "out"
        for repeat in range(6):
            names = list(sources)
            rng.shuffle(names)
            for name in names:
                codecs = list(bins)
                rng.shuffle(codecs)
                for codec in codecs:
                    enc, dec = commands(codec, bins[codec], sources[name], frame, out, None)
                    et, dt = measure(enc), measure(dec)
                    assert (digest(out), digest(frame)) == expected[name], (name, codec)
                    results[name].append(dict(codec=codec, repeat=repeat, encode=et, decode=dt))
                    out.unlink()
                    frame.unlink()
            print("Regression round", repeat, flush=True)
    save(args.evidence_dir / "regression.json", dict(expected=expected, trials=results))


if __name__ == "__main__":
    main()
