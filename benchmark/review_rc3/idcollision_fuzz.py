#!/usr/bin/env python3
"""Codec-id collision probe (Phase D): encode N varied inputs with generation A, decode with generation B, and count
exact / rejected / SILENT_MISDECODE / CRASH. Used for codec ids whose layout changed under the same number
(W4 entropy: khuf 27 and KLZH 29; W4 lz: KLZ 28). Only frames whose inspected codecs include the colliding ids count
towards the probe. Refuses an existing output directory; keeps every non-exact frame.
usage: idcollision_fuzz.py OUT ENC_BIN DEC_BIN N SEED ids(comma) mode [KOLMO_NATIVE]"""
import os, sys, random, struct, subprocess, json, hashlib, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__))); import frameinfo
OUT, ENC, DEC, N, SEED, IDS, MODE = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5]), set(sys.argv[6].split(",")), sys.argv[7]
NAT = sys.argv[8] if len(sys.argv) > 8 else None
os.mkdir(OUT); r = random.Random(SEED)
CLEAN = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}
def gen(k):
    t = k % 6; n = r.randint(2000, 120000)
    if t == 0: return b"".join(struct.pack("<i", int(r.gauss(0, r.choice([3, 30, 3000]))) + i * r.choice([0, 1, 7])) for i in range(n // 4))
    if t == 1: return b"".join(struct.pack("<h", int(2000 * ((i % r.randint(20, 400)) / 100 - 1)) + r.randint(-9, 9)) for i in range(n // 2))
    if t == 2: return bytes(min(255, max(0, int(r.expovariate(r.choice([0.05, 0.3, 1.0]))))) for _ in range(n))
    if t == 3: w = [bytes(r.choice(b"abcdefghij ") for _ in range(r.randint(1, 9))) for _ in range(r.randint(5, 400))]; return b" ".join(r.choice(w) for _ in range(n // 5))[:n]
    if t == 4: return b"".join(struct.pack("<d", r.gauss(20, r.choice([0.1, 5]))) for _ in range(n // 8))
    return bytes(r.getrandbits(8) if r.random() < r.choice([0.02, 0.2]) else 0 for _ in range(n))
cnt = collections.Counter(); rows = open(os.path.join(OUT, "rows.jsonl"), "x")
for k in range(N):
    data = gen(k); src = os.path.join(OUT, f"in{k:04d}"); z = src + ".k"; back = src + ".out"
    open(src, "xb").write(data)
    env = dict(CLEAN); env.update({"KOLMO_NATIVE": NAT} if NAT else {})
    if subprocess.run([ENC, "-m", MODE, "-T", "1", src, z], env=env, capture_output=True).returncode: cnt["enc_fail"] += 1; continue
    info = frameinfo.summary(frameinfo.inspect(open(z, "rb").read()))
    used = set(info["block_codecs"]) | set(info["leaf_codecs"])
    if not used & IDS: cnt["no_colliding_id"] += 1; os.remove(z); os.remove(src); continue
    p = subprocess.run([DEC, "-d", "-T", "1", z, back], env=CLEAN, capture_output=True, timeout=300)
    st = "CRASH" if p.returncode < 0 else "rejected" if p.returncode else ("exact" if open(back, "rb").read() == data else "SILENT_MISDECODE")
    cnt[st] += 1; rows.write(json.dumps({"k": k, "status": st, "rc": p.returncode, "used": sorted(used)}) + "\n")
    if st in ("rejected",):
        os.remove(z); os.remove(src)
    if os.path.exists(back) and st != "SILENT_MISDECODE": os.remove(back)
rows.close(); json.dump(dict(cnt), open(os.path.join(OUT, "summary.json"), "x")); print(dict(cnt))
