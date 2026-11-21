#!/usr/bin/env python3
"""rc4_identity.py V : rc4 must equal C2 bytes on non-image, non-eco-text frames; equal C3r3 on images; equal C1 on eco text.
Writes evidence/rc4-identity/rows.jsonl. Prints a summary line."""
import sys, os, subprocess, json, hashlib
V = sys.argv[1]; O = V + "/evidence/rc4-identity"; os.makedirs(O)
B = {k: V + "/final-builds/%s/build/kolmo" % d for k, d in [("rc4", "rc4"), ("c2", "c2"), ("c1", "c1"), ("c3r3", "c3image-r3")]}
B["ecoalt"] = V + "/c2-ecoalt/build/kolmo"
I = V + "/evidence/native-matrix-c1-release/inputs"; H = V + "/corpus-holdout"; D = V + "/corpus-domain"
ins = [os.path.join(I, f) for f in sorted(os.listdir(I))] + [H + "/reymont", H + "/chelsea.ppm"] + [os.path.join(D, f) for f in sorted(os.listdir(D)) if f.endswith(".ppm")]
modes = [["-m", m] for m in ("speed", "balanced", "eco", "hc", "ratio")] + [["-x"]]
env = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}
def enc(b, f, a):
    o = O + "/tmp.k"; subprocess.run([b, "-T", "1"] + a + [f, o], env=env, check=True, capture_output=True)
    d = open(o, "rb").read(); os.remove(o); return hashlib.sha256(d).hexdigest(), len(d)
bad = 0; n = 0; rows = open(O + "/rows.jsonl", "x")
for f in ins:
    img = f.endswith((".ppm", ".pgm", ".pam"))
    for a in modes:
        h = {k: enc(b, f, a) for k, b in B.items()}
        # rule: kimg (ratio, -x) on images -> C3r3; eco -> ecoalt branch (A6); everything else -> C2
        ref = "c3r3" if img and a in (["-m", "ratio"], ["-x"]) else ("ecoalt" if a == ["-m", "eco"] else "c2")
        ok = any(h["rc4"] == h[r] for r in ref.split("|"))
        n += 1; bad += (not ok)
        rows.write(json.dumps({"input": os.path.basename(f), "args": a, "ref": ref, "ok": ok, "eq": [k for k in h if k != "rc4" and h[k] == h["rc4"]], "sizes": {k: v[1] for k, v in h.items()}}) + "\n")
print("identity frames=%d mismatched=%d" % (n, bad))
