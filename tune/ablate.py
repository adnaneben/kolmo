#!/usr/bin/env python3
"""Check tuned settings one at a time, per mode, with a strict speed guard.

The genetic search scores total size across modes with a loose speed penalty, so it can buy ratio in a fast mode
with that mode's speed. This script takes tune/best.json, tries each changed setting alone against the defaults in
every mode, and accepts a change only if it makes that mode smaller and keeps compression and decompression within
--tol of the defaults (best of --reps runs). It then verifies the accepted set together.

  python3 tune/ablate.py --corpus data --modes fast,eco,hc
"""
import argparse, json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga


def measure(exe, files, mode, tmp, reps):
    best = None
    for _ in range(reps):
        r = ga.run(exe, files, [mode], tmp)
        best = r if best is None else (r[0], min(best[1], r[1]), min(best[2], r[2]))
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", default="data"); ap.add_argument("--modes", default="fast,eco,hc")
    ap.add_argument("--reps", type=int, default=3); ap.add_argument("--tol", type=float, default=0.03)
    a = ap.parse_args()
    best = json.load(open(os.path.join(ga.ROOT, "tune", "best.json")))["settings"]
    corpus = a.corpus if os.path.isabs(a.corpus) else os.path.join(ga.ROOT, a.corpus)
    files = sorted(os.path.join(corpus, f) for f in os.listdir(corpus) if os.path.isfile(os.path.join(corpus, f)))
    bindir = os.path.join(ga.ROOT, "tune", "bin"); os.makedirs(bindir, exist_ok=True)
    tmp = "/tmp/czablate"; os.makedirs(tmp, exist_ok=True)
    changed = [k for k in best if best[k] != ga.DEFAULT[k]]
    accepted = {}
    out = {"modes": {}}
    for mode in a.modes.split(","):
        base = measure(ga.build(ga.DEFAULT, bindir), files, mode, tmp, a.reps)
        print(f"{mode}: defaults {base[0]:,.0f} B, c {base[1]:.2f} s, d {base[2]:.2f} s", flush=True)
        rows = []
        for k in changed:
            g = dict(ga.DEFAULT); g[k] = best[k]
            s, c, d = measure(ga.build(g, bindir), files, mode, tmp, a.reps)
            ok = s < base[0] and c <= base[1] * (1 + a.tol) and d <= base[2] * (1 + a.tol)
            rows.append({"param": k, "value": best[k], "size_rel": s / base[0], "c_rel": c / base[1], "d_rel": d / base[2], "accept": ok})
            print(f"  {k}={best[k]}: size {s / base[0]:.5f}, c {c / base[1]:.3f}, d {d / base[2]:.3f} {'ACCEPT' if ok else ''}", flush=True)
            if ok:
                accepted.setdefault(k, []).append(mode)
        out["modes"][mode] = {"defaults": base, "single": rows}
    # a setting is adopted only if no mode rejected it for speed and at least one mode gained
    adopt = {}
    for k in accepted:
        bad = any(r["param"] == k and not r["accept"] and r["size_rel"] < 1.0 for m in out["modes"].values() for r in m["single"])
        worse = any(r["param"] == k and r["size_rel"] > 1.0005 for m in out["modes"].values() for r in m["single"])
        if not bad and not worse:
            adopt[k] = best[k]
    g = dict(ga.DEFAULT); g.update(adopt)
    print("adopt:", adopt, flush=True)
    out["adopt"] = adopt; out["combined"] = {}
    for mode in a.modes.split(","):
        base = out["modes"][mode]["defaults"]
        s, c, d = measure(ga.build(g, bindir), files, mode, tmp, a.reps)
        out["combined"][mode] = {"size_rel": s / base[0], "c_rel": c / base[1], "d_rel": d / base[2]}
        print(f"combined {mode}: size {s / base[0]:.5f}, c {c / base[1]:.3f}, d {d / base[2]:.3f}", flush=True)
    json.dump(out, open(os.path.join(ga.ROOT, "tune", "ablate.json"), "w"), indent=2)


if __name__ == "__main__":
    main()
