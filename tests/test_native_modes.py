"""Kolmo's own coders in the CLI (native lane, rc3 wave 2): every mode with KOLMO_NATIVE=all and all,strong, 1 and 4
threads, on generated inputs (text, CSV, log, integers, floats, an image, random bytes, zeros). Each frame must decode
to its input with KOLMO_NATIVE unset (the decoder needs no opt-in), and the output with 1 and 4 threads must be the
same. The default output (KOLMO_NATIVE unset) is covered by the determinism check.

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
env0 = {k: v for k, v in os.environ.items() if k != "KOLMO_NATIVE"}
for name, data in files.items():
    src = os.path.join(W, name); open(src, "wb").write(data)
    for nat in ("all", "all,strong"):
        for mode in ("speed", "balanced", "eco", "hc", "ratio"):
            outs = []
            for t in ("1", "4"):
                z = os.path.join(W, f"{name}.{mode}.{t}.k"); r = z + ".out"
                subprocess.run([K, "-m", mode, "-T", t, src, z], check=True, env={**env0, "KOLMO_NATIVE": nat})
                subprocess.run([K, "-d", z, r], check=True, env=env0)
                fz = open(z, "rb").read(); outs.append(fz)
                if open(r, "rb").read() != data: print("MISMATCH", name, nat, mode, t); bad += 1
            if outs[0] != outs[1]: print("THREADS DIFFER", name, nat, mode); bad += 1
print("test_native_modes:", "OK" if not bad else f"{bad} failures")
sys.exit(1 if bad else 0)
