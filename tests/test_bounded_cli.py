"""Bounded CLI memory (review40 #8, #104).

- Levels 4-9 and ultra compress a regular file of 8 MiB or more through kolmo_compress_file (the descriptor path):
  the frame must be byte-identical to the in-memory path (-c, which still holds input and output).
- -t decodes into /dev/null without holding the output, and -dc writes whole-block windows; both must accept and
  reject exactly what the in-memory decoder does, over single and concatenated frames.
- -t's peak memory must not grow with the output (checked on a 96 MiB input).
"""
import hashlib
import os
import random
import subprocess
import sys
import tempfile
from pathlib import Path

CLI = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "build/kolmo").resolve()


def run(*args, input=None, ok=True):
    r = subprocess.run([str(CLI), *map(str, args)], input=input, capture_output=True)
    assert (r.returncode == 0) == ok, (args, r.returncode, r.stderr[-400:])
    return r


def mixed(n, seed=11):
    """text, numbers and noise in 64 KiB pieces, so the planner sees several kinds of data"""
    rng = random.Random(seed)
    words = [bytes(rng.choice(b"etaoinshrdlucmfw") for _ in range(rng.randint(2, 9))) for _ in range(3000)]
    out = bytearray()
    while len(out) < n:
        k = rng.randrange(3)
        if k == 0:
            out += b" ".join(rng.choice(words) for _ in range(9000))[:65536] + b"\n"
        elif k == 1:
            out += b"".join(b"%d,%d.%02d\n" % (rng.randrange(10**6), rng.randrange(999), rng.randrange(100)) for _ in range(4000))
        else:
            out += rng.randbytes(16384) * 2
    return bytes(out[:n])


def sha(b):
    return hashlib.sha256(b).hexdigest()


with tempfile.TemporaryDirectory() as d:
    root = Path(d)
    src = root / "src"
    data = mixed(9 << 20)
    src.write_bytes(data)

    # 1. the descriptor path gives the in-memory path's frame, at every level from 4 and at two thread counts
    frames = {}
    for level, threads in [("4", 4), ("5", 4), ("6", 4), ("7", 4), ("8", 4), ("9", 4), ("ultra", 4)]:
        out = root / f"f{level}.{threads}"
        run("--level", level, "-T", threads, "-f", "-o", out, src)
        mem = run("--level", level, "-T", threads, "-c", src)
        assert out.read_bytes() == mem.stdout, f"level {level} -T {threads}: descriptor and in-memory frames differ"
        if level == "7":
            verbose = run("--level", level, "-T", threads, "-f", "-vv", "-o", out, src)
            assert b"[file path]" in verbose.stderr, verbose.stderr
        frames[level] = out.read_bytes()
    print("descriptor path: identical frames at levels 4-9 and ultra")

    # 2. -t and -dc over one frame, concatenated frames, small blocks (many windows) and damage
    small = root / "small"
    run("-1", "--block-size", "1", "-T", 4, "-f", "-o", small, src)   # 1 MiB blocks: several -dc windows
    cat = root / "cat"
    cat.write_bytes(frames["4"] + small.read_bytes() + frames["9"])
    for f, want in [(root / "f4.4", data), (small, data), (cat, data * 3)]:
        run("-t", f)
        run("-t", "-T", 1, f)
        for threads in (1, 4):
            got = run("-dc", "-T", threads, f).stdout
            assert sha(got) == sha(want), f"-dc {f.name} -T {threads}"
        piped = run("-dc", input=f.read_bytes()).stdout
        assert sha(piped) == sha(want), f"-dc from a pipe {f.name}"
    for f in (small, cat):
        damaged = bytearray(f.read_bytes())
        damaged[len(damaged) * 2 // 3] ^= 0x10            # a payload byte of a later block
        bad = root / "bad"
        bad.write_bytes(damaged)
        run("-t", bad, ok=False)
        run("-dc", bad, ok=False)
        run("-dc", input=bytes(damaged), ok=False)
    trailing = root / "trailing"
    trailing.write_bytes(small.read_bytes() + b"junk")
    run("-t", trailing, ok=False)
    run("-dc", trailing, ok=False)
    empty = root / "empty"
    empty.write_bytes(b"")
    run("-t", empty, ok=False)
    print("-t / -dc: single, concatenated, damaged and trailing inputs as before")

    # 3. -t holds neither the input nor the output: peak well below the 96 MiB the in-memory path needs
    big = root / "big"
    big.write_bytes(data * 10 + data[: 6 << 20])  # 96 MiB
    bigf = root / "big.kolmo"
    run("-1", "-T", 1, "-f", "-o", bigf, big)
    # Linux charges the parent's resident set at exec to the child's ru_maxrss, so kolmo is started by a small
    # interpreter (not by this one, which holds the test data) and that one reports kolmo's peak
    probe = ("import os, sys; pid = os.posix_spawn(sys.argv[1], sys.argv[1:], os.environ); _, st, ru = os.wait4(pid, 0); "
             "print(os.waitstatus_to_exitcode(st), ru.ru_maxrss if sys.platform == 'darwin' else ru.ru_maxrss * 1024)")
    del data, frames
    r = subprocess.run([sys.executable, "-I", "-c", probe, str(CLI), "-t", "-T", "1", str(bigf)], capture_output=True, text=True)
    code, peak_bytes = map(int, r.stdout.split())
    assert code == 0, r.stderr
    assert peak_bytes < (48 << 20), f"-t peak {peak_bytes} bytes for a {big.stat().st_size}-byte output"
    print(f"-t peak resident: {peak_bytes >> 20} MiB for a {big.stat().st_size >> 20} MiB output")
print("test_bounded_cli: OK")
