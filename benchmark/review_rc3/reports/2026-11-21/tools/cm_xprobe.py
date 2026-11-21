#!/usr/bin/env python3
"""CM model-nibble probe (C2-cm review, 2026-11-20).

Encodes N varied inputs with ENC and the given encoder arguments (default: -x), checks that ENC decodes its own frame
exactly, then decodes with each DEC and counts outcomes per CM model nibble found in the frame's codec-7 blocks:
exact / rejected / SILENT_MISDECODE / CRASH. Inputs are generated as in idcollision_fuzz.py, plus text and log
shapes, so that -x writes CM streams. The output directory must not exist. Non-exact frames are kept.

usage: cm_xprobe.py OUT ENC N SEED DEC[,DEC...] [-- ENCARGS...]
       (DEC may be name=path)
"""
import os, sys, random, struct, subprocess, json, collections
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "repo", "benchmark", "review_rc3"))
import frameinfo

OUT, ENC, N, SEED = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
DECS = [d.split("=", 1) if "=" in d else (os.path.basename(os.path.dirname(os.path.dirname(d))), d) for d in sys.argv[5].split(",")]
ENCARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["-x"]
os.mkdir(OUT); r = random.Random(SEED)
CLEAN = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}

def gen(k):
    t = k % 8; n = r.randint(2000, 160000)
    if t == 0: return b"".join(struct.pack("<i", int(r.gauss(0, r.choice([3, 30, 3000]))) + i * r.choice([0, 1, 7])) for i in range(n // 4))
    if t == 1: return b"".join(struct.pack("<h", int(2000 * ((i % r.randint(20, 400)) / 100 - 1)) + r.randint(-9, 9)) for i in range(n // 2))
    if t == 2: return bytes(min(255, max(0, int(r.expovariate(r.choice([0.05, 0.3, 1.0]))))) for _ in range(n))
    if t == 3:
        w = [bytes(r.choice(b"abcdefghijklmnop ") for _ in range(r.randint(1, 9))) for _ in range(r.randint(5, 400))]
        return b" ".join(r.choice(w) for _ in range(n // 5))[:n]
    if t == 4: return b"".join(struct.pack("<d", r.gauss(20, r.choice([0.1, 5]))) for _ in range(n // 8))
    if t == 5: return bytes(r.getrandbits(8) if r.random() < r.choice([0.02, 0.2]) else 0 for _ in range(n))
    if t == 6:
        lv = [b"INFO", b"WARN", b"DEBUG", b"ERROR"]
        return b"".join(b"2026-11-%02d %02d:%02d:%02d %s worker-%d request %d took %d ms\n" % (r.randint(1, 30), r.randint(0, 23), r.randint(0, 59), r.randint(0, 59), r.choice(lv), r.randint(1, 9), r.randint(1, 10**6), r.randint(1, 900)) for _ in range(n // 60))
    return b"".join(b"%d,%s,%.3f,%d\n" % (i, r.choice([b"alpha", b"beta", b"gamma"]), r.gauss(5, 2), r.randint(0, 99)) for i in range(n // 22))

def frames_with_offsets(z):
    if z[:4] != b"CZNS": return [(0, frameinfo.container(z))]
    out, i = [], 12
    while i + 8 <= len(z):
        (n,) = struct.unpack_from("<Q", z, i); i += 8
        if n == 0: break
        out.append((i, frameinfo.container(z, i))); i += n
    return out

def cm_nibbles(z):
    """model nibbles of codec-7 block streams (block payload: 4-byte plan prefix, then the CM stream)"""
    out = set()
    for off, f in frames_with_offsets(z):
        if "error" in f: out.add("bad-frame"); continue
        pos = off + 32 + 16 * f["nblocks"]
        for e in f["blocks"]:
            if e["codec"] == 7:
                # plan prefix (4 bytes) then, for transformed blocks (tflag != 0), the transform's own header; the
                # CM stream starts with size u32 whose top nibble is 8 for an extended header. Scan the first bytes.
                p = z[pos:pos + e["comp"]]; found = "basic"
                for i in range(4, min(len(p) - 5, 40)):
                    if p[i + 3] >> 4 == 8 and (p[i + 4] >> 4) in (3, 5, 6, 7, 9, 10, 14, 15):
                        found = p[i + 4] >> 4; break
                out.add(found)
            pos += e["comp"]
    return out

cnt = collections.Counter(); rows = open(os.path.join(OUT, "rows.jsonl"), "x")
for k in range(N):
    data = gen(k); src = os.path.join(OUT, f"in{k:04d}"); z = src + ".k"
    open(src, "xb").write(data)
    if subprocess.run([ENC, *ENCARGS, "-T", "1", src, z], env=CLEAN, capture_output=True).returncode:
        cnt["enc_fail"] += 1; rows.write(json.dumps({"k": k, "status": "enc_fail"}) + "\n"); continue
    zb = open(z, "rb").read(); nib = sorted(map(str, cm_nibbles(zb))); key = "+".join(nib) or "no-cm"
    row = {"k": k, "bytes": len(data), "frame": len(zb), "cm_nibbles": nib, "dec": {}}
    keep = False
    for name, dec in [("self", ENC)] + DECS:
        back = src + "." + name + ".out"
        try:
            p = subprocess.run([dec, "-d", "-T", "1", z, back], env=CLEAN, capture_output=True, timeout=600)
            st = "CRASH" if p.returncode < 0 else "rejected" if p.returncode else ("exact" if open(back, "rb").read() == data else "SILENT_MISDECODE")
        except subprocess.TimeoutExpired:
            st = "TIMEOUT"
        row["dec"][name] = st; cnt[(name, key, st)] += 1
        if st in ("SILENT_MISDECODE", "CRASH", "TIMEOUT") or (name == "self" and st != "exact"): keep = True
        if os.path.exists(back) and st != "SILENT_MISDECODE": os.remove(back)
    rows.write(json.dumps(row) + "\n")
    if not keep: os.remove(z); os.remove(src)
rows.close()
summ = {f"{a} | nibbles {b} | {c}": v for (a, b, c), v in sorted(cnt.items(), key=str)} if cnt else {}
json.dump(summ, open(os.path.join(OUT, "summary.json"), "x"), indent=1); print(json.dumps(summ, indent=1))
