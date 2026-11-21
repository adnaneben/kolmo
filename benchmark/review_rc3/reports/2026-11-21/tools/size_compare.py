#!/usr/bin/env python3
"""size_compare.py OUT A_BIN B_BIN MODES(comma) INPUT... : compressed sizes (T1) for two builds, with round-trip check of B."""
import sys, os, subprocess, json, hashlib
OUT, A, B, MODES, *INP = sys.argv[1:]; os.makedirs(OUT); rows = open(os.path.join(OUT, "rows.jsonl"), "x")
env = {k: v for k, v in os.environ.items() if k != "KOLMO_NATIVE"}
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()
tot = {}
for f in INP:
    ref = sha(f)
    for m in MODES.split(","):
        r = {"input": os.path.basename(f), "mode": m}
        for tag, b in (("a", A), ("b", B)):
            z = os.path.join(OUT, f"{r['input']}.{m}.{tag}.k")
            subprocess.run([b, *(m.split()), "-T", "1", f, z] if m.startswith("-") else [b, "-m", m, "-T", "1", f, z], env=env, capture_output=True)
            r[tag] = os.path.getsize(z) if os.path.exists(z) else None
            if tag == "b" and r[tag]:
                o = z + ".out"; subprocess.run([b, "-d", "-T", "1", z, o], env=env, capture_output=True)
                r["b_rt"] = os.path.exists(o) and sha(o) == ref
                if os.path.exists(o): os.unlink(o)
            if os.path.exists(z): os.unlink(z)
        rows.write(json.dumps(r) + "\n"); rows.flush()
        t = tot.setdefault(m, [0, 0, 0]); t[0] += r["a"] or 0; t[1] += r["b"] or 0; t[2] += (r.get("b_rt") is False)
for m, (a, b, bad) in tot.items(): print(f"{m:10} A {a:>13,} B {b:>13,} B vs A {100*(b-a)/a:+.3f}%  rt_fail {bad}")
json.dump(tot, open(os.path.join(OUT, "totals.json"), "x"))
