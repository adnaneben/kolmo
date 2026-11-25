#!/usr/bin/env python3
"""Per-feature overview: kind x level, the best and worst file of the kind (kept %, T1, dev + holdout 4 MiB set), and the
decode-speed ratio where the feature wins. usage: overview.py [FEATURE...] [--set dev|holdout|all]"""
import json, os, sys, collections, statistics
HERE = os.path.dirname(os.path.abspath(__file__))
R = [json.loads(l) for l in open(os.path.join(HERE, "raw", "rows.jsonl"))]
args = [a for a in sys.argv[1:] if not a.startswith("--")]
SET = "all"
if "--set" in sys.argv: SET = sys.argv[sys.argv.index("--set") + 1]
B = {(r["file"], r["level"], r["threads"]): r for r in R if r["feature"] == "base" and r.get("ok")}
T = dict(B); T.update({(r["file"], r["level"], r["threads"]): r for r in R if r["feature"] == "base2" and r.get("ok")})   # timing reference
LV = lambda l: "U" if l == 10 else str(l)
KORDER = ["text", "code", "json", "xml", "html", "csv", "tsv", "log", "numeric", "weights", "img8", "img16", "audio", "video", "exe", "container", "genomics", "tar"]
def cells(feat):
    out = collections.defaultdict(list)
    for r in R:
        if r["feature"] != feat or r["threads"] != 1 or not r.get("ok") or r["set"].endswith("small"): continue
        if SET != "all" and r["set"] != SET: continue
        b = B.get((r["file"], r["level"], 1))
        if not b: continue
        t = T[(r["file"], r["level"], 1)]
        out[(r["kind"], r["level"])].append((100 * (r["size"] / b["size"] - 1), r["d_mbs"] / t["d_mbs"] if t["d_mbs"] else 0, os.path.basename(r["file"]).split("__")[1]))
    return out
for feat in args or sorted({r["feature"] for r in R} - {"base", "base2"}):
    C = cells(feat)
    if not C: continue
    levels = sorted({l for _, l in C})
    print("\n### %s (set %s): kept %% vs the level, best .. worst file of the kind; [decode speed x] where the best wins" % (feat, SET))
    print("| kind | " + " | ".join("L" + LV(l) for l in levels) + " |\n|---|" + "---:|" * len(levels))
    for k in KORDER:
        row = []
        for l in levels:
            v = C.get((k, l))
            if not v: row.append(""); continue
            lo = min(v); hi = max(v, key=lambda t: t[0])
            s = "%+.1f" % lo[0] if abs(lo[0] - hi[0]) < 0.05 else "%+.1f..%+.1f" % (lo[0], hi[0])
            if lo[0] < -0.5: s += " [%.2fx]" % lo[1]
            row.append(s)
        if any(row): print("| %s | %s |" % (k, " | ".join(row)))
