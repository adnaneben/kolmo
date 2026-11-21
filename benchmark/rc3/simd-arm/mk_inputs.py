"""Synthetic inputs for the simd-arm profile (fixed seed): WAV, Y4M, 16-bit PGM, 8-bit PPM, int32 NPY.

usage: python3 -I benchmark/rc3/simd-arm/mk_inputs.py OUT_DIR
The benchmark set has no audio, video or 16-bit image; these stand in for them. Real-data rows use the files of
kolmo-bench/data (dickens, webster, owid-co2-data.csv, loghub_all.log, x-ray, mr, owid_energy_f64.npy, ...).
"""
import math
import os
import random
import struct
import sys

out = sys.argv[1]
rnd = random.Random(20261116)

# audio: 20 s stereo 44.1 kHz PCM16, decaying notes with four partials and vibrato, plus a noise floor
sr, n = 44100, 44100 * 20
pcm = []
for i in range(n):
    t = i / sr
    note = 220 * 2 ** (((i // 22050) * 5 % 12) / 12)
    env = math.exp(-3 * ((i % 22050) / sr))
    s = sum(math.sin(2 * math.pi * note * (k + 1) * t + 0.3 * math.sin(5 * t)) / (k + 1) for k in range(4)) * env
    pcm.append(max(-32768, min(32767, int(9000 * s + rnd.gauss(0, 30)))))
    pcm.append(max(-32768, min(32767, int(7200 * s + rnd.gauss(0, 30)))))
raw = struct.pack(f"<{len(pcm)}h", *pcm)
with open(os.path.join(out, "music.wav"), "wb") as f:
    f.write(b"RIFF" + struct.pack("<I", 36 + len(raw)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 2, sr, sr * 4, 4, 16))
    f.write(b"data" + struct.pack("<I", len(raw)) + raw)

# video: CIF 4:2:0, 60 frames, a moving smooth background, a moving square and a little noise
W, H, F = 352, 288, 60
with open(os.path.join(out, "clip.y4m"), "wb") as f:
    f.write(b"YUV4MPEG2 W352 H288 F30:1 Ip A1:1 C420jpeg\n")
    for fr in range(F):
        f.write(b"FRAME\n")
        y = bytearray(W * H)
        for r in range(H):
            cr = math.cos((r + fr) / 31.0)
            for c in range(W):
                v = 128 + 60 * math.sin((c + 2 * fr) / 23.0) * cr
                if 100 + fr * 2 <= c < 160 + fr * 2 and 80 <= r < 140:
                    v = 200 - (r - 80)
                y[r * W + c] = max(0, min(255, int(v + rnd.randint(-2, 2))))
        f.write(y)
        for p in range(2):
            f.write(bytes(int(128 + 20 * math.sin((c + fr + p * 9) / 17.0) + rnd.randint(-1, 1)) & 255
                          for r in range(H // 2) for c in range(W // 2)))

# 16-bit grey image 1024x1024 (12-bit values), smooth field plus noise, PGM P5
W = H = 1024
img = [max(0, min(4095, int(2048 + 1500 * math.sin(r / 97.0) * math.cos(c / 61.0) + 300 * math.sin((r + c) / 13.0)
                            + rnd.gauss(0, 6)))) for r in range(H) for c in range(W)]
with open(os.path.join(out, "img16.pgm"), "wb") as f:
    f.write(b"P5\n1024 1024\n4095\n" + struct.pack(f">{len(img)}H", *img))

# 8-bit RGB 1024x768, PPM P6
W, H = 1024, 768
px = bytearray()
for r in range(H):
    for c in range(W):
        base = 128 + 90 * math.sin(r / 50.0) * math.cos(c / 70.0)
        px += bytes(max(0, min(255, int(base * k + rnd.randint(-3, 3)))) for k in (1.0, 0.8, 0.6))
with open(os.path.join(out, "rgb8.ppm"), "wb") as f:
    f.write(b"P6\n1024 768\n255\n" + bytes(px))

# int32 NPY random walk, 4 M values
v, walk = 0, []
for _ in range(1 << 22):
    v += rnd.randint(-500, 500)
    walk.append(v)
d = b"{'descr': '<i4', 'fortran_order': False, 'shape': (%d,), }" % len(walk)
d = d.ljust(128 - 10 - 1) + b"\n"
with open(os.path.join(out, "walk_i32.npy"), "wb") as f:
    f.write(b"\x93NUMPY\x01\x00" + struct.pack("<H", len(d)) + d + struct.pack(f"<{len(walk)}i", *walk))
