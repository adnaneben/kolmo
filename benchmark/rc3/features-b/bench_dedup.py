"""Deduplicating archives against zstd --long, tar | zstd --long=31, borg and restic (features-b report).

usage: python3 -I bench_dedup.py KOLMO DATA OUT.json [set ...]
Each set is a directory under DATA; every tool archives all its files, then restores them, and the restored files are
compared with the originals. Wall time of whole processes (best of `runs`), one thread unless the row says T4.
borg: `borg` on PATH or $BORG. restic: GOMAXPROCS=1 for one thread. Sizes: the archive file, or the repository's bytes. """
import json, os, shutil, subprocess, sys, tempfile, time

KOLMO, DATA, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
SETS = sys.argv[4:] or ["snapshots", "srctree", "finetune", "growlog", "pythia"]
BORG = os.environ.get("BORG", "borg")
RUNS = int(os.environ.get("RUNS", "1"))

def sh(cmd, cwd=None, env=None):
    e = dict(os.environ); e.update(env or {})
    t = time.perf_counter(); subprocess.run(cmd, shell=True, check=True, cwd=cwd, env=e, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    return time.perf_counter() - t

def dirsize(p):
    if os.path.isfile(p): return os.path.getsize(p)
    return sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(p) for f in fs)

def same_tree(a, b):
    for r, _, fs in os.walk(a):
        for f in fs:
            x = os.path.join(r, f); y = os.path.join(b, os.path.relpath(x, a))
            if not os.path.exists(y) or subprocess.run(["cmp", "-s", x, y]).returncode: return False
    return True

def tools():
    k = KOLMO
    T = [
        ("kolmo a, balanced, 16 KiB chunks", f"{k} a -T1 {{arc}} {{files}}", f"{k} x -T1 -C {{out}} {{arc}}"),
        ("kolmo a, balanced, 4 KiB chunks", f"{k} a -T1 --chunk 4 {{arc}} {{files}}", f"{k} x -T1 -C {{out}} {{arc}}"),
        ("kolmo a, hc, 4 KiB chunks", f"{k} a -T1 -m hc --chunk 4 {{arc}} {{files}}", f"{k} x -T1 -C {{out}} {{arc}}"),
        ("kolmo, balanced, no dedup (one frame per file)", f"mkdir -p {{arc}} && for f in {{files}}; do mkdir -p {{arc}}/$(dirname $f) && {k} -T1 $f {{arc}}/$f.kol; done",
         f"cd {{arc}} && find . -name '*.kol' | while read f; do mkdir -p {{out}}/$(dirname $f) && {k} -d -T1 $f {{out}}/${{{{f%.kol}}}}; done"),
        ("zstd -19 --long=27, per file", f"mkdir -p {{arc}} && for f in {{files}}; do mkdir -p {{arc}}/$(dirname $f) && zstd -q -19 --long=27 -T1 $f -o {{arc}}/$f.zst; done",
         f"cd {{arc}} && find . -name '*.zst' | while read f; do mkdir -p {{out}}/$(dirname $f) && zstd -q -d --long=27 $f -o {{out}}/${{{{f%.zst}}}}; done"),
        ("tar | zstd -3 --long=31", "tar cf - {files} | zstd -q -3 --long=31 -T1 -o {arc}", "zstd -q -d --long=31 -c {arc} | tar xf - -C {out}"),
        ("tar | zstd -19 --long=31", "tar cf - {files} | zstd -q -19 --long=31 -T1 -o {arc}", "zstd -q -d --long=31 -c {arc} | tar xf - -C {out}"),
        ("borg, zstd,3 (default chunker)", f"BORG_UNKNOWN_UNENCRYPTED_REPO_ACCESS_IS_OK=yes {BORG} init -e none {{arc}} && {BORG} create --compression zstd,3 {{arc}}::a {{files}}",
         f"cd {{out}} && {BORG} extract {{arc}}::a"),
        ("borg, zstd,19 (default chunker)", f"BORG_UNKNOWN_UNENCRYPTED_REPO_ACCESS_IS_OK=yes {BORG} init -e none {{arc}} && {BORG} create --compression zstd,19 {{arc}}::a {{files}}",
         f"cd {{out}} && {BORG} extract {{arc}}::a"),
        ("borg, zstd,19, 64 KiB chunks", f"BORG_UNKNOWN_UNENCRYPTED_REPO_ACCESS_IS_OK=yes {BORG} init -e none {{arc}} && {BORG} create --chunker-params buzhash,14,20,16,4095 --compression zstd,19 {{arc}}::a {{files}}",
         f"cd {{out}} && {BORG} extract {{arc}}::a"),
        ("restic, compression max", "restic init -q --repo {arc} && restic -q --repo {arc} backup --compression max {files}",
         "restic -q --repo {arc} restore latest --target {out}"),
        ("kolmo a, balanced, 16 KiB chunks, T4", f"{k} a -T4 {{arc}} {{files}}", f"{k} x -T4 -C {{out}} {{arc}}"),
        ("tar | zstd -19 --long=31, T4", "tar cf - {files} | zstd -q -19 --long=31 -T4 -o {arc}", "zstd -q -d --long=31 -c {arc} | tar xf - -C {out}"),
        ("restic, compression max, all threads", "restic init -q --repo {arc} && restic -q --repo {arc} backup --compression max {files}",
         "restic -q --repo {arc} restore latest --target {out}"),
    ]
    return T

res = json.load(open(OUT)) if os.path.exists(OUT) else {}
for s in SETS:
    base = os.path.join(DATA, s)
    files = sorted(os.path.relpath(os.path.join(r, f), base) for r, _, fs in os.walk(base) for f in fs)
    total = sum(os.path.getsize(os.path.join(base, f)) for f in files)
    flist = " ".join(files)
    res.setdefault(s, {"files": len(files), "bytes": total, "rows": {}})
    for name, c, d in tools():
        if name in res[s]["rows"]: continue
        env = {"RESTIC_PASSWORD": "bench", "GOMAXPROCS": "4" if "all threads" in name else "1"}
        best_c = best_d = None; size = None; ok = True
        for _ in range(RUNS):
            tmp = tempfile.mkdtemp(dir=os.environ.get("TMPDIR")); arc = os.path.join(tmp, "arc"); out = os.path.join(tmp, "out"); os.makedirs(out)
            try:
                tc = sh(c.format(arc=arc, files=flist, out=out), cwd=base, env=env)
                size = dirsize(arc)
                td = sh(d.format(arc=arc, files=flist, out=out), cwd=base, env=env)
                root = out + base if os.path.isdir(out + base) else out      # restic restores the absolute path
                ok = ok and same_tree(base, root)
            except subprocess.CalledProcessError as e:
                print(s, name, "failed:", e.stderr.decode()[-400:], flush=True); ok = False; tc = td = float("nan")
            finally: shutil.rmtree(tmp, ignore_errors=True)
            best_c = tc if best_c is None or tc < best_c else best_c; best_d = td if best_d is None or td < best_d else best_d
        res[s]["rows"][name] = {"size": size, "ratio": total / size if size else None, "c_s": best_c, "d_s": best_d, "ok": ok}
        print(f"{s:10s} {name:50s} {size or 0:>12} {total / (size or 1):7.2f}x  c {best_c:7.2f}s  d {best_d:7.2f}s  {'ok' if ok else 'MISMATCH'}", flush=True)
        json.dump(res, open(OUT, "w"), indent=1)
