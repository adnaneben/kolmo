#!/usr/bin/env python3
"""Interleaved A/B/A' check: is a speed change between two Kolmo binaries larger than the noise?

For each input and level, REPS rounds; each round runs every arm once (encode then decode),
and the arm order rotates from round to round (A B A2, B A2 A, A2 A B, ...). A2 is the
first binary run as a separate arm, so the A/A2 spread measures the noise directly.
Each round runs only at 1-minute load < --max-load (read at the start and the end of the
round; a round that ends at or above it is discarded and re-run). Every decode is checked
by SHA-256. Rows go to a checkpointed JSONL (one row per round); a resumed run skips
rounds already recorded.

Run it inside the timing lock (wrapped in timed.sh); it does not take the lock itself.

  python3 aa.py --corpus $V/impact/corpus --out rows/aa.jsonl --arm A=bin1 --arm B=bin2 \
      --levels 5,6,7 [--inputs dickens,api] [--reps 5]
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from cell import run, sha256  # noqa: E402
from run import load_inputs  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--set", default="dev")
    ap.add_argument("--out", required=True)
    ap.add_argument("--arm", action="append", required=True, help="NAME=binary; the first is repeated as NAME2")
    ap.add_argument("--levels", default="5,6,7")
    ap.add_argument("--inputs", default="", help="comma list of input names (default: the whole set)")
    ap.add_argument("--reps", type=int, default=5)
    ap.add_argument("--max-load", type=float, default=12.0)
    ap.add_argument("--work", required=True)
    ap.add_argument("--no-repeat", action="store_true", help="do not add the repeated first arm")
    a = ap.parse_args()

    arms = []
    for s in a.arm:
        n, p = s.split("=", 1)
        arms.append((n, os.path.abspath(p)))
    if not a.no_repeat:
        arms.append((arms[0][0] + "2", arms[0][1]))
    shas = {n: sha256(p) for n, p in arms}
    inputs = load_inputs(a.corpus, a.set)
    if a.inputs:
        keep = a.inputs.split(",")
        inputs = [i for i in inputs if i["name"] in keep]
    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            r = json.loads(line)
            done.add((r["input"], r["level"], r["round"]))
    env = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}
    os.makedirs(a.work, exist_ok=True)
    cbin, rbin = os.path.join(a.work, "c.bin"), os.path.join(a.work, "rt.bin")

    def log(m):
        print(time.strftime("%H:%M:%S"), m, flush=True)

    for inp in inputs:
        for lv in a.levels.split(","):
            lvarg = ["--level", "ultra"] if lv == "ultra" else ["-" + lv]
            for rnd in range(a.reps):
                if (inp["name"], lv, rnd) in done:
                    continue
                k = rnd % len(arms)
                order = arms[k:] + arms[:k]
                while True:
                    while os.getloadavg()[0] >= a.max_load:
                        time.sleep(60)
                    l0 = os.getloadavg()[0]
                    res = {}
                    for name, binary in order:
                        for p in (cbin, rbin):
                            if os.path.exists(p):
                                os.remove(p)
                        e = run([binary, "-q", "-T", "1", *lvarg, "-c", inp["path"]], None, cbin, a.work, env)
                        d = run([binary, "-q", "-d", "-T", "1", "-c", cbin], None, rbin, a.work, env)
                        ok = e[1] == 0 and d[1] == 0 and sha256(rbin) == inp["sha256"]
                        res[name] = dict(enc_ns=e[0], dec_ns=d[0], enc_rss=e[2], dec_rss=d[2],
                                         comp_bytes=os.path.getsize(cbin), comp_sha=sha256(cbin), rt_ok=ok)
                        if not ok:
                            sys.exit(f"round trip failed: {name} L{lv} {inp['name']}")
                    l1 = os.getloadavg()[0]
                    if l1 < a.max_load:
                        break
                    log(f"discarded {inp['name']} L{lv} round {rnd}: load {l1:.1f}")
                row = dict(set=a.set, group=inp["group"], input=inp["name"], input_bytes=inp["bytes"],
                           input_sha256=inp["sha256"], level=lv, round=rnd, order=[n for n, _ in order],
                           arms={n: dict(binary=p, binary_sha256=shas[n], **res[n]) for n, p in arms},
                           load1_before=round(l0, 2), load1_after=round(l1, 2),
                           utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
                with open(a.out, "a") as f:
                    f.write(json.dumps(row) + "\n")
                log(f"{inp['name']:<16} L{lv} r{rnd} " + " ".join(
                    f"{n}:{res[n]['enc_ns'] / 1e9:.3f}s" for n, _ in arms) + f" load {l0:.1f}/{l1:.1f}")
    shutil.rmtree(a.work, ignore_errors=True)


if __name__ == "__main__":
    main()
