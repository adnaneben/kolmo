#!/usr/bin/env python3
"""make_c2_configs.py : holdout C2 vs C1 config (+ native lane, + rivals for the native lane), from holdout-config.json."""
import json, hashlib, copy
V = "/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD"
sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
b = json.load(open(V + "/evidence/config/holdout-config.json"))
c = copy.deepcopy(b); c["profile"] = "holdout-c2"
K2 = V + "/final-builds/c2/build/kolmo"
c["tools"]["kolmo-c2"] = {"path": K2, "sha256": sha(K2), "version_argv": [], "license": "project"}
c["tools"].pop("kolmo-rc3", None)
cm = {x["id"]: x for x in b["codecs"]}
new = []
for x in b["codecs"]:
    if x["id"].startswith("c1/"):
        new.append(x); y = copy.deepcopy(x); y["id"] = "c2" + x["id"][2:]; y["tool"] = "kolmo-c2"; new.append(y)
    elif not x["id"].startswith("rc3/"):
        new.append(x)
c["codecs"] = new
ids = {x["id"] for x in new}
for need in ("c2/speed+native-all", "c2/balanced+native-all", "c2/hc+native-all+strong", "c2/x", "zstd-1"):
    assert need in ids, need
H = b["suites"]["holdout-main"]["inputs"]
c["suites"] = {
 "holdout-c2": {"inputs": H, "codecs": [f"{t}/{m}" for t in ("c2", "c1") for m in ("speed", "balanced", "eco", "hc", "ratio")],
                "purpose": "C2 vs C1 on the source-disjoint holdout; rivals measured in holdout-main (same machine, harness, inputs)"},
 "holdout-native": {"inputs": H, "codecs": ["c2/speed+native-all", "c2/balanced+native-all", "c2/hc+native-all+strong", "lz4-1", "zstd-1", "zstd-3", "zstd-19"],
                    "purpose": "native lane (opt-in) vs its nearest rivals, measured together"},
}
out = V + "/evidence/config/holdout-c2-config.json"; json.dump(c, open(out, "x"), indent=1); print(out, len(new), "codecs")
