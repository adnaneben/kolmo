#!/usr/bin/env python3
"""Data for the ratio / speed chart: per (feature, subset, threads, level) the corpus totals. Ratio = input bytes / frame
bytes; speeds from whole-process wall time. A file the feature did not touch at that level keeps the level's frame and
the level's low-load timing (base2). A T10 point is kept only when every file whose frame the feature changed has its
own T10 row. usage: chartdata.py OUT.json"""
import json, os, sys, collections
HERE = os.path.dirname(os.path.abspath(__file__))
R = [json.loads(l) for l in open(os.path.join(HERE, "raw", "rows.jsonl"))]
B = {(r["file"], r["level"], r["threads"]): r for r in R if r["feature"] == "base" and r.get("ok")}
T = {(r["file"], r["level"], r["threads"]): r for r in R if r["feature"] == "base2" and r.get("ok")}
F = collections.defaultdict(dict)
for r in R:
    if r.get("ok") and r["feature"] not in ("base", "base2"): F[r["feature"]][(r["file"], r["level"], r["threads"])] = r
man = [json.loads(l) for l in open(os.path.join(HERE, "raw", "manifest.jsonl"))]
TEXTLIKE = {"text", "code", "json", "xml", "html", "csv", "tsv", "log"}
SUB = {
    "all": lambda m: m["set"] in ("dev", "holdout"),
    "holdout": lambda m: m["set"] == "holdout",
    "text": lambda m: m["set"] in ("dev", "holdout") and m["kind"] in {"text", "code", "json", "xml", "html"},
    "tables": lambda m: m["set"] in ("dev", "holdout") and m["kind"] in {"csv", "tsv", "log"},
    "binary": lambda m: m["set"] in ("dev", "holdout") and m["kind"] not in TEXTLIKE | {"genomics"},
    "small": lambda m: m["set"] in ("dev-small", "holdout-small"),
}
def point(files, lev, th, feat=None):
    n = cs = ct = dt = 0; changed = missing = touched = 0
    for m in files:
        f = m["file"]; b = B.get((f, lev, th)) or B.get((f, lev, 1)); t = T.get((f, lev, th))
        if not b or not t: return None
        size, c, d = b["size"], t["c_wall"], t["d_wall"]
        if feat:
            r = F[feat].get((f, lev, th)); r1 = F[feat].get((f, lev, 1))
            if r1: touched += 1
            if r: size, c, d = r["size"], r["c_wall"], r["d_wall"]
            elif r1:
                size = r1["size"]
                if r1["size"] != b["size"]: missing += 1
            if (r or r1) and (r or r1)["size"] != b["size"]: changed += 1
        n += m["bytes"]; cs += size; ct += c; dt += d
    if feat and (not touched or missing): return None
    return {"ratio": round(n / cs, 4), "c": round(n / 1e6 / ct, 2), "d": round(n / 1e6 / dt, 2), "cd": round(n / 1e6 / (ct + dt), 3),
            "bytes": n, "size": cs, "ct": round(ct, 3), "dt": round(dt, 3), "files": len(files), "changed": changed}
out = {"levels": {}, "features": {}}
for sk, fn in SUB.items():
    files = [m for m in man if fn(m)]
    for th in (1, 10):
        out["levels"]["%s|%d" % (sk, th)] = {str(l): p for l in range(1, 11) if (p := point(files, l, th))}
        for feat in F:
            pts = {}
            for l in range(1, 11):
                p = point(files, l, th, feat)
                if p and p["changed"]: pts[str(l)] = p
            if pts: out["features"]["%s|%s|%d" % (feat, sk, th)] = pts
out["subsets"] = {k: [m["name"] for m in man if f(m)] for k, f in SUB.items()}
json.dump(out, open(sys.argv[1], "w"), separators=(",", ":"))
print(len(out["features"]), "feature series", os.path.getsize(sys.argv[1]), "bytes")
