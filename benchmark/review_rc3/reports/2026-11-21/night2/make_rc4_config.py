#!/usr/bin/env python3
"""make_rc4_config.py : holdout rc4 vs C2, with the nearest rival per preset in the same suite (from holdout-c2-config.json)."""
import json, hashlib, copy
V = "/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD"
sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
b = json.load(open(V + "/evidence/config/holdout-c2-config.json"))
c = copy.deepcopy(b); c["profile"] = "holdout-rc4"
K4 = V + "/final-builds/rc4/build/kolmo"
c["tools"]["kolmo-rc4"] = {"path": K4, "sha256": sha(K4), "version_argv": [], "license": "project"}
add = []
for x in b["codecs"]:
    if x["id"].startswith("c2/"):
        y = copy.deepcopy(x); y["id"] = "rc4" + x["id"][2:]; y["tool"] = "kolmo-rc4"; add.append(y)
c["codecs"] = b["codecs"] + add
ids = {x["id"] for x in c["codecs"]}
R = ["lz4-1", "zstd-3", "zstd-9", "zstd-19", "xz-9"]
for r in R: assert r in ids, r
H = b["suites"]["holdout-c2"]["inputs"]
c["suites"] = {"holdout-rc4": {"inputs": H, "codecs": [f"{t}/{m}" for t in ("rc4", "c2") for m in ("speed", "balanced", "eco", "hc", "ratio")] + R,
               "purpose": "rc4 vs C2 and the nearest rival per preset (speed lz4-1, balanced zstd-3, eco zstd-9, hc zstd-19, ratio xz-9), one suite"}}
out = V + "/evidence/config/holdout-rc4-config.json"; json.dump(c, open(out, "x"), indent=1); print(out, len(c["codecs"]), "codecs")
