#!/usr/bin/env python3
"""Finding 6 check: frames written with KOLMO_NATIVE set are version 11 only when a native id is written.
For every (input, mode, KOLMO_NATIVE): encode with the old build (C1) and new build (C2) at -T 1; record versions and
native usage; decode the C2 frame exactly with C2, C1 and rc3; when the C2 frame is below version 11, also with rc2
and main-1.1.0. Usage: native_version_check.py OUT NEW_BIN OLD_BIN RC3_BIN RC2_BIN V110_BIN INPUT..."""
import sys, os, json, subprocess, hashlib, itertools
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "repo", "benchmark", "review_rc3"))
import frameinfo as F
OUT, NEW, OLD, RC3, RC2, V110, *INPUTS = sys.argv[1:]
os.makedirs(OUT); rows = open(os.path.join(OUT, "rows.jsonl"), "x")
MODES = ["speed", "balanced", "eco", "hc", "ratio"]; NATS = [None, "lz", "huf", "lzma", "strong", "all", "all,strong"]
CLEAN = {k: v for k, v in os.environ.items() if k != "KOLMO_NATIVE"}
def sha(b): return hashlib.sha256(b).hexdigest()
def enc(binp, src, mode, nat, z):
    env = dict(CLEAN); 
    if nat: env["KOLMO_NATIVE"] = nat
    p = subprocess.run([binp, "-m", mode, "-T", "1", src, z], env=env, capture_output=True); return p.returncode
def dec(binp, z, ref):
    o = z + "." + hashlib.md5(binp.encode()).hexdigest()[:6] + ".out"
    p = subprocess.run([binp, "-d", "-T", "1", z, o], env=CLEAN, capture_output=True)
    ok = p.returncode == 0 and os.path.exists(o) and sha(open(o, "rb").read()) == ref
    if os.path.exists(o): os.unlink(o)   # decoded copy only; the frame is kept
    return "exact" if ok else ("refused" if p.returncode else "MISDECODE")
fails = 0; stats = {}
for src, mode, nat in itertools.product(INPUTS, MODES, NATS):
    ref = sha(open(src, "rb").read()); tag = f"{os.path.basename(src)}.{mode}.{(nat or 'unset').replace(',', '+')}"
    zn, zo = os.path.join(OUT, tag + ".c2.k"), os.path.join(OUT, tag + ".c1.k")
    r = {"input": src, "mode": mode, "native": nat, "enc_new": enc(NEW, src, mode, nat, zn), "enc_old": enc(OLD, src, mode, nat, zo)}
    bn, bo = open(zn, "rb").read(), open(zo, "rb").read()
    sn, so = F.summary(F.inspect(bn)), F.summary(F.inspect(bo))
    r.update(ver_new=sn["versions"], ver_old=so["versions"], native_used=sn["native_used"], same_bytes=bn == bo,
             same_but_version=len(bn) == len(bo) and sum(a != b for a, b in zip(bn, bo)) <= 2 * max(1, bn.count(b"CZN1")))
    r["dec"] = {"c2": dec(NEW, zn, ref), "c1": dec(OLD, zn, ref), "rc3": dec(RC3, zn, ref)}
    if max(sn["versions"] or [11]) < 11: r["dec"].update(rc2=dec(RC2, zn, ref), v110=dec(V110, zn, ref))
    # invariants: no misdecode anywhere; a frame with native ids is version 11; new frame equals old except the version byte(s)
    bad = [k for k, v in r["dec"].items() if v != "exact" and k in ("c2", "c1", "rc3")] + [k for k, v in r["dec"].items() if v == "MISDECODE"]
    if sn["native_used"] and max(sn["versions"]) < 11: bad.append("native-below-v11")
    if not (r["same_bytes"] or r["same_but_version"]): bad.append("payload-changed")
    if sn["native_used"] != so["native_used"]: bad.append("native-usage-changed")
    r["status"] = "ok" if not bad else "FAIL:" + ",".join(bad); fails += bool(bad)
    k = (nat or "unset", tuple(so["versions"]), tuple(sn["versions"])); stats[str(k)] = stats.get(str(k), 0) + 1
    rows.write(json.dumps(r) + "\n"); rows.flush()
json.dump({"rows": sum(stats.values()), "failures": fails, "versions(native, old, new)": stats}, open(os.path.join(OUT, "summary.json"), "x"), indent=1)
print(json.dumps({"failures": fails, "rows": sum(stats.values())})); print(json.dumps(stats, indent=0))
