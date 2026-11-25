#!/usr/bin/env python3
"""Small inputs for the per-kind priors: the first 16 KiB, 128 KiB and 1 MiB (cut at a line end) of every text-like
corpus file, appended to the manifest as sets dev-small / holdout-small. usage: mksmall.py CORPUS"""
import hashlib, json, os, sys
C = sys.argv[1]; M = os.path.join(C, "manifest.jsonl")
rows = [json.loads(l) for l in open(M)]
have = {r["file"] for r in rows}
TEXT = {"text", "code", "json", "xml", "html", "csv", "tsv", "log"}
new = []
for r in rows:
    if r["kind"] not in TEXT or r["set"].endswith("-small"): continue
    d = open(os.path.join(C, r["file"]), "rb").read()
    for cap, tag in ((16 << 10, "16k"), (128 << 10, "128k"), (1 << 20, "1m")):
        if len(d) <= cap: continue
        x = d[:cap]; x = x[: x.rindex(b"\n") + 1] if b"\n" in x else x
        st = r["set"] + "-small"; fn = "%s/%s__%s-%s" % (st, r["kind"], r["name"], tag)
        if fn in have: continue
        os.makedirs(os.path.join(C, st), exist_ok=True)
        open(os.path.join(C, fn), "wb").write(x)
        new.append(dict(r, set=st, name=r["name"] + "-" + tag, file=fn, bytes=len(x), sha256=hashlib.sha256(x).hexdigest()))
with open(M, "a") as f:
    for r in new: f.write(json.dumps(r) + "\n")
print(len(new), "small files")
