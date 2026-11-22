#!/usr/bin/env python3
"""Phase D full-container compatibility matrix. For each encoder generation x setting x input, encode once (fresh
process, explicit env), then decode the frame with EVERY decoder generation. Classes:
  exact     decoder exit 0 and bytes equal the input
  rejected  decoder exit > 0 (safe refusal)
  SILENT_MISDECODE  exit 0 but different bytes   (a format hazard)
  CRASH     killed by a signal                    (a decoder bug)
  TIMEOUT   no answer within the limit
Refuses an existing output directory; keeps every frame and every non-exact decode.
usage: compat_matrix.py SPEC.json OUT_DIR
SPEC: {"gens": {name: {"bin": path, "sha": "...", "src": "sha"}},
       "encodes": [{"gen": name, "args": [...], "env": {...}, "label": "..."}], "inputs": {name: path}}"""
import json, os, sys, subprocess, hashlib, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frameinfo
spec = json.load(open(sys.argv[1])); OUT = sys.argv[2]; os.mkdir(OUT)
CLEAN = {k: v for k, v in os.environ.items() if not k.startswith(("KOLMO_", "OMP_"))}
def sha(b): return hashlib.sha256(b).hexdigest()
for g, d in spec["gens"].items():
    d["sha256"] = sha(open(d["bin"], "rb").read())
json.dump(spec, open(os.path.join(OUT, "spec-resolved.json"), "x"), indent=1)
rows = open(os.path.join(OUT, "rows.jsonl"), "x"); fd = os.path.join(OUT, "frames"); os.mkdir(fd)
cnt = collections.Counter()
for iname, ipath in spec["inputs"].items():
    data = open(ipath, "rb").read()
    for e in spec["encodes"]:
        tag = f"{e['gen']}.{e['label']}.{iname}"; z = os.path.join(fd, tag + ".k")
        env = dict(CLEAN); env.update(e.get("env", {}))
        p = subprocess.run([spec["gens"][e["gen"]]["bin"], *e["args"], "-T", "1", ipath, z], env=env, capture_output=True, timeout=3600)
        if p.returncode or not os.path.exists(z):
            rows.write(json.dumps({"enc": e["gen"], "label": e["label"], "input": iname, "status": "ENCODE_FAILED", "rc": p.returncode, "err": p.stderr.decode(errors="replace")[-200:]}) + "\n"); cnt["ENCODE_FAILED"] += 1; continue
        zb = open(z, "rb").read()
        try: info = frameinfo.summary(frameinfo.inspect(zb))
        except Exception as ex: info = {"error": repr(ex)}
        for dg, dd in spec["gens"].items():
            back = os.path.join(fd, f"{tag}.dec-{dg}.out")
            try:
                q = subprocess.run([dd["bin"], "-d", "-T", "1", z, back], env=CLEAN, capture_output=True, timeout=1800)
                rc = q.returncode; err = q.stderr.decode(errors="replace")[-160:]
            except subprocess.TimeoutExpired:
                rc, err = None, "timeout"
            if rc is None: st = "TIMEOUT"
            elif rc < 0: st = "CRASH"
            elif rc > 0: st = "rejected"
            else: st = "exact" if os.path.exists(back) and open(back, "rb").read() == data else "SILENT_MISDECODE"
            if st == "exact": os.remove(back)          # verified copy of the input; non-exact outputs are kept
            cnt[st] += 1
            rows.write(json.dumps({"enc": e["gen"], "label": e["label"], "input": iname, "dec": dg, "status": st, "rc": rc,
                                   "frame": os.path.basename(z), "frame_bytes": len(zb), "frame_sha256": sha(zb), "info": info, "err": err}) + "\n")
        rows.flush()
rows.close(); json.dump(dict(cnt), open(os.path.join(OUT, "summary.json"), "x"), indent=1); print(dict(cnt))
sys.exit(1 if cnt["SILENT_MISDECODE"] or cnt["CRASH"] else 0)
