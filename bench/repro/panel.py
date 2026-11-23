#!/usr/bin/env python3
"""Structured-data panel and the OpenZL rematch: Kolmo (no configuration, no training) against OpenZL with its
format profile untrained AND trained on a separate training split, plus zstd/xz baselines (and ZipNN on weights).

usage: panel.py TOOLS.json DATA_DIR SUITE.json OUT.json [--sets a,b] [--reps N]

- Every tool is a command-line process, file to file, one thread, best of N (N=3; 1 above 60 MB or for slow
  settings). Each round trip is compared byte for byte; any failure or mismatch is recorded as an error row.
- CPU time / wall time is recorded per row to show the run stayed on one thread.
- A set with "train": OpenZL is trained once per profile with `zli train -p <profile> <train dir>` (the
  training files are never measured), then the trained compressor is measured on the test files. Training time
  and the trainer's settings go into the output.
- Resumable: files already in OUT.json are skipped. Use the timing lock wrapper around this whole script.
Output schema (unchanged from the 0.0.2 market report's openzl.json, plus "set" and "trained" fields):
  {file: {set, area, bytes, rows: [{tool, size, ratio, enc_mibs, dec_mibs, cpu_per_wall} | {tool, error}]},
   "_training": {set/profile: {seconds, args, compressor_bytes}}}"""
import argparse, filecmp, glob, json, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))


def run(argv, env=None):
    t = time.perf_counter()
    pid = os.fork()
    if pid == 0:
        fd = os.open("/dev/null", os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2)
        if env:
            os.environ.update(env)
        os.execvp(argv[0], argv)
    _, st, ru = os.wait4(pid, 0)
    el = time.perf_counter() - t
    return None if st != 0 else (el, ru.ru_utime + ru.ru_stime)


def measure(name, c, d, src, reps, work, internal=False):
    """internal=True: the tool writes its own in-process seconds to <out>.time (used for Python libraries, so the
    interpreter's start-up is not charged to them)."""
    cf, bf = os.path.join(work, "p.c"), os.path.join(work, "p.d")
    be = bd = 1e18; cpu = 0.0
    for _ in range(reps):
        for p in (cf, bf, cf + ".time", bf + ".time"):
            if os.path.exists(p): os.remove(p)
        rc = run([a.format(i=src, o=cf) for a in c])
        if rc is None: return dict(tool=name, error="compress failed")
        rd = run([a.format(i=cf, o=bf) for a in d])
        if rd is None: return dict(tool=name, error="decompress failed")
        if not filecmp.cmp(src, bf, shallow=False): return dict(tool=name, error="MISMATCH")
        te, td = rc[0], rd[0]
        if internal:
            te, td = float(open(cf + ".time").read()), float(open(bf + ".time").read())
        be = min(be, te); bd = min(bd, td)
        cpu = max(cpu, rc[1] / max(rc[0], 1e-9), rd[1] / max(rd[0], 1e-9))
    n = os.path.getsize(src); s = os.path.getsize(cf)
    r = dict(tool=name, size=s, ratio=n / s, enc_mibs=n / be / 2**20, dec_mibs=n / bd / 2**20, cpu_per_wall=cpu)
    if internal:
        r["timing"] = "in-process (interpreter start-up excluded)"
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tools"); ap.add_argument("data"); ap.add_argument("suite"); ap.add_argument("out")
    ap.add_argument("--sets"); ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--python", default=sys.executable, help="interpreter with zipnn installed (weights set)")
    a = ap.parse_args()
    T = json.load(open(a.tools)); suite = json.load(open(a.suite))
    K, Z = T["kolmo"]["path"], T["openzl"]["path"]
    work = os.path.join(os.path.dirname(os.path.abspath(a.out)), "work-panel"); os.makedirs(work, exist_ok=True)
    res = json.load(open(a.out)) if os.path.exists(a.out) else {}
    training = res.setdefault("_training", {})
    want = set(a.sets.split(",")) if a.sets else None

    def save():
        json.dump(res, open(a.out + ".tmp", "w"), indent=1); os.replace(a.out + ".tmp", a.out)

    for st in suite["sets"]:
        if want and st["name"] not in want:
            continue
        files = sorted(glob.glob(os.path.join(a.data, st["files"])))
        if not files:
            print(f"{st['name']}: no files ({st['files']}), skipped" + (" (optional)" if st.get("optional") else ""), flush=True)
            continue
        trained = {}
        for prof in st.get("openzl", []) if st.get("train") else []:
            key = f"{st['name']}/{' '.join(prof)}"
            zc = os.path.join(work, st["name"] + "-" + "_".join(p.strip("-") for p in prof) + ".zlc")
            if key not in training or not os.path.exists(zc):
                args = [Z, "train"] + prof + [os.path.join(a.data, st["train"]), "-o", zc, "-f"] + st.get("train_args", [])
                t = time.perf_counter(); ok = run(args)
                training[key] = dict(seconds=time.perf_counter() - t, args=args[1:], ok=ok is not None,
                                     compressor_bytes=os.path.getsize(zc) if os.path.exists(zc) else None)
                save()
                print(f"trained {key}: {'ok' if ok else 'FAILED'} in {training[key]['seconds']:.0f} s", flush=True)
            if os.path.exists(zc):
                trained[" ".join(prof)] = zc
        for src in files:
            f = os.path.relpath(src, a.data)
            if f in res:
                continue
            n = os.path.getsize(src); reps = a.reps if n < 60e6 else 1
            rows = []
            for lv in [str(x) for x in st.get("kolmo_levels", [3, 5, 7, 9])] + (["U"] if n <= st.get("ultra_max", 12e6) else []):
                r = 1 if lv == "U" or (lv == "9" and n > 30e6) else reps
                rows.append(measure("kolmo -" + lv, [K, "-" + lv, "-T1", "-q", "-f", "{i}", "-o", "{o}"],
                                    [K, "-d", "-T1", "-q", "-f", "{i}", "-o", "{o}"], src, r, work))
            for pa in st.get("openzl", []):
                for lv in ["6", "9"]:
                    rows.append(measure("openzl -l %s [%s]" % (lv, " ".join(pa)), [Z, "compress"] + pa + ["-l", lv, "-f", "{i}", "-o", "{o}"],
                                        [Z, "decompress", "-f", "{i}", "-o", "{o}"], src, reps, work))
                if " ".join(pa) in trained:
                    row = measure("openzl trained [%s]" % " ".join(pa), [Z, "compress", "-c", trained[" ".join(pa)], "-f", "{i}", "-o", "{o}"],
                                  [Z, "decompress", "-f", "{i}", "-o", "{o}"], src, reps, work)
                    row["trained"] = True; rows.append(row)
                elif st.get("train"):
                    rows.append(dict(tool="openzl trained [%s]" % " ".join(pa), error="training failed or unsupported", trained=True))
            zs, xz = T["cli"]["zstd"]["path"] or "zstd", T["cli"]["xz"]["path"] or "xz"
            for nm, c, d in [("zstd -3", [zs, "-3", "-T1", "-q", "-f", "{i}", "-o", "{o}"], [zs, "-d", "-q", "-f", "{i}", "-o", "{o}"]),
                             ("zstd -19", [zs, "-19", "-T1", "-q", "-f", "{i}", "-o", "{o}"], [zs, "-d", "-q", "-f", "{i}", "-o", "{o}"]),
                             ("xz -9", ["sh", "-c", xz + " -9 -T1 -c '{i}' > '{o}'"], ["sh", "-c", xz + " -d -T1 -c '{i}' > '{o}'"])]:
                rows.append(measure(nm, c, d, src, 1 if (nm != "zstd -3" and n > 30e6) else reps, work))
            if st.get("zipnn"):
                zh = os.path.join(HERE, "zipnn_tool.py")
                for dt in st["zipnn"]:
                    rows.append(measure(f"zipnn [{dt}]", [a.python, "-I", zh, "c", dt, "{i}", "{o}"],
                                        [a.python, "-I", zh, "d", dt, "{i}", "{o}"], src, 1 if n > 60e6 else reps, work, internal=True))
            res[f] = dict(set=st["name"], area=st["label"], bytes=n, rows=rows)
            save()
            ok = [r for r in rows if "size" in r]
            best = lambda p: min((r for r in ok if r["tool"].startswith(p)), key=lambda r: r["size"], default=None)
            bk, bz, bt = best("kolmo"), best("openzl -l"), best("openzl trained")
            fmt = lambda r: f"{r['tool']} {r['ratio']:.3f}" if r else "-"
            print(f"{f:50s} kolmo {fmt(bk)} | openzl {fmt(bz)} | trained {fmt(bt)} | errors "
                  f"{[r['tool'] + ':' + r['error'] for r in rows if 'error' in r]}", flush=True)


if __name__ == "__main__":
    main()
