"""Kolmo's own coders in the CLI: every level, 1 and 4 threads, on generated inputs (text, CSV, log, integers, floats,
an image, random bytes, zeros). Every frame must be container version 12, name only codecs this build has (no retired
library id: 1-4, 6, 13, 22) in its block index, decode to its input, give the same bytes with 1 and 4 threads, and
not depend on the environment: the KOLMO_NATIVE switch of earlier builds is gone, so setting it changes nothing.

usage: python3 tests/test_native_modes.py build/kolmo WORK_DIR"""
import os, random, struct, subprocess, sys
K, W = sys.argv[1], sys.argv[2]
os.makedirs(W, exist_ok=True)
rnd = random.Random(20261117)
words = ["kolmo", "block", "the", "of", "native", "coder", "match", "literal", "range", "huffman"]
files = {
    "text.txt": " ".join(rnd.choice(words) for _ in range(60000)).encode(),
    "table.csv": ("id,x,y\n" + "".join(f"{i},{rnd.randint(0, 999)},{rnd.random():.3f}\n" for i in range(20000))).encode(),
    "log.txt": "".join(f"2026-11-17T10:{i // 60 % 60:02d}:{i % 60:02d} host{i % 5} GET /a/{rnd.randint(1, 300)} 200\n" for i in range(12000)).encode(),
    "ints.i32": b"".join(struct.pack("<i", 1000 + i * 3 + rnd.randint(-5, 5)) for i in range(60000)),
    "floats.f64": b"".join(struct.pack("<d", 20.0 + rnd.gauss(0, 1)) for i in range(20000)),
    "image.u8": bytes((x * 3 + y * 5 + rnd.randint(0, 3)) & 255 for y in range(256) for x in range(256)),
    "random.bin": bytes(rnd.getrandbits(8) for _ in range(70000)),
    "zeros.bin": bytes(100000),
    "tiny.txt": b"a",
}
bad = 0
RETIRED = {1, 2, 3, 4, 6, 13, 22}
env0 = {k: v for k, v in os.environ.items() if k != "KOLMO_NATIVE"}
def frame_codecs(f):
    nb = struct.unpack_from("<I", f, 20)[0]
    return [f[32 + 16 * i + 12] for i in range(nb)]
for name, data in files.items():
    src = os.path.join(W, name); open(src, "wb").write(data)
    for mode in ("-1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "-U"):
        outs = []
        for t in ("1", "4"):
            z = os.path.join(W, f"{name}.{mode}.{t}.k"); r = z + ".out"
            subprocess.run([K, mode, "-f", "-T", t, src, "-o", z], check=True, env=env0)
            subprocess.run([K, "-d", "-f", z, "-o", r], check=True, env=env0)
            fz = open(z, "rb").read(); outs.append(fz)
            if fz[:5] != b"CZN1\x0c": print("NOT VERSION 12", name, mode, t, fz[:5]); bad += 1
            if RETIRED & set(frame_codecs(fz)): print("RETIRED CODEC ID", name, mode, t, frame_codecs(fz)); bad += 1
            if open(r, "rb").read() != data: print("MISMATCH", name, mode, t); bad += 1
        if outs[0] != outs[1]: print("THREADS DIFFER", name, mode); bad += 1
        for nat in ("none", "all,strong", "lz"):                 # the retired switch must not change the output
            z = os.path.join(W, f"{name}.{mode}.env.k")
            subprocess.run([K, mode, "-f", "-T", "1", src, "-o", z], check=True, env={**env0, "KOLMO_NATIVE": nat})
            if open(z, "rb").read() != outs[0]: print("KOLMO_NATIVE CHANGED THE OUTPUT", name, mode, nat); bad += 1
print("test_native_modes:", "OK" if not bad else f"{bad} failures")
sys.exit(1 if bad else 0)
