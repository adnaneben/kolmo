#!/usr/bin/env python3
"""Campaign 2 seed inputs, part 6: unsigned 32-bit series crossing 2^31 with spikes to 0 (fi_inv_4_8_0), 8-bit walks,
small -x inputs (32-64 KiB slices, 8-bit images for the context-mixing image model) and forged CZNS streams.
Usage: gen6_misc.py RAWDIR(gen1..gen5 output) OUTRAW XRAW STREAMDIR"""
import random, struct, math, sys, os
RAW, O, XR, ST = sys.argv[1:5]
for d in (O, XR, ST): os.makedirs(d, exist_ok=True)
R = random.Random(3)
b = bytearray(); v = 2**31
for i in range(150000):
    v = max(0, min(2**32-1, v + R.randint(-300, 300))); b += struct.pack('<I', 0 if i % 997 == 500 else v)
open(O + '/w4o8_u2.bin', 'wb').write(bytes(b))
b = bytearray(); v = 128
for i in range(600000): v = max(0, min(255, v + R.randint(-2, 2))); b.append(v)
open(O + '/u8_walk.bin', 'wb').write(bytes(b))
b = bytearray(); v = 0
for i in range(600000): v = max(-128, min(127, v + R.randint(-2, 2))); b += struct.pack('<b', v)
open(O + '/i8_walk.bin', 'wb').write(bytes(b))
# small inputs for the context-mixing (-x) targets
for src, n, name in [('tbl_1m.csv', 32768, 'csv_32k.csv'), ('access_600k.log', 49152, 'log_48k.log'),
                     ('ndjson_3m.json', 40000, 'json_40k.json'), ('wide300_200k.csv', 65536, 'wide_64k.csv'),
                     ('i32_s1.bin', 32768, 'i32_32k.bin'), ('x86_tmux.elf', 32768, 'elf_32k.bin'), ('fixed_500k.txt', 24576, 'fixed_24k.txt')]:
    p = os.path.join(RAW, src)
    if os.path.exists(p): open(os.path.join(XR, name), 'wb').write(open(p, 'rb').read()[:n])
R = random.Random(2)
for w, h, ch in [(128, 128, 1), (128, 96, 3), (160, 120, 1)]:
    b = bytearray(b'P%d\n%d %d\n255\n' % (5 if ch == 1 else 6, w, h))
    for y in range(h):
        for x in range(w):
            for c in range(ch): b.append(int(128 + 60 * math.sin(x / 9 + c) + 50 * math.cos(y / 7)) + R.randint(0, 6) & 255)
    open(XR + '/img_%dx%d_%d.%s' % (w, h, ch, 'pgm' if ch == 1 else 'ppm'), 'wb').write(b)
# forged CZNS frame-length claims (alone and after a valid frame) and an 8 GiB original-size claim
H = b'CZNS\x01\x00\x00\x00' + struct.pack('<I', 1 << 20)
small = open(sys.argv[5], 'rb').read() if len(sys.argv) > 5 else b'CZN1' + b'\0' * 60
for L in [1 << 30, (1 << 30) - 1, (1 << 30) + 1, 1 << 34, (1 << 34) + 1, 1 << 63, 64 << 20, (64 << 20) + 1]:
    open(ST + '/forged_%x.czns' % L, 'wb').write(H + struct.pack('<Q', L) + b'CZN1' + b'\0' * 60)
    open(ST + '/forged_after_valid_%x.czns' % L, 'wb').write(H + struct.pack('<Q', len(small)) + small + struct.pack('<Q', L) + small[:100])
f = bytearray(small); f[12:20] = struct.pack('<Q', 1 << 33)
open(ST + '/claim_8g.czns', 'wb').write(H + struct.pack('<Q', len(f)) + bytes(f) + b'\0' * 8)
