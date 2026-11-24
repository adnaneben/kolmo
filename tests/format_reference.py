#!/usr/bin/env python3
"""Independent reference for the container layer of FORMAT.md (container version 12), in plain Python.

usage: python3 tests/format_reference.py [tests/v12]

It shares no code with the C implementation: CRC-32C, kolmo_khash64, the header, the block index, the plan prefix
and tflag rules, the decode working-set byte, container extensions and the CZNS stream are written here from
FORMAT.md. Run over the frozen corpus it checks, for every frame and its original, that:
  - the header, index and trailer obey every rule of FORMAT.md sections 3-6 and 9-10;
  - each block's checksum equals the low 32 bits of khash64 of the original's bytes for that block (or, in a frame
    with flag bit 0, of the stored payload seeded with its plan bytes), which pins down khash64 on real data;
  - the khash64 test vectors of tests/khash64_vectors.txt (every length class, three seeds) are reproduced;
  - stored blocks (codec 0) decode by the rule of section 7, and the codec 20 payload holds a valid nested frame.
The coders' own streams (section 8) are not decoded here.
"""
import os, struct, sys

M64 = (1 << 64) - 1


# ---- CRC-32C (FORMAT.md 2.1)
def _crc_table():
    t = []
    for i in range(256):
        c = i
        for _ in range(8): c = (c >> 1) ^ 0x82F63B78 if c & 1 else c >> 1
        t.append(c)
    return t
_CT = _crc_table()


def crc32c(data, crc=0):
    c = crc ^ 0xFFFFFFFF
    for b in data: c = (c >> 8) ^ _CT[(c ^ b) & 0xFF]
    return c ^ 0xFFFFFFFF


# ---- kolmo_khash64 (FORMAT.md 2.2)
G = 0x9E3779B97F4A7C15
P1 = 0xD6E8FEB86659FD93


def mix64(z):
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


K0 = [mix64((0 + (i + 1) * G) & M64) for i in range(16)]


def fold(a, b):
    r = a * b
    return (r & M64) ^ (r >> 64)


def aval(h):
    h ^= h >> 32; h = (h * P1) & M64; h ^= h >> 32; h = (h * P1) & M64; return h ^ (h >> 32)


def r64(p, i): return struct.unpack_from("<Q", p, i)[0]
def r32(p, i): return struct.unpack_from("<I", p, i)[0]


def khash64(p, seed):
    n = len(p); L = (n * G) & M64; S = (seed * P1) & M64
    if n <= 16:
        if n >= 8: a, b = r64(p, 0), r64(p, n - 8)
        elif n >= 4: a, b = r32(p, 0), r32(p, n - 4)
        elif n: a, b = p[0] | p[n >> 1] << 8 | p[n - 1] << 16, 0
        else: a = b = 0
        return aval(fold(a ^ K0[0] ^ S, b ^ K0[1] ^ L) ^ seed ^ n)
    if n <= 64:
        h = (fold(r64(p, 0) ^ K0[0] ^ S, r64(p, 8) ^ K0[1]) + fold(r64(p, n - 16) ^ K0[2], r64(p, n - 8) ^ K0[3] ^ L)) & M64
        if n > 32:
            h = (h + fold(r64(p, 16) ^ K0[4], r64(p, 24) ^ K0[5] ^ S) + fold(r64(p, n - 32) ^ K0[6], r64(p, n - 24) ^ K0[7])) & M64
        return aval(h ^ seed ^ n)
    K = [(K0[i] + (-S if i & 1 else S)) & M64 for i in range(16)]
    K += K[:8]
    acc = [K[8 + i] ^ K[i] for i in range(8)]

    def stripe(off, t):
        for i in range(8):
            v = r64(p, off + 8 * i); d = v ^ K[t + i]
            acc[i] = (acc[i] + (d & 0xFFFFFFFF) * (d >> 32) + v) & M64
    ns = (n - 1) // 64; s = 0
    while s + 16 <= ns:
        for t in range(16): stripe((s + t) * 64, t)
        for i in range(8): acc[i] = ((acc[i] ^ (acc[i] >> 47) ^ K[8 + i]) * 0x9E3779B1) & M64
        s += 16
    while s < ns: stripe(s * 64, s & 15); s += 1
    stripe(n - 64, ns & 15)
    h = L ^ seed
    for i in range(0, 8, 2): h = (h + fold(acc[i] ^ K[8 + i], acc[i + 1] ^ K[9 + i])) & M64
    return aval(h)


# ---- the frame (FORMAT.md 3-7, 9-10)
class Bad(Exception): pass

MAXF = {0: 0, 1: 2, 2: 2, 3: 4, 4: 10, 5: 6, 6: 7, 7: 8, 8: 9, 9: 0, 10: 12, 11: 14}
STEPS = [1048576, 1143481, 1246975, 1359835, 1482911, 1617126, 1763488, 1923097]
V12_CODECS = {0, 5, 7, 12, 14, 16, 17, 18, 19, 20, 24, 25, 26, 27, 28, 29, 31, 32}
LEAF_ONLY = {14, 15, 21, 23, 27}   # 14 and 27: refused as block codecs from the Unreleased fix (FORMAT.md 8.1)


def memd_bytes(v): return 0 if v == 0 else STEPS[v & 7] << (v >> 3)


def tflag_ok(t, f):
    if f == 0: return True
    if f > MAXF[t] and not (t == 3 and f == 13): return False   # tflag 13 is above the plan's range (FORMAT.md 6.1, 8.3)
    allowed = {1: {1, 2}, 2: {1}, 3: {3, 4, 13}, 4: {5, 10}, 5: {6}, 6: {7}, 7: {8}, 8: {9}, 10: {12}, 11: {14}}
    return f in allowed.get(t, set())


def check_frame(f, orig=None, nested=False):
    """returns the number of blocks whose checksum was verified against orig"""
    if len(f) < 32 or f[:4] != b"CZN1": raise Bad("magic")
    if f[4] != 12: raise Bad("version %d" % f[4])
    if f[10] & ~1: raise Bad("flags")
    rt = f[10] & 1
    o, nb, bsz = struct.unpack_from("<QII", f, 12)
    if o >= 1 << 63: raise Bad("orig")
    if 32 + 16 * nb > len(f): raise Bad("index outside the frame")
    if crc32c(f[:28] + f[32:32 + 16 * nb]) != r32(f, 28): raise Bad("header crc")
    po, ro, checked, blocks = 32 + 16 * nb, 0, 0, []
    for i in range(nb):
        raw, comp, ck, codec, tflag = struct.unpack_from("<IIIBB", f, 32 + 16 * i)   # bytes 14-15: ignored
        if raw > o - ro: raise Bad("raw sizes exceed orig")
        if codec == 0 and comp != raw + 4: raise Bad("stored block size")
        if codec not in V12_CODECS and codec != 20: raise Bad("codec %d not in version 12" % codec)
        if po + comp > len(f): raise Bad("payload outside the frame")
        blocks.append((raw, comp, ck, codec, tflag, po, ro)); po += comp; ro += raw
    if ro != o: raise Bad("raw sizes do not add up to orig")
    if po != len(f):   # container extensions (section 9)
        tr = f[-24:]
        if len(f) - po < 24 or tr[:4] != b"KEXT" or tr[4] != 1 or tr[5:8] != b"\0\0\0" or crc32c(tr[:20]) != r32(tr, 20):
            raise Bad("bytes after the last payload are not extensions")
        count, total = struct.unpack_from("<IQ", tr, 8)
        if total != len(f) - 24 - po: raise Bad("extension length")
        q = po   # section CRCs belong to the features that read them (FORMAT.md 9), not to frame validity
        for _ in range(count):
            if po + total - q < 16: raise Bad("extension section")
            ln = struct.unpack_from("<Q", f, q + 4)[0]
            if ln > po + total - q - 16: raise Bad("extension section length")
            q += 16 + ln
        if q != po + total: raise Bad("extension sections")
    if f[11]:
        floor = max((2 * b[0] + b[0] // 8 + 16384 + (1 << 20) for b in blocks), default=0)
        if floor > memd_bytes(f[11]): raise Bad("memd below the buffer floor")
    if nb == 1 and blocks[0][3] == 20:   # video: one block, plan bytes 0, then a nested frame (section 7.3)
        raw, comp, ck, codec, tflag, p0, _ = blocks[0]
        if rt or tflag or raw != o or f[p0:p0 + 4] != b"\0\0\0\0": raise Bad("video block")
        check_frame(f[p0 + 4:p0 + comp], None, nested=True)
        if orig is not None:
            if (khash64(orig, 0x637a6e31) & 0xFFFFFFFF) != ck: raise Bad("video checksum")
            checked += 1
        return checked
    for raw, comp, ck, codec, tflag, p0, r0 in blocks:
        if codec == 20 or codec in LEAF_ONLY: raise Bad("codec %d as a block" % codec)
        if comp < 4: raise Bad("no plan prefix")
        t, a, b, c = f[p0:p0 + 4]
        if t > 11 or (t == 11 and (b > 15 or c)): raise Bad("plan prefix")
        if codec == 0 and (t or a or b or c or tflag): raise Bad("stored block with a plan or a transform")   # 7.1
        if codec in (12,) or t == 9:
            if codec != 12 or t != 9 or tflag: raise Bad("graph block")
        elif codec in (16, 19, 26, 31):
            if t or b or c or tflag or (a and not (codec == 19 and a == 1)): raise Bad("plan of codec %d" % codec)
        elif not tflag_ok(t, tflag): raise Bad("tflag %d under plan %d" % (tflag, t))
        if rt:
            if (khash64(f[p0 + 4:p0 + comp], 0x637a6e31 ^ r32(f, p0)) & 0xFFFFFFFF) != ck: raise Bad("rt checksum")
            checked += 1
        if codec == 0 and orig is not None and f[p0 + 4:p0 + comp] != orig[r0:r0 + raw]: raise Bad("stored bytes")
        if not rt and orig is not None:
            if (khash64(orig[r0:r0 + raw], 0x637a6e31) & 0xFFFFFFFF) != ck: raise Bad("block checksum")
            checked += 1
    return checked


def check_stream(s, orig):
    if len(s) < 12 or s[:4] != b"CZNS" or s[4] != 1 or s[5:8] != b"\0\0\0": raise Bad("stream header")
    p, ro, checked = 12, 0, 0
    while True:
        n = struct.unpack_from("<Q", s, p)[0]; p += 8
        if n == 0: break
        fr = s[p:p + n]; o = struct.unpack_from("<Q", fr, 12)[0]
        checked += check_frame(fr, orig[ro:ro + o] if orig is not None else None); ro += o; p += n
    if p != len(s): raise Bad("bytes after the stream terminator")
    if orig is not None and ro != len(orig): raise Bad("stream length")
    return checked


def mutations(f):
    """frames that break one rule each (the header CRC recomputed where the rule is not the CRC itself)"""
    def fix(g):
        g = bytearray(g); nb = r32(g, 20)
        struct.pack_into("<I", g, 28, crc32c(bytes(g[:28] + g[32:32 + 16 * nb]))); return bytes(g)
    def put(off, val, crc=True):
        g = bytearray(f); g[off] = val; return fix(g) if crc else bytes(g)
    yield "version 11", put(4, 11)
    yield "version 13", put(4, 13)
    yield "flag bit 1", put(10, f[10] | 2)
    yield "header crc", put(29, f[29] ^ 1, crc=False)
    yield "retired codec 3", put(32 + 12, 3)
    yield "unknown codec 30", put(32 + 12, 30)
    yield "tflag 6 under plan 0", put(32 + 13, 6)
    yield "codec 14 as a block", put(32 + 12, 14)
    yield "codec 27 as a block", put(32 + 12, 27)
    yield "orig one more", fix(f[:12] + struct.pack("<Q", r64(f, 12) + 1) + f[20:])
    yield "truncated", f[:-1]
    yield "stray byte", f + b"\0"


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "v12")
    assert crc32c(b"123456789") == 0xE3069283   # the CRC-32C check value
    # khash64 test vectors (FORMAT.md 2.2): input byte i is (31 i + 7) mod 256; every length class and seed
    data = bytes((31 * i + 7) & 255 for i in range(70000))
    vec = os.path.join(os.path.dirname(os.path.abspath(__file__)), "khash64_vectors.txt")
    for line in open(vec):
        n, s, h = line.split()
        if khash64(data[:int(n)], int(s, 16)) != int(h, 16): sys.exit("khash64 vector failed: length %s seed %s" % (n, s))
    pairs = [l.split() for l in open(os.path.join(root, "MANIFEST")) if l.strip()]
    import json, glob
    for j in glob.glob(os.path.join(root, "r*", "frames.json")):
        rel = os.path.basename(os.path.dirname(j))
        pairs += [[rel + "/" + e["file"], rel + "/" + e["input"]] for e in json.load(open(j))["frames"] if "stream1M" in e["file"]]
    probe = open(os.path.join(root, "r0.0.2", "text.txt.L3.kolmo"), "rb").read()   # one block, codec 32, plan 0
    for name, g in mutations(probe):
        try: check_frame(g); sys.exit("reference accepted a broken frame: " + name)
        except (Bad, struct.error): pass
    frames = blocks = 0; bad = []
    for fr, og in pairs:
        f = open(os.path.join(root, fr), "rb").read(); o = open(os.path.join(root, og), "rb").read()
        try:
            blocks += check_stream(f, o) if f[:4] == b"CZNS" else check_frame(f, o)
        except Bad as e:
            bad.append("%s: %s" % (fr, e))
        frames += 1
    for b in bad: print(b)
    if bad: sys.exit("FORMAT REFERENCE FAILED (%d of %d frames)" % (len(bad), frames))
    print("FORMAT REFERENCE PASSED (%d frames, %d block checksums recomputed from FORMAT.md)" % (frames, blocks))


if __name__ == "__main__":
    main()
