#!/usr/bin/env python3
"""Performance-graph runner: every Kolmo level of each snapshot plus the competitors,
one thread each, on fixed data groups. Checkpointed JSONL: a cell already in the
output file (same config, input SHA-256 and binary SHA-256) is skipped, so runs resume.

  python3 run.py --corpus $V/impact/corpus --set dev --out rows/dev.jsonl \
      --kolmo A=/path/kolmo-v0.0.2 --kolmo B=/path/kolmo-main [--competitors] \
      [--timed $V/tools/timed.sh] [--max-load 12] [--reps 3]

Each cell runs under timed.sh (one timed workload on the machine at a time) only
when the 1-minute load is under --max-load. The load is read before and after the
cell; if either is at or above the limit the cell is discarded and retried later.
"""
import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from configs import COMPETITORS, kolmo_configs  # noqa: E402


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def tool_version(cfg):
    exe = cfg["enc"][0]
    if exe == "{kolmo}":
        return None
    for args in (["--version"], ["-V"], []):
        try:
            out = subprocess.run([exe, *args], capture_output=True, text=True, timeout=10)
            line = (out.stdout or out.stderr).strip().splitlines()
            if line:
                return line[0][:120]
        except Exception:
            pass
    return None


def load_inputs(corpus, which):
    groups = json.load(open(os.path.join(HERE, "groups.json")))
    man = {}
    for line in open(os.path.join(corpus, "manifest.jsonl")):
        r = json.loads(line)
        if r["set"] == which:
            man[r["name"]] = r
    inputs = []
    for g in groups["order"]:
        for name in groups[which][g]:
            r = man[name]
            path = os.path.join(corpus, r["file"])
            got = sha256(path)
            if got != r["sha256"] or os.path.getsize(path) != r["bytes"]:
                sys.exit(f"input {path}: hash or size differs from the manifest")
            inputs.append(dict(group=g, name=name, kind=r["kind"], path=path, bytes=r["bytes"], sha256=got,
                               source=r["source"]))
    return inputs


def wait_for_quiet(limit, log):
    waited = False
    while True:
        l1 = os.getloadavg()[0]
        if l1 < limit:
            if waited:
                log(f"load {l1:.1f} < {limit}: timing resumes")
            return l1
        if not waited:
            log(f"load {l1:.1f} >= {limit}: waiting for a quiet machine")
            waited = True
        time.sleep(60)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--set", default="dev", choices=["dev", "holdout"])
    ap.add_argument("--out", required=True)
    ap.add_argument("--kolmo", action="append", default=[], help="SNAPSHOT=binary (repeatable)")
    ap.add_argument("--snapshot-label", action="append", default=[], help="SNAPSHOT=label for the chart")
    ap.add_argument("--competitors", action="store_true")
    ap.add_argument("--only", default="", help="comma list of config ids or prefixes")
    ap.add_argument("--groups", default="", help="comma list of groups")
    ap.add_argument("--timed", default="/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/tools/timed.sh")
    ap.add_argument("--tag", default="perfgraph")
    ap.add_argument("--max-load", type=float, default=12.0)
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--work", default=None, help="scratch directory (default: next to --out)")
    ap.add_argument("--max-retries", type=int, default=50)
    ap.add_argument("--size-only", action="store_true",
                    help="untimed pass under nice -n 15, 1 rep, any load: sizes and round trips only; rows are "
                         "marked size_only and the plotter uses them for ratios, never for speed")
    a = ap.parse_args()
    if a.size_only:
        a.reps, a.max_load, a.timed = 1, float("inf"), "nice"
    # --timed none: the caller already holds the timing lock (e.g. the whole run is wrapped in timed.sh).

    def log(msg):
        print(time.strftime("%H:%M:%S"), msg, flush=True)

    labels = dict(s.split("=", 1) for s in a.snapshot_label)
    cfgs = []
    for s in a.kolmo:
        snap, path = s.split("=", 1)
        path = os.path.abspath(path)
        bsha = sha256(path)
        for c in kolmo_configs(snap, path):
            c["binary_sha256"] = bsha
            c["snapshot_label"] = labels.get(snap, snap)
            cfgs.append(c)
    if a.competitors:
        for c in COMPETITORS:
            exe = subprocess.run(["/bin/sh", "-c", f"command -v {c['enc'][0]}"], capture_output=True,
                                 text=True).stdout.strip()
            c = dict(c, binary=os.path.realpath(exe), binary_sha256=sha256(os.path.realpath(exe)))
            c["version"] = tool_version(c)
            cfgs.append(c)
    if a.only:
        pre = a.only.split(",")
        cfgs = [c for c in cfgs if any(c["config"].startswith(p) for p in pre)]
    inputs = load_inputs(a.corpus, a.set)
    if a.groups:
        inputs = [i for i in inputs if i["group"] in a.groups.split(",")]

    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            r = json.loads(line)
            if bool(r.get("size_only")) == a.size_only:
                done.add((r["config"], r["input_sha256"], r["binary_sha256"]))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    work = a.work or os.path.join(os.path.dirname(os.path.abspath(a.out)), ".work")
    todo = [(i, c) for i in inputs for c in cfgs if (c["config"], i["sha256"], c["binary_sha256"]) not in done]
    log(f"{len(todo)} cells to run ({len(done)} already in {a.out})")

    for n, (inp, c) in enumerate(todo, 1):
        spec = dict(input=inp["path"], input_sha256=inp["sha256"], enc=c["enc"], dec=c["dec"],
                    stdout=list(c["stdout"]), rt_path=c.get("rt_path", "{rt}"), enc_cwd=c.get("enc_cwd"),
                    reps=a.reps, max_load=a.max_load, work=os.path.join(work, "cell"), subs={"kolmo": c.get("binary", "")})
        os.makedirs(work, exist_ok=True)
        spec_path = os.path.join(work, "spec.json")
        for attempt in range(a.max_retries):
            l_before = wait_for_quiet(a.max_load, log) if not a.size_only else os.getloadavg()[0]
            json.dump(spec, open(spec_path, "w"))
            t0 = time.time()
            if a.timed == "nice":
                pre = ["nice", "-n", "15"]
            elif a.timed == "none":
                pre = []
            else:
                pre = [a.timed, a.tag]
            p = subprocess.run([*pre, sys.executable, "-I", os.path.join(HERE, "cell.py"), spec_path],
                               capture_output=True, text=True)
            if p.returncode != 0:
                sys.exit(f"cell runner failed: {p.stderr[-800:]}")
            res = json.loads(p.stdout.strip().splitlines()[-1])
            if res.get("skipped"):
                log(f"  load {res['load_start']:.1f} when the lock was taken; nothing timed")
                continue
            # Load read inside the lock at the start and the end of the cell.
            l_before, l_after = res.pop("load_start"), res.pop("load_end")
            if l_after >= a.max_load:
                log(f"  discarded {c['config']} on {inp['name']}: load {l_after:.1f} at the end; retrying")
                continue
            break
        else:
            sys.exit("too many discarded attempts")
        row = dict(set=a.set, group=inp["group"], input=inp["name"], kind=inp["kind"], input_bytes=inp["bytes"],
                   input_sha256=inp["sha256"], input_source=inp["source"],
                   tool=c["tool"], config=c["config"], label=c["label"], level=c["level"],
                   snapshot=c.get("snapshot"), snapshot_label=c.get("snapshot_label"),
                   binary=c.get("binary"), binary_sha256=c["binary_sha256"], version=c.get("version"),
                   enc_argv=c["enc"], dec_argv=c["dec"], threads=1, reps=a.reps,
                   size_only=a.size_only, load1_before=round(l_before, 2), load1_after=round(l_after, 2),
                   started_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(t0)),
                   wall_cell_s=round(time.time() - t0, 3), host=platform.node(), machine=platform.machine(),
                   **res)
        with open(a.out, "a") as f:
            f.write(json.dumps(row) + "\n")
        ok = "OK" if not res["error"] else "FAIL " + res["error"][:120]
        cb = res["comp_bytes"][-1] if res["comp_bytes"] else 0
        log(f"[{n}/{len(todo)}] {inp['name']:<16} {c['config']:<22} ratio {inp['bytes'] / max(cb, 1):7.3f} "
            f"load {l_before:.1f}/{l_after:.1f} {ok}")


if __name__ == "__main__":
    main()
