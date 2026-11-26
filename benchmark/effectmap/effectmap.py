#!/usr/bin/env python3
"""Effect map: docs/ACCEPTANCE.md applied to one change. See README.md.

  effectmap.py run    --out DIR (--a NAME=BIN --b NAME=BIN | --impact BIN --switch VAR=VALUE ...) [options]
  effectmap.py report --out DIR
  effectmap.py queue  --out DIR (same options as run)   # timed part -> $V/evidence/QUIET_WINDOW_QUEUE.txt

run --mode sizes (default): exact bytes at every thread count, a round trip of every frame, and instructions
retired, RSS and (untimed) wall per phase. One round, under nice, up to 2 processes. Safe on a busy machine.
run --mode timing: --reps rounds, one process, each round only at 1-minute load < --max-load, a round that ends
above it is run again; must run inside timed.sh (pass --locked; `queue` writes the job that does this).
Each round runs A, B and A2 (A again: the A/A noise control), the order rotating from round to round."""
import argparse
import json
import os
import shlex
import shutil
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import measure  # noqa: E402

V = os.environ.get("V", "/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD")
CORPUS = os.path.join(V, "impact", "corpus")
QUEUE = os.path.join(V, "evidence", "QUIET_WINDOW_QUEUE.txt")
TIMED = os.path.join(V, "tools", "timed.sh")
KIB, MIB = 1024, 1024 * 1024

# stacking partners: kolmo-impact switches (benchmark/impact/run.py feature names and levels)
PARTNERS = {
    "klz": ({"KOLMO_IMPACT_CODEC": "28"}, range(1, 10)),
    "klzh2-9": ({"KOLMO_IMPACT_CODEC": "32", "KOLMO_IMPACT_CLEVEL": "9"}, range(1, 7)),
    "klzma": ({"KOLMO_IMPACT_CODEC": "24"}, range(1, 9)),
    "bwt": ({"KOLMO_IMPACT_CODEC": "18"}, range(1, 8)),
    "cm": ({"KOLMO_IMPACT_CODEC": "7"}, range(1, 10)),
    "gr6": ({"KOLMO_IMPACT_CODEC": "26"}, range(1, 10)),
    "stack-klzma": ({"KOLMO_IMPACT_CODEC": "24", "KOLMO_IMPACT_INPUT": "stream"}, range(1, 9)),
    "stack-bwt": ({"KOLMO_IMPACT_CODEC": "18", "KOLMO_IMPACT_INPUT": "stream"}, range(1, 8)),
    "stack-cm": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_IMPACT_INPUT": "stream"}, range(1, 10)),
    "notf": ({"KOLMO_IMPACT_NOTF": "1"}, range(1, 11)),
    "tf-none": ({"KOLMO_IMPACT_TRANSFORMS": "0"}, range(1, 11)),
    "tf-full": ({"KOLMO_IMPACT_TRANSFORMS": "1"}, range(1, 11)),
}


def size_class(n):
    return "S0" if n < 4 * KIB else "S1" if n <= MIB else "S2" if n <= 64 * MIB else "S3"


def lvl_args(lv):
    return ["--level", "ultra"] if lv == "ultra" else ["-" + lv]


def lvl_num(lv):
    return 10 if lv == "ultra" else int(lv)


# ---------------------------------------------------------------- inputs

def load_inputs(a):
    out = []
    man = os.path.join(a.corpus, "manifest.jsonl")
    sets = set(a.sets.split(","))
    kinds = set(a.kinds.split(",")) if a.kinds else None
    names = set(a.inputs.split(",")) if a.inputs else None
    for line in open(man):
        m = json.loads(line)
        split = m["set"].replace("-small", "")
        if split not in sets or (kinds and m["kind"] not in kinds) or (names and m["name"] not in names):
            continue
        path = os.path.join(a.corpus, m["file"])
        out.append(dict(name=m["name"] + ("@16k" if m["set"].endswith("-small") else ""), path=path,
                        kind=m["kind"], split=split, bytes=m["bytes"], sha256=m["sha256"]))
    # S0: prefix cuts of every full-size slice
    if a.cuts:
        cdir = os.path.join(V, "effectmap", "cuts")
        for i in [x for x in out if x["bytes"] > 64 * KIB]:
            for n in [int(c) for c in a.cuts.split(",")]:
                p = os.path.join(cdir, i["split"], f"{i['kind']}__{i['name']}__{n}")
                if not os.path.exists(p):
                    os.makedirs(os.path.dirname(p), exist_ok=True)
                    with open(i["path"], "rb") as f, open(p + ".tmp", "wb") as g:
                        g.write(f.read(n))
                    os.replace(p + ".tmp", p)
                out.append(dict(name=f"{i['name']}@{n}", path=p, kind=i["kind"], split=i["split"], bytes=n,
                                sha256=measure.sha256(p)))
    for spec in a.large or []:
        p, kind, split = (spec.split(":") + ["large", "holdout"])[:3]
        out.append(dict(name=os.path.basename(p), path=p, kind=kind, split=split, bytes=os.path.getsize(p),
                        sha256=measure.sha256(p)))
    for i in out:
        i["size_class"] = size_class(i["bytes"])
    if a.size_classes:
        out = [i for i in out if i["size_class"] in a.size_classes.split(",")]
    return out


def arms_of(a):
    """-> main arms [(name, binary, env)], stacking arms (a_bin, b_bin, b_env) or None"""
    if a.impact:
        sw = dict(s.split("=", 1) for s in a.switch)
        arms = [("A", a.impact, {}), ("B", a.impact, sw)]
        stack = (a.impact, a.impact, sw)
    else:
        (an, ab), (bn, bb) = (x.split("=", 1) for x in (a.a, a.b))
        arms = [("A", os.path.abspath(ab), {}), ("B", os.path.abspath(bb), {})]
        stack = (os.path.abspath(a.a_impact), os.path.abspath(a.b_impact), {}) if a.a_impact and a.b_impact else None
    arms.append(("A2", arms[0][1], arms[0][2]))
    return arms, stack


# ---------------------------------------------------------------- measuring

def one_arm(binary, env, inp, lv, threads, work):
    c, r = os.path.join(work, "c.bin"), os.path.join(work, "rt.bin")
    for p in (c, r):
        if os.path.exists(p):
            os.remove(p)
    e = measure.run([binary, "-q", "-T", str(threads), *lvl_args(lv), "-c", inp["path"]], c, measure.clean_env(env))
    if e["rc"] != 0:
        return dict(error=f"encode rc={e['rc']}: {e['err']}")
    d = measure.run([binary, "-q", "-d", "-T", str(threads), "-c", c], r, measure.clean_env(env))
    ok = d["rc"] == 0 and measure.sha256(r) == inp["sha256"]
    return dict(comp_bytes=os.path.getsize(c), comp_sha=measure.sha256(c), rt_ok=ok,
                **{f"enc_{k}": e[k] for k in ("ns", "cpu", "rss", "foot", "instr")},
                **{f"dec_{k}": d[k] for k in ("ns", "cpu", "rss", "foot", "instr")},
                error=None if ok else f"round trip failed (decode rc={d['rc']})")


def job(spec):
    """one (part, input, level): every thread count and round. Returns rows."""
    part, arms, inp, lv, threads_l, rounds, mode, max_load, workroot, done = spec
    if mode == "sizes":
        try:
            os.nice(15)
        except OSError:
            pass
    work = os.path.join(workroot, f"{os.getpid()}")
    os.makedirs(work, exist_ok=True)
    rows = []
    for t in threads_l:
        for rnd in range(rounds):
            if (part, inp["name"], lv, t, rnd) in done:
                continue
            k = rnd % len(arms)
            order = arms[k:] + arms[:k]
            while True:
                if mode == "timing":
                    while measure.load1() >= max_load:
                        time.sleep(30)
                l0 = measure.load1()
                res = {n: one_arm(b, e, inp, lv, t, work) for n, b, e in order}
                l1 = measure.load1()
                if mode != "timing" or l1 < max_load:
                    break
            rows.append(dict(mode=mode, part=part, input=inp["name"], kind=inp["kind"], split=inp["split"],
                             size_class=inp["size_class"], bytes_in=inp["bytes"], level=lv, threads=t, round=rnd,
                             order=[n for n, _, _ in order], arms=res, load_before=round(l0, 2),
                             load_after=round(l1, 2), utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())))
    shutil.rmtree(work, ignore_errors=True)
    return rows


def cmd_run(a):
    if a.mode == "timing" and not a.locked:
        sys.exit("timing mode runs only inside timed.sh: use `effectmap.py queue` (or pass --locked inside a hold)")
    os.makedirs(a.out, exist_ok=True)
    rows_path = os.path.join(a.out, f"rows-{a.mode}.jsonl")
    arms, stack = arms_of(a)
    inputs = load_inputs(a)
    levels = a.levels.split(",")
    threads = [int(t) for t in a.threads.split(",")]
    rounds = 1 if a.mode == "sizes" else a.reps
    done = set()
    if os.path.exists(rows_path):
        for line in open(rows_path):
            r = json.loads(line)
            done.add((r["part"], r["input"], r["level"], r["threads"], r["round"]))
    meta = dict(cmd=sys.argv, utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), corpus=a.corpus,
                arms={n: dict(binary=b, sha256=measure.sha256(b), env=e) for n, b, e in arms},
                names=dict(A=(a.a or "A=").split("=")[0], B=(a.b or "B=").split("=")[0]) if not a.impact else
                dict(A="impact", B="impact+" + "+".join(a.switch)), levels=levels, threads=threads)
    if stack:
        meta["stack"] = dict(a=stack[0], b=stack[1], b_env=stack[2], partners=a.partners)
    with open(os.path.join(a.out, f"meta-{a.mode}.json"), "w") as f:
        json.dump(meta, f, indent=1)
    workroot = os.path.join(V, "effectmap", "work", os.path.basename(os.path.abspath(a.out)))
    specs = []
    for inp in inputs:
        for lv in levels:
            specs.append(("main", arms, inp, lv, threads, rounds, a.mode, a.max_load, workroot, done))
    if stack and a.partners and a.mode == "sizes":
        sa, sb, senv = stack
        for p in a.partners.split(","):
            penv, plv = PARTNERS[p]
            if any(k in senv for k in penv):
                print(f"skip partner {p}: it sets the same variable as the switch", file=sys.stderr)
                continue
            sarms = [("A", sa, dict(penv)), ("B", sb, dict(penv, **senv))]
            for inp in inputs:
                if inp["size_class"] not in a.stack_sizes.split(","):
                    continue
                for lv in levels:
                    if lvl_num(lv) in plv:
                        specs.append((f"stack:{p}", sarms, inp, lv, [1], 1, "sizes", a.max_load, workroot, done))
    jobs = 1 if a.mode == "timing" else max(1, min(2, a.jobs))
    n = 0
    with open(rows_path, "a") as out, ProcessPoolExecutor(jobs) as ex:
        futs = [ex.submit(job, s) for s in specs]
        for fu in as_completed(futs):
            for r in fu.result():
                out.write(json.dumps(r) + "\n")
                n += 1
                bad = [k for k, v in r["arms"].items() if v.get("error")]
                if bad:
                    print(f"ERROR {r['part']} {r['input']} L{r['level']} T{r['threads']}: "
                          + "; ".join(r["arms"][k]["error"] for k in bad), file=sys.stderr)
            out.flush()
    print(f"{n} rows -> {rows_path}")


def cmd_queue(a):
    os.makedirs(a.out, exist_ok=True)
    argv = [sys.executable, os.path.abspath(__file__), "run", "--mode", "timing", "--locked"]
    skip = {"queue", "--mode", "sizes", "timing"}
    rest = [x for x in sys.argv[2:] if x not in skip]
    job_path = os.path.abspath(os.path.join(a.out, "timing-job.sh"))
    tag = "effectmap-" + os.path.basename(os.path.abspath(a.out))
    with open(job_path, "w") as f:
        f.write("#!/bin/bash\n# effect map timed part (written by effectmap.py queue). Runs inside one timed.sh hold;\n"
                "# every round waits for load1 < --max-load and is redone if it ends above it. Resumable.\n")
        f.write(f"cd {shlex.quote(REPO)}\n")
        f.write(" ".join(shlex.quote(x) for x in [TIMED, tag] + argv + rest) + "\n")
        f.write(" ".join(shlex.quote(x) for x in [sys.executable, os.path.abspath(__file__), "report", "--out",
                                                   os.path.abspath(a.out)]) + "\n")
    os.chmod(job_path, 0o755)
    n_in = len(load_inputs(a))
    cells = n_in * len(a.levels.split(",")) * len(a.threads.split(",")) * a.reps * 3
    line = (f"{tag} (effect map, {a.reps} rounds, A/B/A2 interleaved) | {job_path}   (inside timed.sh itself; "
            f"waits for load < {a.max_load:g} per round; resumable) | ~{max(5, cells // 60)} | "
            f"{os.path.abspath(a.out)}/rows-timing.jsonl (then effectmap.md)\n")
    with open(QUEUE, "a") as f:
        f.write(line)
    print(f"queued: {line.strip()}")


# ---------------------------------------------------------------- report

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["run", "report", "queue"])
    ap.add_argument("--out", required=True)
    ap.add_argument("--a", help="NAME=binary of the base (A)")
    ap.add_argument("--b", help="NAME=binary of the change (B)")
    ap.add_argument("--a-impact", help="kolmo-impact build of A (stacking)")
    ap.add_argument("--b-impact", help="kolmo-impact build of B (stacking)")
    ap.add_argument("--impact", help="switch mode: one kolmo-impact binary; A = no switch, B = --switch")
    ap.add_argument("--switch", action="append", default=[], help="VAR=VALUE for B in switch mode (repeatable)")
    ap.add_argument("--partners", default="", help="stacking partners, comma list of: " + ",".join(PARTNERS))
    ap.add_argument("--stack-sizes", default="S1,S2", help="size classes measured for stacking")
    ap.add_argument("--mode", choices=["sizes", "timing"], default="sizes")
    ap.add_argument("--corpus", default=CORPUS)
    ap.add_argument("--sets", default="dev,holdout")
    ap.add_argument("--kinds", default="")
    ap.add_argument("--inputs", default="")
    ap.add_argument("--cuts", default="1024,3072", help="S0 prefix cuts in bytes ('' for none)")
    ap.add_argument("--size-classes", default="", help="keep only these size classes (S0,S1,S2,S3)")
    ap.add_argument("--large", action="append", help="PATH[:kind[:split]] for S3 inputs")
    ap.add_argument("--levels", default="5,6,7")
    ap.add_argument("--threads", default="1,4,10")
    ap.add_argument("--reps", type=int, default=5)
    ap.add_argument("--max-load", type=float, default=12.0)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--locked", action="store_true", help="timing mode: the caller holds the timed.sh lock")
    a = ap.parse_args()
    if a.cmd == "report":
        import report
        report.main(a.out)
        return
    if not a.impact and not (a.a and a.b):
        sys.exit("give --a and --b, or --impact with --switch")
    (cmd_run if a.cmd == "run" else cmd_queue)(a)


if __name__ == "__main__":
    main()
