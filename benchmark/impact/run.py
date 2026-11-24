#!/usr/bin/env python3
"""Impact-matrix runner (benchmark/impact/README in RESULTS.md). Resumable: a row is keyed by
(feature, file, level, threads) and skipped when raw/rows.jsonl already has it, so a stopped run continues.

usage:
  run.py plan  FEATURE[,FEATURE...] [--sets dev,holdout] [--kinds k1,k2] [--levels 1-10] [--threads 1,10]
      runs every missing cell; each file's cells run as one batch under the machine-wide timing lock
      ($V/tools/timed.sh protocol: mkdir $V/locks/timing.d)
  run.py list  shows the features

Each cell: compress with build/kolmo-impact (the feature's KOLMO_IMPACT_* variables), decompress with the stock
build/kolmo, SHA-256 of the decoded bytes against the manifest; wall seconds, user+sys CPU seconds and peak RSS from
/usr/bin/time -l, the 1-minute load average before the cell, and the per-block trial log aggregated."""
import json, os, subprocess, sys, time, hashlib, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
V = os.environ.get("V", "/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD")
CORPUS = os.path.join(V, "impact", "corpus")
WORK = os.path.join(V, "impact", "work")
ROWS = os.path.join(HERE, "raw", "rows.jsonl")
IMPACT_BIN = os.environ.get("IMPACT_BIN", os.path.join(REPO, "build", "kolmo-impact"))
STOCK_BIN = os.environ.get("STOCK_BIN", os.path.join(REPO, "build", "kolmo"))
BIN_ID = os.environ.get("IMPACT_BIN_ID", os.path.basename(os.path.dirname(IMPACT_BIN)))
LOCK = os.path.join(V, "locks", "timing.d")
KPRIOR = os.path.join(V, "impact", "kprior.bin")
os.environ.setdefault("KOLMO_CM_KPRIOR_FILE", KPRIOR)
OWNER = os.path.join(V, "locks", "timing.owner")
TEXTISH = ["text", "code", "json", "xml", "html", "csv", "tsv", "log", "genomics"]

# feature -> (env, default levels, kinds or None for all)
F = {
    "base": ({}, range(1, 11), None),
    # GR6+ (codec 26) where ultra's gate admits the block, at every level
    "gr6": ({"KOLMO_IMPACT_CODEC": "26"}, range(1, 10), None),
    # GR6+ with the gate off: every text block (shows what the gate rejects)
    "gr6-nogate": ({"KOLMO_IMPACT_CODEC": "26", "KOLMO_IMPACT_GATE": "0"}, [3, 9, 10], TEXTISH),
    # coders forced where the level never tries them (plain bytes, kept when smaller)
    "cm": ({"KOLMO_IMPACT_CODEC": "7"}, range(1, 10), None),
    "cm1": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_IMPACT_CLEVEL": "1"}, range(1, 10), None),
    "bwt": ({"KOLMO_IMPACT_CODEC": "18"}, range(1, 8), None),
    "klzma": ({"KOLMO_IMPACT_CODEC": "24"}, range(1, 9), None),
    "klzh2-9": ({"KOLMO_IMPACT_CODEC": "32", "KOLMO_IMPACT_CLEVEL": "9"}, range(1, 7), None),
    # stacking: the level's transform output coded by another coder
    "stack-cm": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_IMPACT_INPUT": "stream"}, range(1, 10), None),
    "stack-bwt": ({"KOLMO_IMPACT_CODEC": "18", "KOLMO_IMPACT_INPUT": "stream"}, range(1, 8), None),
    "stack-klzma": ({"KOLMO_IMPACT_CODEC": "24", "KOLMO_IMPACT_INPUT": "stream"}, range(1, 9), None),
    # transforms: ablation (each level's transform against its own coder on the plain bytes) and analysis settings
    "notf": ({"KOLMO_IMPACT_NOTF": "1"}, range(1, 11), None),
    "tf-full": ({"KOLMO_IMPACT_TRANSFORMS": "1"}, [1, 2], None),
    "tf-none": ({"KOLMO_IMPACT_TRANSFORMS": "0"}, range(1, 11), None),
    # lexer/regex token-class model and per-kind priors (research port of claude/rc3w3-prior-20261118, cm.c CM_LEX)
    "lex": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_CM_LEX": "1"}, range(1, 11), TEXTISH),
    "lex-prior": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_CM_LEX": "1", "KOLMO_CM_KPRIOR": "1"}, range(1, 11), TEXTISH),
    "prior": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_CM_KPRIOR": "1"}, range(1, 11), TEXTISH),
    "lex-stream": ({"KOLMO_IMPACT_CODEC": "7", "KOLMO_CM_LEX": "1", "KOLMO_IMPACT_INPUT": "stream"}, range(3, 11), TEXTISH),
}

def manifest():
    with open(os.path.join(CORPUS, "manifest.jsonl")) as f: return [json.loads(l) for l in f]

def done_keys():
    k = set()
    if os.path.exists(ROWS):
        with open(ROWS) as f:
            for l in f:
                try: r = json.loads(l)
                except ValueError: continue
                k.add((r["feature"], r["file"], r["level"], r["threads"]))
    return k

def timel(cmd, env, stdout_path):
    """run cmd under /usr/bin/time -l; returns (rc, wall, cpu, maxrss_bytes)"""
    with open(stdout_path, "wb") as out:
        t0 = time.monotonic()
        p = subprocess.run(["/usr/bin/time", "-l"] + cmd, stdout=out, stderr=subprocess.PIPE, env=env)
        wall = time.monotonic() - t0
    user = sys_ = 0.0; rss = 0
    for line in p.stderr.decode(errors="replace").splitlines():
        s = line.split()
        if len(s) >= 6 and s[1] == "real" and s[3] == "user": user, sys_ = float(s[2]), float(s[4])
        elif line.strip().endswith("maximum resident set size"): rss = int(s[0])
    return p.returncode, wall, user + sys_, rss, p.stderr.decode(errors="replace")[-400:]

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()

def lvl_args(l): return ["-U"] if l == 10 else [f"-{l}"]

def cell(feature, m, level, threads):
    env_extra, _, _ = F[feature]
    os.makedirs(WORK, exist_ok=True)
    src = os.path.join(CORPUS, m["file"])
    comp = os.path.join(WORK, "c.kolmo"); dec = os.path.join(WORK, "d.out")
    blog = os.path.join(WORK, "blocks.jsonl")
    if os.path.exists(blog): os.remove(blog)
    env_extra = dict(env_extra)
    if level == 10 and env_extra.get("KOLMO_IMPACT_CODEC") == "7" and ("KOLMO_CM_LEX" in env_extra or "KOLMO_CM_KPRIOR" in env_extra):
        del env_extra["KOLMO_IMPACT_CODEC"]           # ultra codes with CM already: the switch applies to the level's own CM
        env_extra.pop("KOLMO_IMPACT_INPUT", None)
    env = dict(os.environ); env.update(env_extra)
    if env_extra: env["KOLMO_IMPACT_LOG"] = blog
    load = os.getloadavg()[0]
    rc, cw, cc, cr, err = timel([IMPACT_BIN] + lvl_args(level) + ["-T", str(threads), "-c", src], env, comp)
    row = {"feature": feature, "file": m["file"], "kind": m["kind"], "set": m["set"], "level": level, "threads": threads,
           "bytes": m["bytes"], "load1": round(load, 1), "env": env_extra, "bin": BIN_ID, "ts": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    if rc != 0:
        row.update({"ok": False, "error": "compress rc %d: %s" % (rc, err)}); return row
    size = os.path.getsize(comp)
    denv = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}
    research_fmt = "KOLMO_CM_LEX" in env_extra or "KOLMO_CM_KPRIOR" in env_extra
    if research_fmt: denv["KOLMO_CM_KPRIOR_FILE"] = KPRIOR   # mask bits 30-31 and prior ids 3-10: only the research decoder reads them
    rc2, dw, dc, dr, err2 = timel([IMPACT_BIN if research_fmt else STOCK_BIN, "-d", "-T", str(threads), "-c", comp], denv, dec)
    row["decoder"] = "research" if research_fmt else "stock"
    ok = rc2 == 0 and sha(dec) == m["sha256"]
    row.update({"ok": ok, "size": size, "c_wall": round(cw, 4), "c_cpu": round(cc, 4), "c_rss": cr,
                "d_wall": round(dw, 4), "d_cpu": round(dc, 4), "d_rss": dr,
                "c_mbs": round(m["bytes"] / 1e6 / cw, 3) if cw else None, "d_mbs": round(m["bytes"] / 1e6 / dw, 3) if dw else None,
                "frame_sha256": sha(comp)})
    if not ok: row["error"] = "decode rc %d or SHA-256 mismatch: %s" % (rc2, err2)
    if os.path.exists(blog):
        B = [json.loads(l) for l in open(blog)]
        tried = [b for b in B if b.get("trial_size", 0) > 0]
        row["blocks"] = {"n": len(B), "tried": len(tried), "kept": sum(b["kept"] for b in B),
                         "base_sum": sum(b["base_size"] for b in B),
                         # forced: every block where the trial coded, at the trial's size; others at the level's size
                         "forced_sum": sum(min(b["trial_size"], b["len"]) if b.get("trial_size", 0) > 0 else b["base_size"] for b in B),
                         "trial_sec": round(sum(b.get("trial_sec", 0) for b in B), 4),
                         "base_codecs": sorted({(b["base_codec"], b["base_tflag"]) for b in B})}
    return row

WAIT = os.path.join(V, "impact", "waiting.%d" % os.getpid())
def lock():
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    first = True
    while True:
        try: os.mkdir(LOCK); break
        except FileExistsError:
            if first: open(WAIT, "w").close(); first = False   # another impact runner yields to us on release
            time.sleep(15)
    if os.path.exists(WAIT): os.remove(WAIT)
    with open(OWNER, "w") as f: f.write("impact-matrix %s\n" % time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
def unlock():
    with open(OWNER, "w") as f: f.write("free %s\n" % time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()))
    try: os.rmdir(LOCK)
    except OSError: pass
    import glob
    if [w for w in glob.glob(os.path.join(V, "impact", "waiting.*")) if w != WAIT]: time.sleep(20)   # let a waiting runner in

def parse_levels(s):
    out = []
    for part in s.split(","):
        if "-" in part: a, b = part.split("-"); out += list(range(int(a), int(b) + 1))
        else: out.append(int(part))
    return out

def plan(args):
    feats = args[0].split(","); opts = dict(zip(args[1::2], args[2::2]))
    sets = opts.get("--sets", "dev,holdout").split(",")
    kinds = opts.get("--kinds"); kinds = kinds.split(",") if kinds else None
    threads = [int(t) for t in opts.get("--threads", "1,10").split(",")]
    only = opts.get("--files"); only = only.split(",") if only else None
    os.makedirs(os.path.dirname(ROWS), exist_ok=True)
    M = [m for m in manifest() if m["set"] in sets and (not kinds or m["kind"] in kinds) and (not only or m["name"] in only)]
    for feat in feats:
        env, dl, fk = F[feat]
        levels = parse_levels(opts["--levels"]) if "--levels" in opts else list(dl)
        levels = [l for l in levels if l in dl] if "--levels" in opts and feat != "base" else levels
        for m in M:
            if fk and m["kind"] not in fk: continue
            have = done_keys()
            todo = [(l, t) for l in levels for t in threads if (feat, m["file"], l, t) not in have]
            if not todo: continue
            lock()
            try:
                for l, t in todo:
                    r = cell(feat, m, l, t)
                    with open(ROWS, "a") as f: f.write(json.dumps(r, sort_keys=True) + "\n")
                    print("%-10s %-32s L%-2d T%-2d %s size=%s c=%.2fs d=%.2fs load=%.0f" % (feat, m["file"], l, t, "ok" if r["ok"] else "FAIL",
                          r.get("size"), r.get("c_wall", 0), r.get("d_wall", 0), r["load1"]), flush=True)
            finally:
                unlock()

if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in ("plan", "list"): print(__doc__); sys.exit(2)
    if sys.argv[1] == "list":
        for k, (e, l, kk) in F.items(): print(k, e, list(l), kk or "all")
    else: plan(sys.argv[2:])
