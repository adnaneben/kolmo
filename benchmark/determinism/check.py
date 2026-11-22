"""Cross-platform determinism: the same frames on every CPU, compiler and thread count.

usage: python3 benchmark/determinism/check.py KOLMO WORK_DIR [--write] [--budgets 64M,512M,4G,unlimited]
Builds a fixed set of inputs from a seeded generator only (no third-party data, and nothing that changes with the code),
compresses each at every setting below with 1 and 4 threads, and compares the SHA-256 of every frame with
benchmark/determinism/expected.sha256. --write regenerates that file (do it only when an encoder change is meant
to change output, and say so in the commit). Also checks that 1 and 4 threads give the same frame, and that every
frame decodes back to its input.
--budgets runs every encode and decode once per memory budget (KOLMO_MEM_FORCE_BUDGET: the governor behaves as if that
much memory were available) and requires the same frames each time: memory changes speed, never bytes
(docs/MEMORY-MODEL.md).
"""
import hashlib
import os
import random
import struct
import subprocess
import sys

KOLMO, W = sys.argv[1], sys.argv[2]
WRITE = "--write" in sys.argv[3:]
BUDGETS = [None]
if "--budgets" in sys.argv[3:]:
    BUDGETS = sys.argv[sys.argv.index("--budgets") + 1].split(",")
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
EXPECTED = os.path.join(HERE, "expected.sha256")
os.makedirs(W, exist_ok=True)

# 0.1.0 levels (docs/LEVELS.md). The frames are those of the 0.0.x settings they replace (in brackets): the level
# table maps onto the same encoder settings, so these digests did not change with the command line.
SETTINGS = [["-1"],                                  # [-m speed]
            ["-1", "--analysis", "full"],            # [-m rt]
            [],                                      # level 3, the default [balanced]
            ["-3", "--analysis", "quick"],           # [-m fast -q]
            ["-5"], ["-7"], ["-9"],                  # [-m eco], [-m hc], [-m ratio]
            ["-8"],                                  # [-m max -l 1]
            ["-2"], ["-4"], ["-6"],                  # levels new in 0.1.0 (lane 3's measured table)
            ["-U", "--codec-level", "1"], ["-U"], ["-U", "--codec-level", "2"],   # [-x -l 1], [-x], [-x -l 2]
            ["-3", "--native-leaf"]]                 # [-m balanced --native-leaf]
TYPED = [["-3", "--typed-huf"]]   # one thread only by design [-m fast --typed-huf]


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
    y4m = bytearray(b"YUV4MPEG2 W96 H64 F25:1 Ip A1:1 C420jpeg\n")   # format 2: the video route (codec 20)
    for f in range(12):
        y4m += b"FRAME\n" + bytes(((x + 2 * f) * 3 + y * 5 + rnd.randint(0, 3)) & 255 for y in range(64) for x in range(96))
        y4m += bytes((128 + k * 9 + ((x + y + f) & 15)) & 255 for k in range(2) for y in range(32) for x in range(48))
    yield "video.y4m", bytes(y4m), False


ENV = dict(os.environ)
ENV.pop("KOLMO_MEMLIMIT", None)


def run(args):
    r = subprocess.run([KOLMO, *args], capture_output=True, env=ENV)
    if r.returncode:
        sys.exit(f"kolmo {' '.join(args)} failed: {r.stderr.decode(errors='replace')[:200]}")


lines, bad = [], []
for name, data, typed in inputs():
    p = os.path.join(W, name)
    open(p, "wb").write(data)
    for s in SETTINGS + (TYPED if typed else []):
        digests = []
        for budget in BUDGETS:
            if budget: ENV["KOLMO_MEM_FORCE_BUDGET"] = budget
            else: ENV.pop("KOLMO_MEM_FORCE_BUDGET", None)
            for t in (["1"] if s in TYPED else ["1", "4"]):
                z, back = p + ".k", p + ".back"
                run([*s, "-f", "-T", t, p, "-o", z])
                run(["-d", "-f", z, "-o", back])
                if open(back, "rb").read() != data:
                    bad.append(f"round trip: {name} {' '.join(s)} -T {t} budget {budget}")
                digests.append(hashlib.sha256(open(z, "rb").read()).hexdigest())
        if len(set(digests)) != 1:
            bad.append(f"thread count or memory budget changes the frame: {name} {' '.join(s)}")
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
print(f"{len(lines)} frames checked{' under budgets ' + ','.join(BUDGETS) if BUDGETS != [None] else ''}; {'OK' if not bad else 'FAILED'}")
sys.exit(1 if bad else 0)
