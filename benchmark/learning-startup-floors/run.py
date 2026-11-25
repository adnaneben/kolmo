#!/usr/bin/env python3
"""Startup-aware --target floor experiment (see protocol.md).

  python3 run.py measure --encoder E --reader R --corpus DIR --out OUT   # launch, tables, development, holdout
  python3 run.py score --out OUT                                          # rescore raw.jsonl (also run by measure)

DIR holds dev/ and holdout/ with the files named in dev.json / holdout.json (exact SHA-256). OUT must not exist.
Run `measure` under the machine-wide timing lock; nothing else should be timed on the host meanwhile.
"""
import argparse, hashlib, json, math, os, random, re, statistics, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ENCODER_SHA = "b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f"   # pinned main a1d39559 (= B26)
READER_SHA = "5c116ba0adb839757a3bfbcd53c334df354e804ab5fd9b2281ee1163e26221a5"    # Homebrew kolmo 0.0.2
SEED = 20261125
SETTINGS = {  # B26 freeze: the nine settings the selector chooses among
    "speed": ["-1"], "balanced-n": ["-3", "--analysis", "none"], "balanced": ["-3"], "eco": ["-5"], "hc": ["-7"],
    "ratio-l1": ["-8"], "ratio": ["-9"], "x-l1": ["--ultra", "--codec-level", "1"], "x": ["--ultra"]}
FLOORS = [("encode", 1), ("encode", 20), ("encode", 60), ("decode", 1), ("decode", 30), ("decode", 80)]
SPECS = [f"{ph}>={r}MB/s" for ph, r in FLOORS] + ["size"]
CLASSES = ["text", "table", "log", "json", "numeric", "binary"]
ROUNDS = {"dev": 5, "holdout": 11, "holdout2": 11, "reference": 7}
MAX_LOAD = None   # --max-load: wait for the 1-minute load to fall to this before each round; redo a round that ends above it


def load1(): return os.getloadavg()[0]


def wait_quiet():
    if MAX_LOAD is None: return load1()
    while load1() > MAX_LOAD: time.sleep(30)
    return load1()


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()


class Runner:
    def __init__(self, encoder, reader, out):
        self.encoder, self.reader, self.out = encoder, reader, out
        self.home = os.path.join(out, "home"); os.makedirs(self.home)   # empty HOME: no ~/.config/kolmo/calibration
        self.env = {"PATH": "/usr/bin:/bin", "HOME": self.home, "LC_ALL": "C"}   # no KOLMO_* / CZN_*
        self.work = os.path.join(out, "work"); os.makedirs(self.work)

    def spawn(self, argv, errpath):
        acts = [(os.POSIX_SPAWN_OPEN, 1, os.devnull, os.O_WRONLY, 0),
                (os.POSIX_SPAWN_OPEN, 2, errpath, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)]
        t0 = time.perf_counter_ns()
        pid = os.posix_spawn(argv[0], argv, self.env, file_actions=acts)
        _, status, ru = os.wait4(pid, 0)
        t1 = time.perf_counter_ns()
        if status != 0:
            raise RuntimeError(f"exit {status}: {argv}: {open(errpath, errors='replace').read()[-500:]}")
        rss = ru.ru_maxrss if sys.platform == "darwin" else ru.ru_maxrss * 1024
        return (t1 - t0) / 1e6, rss

    def roundtrip(self, src, enc_args):
        frame, back, err = (os.path.join(self.work, x) for x in ("frame.kolmo", "restored", "stderr"))
        for p in (frame, back):
            if os.path.exists(p): os.unlink(p)
        ems, erss = self.spawn([self.encoder, "-T", "1"] + enc_args + ["-o", frame, "--", src], err)
        msg = open(err, errors="replace").read()
        dms, drss = self.spawn([self.reader, "-d", "-T", "1", "-q", "-o", back, "--", frame], err + ".d")
        return {"enc_ms": ems, "enc_rss": erss, "dec_ms": dms, "dec_rss": drss, "bytes": os.path.getsize(frame),
                "frame_sha": sha(frame), "restored_sha": sha(back), "stderr": msg.strip()}


def builtin_table():
    rows = {}
    text = open(os.path.join(HERE, "..", "..", "src", "plan", "target_table.inc")).read()
    for m in re.finditer(r"/\*\s*([a-z0-9-]+)\s*\*/\s*\{(.*?)\},\s*$", text, re.M):
        vals = re.findall(r"\{([^{}]*)\}", m.group(2) + "}")
        assert len(vals) == 6, m.group(1)
        for c, v in zip(CLASSES, vals): rows[(m.group(1), c)] = [float(x) for x in v.split(",")]
    assert len(rows) == 54
    return rows


def parse_table(path):
    rows = {}
    for line in open(path):
        line = line.split("#")[0].split()
        if len(line) == 6: rows[(line[0], line[1])] = [float(x) for x in line[2:]]
    assert len(rows) == 54, path
    return rows


def write_table(path, rows, pe, pd, note):
    with open(path, "w") as f:
        f.write(f"kolmo-calibration 1\n# {note}; launch +{pe:.3f} ms encode, +{pd:.3f} ms decode on every setup term\n")
        for s in SETTINGS:
            for c in CLASSES:
                e, d, es, ds = rows[(s, c)]
                f.write(f"{s} {c} {e:.4f} {d:.4f} {es + pe:.3f} {ds + pd:.3f}\n")


def load_split(corpus, split, mdir=HERE):
    man = json.load(open(os.path.join(mdir, f"{split}.json")))
    for r in man:
        p = os.path.join(corpus, split, r["path"])
        if sha(p) != r["sha256"] or os.path.getsize(p) != r["bytes"]: raise SystemExit(f"corpus mismatch: {p}")
        r["file"] = p
    return man


def run_split(R, raw, split, files, arms, rng):
    cells = [("direct", s, None) for s in SETTINGS] + [("target", a, spec) for a in arms for spec in SPECS]
    for rnd in range(ROUNDS[split] + 1):          # round 0 is the warmup
      for attempt in range(1, 7):
        before = wait_quiet(); buf = []
        order = files[:]; rng.shuffle(order)
        for f in order:
            cs = cells[:]; rng.shuffle(cs)
            for kind, name, spec in cs:
                if kind == "direct": args = ["-q"] + SETTINGS[name]
                else:
                    args = ["-v", "--target", spec] + (["--calib", arms[name]] if arms[name] else [])
                r = R.roundtrip(f["file"], args)
                r.update(split=split, round=rnd, warmup=rnd == 0, file=f["path"], n=f["bytes"], input_sha=f["sha256"],
                         kind=kind, arm=name, spec=spec)
                buf.append(r)
        after = load1(); bad = MAX_LOAD is not None and after > MAX_LOAD
        for r in buf:
            r.update(load1_before=before, load1_after=after, attempt=attempt, invalid_load=bad)
            raw.write(json.dumps(r) + "\n")
        raw.flush()
        print(f"{split} round {rnd} attempt {attempt} done {time.strftime('%H:%M:%S')} load {before:.1f}->{after:.1f}"
              + (" REDO" if bad else ""), flush=True)
        if not bad: break


def measure(a):
    out = os.path.abspath(a.out); os.makedirs(out)
    if sha(a.encoder) != ENCODER_SHA or sha(a.reader) != READER_SHA: raise SystemExit("binary identity mismatch")
    global MAX_LOAD
    MAX_LOAD = a.max_load
    holdouts = a.holdouts.split(",")
    if a.smoke: ROUNDS.update({k: 1 for k in ROUNDS})
    mdir = a.manifests or HERE
    dev = load_split(a.corpus, "dev", mdir); hold = {h: load_split(a.corpus, h, mdir) for h in holdouts}
    R = Runner(os.path.abspath(a.encoder), os.path.abspath(a.reader), out)
    meta = {"encoder": a.encoder, "encoder_sha256": ENCODER_SHA, "reader": a.reader, "reader_sha256": READER_SHA,
            "seed": SEED, "rounds": ROUNDS, "start": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "uname": os.uname()._asdict() if hasattr(os.uname(), "_asdict") else list(os.uname()),
            "load_start": os.getloadavg(), "max_load": MAX_LOAD, "holdouts": holdouts}
    rng = random.Random(SEED)
    # 1. launch constants on a 1-byte file (development phase)
    one = os.path.join(out, "one-byte"); open(one, "wb").write(b"x")
    while True:
        lb = wait_quiet(); lr = [R.roundtrip(one, ["-q", "-1"]) for _ in range(22)][1:]; la = load1()
        if MAX_LOAD is None or la <= MAX_LOAD: break
    meta["launch_load"] = [lb, la]
    pe = statistics.median(r["enc_ms"] for r in lr); pd = statistics.median(r["dec_ms"] for r in lr)
    meta["launch"] = {"rounds": 21, "Pe_ms": pe, "Pd_ms": pd, "enc_ms": [r["enc_ms"] for r in lr], "dec_ms": [r["dec_ms"] for r in lr]}
    # 2. tables
    write_table(os.path.join(out, "B-launch.tab"), builtin_table(), pe, pd, "B: built-in rates and setups (src/plan/target_table.inc)")
    host = os.path.join(out, "calibrate-dev.tab")
    t0 = time.time(); meta["calibrate_load_before"] = wait_quiet()
    subprocess.run([R.encoder, "calibrate", "-s", "0.1", "-o", host] + [f["file"] for f in dev], env=R.env, check=True)
    meta["calibrate_s"] = time.time() - t0; meta["calibrate_load_after"] = load1()
    write_table(os.path.join(out, "C-host-launch.tab"), parse_table(host), pe, pd, "C: kolmo calibrate -s 0.1 on the development files")
    tabs = {"B": os.path.join(out, "B-launch.tab"), "C": os.path.join(out, "C-host-launch.tab")}
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    raw = open(os.path.join(out, "raw.jsonl"), "w")
    # 3. development: stock, B, C
    run_split(R, raw, "dev", dev, {"stock": None, "B": tabs["B"], "C": tabs["C"]}, rng)
    dsc = score_split([json.loads(l) for l in open(os.path.join(out, "raw.jsonl"))], "dev", ["B", "C"], "stock", None)
    met = {k: dsc["arms"][k]["met"] for k in ("stock", "B", "C")}
    pick = max(("B", "C"), key=lambda k: (met[k], -dsc["arms"][k]["bytes_on_met"]))
    improved = met[pick] > met["stock"]
    if not improved: pick = "B"
    meta.update(dev_met=met, chosen=pick, dev_improved=improved)
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    print("development met:", met, "-> holdout candidate", pick, "(improved)" if improved else "(no development gain)", flush=True)
    # 4. holdout: stock, stock-dup, chosen
    for h in holdouts:
        run_split(R, raw, h, hold[h], {"stock": None, "stock-dup": None, pick: tabs[pick]}, rng)
    raw.close()
    meta.update(end=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), load_end=os.getloadavg())
    json.dump(meta, open(os.path.join(out, "meta.json"), "w"), indent=1)
    score(argparse.Namespace(out=out))


def med(xs): return statistics.median(xs)


def score_split(rows, split, cands, base, dup):
    rows = [r for r in rows if r["split"] == split and not r["warmup"] and not r.get("invalid_load")]
    integrity = []
    cell = {}
    for r in rows:
        if r["restored_sha"] != r["input_sha"]: integrity.append(f"restore {r['file']} {r['arm']} {r['spec']} round {r['round']}")
        cell.setdefault((r["file"], r["kind"], r["arm"], r["spec"]), []).append(r)
    for k, rs in cell.items():
        if len({r["frame_sha"] for r in rs}) != 1: integrity.append(f"frame drift {k}")
    files = sorted({r["file"] for r in rows})
    n = {r["file"]: r["n"] for r in rows}
    rate = lambda f, ms: n[f] / 1e6 / (ms / 1e3)
    direct = {}
    for f in files:
        for s in SETTINGS:
            rs = cell[(f, "direct", s, None)]
            direct[(f, s)] = {"bytes": rs[0]["bytes"], "frame_sha": rs[0]["frame_sha"],
                              "encode": rate(f, med([r["enc_ms"] for r in rs])), "decode": rate(f, med([r["dec_ms"] for r in rs]))}
    arms = [base] + ([dup] if dup else []) + cands
    cases, summary = [], {a: {"met": 0, "bytes_on_met": 0} for a in arms}
    for f in files:
        for spec in SPECS:
            row = {"file": f, "n": n[f], "spec": spec}
            if spec != "size":
                ph, fl = spec.split(">="); fl = float(fl[:-4])
                ok = [(direct[(f, s)]["bytes"], s) for s in SETTINGS if direct[(f, s)][ph] >= fl]
                row["oracle"] = min(ok)[1] if ok else ""; row["oracle_bytes"] = min(ok)[0] if ok else ""
                row["feasible_settings"] = len(ok)
            for a in arms:
                rs = cell[(f, "target", a, spec)]
                m = re.search(r"chunk: ([a-z0-9-]+) x1; predicted (\d+) bytes", rs[0]["stderr"])
                row[f"{a}_route"] = m.group(1) if m else "?"
                row[f"{a}_predicted_B"] = int(m.group(2)) if m else ""
                row[f"{a}_bytes"] = rs[0]["bytes"]; row[f"{a}_frame_sha"] = rs[0]["frame_sha"]
                row[f"{a}_enc_ms"] = med([r["enc_ms"] for r in rs]); row[f"{a}_dec_ms"] = med([r["dec_ms"] for r in rs])
                row[f"{a}_enc_MBps"] = rate(f, row[f"{a}_enc_ms"]); row[f"{a}_dec_MBps"] = rate(f, row[f"{a}_dec_ms"])
                row[f"{a}_enc_rss_MiB"] = med([r["enc_rss"] for r in rs]) / 2**20
                row[f"{a}_dec_rss_MiB"] = med([r["dec_rss"] for r in rs]) / 2**20
                if spec != "size":
                    met = row[f"{a}_{'enc' if ph == 'encode' else 'dec'}_MBps"] >= fl
                    row[f"{a}_met"] = met
                    ds = direct.get((f, row[f"{a}_route"]))
                    row[f"{a}_direct_route_MBps"] = ds[ph] if ds else ""
                    if met: summary[a]["met"] += 1; summary[a]["bytes_on_met"] += rs[0]["bytes"]
                if a != base:
                    b = {r["round"]: r for r in cell[(f, "target", base, spec)]}
                    pr = [(r["enc_ms"] / b[r["round"]]["enc_ms"], r["enc_rss"] / b[r["round"]]["enc_rss"],
                           r["dec_ms"] / b[r["round"]]["dec_ms"], (r["enc_ms"] + r["dec_ms"]) / (b[r["round"]]["enc_ms"] + b[r["round"]]["dec_ms"]))
                          for r in rs]
                    row[f"{a}_paired_enc_ratio"] = med([p[0] for p in pr]); row[f"{a}_paired_enc_rss_ratio"] = med([p[1] for p in pr])
                    row[f"{a}_paired_dec_ratio"] = med([p[2] for p in pr]); row[f"{a}_paired_combined_ratio"] = med([p[3] for p in pr])
            cases.append(row)
    return {"cases": cases, "arms": summary, "integrity": integrity, "direct": {f"{k[0]}|{k[1]}": v for k, v in direct.items()}}


def gates(sc, base, dup, cand):
    fc = [c for c in sc["cases"] if c["spec"] != "size"]
    dis = sum(c[f"{base}_met"] != c[f"{dup}_met"] for c in fc)
    gain = sc["arms"][cand]["met"] - sc["arms"][base]["met"]
    new_miss = [f"{c['file']} {c['spec']}" for c in fc if c[f"{base}_met"] and c[f"{dup}_met"] and not c[f"{cand}_met"]]
    both = [c for c in fc if c[f"{base}_met"] and c[f"{cand}_met"]]
    bsum, csum = sum(c[f"{base}_bytes"] for c in both), sum(c[f"{cand}_bytes"] for c in both)
    size_same = all(c[f"{cand}_frame_sha"] == c[f"{base}_frame_sha"] for c in sc["cases"] if c["spec"] == "size")
    gm = math.exp(statistics.fmean(math.log(c[f"{cand}_paired_enc_ratio"]) for c in fc))
    rss = max(c[f"{cand}_paired_enc_rss_ratio"] for c in fc)
    g = {
        "G1_attainment": {"stock_met": sc["arms"][base]["met"], "dup_met": sc["arms"][dup]["met"], "cand_met": sc["arms"][cand]["met"],
                          "cases": len(fc), "gain": gain, "stock_dup_disagreements": dis, "pass": gain > max(1, dis)},
        "G2_no_new_misses": {"cases": new_miss, "pass": not new_miss},
        "G3_objective": {"both_met": len(both), "stock_bytes": bsum, "cand_bytes": csum,
                         "ratio": csum / bsum if bsum else None, "size_spec_identical": size_same,
                         "pass": (not bsum or csum <= 1.01 * bsum) and size_same},
        "G4_selection_cost": {"geomean_paired_enc_ratio": gm, "max_paired_enc_rss_ratio": rss, "pass": gm <= 1.05 and rss <= 1.10},
        "G5_integrity": {"problems": sc["integrity"], "pass": not sc["integrity"]},
    }
    g["qualifies"] = all(v["pass"] for v in g.values())
    return g


def write_csv(path, rows):
    keys = []
    for r in rows:
        for k in r:
            if k not in keys: keys.append(k)
    with open(path, "w") as f:
        f.write(",".join(keys) + "\n")
        for r in rows:
            f.write(",".join(f"{r[k]:.6g}" if isinstance(r.get(k), float) else str(r.get(k, "")) for k in keys) + "\n")


def score(a):
    out = a.out
    meta = json.load(open(os.path.join(out, "meta.json")))
    rows = [json.loads(l) for l in open(os.path.join(out, "raw.jsonl"))]
    res = {"meta": {k: meta[k] for k in meta if k != "launch"}, "launch": {k: meta["launch"][k] for k in ("Pe_ms", "Pd_ms")}}
    if any(r["split"] == "dev" for r in rows):
        d = score_split(rows, "dev", ["B", "C"], "stock", None)
        res["dev"] = {"arms": d["arms"], "integrity": d["integrity"]}
        write_csv(os.path.join(out, "dev-cases.csv"), d["cases"])
    for hs in meta.get("holdouts", ["holdout"]):
        if not any(r["split"] == hs for r in rows): continue
        cand = meta["chosen"]
        h = score_split(rows, hs, [cand], "stock", "stock-dup")
        res[hs] = {"candidate": cand, "arms": h["arms"], "gates": gates(h, "stock", "stock-dup", cand)}
        write_csv(os.path.join(out, f"{hs}-cases.csv"), h["cases"])
        write_csv(os.path.join(out, f"{hs}-direct.csv"), [dict(file=k.split("|")[0], setting=k.split("|")[1], **v) for k, v in h["direct"].items()])
    res["rows_discarded_for_load"] = sum(1 for r in rows if r.get("invalid_load")) 
    json.dump(res, open(os.path.join(out, "score.json"), "w"), indent=1)
    print(json.dumps({k: res[k] for k in res if k not in ("meta",)}, indent=1))


def reference(a):
    """Explanatory replay on the three recorded B26 inputs, run only after the holdout decision: stock vs a table."""
    out = os.path.abspath(a.out); os.makedirs(out)
    if sha(a.encoder) != ENCODER_SHA or sha(a.reader) != READER_SHA: raise SystemExit("binary identity mismatch")
    files = load_split(a.corpus, "reference", a.manifests)
    R = Runner(os.path.abspath(a.encoder), os.path.abspath(a.reader), out)
    global MAX_LOAD
    MAX_LOAD = a.max_load
    tab = os.path.join(out, os.path.basename(a.table)); open(tab, "w").write(open(a.table).read())
    with open(os.path.join(out, "raw.jsonl"), "w") as raw:
        run_split(R, raw, "reference", files, {"stock": None, "cand": tab}, random.Random(SEED + 1))
    rows = [json.loads(l) for l in open(os.path.join(out, "raw.jsonl"))]
    sc = score_split(rows, "reference", ["cand"], "stock", None)
    write_csv(os.path.join(out, "reference-cases.csv"), sc["cases"])
    json.dump({"arms": sc["arms"], "integrity": sc["integrity"], "table": a.table}, open(os.path.join(out, "reference-score.json"), "w"), indent=1)
    print(json.dumps({"arms": sc["arms"], "integrity": sc["integrity"]}, indent=1))


if __name__ == "__main__":
    ap = argparse.ArgumentParser(); sub = ap.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("measure"); m.add_argument("--encoder", required=True); m.add_argument("--reader", required=True)
    m.add_argument("--corpus", required=True); m.add_argument("--out", required=True)
    m.add_argument("--manifests", help="smoke tests only: other dev.json/holdout.json"); m.add_argument("--smoke", action="store_true")
    m.add_argument("--holdouts", default="holdout", help="comma list of sealed holdout manifests, scored separately")
    m.add_argument("--max-load", type=float, default=None, help="quiet-host gate on the 1-minute load average")
    s = sub.add_parser("score"); s.add_argument("--out", required=True)
    r = sub.add_parser("reference"); r.add_argument("--encoder", required=True); r.add_argument("--reader", required=True)
    r.add_argument("--corpus", required=True, help="directory with reference/<name> files"); r.add_argument("--out", required=True)
    r.add_argument("--table", required=True); r.add_argument("--manifests", default=HERE)
    r.add_argument("--max-load", type=float, default=None)
    a = ap.parse_args()
    {"measure": measure, "score": score, "reference": reference}[a.cmd](a)
