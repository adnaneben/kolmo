"""Byte identity of two kolmo builds: every mode and option, 1 and 4 threads, on repository files and generated data.

usage: python3 benchmark/rc3/structure/compare_frames.py OLD_KOLMO NEW_KOLMO WORK_DIR [--quick]
Compresses each input with both binaries and compares the frames byte for byte; decodes the new frame with the old
binary and the old frame with the new one. Prints one line per input and a summary; exits 1 on any difference.
Used by the structure lane of rc3 to show that moving the sources changed no output (benchmark/rc3/structure/RESULTS.md).
"""
import concurrent.futures as cf
import hashlib
import io
import os
import random
import struct
import subprocess
import sys
import tarfile

OLD, NEW, W = sys.argv[1:4]
QUICK = "--quick" in sys.argv[4:]
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
os.makedirs(W, exist_ok=True)

FAST = [["-m", "speed"], ["-m", "rt"], [], ["-m", "fast"], ["-m", "fast", "-q"], ["-m", "fast", "-Q"], ["-m", "fast", "-n"],
        ["-m", "eco"], ["-m", "hc"], ["-m", "ratio"], ["-m", "balanced", "--native-leaf"], ["-m", "fast", "-B", "1"]]
SLOW = [["-m", "max"], ["-m", "max", "-l", "1"], ["-x"], ["-x", "-l", "1"], ["-x", "-l", "2"]]
ONE_THREAD = [["-m", "fast", "--typed-huf"]]
SLOW_LIMIT = 1_200_000          # -x and max only on inputs up to this size (time)


def gen():
    rnd = random.Random(31337)
    yield "zeros.bin", bytes(300_000)
    yield "random.bin", rnd.randbytes(400_000)
    yield "tiny.txt", b"a"
    yield "empty.bin", b""
    words = ["alpha", "beta", "gamma", "delta", "kolmo", "frame", "block", "stream", "zeta", "omega"] + [f"w{i}" for i in range(500)]
    yield "text.txt", " ".join(rnd.choice(words) + ("\n" if rnd.random() < 0.1 else "") for _ in range(250_000)).encode()
    rows = ["id;city;temp;flag;note"] + [f"{i};C{rnd.randint(1, 40)};{rnd.gauss(15, 8):.1f};{rnd.choice('YN')};\"n,{i % 13}\"" for i in range(90_000)]
    yield "semicolon.csv", ("\n".join(rows) + "\n").encode()
    yield "wide.tsv", ("\n".join("\t".join(str(rnd.randint(0, 10 ** (c % 9))) for c in range(40)) for _ in range(6000)) + "\n").encode()
    yield "access.log", "".join(f"10.0.{i % 256}.{rnd.randint(1, 254)} - - [16/Nov/2026:10:{i // 60 % 60:02d}:{i % 60:02d} +0000] \"GET /p/{rnd.randint(1, 900)} HTTP/1.1\" {rnd.choice([200, 200, 304, 404])} {rnd.randint(100, 9000)}\n" for i in range(40_000)).encode()
    yield "records.jsonl", "".join(f'{{"id":{i},"name":"user{rnd.randint(1, 3000)}","score":{rnd.random():.5f},"tags":["a","b{i % 7}"]}}\n' for i in range(30_000)).encode()
    yield "minified.json", ("[" + ",".join(f'{{"k":{i},"v":"{rnd.randint(0, 99)}"}}' for i in range(40_000)) + "]").encode()
    v = 0
    a = []
    for _ in range(300_000):
        v += rnd.randint(-40, 40)
        a.append(v)
    yield "series.i32", struct.pack(f"<{len(a)}i", *a)
    yield "series.i16", struct.pack(f"<{len(a)}h", *[max(-32768, min(32767, x)) for x in a])
    yield "series.i64", struct.pack("<100000q", *[1_700_000_000_000 + 1000 * i + rnd.randint(0, 3) for i in range(100_000)])
    yield "floats.f32", struct.pack("<120000f", *[rnd.gauss(0, 1) for _ in range(120_000)])
    yield "dec.f64", struct.pack("<80000d", *[round(rnd.uniform(-50, 50), 3) for _ in range(80_000)])
    d = b"{'descr': '|u1', 'fortran_order': False, 'shape': (300, 400, 3), }"
    d = d.ljust(128 - 10 - 1) + b"\n"
    img = bytes(((x * 2 + y + c * 40 + rnd.randint(0, 5)) & 255) for y in range(300) for x in range(400) for c in range(3))
    yield "image.npy", b"\x93NUMPY\x01\x00" + struct.pack("<H", len(d)) + d + img
    pcm = struct.pack(f"<{len(a)}h", *[max(-32768, min(32767, x * 7)) for x in a])
    yield "mono.wav", b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 44100, 88200, 2, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm
    fq = []
    for r in range(12_000):
        s = "".join(rnd.choice("ACGT") for _ in range(100))
        fq.append(f"@read{r}:{r * 7} 1/1\n{s}\n+\n{''.join(chr(35 + rnd.randint(0, 40)) for _ in range(100))}\n")
    yield "reads.fastq", "".join(fq).encode()
    big = io.BytesIO()
    with tarfile.open(fileobj=big, mode="w", format=tarfile.USTAR_FORMAT) as t:
        for name, data in [("a.txt", b"small member\n" * 100), ("big.bin", bytes((i * 31 + (i >> 9)) & 255 for i in range(1_300_000))),
                           ("c.csv", ("\n".join(rows[:20000]) + "\n").encode())]:
            ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = 0
            t.addfile(ti, io.BytesIO(data))
    yield "archive.tar", big.getvalue()
    yield "big.csv", ("\n".join(rows * 3) + "\n").encode()          # several blocks at the fast modes


def repo_files():
    picks = ["tests/kolmotest.c", "tests/legacy_frames.h", "README.md", "SPEC.md", "CHANGELOG.md",
             "benchmark/rc1/results/inprocess.jsonl", "benchmark/openzl/results.json", "benchmark/wide/decision.png",
             "research/STACK-AUDIT.md", "vendor/zstd/compress/zstd_compress.c", "vendor/brotli/enc/encode.c"]
    for p in picks:
        f = os.path.join(ROOT, p)
        if os.path.exists(f):
            yield "repo_" + p.replace("/", "_"), open(f, "rb").read()
    yield "exe_kolmo_old", open(OLD, "rb").read()                   # an x86-64 executable (E8/E9 transform)


def run(cmd):
    r = subprocess.run(cmd, capture_output=True)
    return r.returncode, r.stderr.decode(errors="replace")[:200]


def one(name, data, settings, threads):
    src = os.path.join(W, name)
    if not os.path.exists(src):
        open(src, "wb").write(data)
    tag = "_".join(a.strip("-") for a in settings) or "default"
    out = {}
    for which, exe in (("old", OLD), ("new", NEW)):
        f = os.path.join(W, f"{name}.{tag}.T{threads}.{which}.kz")
        rc, err = run([exe, *settings, "-T", str(threads), src, f])
        if rc:
            return f"{name} {tag} T{threads}: {which} failed: {err}"
        out[which] = f
    a, b = open(out["old"], "rb").read(), open(out["new"], "rb").read()
    if a != b:
        return f"{name} {tag} T{threads}: FRAMES DIFFER ({len(a)} vs {len(b)} bytes)"
    for exe, f in ((OLD, out["new"]), (NEW, out["old"])):
        d = f + ".d"
        rc, err = run([exe, "-d", "-T", str(threads), f, d])
        if rc or open(d, "rb").read() != data:
            return f"{name} {tag} T{threads}: cross decode failed ({exe}): {err}"
        os.remove(d)
    h = hashlib.sha256(a).hexdigest()[:16]
    for f in out.values():
        os.remove(f)
    return None, len(a), h


inputs = list(gen()) + list(repo_files())
jobs = []
for name, data in inputs:
    sets = FAST + (SLOW if len(data) <= SLOW_LIMIT else [])
    if QUICK:
        sets = sets[:4]
    for s in sets:
        for t in (1, 4):
            jobs.append((name, data, s, t))
    for s in ONE_THREAD:
        jobs.append((name, data, s, 1))
fails, ok = [], 0
with cf.ThreadPoolExecutor(max_workers=2) as ex:      # each job runs up to 4 threads itself
    for r in ex.map(lambda j: one(*j), jobs):
        if isinstance(r, str):
            fails.append(r); print(r, flush=True)
        else:
            ok += 1
print(f"{ok} of {len(jobs)} frames identical and cross-decoded ({len(inputs)} inputs, {len(FAST) + len(SLOW) + len(ONE_THREAD)} settings)")
sys.exit(1 if fails else 0)
