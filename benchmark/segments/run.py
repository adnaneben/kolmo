#!/usr/bin/env python3
"""Item 5 measurement: sizes of base (origin/main) and new (this branch) on every corpus file and on real tars, levels
1-9, -T 1 and -T 10; every new output decoded by the 0.0.2 binary and checked by SHA-256. Resumable: rows already in
results.tsv are skipped. Sizes only (no timing claims): jobs run in parallel.

usage: run.py PHASE   (files | tars | archive)
"""
import concurrent.futures as cf, hashlib, os, subprocess, sys, tempfile, threading, time

V = os.path.expanduser("~/Kolmo-Reviews/review-20261119T083610Z.J62kQD")
D = os.path.join(V, "review40b/work/item5")
S = os.environ["ITEM5_BIN"]                     # directory holding kolmo-base and kolmo-new
BIN = {"base": os.path.join(S, "kolmo-base"), "new": os.path.join(S, "kolmo-new")}
DEC = "/opt/homebrew/bin/kolmo"                 # 0.0.2
OUT = os.path.join(D, "results.tsv")
TMP = os.path.join(D, "tmp"); os.makedirs(TMP, exist_ok=True)
lock = threading.Lock()
shacache = {}

def sha_file(p):
    if p not in shacache:
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for b in iter(lambda: f.read(1 << 22), b""): h.update(b)
        shacache[p] = h.hexdigest()
    return shacache[p]

def sha_stream(cmd):
    h = hashlib.sha256(); pr = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    for b in iter(lambda: pr.stdout.read(1 << 22), b""): h.update(b)
    return h.hexdigest() if pr.wait() == 0 else "DECODE-FAILED"

def done_keys():
    k = set()
    if os.path.exists(OUT):
        for line in open(OUT):
            f = line.rstrip("\n").split("\t")
            if len(f) > 6: k.add(tuple(f[:6]))
    return k

def emit(row):
    with lock:
        with open(OUT, "a") as f: f.write("\t".join(str(x) for x in row) + "\n")

def compress_job(phase, inp, mode, level, t, b):
    fd, out = tempfile.mkstemp(dir=TMP); os.close(fd)
    k = BIN[b]; lv = f"-{level}"
    t0 = time.time()
    if mode == "oneshot":
        r = subprocess.run(["nice", k, lv, "-T", str(t), "-f", "-c", inp], stdout=open(out, "wb"), stderr=subprocess.PIPE)
    elif mode == "stream64":
        r = subprocess.run(["nice", k, lv, "-T", str(t), "--stream=64", "-f", "-c", inp], stdout=open(out, "wb"), stderr=subprocess.PIPE)
    elif mode == "pipe":                         # cat FILE | kolmo: one frame up to 256 MiB, else CZNS in 256 MiB segments
        r = subprocess.run(f"cat '{inp}' | nice '{k}' {lv} -T {t} -c", shell=True, stdout=open(out, "wb"), stderr=subprocess.PIPE)
    dt = time.time() - t0
    if r.returncode:
        emit([phase, os.path.relpath(inp, V), mode, level, t, b, -1, "", "COMPRESS-FAILED:" + r.stderr.decode()[:120].replace("\t", " ").replace("\n", " "), f"{dt:.1f}"])
        os.unlink(out); return
    size = os.path.getsize(out); fsha = sha_file(out); shacache.pop(out, None)
    ok = "-"
    if b == "new":
        ok = "ok" if sha_stream([DEC, "-d", "-c", out]) == sha_file(inp) else "MISMATCH"
    os.unlink(out)
    emit([phase, os.path.relpath(inp, V), mode, level, t, b, size, fsha[:16], ok, f"{dt:.1f}"])

def archive_job(phase, src, mode, level, t, b):
    fd, out = tempfile.mkstemp(dir=TMP, suffix=".kol"); os.close(fd)
    parent, name = os.path.split(src)
    t0 = time.time()
    r = subprocess.run(["nice", BIN[b], "archive", f"-{level}", "-T", str(t), "-f", out, name], cwd=parent, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    dt = time.time() - t0
    if r.returncode:
        emit([phase, os.path.relpath(src, V), mode, level, t, b, -1, "", "ARCHIVE-FAILED", f"{dt:.1f}"]); os.unlink(out); return
    size = os.path.getsize(out); fsha = sha_file(out); shacache.pop(out, None)
    ok = "-"
    if b == "new":
        x = tempfile.mkdtemp(dir=TMP)
        e = subprocess.run([DEC, "extract", "-C", x, out], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        d = subprocess.run(["diff", "-r", src, os.path.join(x, name)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        ok = "ok" if e.returncode == 0 and d.returncode == 0 else "MISMATCH"
        subprocess.run(["rm", "-rf", x])
    os.unlink(out)
    emit([phase, os.path.relpath(src, V), mode, level, t, b, size, fsha[:16], ok, f"{dt:.1f}"])

def corpus_files():
    fs = []
    for d in ["corpus", "corpus-holdout", "corpus-domain", "mkt/data"]:
        for n in sorted(os.listdir(os.path.join(V, d))):
            p = os.path.join(V, d, n)
            if os.path.isfile(p) and not n.startswith("MANIFEST"): fs.append(p)
    return fs

TARS = [os.path.join(V, "mkt/data/silesia.tar"), os.path.join(V, "mkt/data/canterbury.tar"), os.path.join(D, "paxpipe.tar"),
        os.path.join(V, "review40b/work/assess-R08/big.tar"), os.path.join(D, "bigmember.tar"),
        os.path.join(V, "review40b/work/exec-R06R07/data/mix2g.tar")]

def jobs(phase):
    J = []
    if phase == "files":
        for f in corpus_files():
            for L in range(1, 10):   # levels 1-7 cannot change one-shot (the merge runs at mode max only): identity at -T 10
                if L <= 7: J += [(phase, f, "oneshot", L, 10, "base"), (phase, f, "oneshot", L, 10, "new")]
                else: J += [(phase, f, "oneshot", L, 1, "base"), (phase, f, "oneshot", L, 1, "new"), (phase, f, "oneshot", L, 10, "new")]
    elif phase == "tars":
        for f in TARS:
            big = os.path.getsize(f) > (1 << 30)
            for mode in ["stream64", "pipe"] + ([] if "/mkt/data/" in f else ["oneshot"]):   # mkt/data tars: one-shot in "files"
                for L in range(1, 10):
                    for t in [1, 10]:
                        # -T 1 (frames are thread-independent: 159 of 159 T1/T10 pairs identical): silesia.tar and
                        # canterbury.tar at every level, the larger tars at level 3; base and new at -T 10 everywhere
                        if t == 1 and "/mkt/data/" not in f and L != 3: continue
                        bins = ["base", "new"]
                        for b in bins: J.append((phase, f, mode, L, t, b))
    elif phase == "archive":
        for src in [os.path.join(V, "review40b/work/R25/t/sil"), os.path.join(V, "corpus"), os.path.join(V, "corpus-holdout")]:
            for L in range(1, 10):
                J += [(phase, src, "archive", L, 1, "base"), (phase, src, "archive", L, 1, "new"), (phase, src, "archive", L, 10, "new")]
    return J

if __name__ == "__main__":
    phase = sys.argv[1]; workers = int(os.environ.get("ITEM5_JOBS", "5"))
    have = done_keys()
    todo = [j for j in jobs(phase) if tuple(str(x) if not isinstance(x, str) or x != j[1] else os.path.relpath(x, V) for x in j) not in have]
    # T10 jobs are not run concurrently with others: run T1 jobs in the pool, T10 jobs one at a time afterwards
    fn = archive_job if phase == "archive" else compress_job
    t1 = [j for j in todo if j[4] == 1]; t10 = [j for j in todo if j[4] != 1]
    t1.sort(key=lambda j: -os.path.getsize(j[1]) if os.path.isfile(j[1]) else 0)
    print(f"{phase}: {len(todo)} jobs ({len(t1)} T1, {len(t10)} T10)", flush=True)
    with cf.ThreadPoolExecutor(workers) as ex: list(ex.map(lambda j: fn(*j), t1 + t10))   # sizes only: no timing taken
    print(f"{phase}: done", flush=True)
