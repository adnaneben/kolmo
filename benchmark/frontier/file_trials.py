"""Time verified file-to-file pairs from an explicit executable + argument manifest."""
import argparse
import fcntl
import json
import random
import sys
import tempfile
from pathlib import Path
from statistics import median

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "holdout"))
from common import digest, measure, save


def render(command, paths):
    return [str(paths.get(arg, arg)) for arg in command]


def trial(source, expected, commands, work, lock):
    with tempfile.TemporaryDirectory(dir=work) as d:
        frame, decoded = Path(d)/"frame", Path(d)/"decoded"
        paths = {"{src}": source, "{frame}": frame, "{out}": decoded}
        with lock.open("a") as f:
            fcntl.flock(f, fcntl.LOCK_EX)
            et = measure(render(commands["encode"], paths))
            dt = measure(render(commands["decode"], paths))
            recovered = digest(decoded)
            return dict(size=frame.stat().st_size, frame_sha256=digest(frame), decoded_sha256=recovered,
                        exact=(recovered == expected), encode=et, decode=dt)


def run(source, codecs, work, dest, rounds=7):
    expected = digest(source)
    hashes = {p: digest(Path(p)) for commands in codecs.values() for p in
              (commands["encode"][0], commands["decode"][0])}
    result = dict(source_bytes=source.stat().st_size, source_sha256=expected,
                  binary_hashes=hashes, commands=codecs, rounds=rounds, trials=[])
    if dest.exists():
        previous = json.loads(dest.read_text())
        if any(previous[k] != result[k] for k in result if k != "trials"):
            raise ValueError(f"Input, executable or protocol changed since {dest}")
        result["trials"] = previous["trials"]
    finished = {(r["codec"], r["repeat"]) for r in result["trials"]}
    rng = random.Random(7610382)
    for rep in range(rounds + 1):
        order = list(codecs)
        rng.shuffle(order)
        for codec in order:
            if (codec, rep) in finished:
                continue
            try:
                row = trial(source, expected, codecs[codec], work, work/"timing.lock")
            except (OSError, RuntimeError) as exc:
                row = dict(exact=False, error=str(exc))
            result["trials"].append(dict(codec=codec, repeat=rep, **row))
            save(dest, result)
    result["metrics"] = {}
    for k in codecs:
        measured = [r for r in result["trials"] if r["codec"] == k and r["repeat"]]
        if (all(r["exact"] for r in result["trials"] if r["codec"] == k)
                and len(measured) == rounds and len({r["size"] for r in measured}) == 1):
            result["metrics"][k] = dict(size=measured[0]["size"],
                encode_ms=1000*median(r["encode"]["seconds"] for r in measured),
                decode_ms=1000*median(r["decode"]["seconds"] for r in measured),
                encode_rss=max(r["encode"]["peak_rss_bytes"] for r in measured),
                decode_rss=max(r["decode"]["peak_rss_bytes"] for r in measured))
    save(dest, result)
    print(source.name, result["metrics"], flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "commands", "work", "dest"):
        p.add_argument(name, type=Path)
    p.add_argument("--rounds", type=int, default=7)
    a=p.parse_args()
    run(a.source.resolve(), json.loads(a.commands.read_text()), a.work.resolve(), a.dest.resolve(), a.rounds)


if __name__ == "__main__":
    main()
