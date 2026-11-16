"""kvid with its NEON kernels against its C code (KVID_FORCE_C=1): the same frames, and both decoders exact.

usage: python3 tests/test_kvid_kernels.py build/kvid WORK_DIR
Generated YUV4MPEG2 clips (fixed seed): 4:2:0 at several sizes (narrow planes, heights that are not a multiple of
the 16-row strips, a width below the 64-sample band), 4:4:4 and mono; still, moving and noisy content so that every
predictor is chosen somewhere. On builds without the kernels both runs take the C code.
"""
import os
import random
import subprocess
import sys

KVID, W = sys.argv[1], sys.argv[2]
os.makedirs(W, exist_ok=True)
rnd = random.Random(1611)


def clip(w, h, frames, chroma, kind):
    planes = [(w, h)]
    if chroma == "420jpeg":
        planes += [((w + 1) // 2, (h + 1) // 2)] * 2
    elif chroma == "444":
        planes += [(w, h)] * 2
    out = bytearray(f"YUV4MPEG2 W{w} H{h} F25:1 Ip A1:1 C{chroma}\n".encode())
    for f in range(frames):
        out += b"FRAME\n"
        for pw, ph in planes:
            for y in range(ph):
                for x in range(pw):
                    if kind == 0:
                        v = (x * 3 + y * 2 + f * 4) & 255
                    elif kind == 1:
                        v = rnd.randrange(256)
                    else:
                        v = (128 + ((x + f * 3) // 9 % 2) * 60 + ((y - f) // 7 % 2) * 40 + rnd.randrange(-2, 3)) & 255
                    out.append(v)
    return bytes(out)


def run(args, force):
    r = subprocess.run([KVID, *args], env=dict(os.environ, KVID_FORCE_C=str(force)), capture_output=True)
    if r.returncode:
        sys.exit(f"kvid {' '.join(args)} failed: {r.stderr.decode(errors='replace')[:200]}")


bad = 0
cases = [(176, 144, 6, "420jpeg", 2), (352, 288, 4, "420jpeg", 0), (100, 37, 5, "420jpeg", 2), (40, 70, 4, "420jpeg", 2),
         (130, 50, 3, "444", 1), (96, 33, 5, "mono", 2), (17, 17, 3, "mono", 0)]
for i, (w, h, fr, ch, kind) in enumerate(cases):
    src = os.path.join(W, f"c{i}.y4m")
    data = clip(w, h, fr, ch, kind)
    open(src, "wb").write(data)
    for m in ("rt", "balanced"):
        a, b = src + f".{m}.c", src + f".{m}.n"
        run(["-m", m, src, a], 1)
        run(["-m", m, src, b], 0)
        if open(a, "rb").read() != open(b, "rb").read():
            print(f"FAIL frames differ: {w}x{h} {ch} kind {kind} {m}"); bad += 1
        for force in (0, 1):
            back = src + f".{m}.back{force}"
            run(["-d", b, back], force)
            if open(back, "rb").read() != data:
                print(f"FAIL round trip: {w}x{h} {ch} kind {kind} {m} force_c {force}"); bad += 1
print(f"KVID KERNEL TESTS {'FAILED' if bad else 'PASSED'} ({len(cases)} clips x 2 modes)")
sys.exit(1 if bad else 0)
