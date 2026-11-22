#!/usr/bin/env python3
"""make_rn_config.py : holdout 0.0.1-rc1 (library-free) vs rc4 and the nearest rival per preset, one suite."""
import json, hashlib, copy
V = "/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD"
sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
b = json.load(open(V + "/evidence/config/holdout-rc4-config.json"))
c = copy.deepcopy(b); c["profile"] = "holdout-rn"
K = V + "/final-builds/rn/build/kolmo"
c["tools"]["kolmo-rn"] = {"path": K, "sha256": sha(K), "version_argv": [], "license": "project"}
add = []
for x in b["codecs"]:
    if x["id"].startswith("rc4/"):
        y = copy.deepcopy(x); y["id"] = "rn" + x["id"][3:]; y["tool"] = "kolmo-rn"; add.append(y)
c["codecs"] = b["codecs"] + add
R = ["lz4-1", "zstd-3", "zstd-9", "zstd-19", "xz-9"]
H = b["suites"]["holdout-rc4"]["inputs"]
c["suites"] = {"holdout-rn": {"inputs": H, "codecs": [f"{t}/{m}" for t in ("rn", "rc4") for m in ("speed", "balanced", "eco", "hc", "ratio")] + R,
               "purpose": "0.0.1-rc1 (no codec library) vs rc4 and the nearest rival per preset"}}
out = V + "/evidence/config/holdout-rn-config.json"; json.dump(c, open(out, "x"), indent=1); print(out)
