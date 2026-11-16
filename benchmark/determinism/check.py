"""Cross-platform determinism: the same frames on every CPU, compiler and thread count.

usage: python3 benchmark/determinism/check.py KOLMO WORK_DIR [--write]
Builds a fixed set of inputs from a seeded generator only (no third-party data, and nothing that changes with the code),
compresses each at every setting below with 1 and 4 threads, and compares the SHA-256 of every frame with
benchmark/determinism/expected.sha256. --write regenerates that file (do it only when an encoder change is meant
to change output, and say so in the commit). Also checks that 1 and 4 threads give the same frame, and that every
frame decodes back to its input.
"""
import hashlib
import os
import random
import struct
import subprocess
import sys

KOLMO, W = sys.argv[1], sys.argv[2]
WRITE = "--write" in sys.argv[3:]
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
EXPECTED = os.path.join(HERE, "expected.sha256")
os.makedirs(W, exist_ok=True)

SETTINGS = [["-m", "speed"], ["-m", "rt"], [], ["-m", "fast", "-q"], ["-m", "eco"], ["-m", "hc"], ["-m", "ratio"],
            ["-m", "max", "-l", "1"], ["-x", "-l", "1"], ["-x"], ["-x", "-l", "2"], ["-m", "balanced", "--native-leaf"]]
TYPED = [["-m", "fast", "--typed-huf"]]   # one thread only by design


def inputs():
    rnd = random.Random(20261116)
    words = [("w%d" % i) * (1 + i % 3) for i in range(3000)]           # a fixed vocabulary, Zipf-like use
    text = " ".join(words[min(int(rnd.paretovariate(1.1)) - 1, 2999)] + ("\n" if rnd.random() < 0.08 else "") for _ in range(110000))
    yield "text.txt", text.encode(), False
    rows = ["country,year,population,co2,share"] + [f"C{i // 40},{1980 + i % 40},{1000000 + i * 37},{rnd.randint(0, 99999) / 100:.2f},{rnd.random():.4f}" for i in range(20000)]
    yield "table.csv", ("\n".join(rows) + "\n").encode(), False
    yield "log.txt", "".join(f"2026-11-16T10:{(i // 60) % 60:02d}:{i % 60:02d}Z host{i % 7} GET /api/item/{rnd.randint(1, 5000)} 200 {rnd.randint(1, 900)}ms\n" for i in range(15000)).encode(), False
    v = 0
    walk = []
    for _ in range(150000):
        v += rnd.randint(-300, 300)
        walk.append(max(-32768, min(32767, v)))
    yield "walk.i16", struct.pack(f"<{len(walk)}h", *walk), False
    yield "stamps.i64", struct.pack("<60000q", *[1763290000000 + i * 1000 + rnd.randint(0, 9) for i in range(60000)]), False
    yield "floats.f64", struct.pack("<40000d", *[round(rnd.gauss(20, 5), 2) for _ in range(40000)]), False
    yield "image.u8", bytes((x * 3 + y * 5 + rnd.randint(0, 7)) & 255 for y in range(256) for x in range(512)), False
    d = b"{'descr': '<i2', 'fortran_order': False, 'shape': (150000,), }"
    d = d.ljust(128 - 10 - 1) + b"\n"                        # magic(6) + version(2) + length(2) + dict = 128 bytes
    hdr = b"\x93NUMPY\x01\x00" + struct.pack("<H", len(d)) + d
    yield "walk.npy", hdr + struct.pack(f"<{len(walk)}h", *walk), True
    pcm = struct.pack(f"<{len(walk)}h", *walk)
    wav = b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 44100, 88200, 2, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm
    yield "walk.wav", wav, True


def run(args):
    r = subprocess.run([KOLMO, *args], capture_output=True)
    if r.returncode:
        sys.exit(f"kolmo {' '.join(args)} failed: {r.stderr.decode(errors='replace')[:200]}")


lines, bad = [], []
for name, data, typed in inputs():
    p = os.path.join(W, name)
    open(p, "wb").write(data)
    for s in SETTINGS + (TYPED if typed else []):
        digests = []
        for t in (["1"] if s in TYPED else ["1", "4"]):
            z, back = p + ".k", p + ".back"
            run([*s, "-T", t, p, z])
            run(["-d", z, back])
            if open(back, "rb").read() != data:
                bad.append(f"round trip: {name} {' '.join(s)} -T {t}")
            digests.append(hashlib.sha256(open(z, "rb").read()).hexdigest())
        if len(set(digests)) != 1:
            bad.append(f"thread count changes the frame: {name} {' '.join(s)}")
        lines.append(f"{digests[0]}  {name}  {' '.join(s) or '(default)'}")

if WRITE:
    open(EXPECTED, "w").write("\n".join(lines) + "\n")
    print(f"wrote {len(lines)} digests to {EXPECTED}")
else:
    want = open(EXPECTED).read().splitlines()
    if want != lines:
        diff = [f"  expected {a}\n  got      {b}" for a, b in zip(want, lines) if a != b]
        bad.append(f"{len(diff)} frame(s) differ from expected.sha256:\n" + "\n".join(diff[:10]))
for b in bad:
    print("FAIL", b)
print(f"{len(lines)} frames checked; {'OK' if not bad else 'FAILED'}")
sys.exit(1 if bad else 0)
