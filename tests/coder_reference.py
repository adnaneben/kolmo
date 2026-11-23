#!/usr/bin/env python3
"""Independent decoders for Kolmo's own coders, FORMAT.md sections 14-19 (container version 12), in plain Python.

usage: python3 tests/coder_reference.py [tests/v12]

Written from the prose of FORMAT.md (khuf 15, krans 16, KLZ 17, KLZH and KLZH2 18, the byte-plane shuffle 19.1, the
x86 filter 19.3) and
SPEC.md (the ARM64 branch filter), not from the C sources. It takes khash64 from tests/format_reference.py (FORMAT.md
2.2) and walks the container itself.

For every frame of the frozen corpus it decodes:
  - every block whose codec is 28 (KLZ), 29 (KLZH) or 32 (KLZH2). A block without a transform (tflag 0) must give the
    original's bytes for that block. A block under the byte-plane shuffle (plan 2, tflag 1), the x86 filter (tflag 8)
    or the ARM64 filter (tflag 14) must match its block checksum after the inverse. A block under any other transform
    must decode to exactly its transformed length, using all of its stream;
  - every graph leaf (FORMAT.md 7.4) whose codec is 27, 28, 29 or 32, to exactly its raw length.
It also checks that one stream for each refusal rule is refused.
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from format_reference import khash64  # noqa: E402  (FORMAT.md 2.2)


class Corrupt(Exception):
    pass


def need(cond, why):
    if not cond:
        raise Corrupt(why)


# ---- varints (FORMAT.md 14.1)
def uv64(p, i):
    """LEB128, at most 10 bytes, value modulo 2^64; returns (value, next index)"""
    v = 0
    for k in range(10):
        need(i < len(p), "varint past the end")
        b = p[i]; i += 1
        v |= (b & 127) << (7 * k)
        if not b & 128:
            return v & ((1 << 64) - 1), i
    raise Corrupt("varint longer than 10 bytes")


def uv32(p, i):
    """LEB128, at most 5 bytes, value modulo 2^32"""
    v = 0
    for k in range(5):
        need(i < len(p), "varint past the end")
        b = p[i]; i += 1
        v |= (b & 127) << (7 * k)
        if not b & 128:
            return v & 0xFFFFFFFF, i
    raise Corrupt("varint longer than 5 bytes")


# ---- khuf (FORMAT.md 15)
KHUF_MAXLEN = 11
KHUF_BLOCK_MAX = 128 << 10


def khuf_table(p, i):
    need(i < len(p), "khuf table")
    m = p[i] + 1; i += 1
    lens = [0] * 256
    if m <= 48:                                     # sparse: m (symbol, length) pairs
        need(len(p) - i >= 2 * m, "khuf sparse table")
        for _ in range(m):
            s, l = p[i], p[i + 1]; i += 2
            need(lens[s] == 0 and 1 <= l <= KHUF_MAXLEN, "khuf sparse entry")
            lens[s] = l
        need(m >= 2, "khuf: one symbol")
    else:                                           # dense: last symbol, then nibbles
        need(i < len(p), "khuf dense table")
        last = p[i]; i += 1
        nb = (last + 2) // 2
        need(len(p) - i >= nb, "khuf dense table")
        for s in range(last + 1):
            l = (p[i + s // 2] >> (4 * (s & 1))) & 15
            need(l <= KHUF_MAXLEN, "khuf length above 11")
            lens[s] = l
        i += nb
        need(sum(1 for l in lens if l) == m, "khuf dense count")
    # Kraft: the code must be complete
    need(sum(1 << (KHUF_MAXLEN - l) for l in lens if l) == 1 << KHUF_MAXLEN, "khuf code not complete")
    return lens, i


def khuf_decode_table(lens):
    """canonical codes -> a 2^11-entry table indexed by the next 11 stream bits (first bit = bit 0)"""
    table = [None] * (1 << KHUF_MAXLEN)
    code = 0
    for l in range(1, KHUF_MAXLEN + 1):
        for s in range(256):
            if lens[s] == l:
                # the code's most significant bit is the first bit in the stream
                rev = int(format(code, "0%db" % l)[::-1], 2)
                for k in range(rev, 1 << KHUF_MAXLEN, 1 << l):
                    table[k] = (s, l)
                code += 1
        code <<= 1
    return table


def khuf_block(p, n):
    need(2 <= n <= KHUF_BLOCK_MAX, "khuf block size")
    lens, i = khuf_table(p, 0)
    table = khuf_decode_table(lens)
    L = []
    for _ in range(3):
        v, i = uv64(p, i)
        need(v <= len(p), "khuf stream length")
        L.append(v)
    need(sum(L) <= len(p) - i, "khuf stream lengths")
    L.append(len(p) - i - sum(L))
    seg = (n + 3) // 4
    for q in range(4):
        need(L[q] <= seg * KHUF_MAXLEN // 8 + 2, "khuf stream too long")
    out = bytearray()
    for q in range(4):
        a, e = min(n, q * seg), min(n, (q + 1) * seg)
        s = bytes(p[i:i + L[q]]) + b"\0\0\0"; i += L[q]
        pos = 0
        for _ in range(e - a):
            w = int.from_bytes(s[pos >> 3:(pos >> 3) + 3], "little") >> (pos & 7)
            sym, l = table[w & 2047]
            out.append(sym); pos += l
            need(pos <= 8 * L[q], "khuf stream overrun")
        need(pos > 8 * (L[q] - 1) if L[q] else pos == 0, "khuf stream not used up")
    return bytes(out)


def khuf_chunks(p, n):
    i, out = 0, bytearray()
    while len(out) < n:
        need(i < len(p), "khuf chunk header")
        mode = p[i]; i += 1
        m, i = uv64(p, i)
        need(1 <= m <= KHUF_BLOCK_MAX and m <= n - len(out), "khuf chunk length")
        if mode == 0:
            need(m <= len(p) - i, "khuf stored chunk"); out += p[i:i + m]; i += m
        elif mode == 1:
            need(i < len(p), "khuf run chunk"); out += bytes([p[i]]) * m; i += 1
        elif mode == 2:
            z, i = uv64(p, i)
            need(z <= len(p) - i, "khuf coded chunk")
            out += khuf_block(p[i:i + z], m); i += z
        else:
            raise Corrupt("khuf chunk mode %d" % mode)
    need(i == len(p), "khuf chunks: bytes left")
    return bytes(out)


# ---- krans (FORMAT.md 16)
def krans(p, n):
    need(n >= 1 and len(p) >= 2, "krans size")
    top = p[0]; i = 1
    f = []
    for _ in range(top + 1):
        v, i = uv32(p, i)
        need(v <= 4096, "krans frequency")
        f.append(v)
    need(sum(f) == 4096, "krans frequencies do not sum to 4096")
    present = [s for s in range(top + 1) if f[s]]
    if len(present) == 1:
        need(i == len(p), "krans one-symbol table: bytes left")
        return bytes([present[0]]) * n
    slot_sym, start = [], [0] * (top + 1)
    for s in range(top + 1):
        start[s] = len(slot_sym); slot_sym += [s] * f[s]
    need(len(p) - i >= 32, "krans states")
    x = list(struct.unpack_from("<8I", p, i)); i += 32
    need(all(v >= 1 << 16 for v in x), "krans initial state")
    lens = []
    for _ in range(7):
        v, i = uv32(p, i); lens.append(v)
    streams = []
    for k in range(8):
        ln = lens[k] if k < 7 else len(p) - i
        need(ln <= len(p) - i, "krans word stream")
        streams.append([i, i + ln]); i += ln
    out = bytearray(n)
    for j in range(n):
        k = j & 7; v = x[k]
        slot = v & 4095; s = slot_sym[slot]
        v = f[s] * (v >> 12) + slot - start[s]
        if v < 1 << 16:
            a, e = streams[k]
            need(e - a >= 2, "krans word stream exhausted")
            v = v << 16 | p[a] << 8 | p[a + 1]; streams[k][0] = a + 2
        x[k] = v; out[j] = s
    need(all(v == 1 << 16 for v in x), "krans final state")
    need(all(a == e for a, e in streams), "krans words left")
    return bytes(out)


# ---- KLZ (FORMAT.md 17)
def lz_copy(out, off, ml):
    need(1 <= off <= len(out), "match offset")
    s = len(out) - off
    if off >= ml:
        out += out[s:s + ml]
    else:
        for k in range(ml): out.append(out[s + k])


def klz(p, n):
    i, out = 0, bytearray()
    while True:
        need(i < len(p), "klz: no token")
        t = p[i]; i += 1
        ll = t & 7
        if ll == 7:
            while True:
                need(i < len(p), "klz literal length"); b = p[i]; i += 1; ll += b
                if b != 255: break
        need(ll <= len(p) - i and ll <= n - len(out), "klz literals")
        out += p[i:i + ll]; i += ll
        if len(out) == n:
            need(t >> 3 == 0 and i == len(p), "klz end")
            return bytes(out)
        w = 3 if t & 128 else 2
        need(len(p) - i >= w, "klz offset")
        off = int.from_bytes(p[i:i + w], "little"); i += w
        ml = (t >> 3) & 15
        if ml == 15:
            while True:
                need(i < len(p), "klz match length"); b = p[i]; i += 1; ml += b
                if b != 255: break
        ml += 4
        need(ml <= n - len(out), "klz match past the end")
        lz_copy(out, off, ml)


# ---- KLZH (29) and KLZH2 (32) (FORMAT.md 17.2)
def length_value(code):
    """code -> (base, extra bit count)"""
    if code < 16: return code, 0
    h = 4 + (code - 16) // 2
    return (1 << h) | ((code & 1) << (h - 1)), h - 1


def klzh(p, n, v2):
    ns, i = uv64(p, 0)
    nl, i = uv64(p, i)
    need(ns <= n // (3 if v2 else 4) + 1 and nl <= n, "klzh counts")
    streams = []
    for q, cnt in enumerate((nl, ns, ns, ns)):
        mode = 0
        if v2:
            need(i < len(p) and p[i] <= (1 if q else 0), "klzh2 stream mode")
            mode = p[i]; i += 1
        z, i = uv64(p, i)
        need(z <= len(p) - i, "klzh stream length")
        if cnt:
            s = krans(p[i:i + z], cnt) if mode else khuf_chunks(p[i:i + z], cnt)
        else:
            need(z == 0, "klzh empty stream with bytes"); s = b""
        streams.append(s); i += z
    lit, llc, mlc, ofc = streams
    need(all(c < 72 for c in llc) and all(c < 72 for c in mlc), "klzh length code")
    need(all(c <= (32 if v2 else 26) for c in ofc), "klzh offset code")
    xs = p[i:]; xbits = int.from_bytes(xs, "little"); bpos = 0   # extra bits, least significant first

    def bits(nb):
        nonlocal bpos
        v = (xbits >> bpos) & ((1 << nb) - 1); bpos += nb
        return v
    mmin = 3 if v2 else 4
    rep = [1, 4, 8]
    out = bytearray(); lp = 0
    for k in range(ns):
        b, nb = length_value(llc[k]); ll = b + bits(nb)
        b, nb = length_value(mlc[k]); ml = (b + bits(nb) + mmin) & 0xFFFFFFFF   # 0.0.2: modulo 2^32 (17.2)
        oc = ofc[k]
        if oc >= 3:
            h = oc - 3; off = (1 << h) | bits(h); rep = [off, rep[0], rep[1]]
        elif oc == 0:
            off = rep[0]
        elif oc == 1:
            off = rep[1]; rep = [off, rep[0], rep[2]]
        else:
            off = rep[2]; rep = [off, rep[0], rep[1]]
        need(ll <= nl - lp and ll <= n - len(out), "klzh literals")
        out += lit[lp:lp + ll]; lp += ll
        need(off <= len(out) and ml <= n - len(out), "klzh match")
        if ml: lz_copy(out, off, ml)
    need(nl - lp == n - len(out), "klzh trailing literals")
    out += lit[lp:]
    need((bpos + 7) // 8 <= len(xs), "klzh extra bits past the end")
    return bytes(out)


def decode(codec, p, n):
    if codec == 27: return khuf_chunks(p, n)
    if codec == 28: return klz(p, n)
    if codec == 29: return klzh(p, n, False)
    if codec == 32: return klzh(p, n, True)
    raise KeyError(codec)


NATIVE = (27, 28, 29, 32)


# ---- in-place and simple inverses used to check transformed blocks against their checksums
def a64_inv(b, phase):
    """plan 11, tflag 14 (SPEC.md, ARM64 branch filter), in place"""
    b = bytearray(b); M = 0xFFFFFFFF
    for i in range(0, len(b) - 3, 4):
        w = struct.unpack_from("<I", b, i)[0]; pc = (phase + i) & M
        if w >> 26 == 0x25:
            w = 0x94000000 | ((w - (pc >> 2)) & 0x03FFFFFF)
        elif (w & 0x9F000000) == 0x90000000:
            s = ((w >> 29) & 3) | ((w >> 3) & 0x001FFFFC)
            if not (s + 0x00020000) & 0x001C0000:
                d = (s - (pc >> 12)) & M
                w = (w & 0x9000001F) | (d & 3) << 29 | (d & 0x0003FFFC) << 3 | ((-(d & 0x00020000)) & 0x00E00000)
        struct.pack_into("<I", b, i, w)
    return bytes(b)


def x86_inv(b):
    """plan 7, tflag 8 (FORMAT.md 19.3), in place, front to back"""
    b = bytearray(b)
    for i in range(len(b) - 4):
        if b[i] & 0xFE == 0xE8 and b[i + 4] in (0, 255):
            a = (b[i + 1] | b[i + 2] << 8 | b[i + 3] << 16) - i
            b[i + 1:i + 4] = (a & 0xFFFFFF).to_bytes(3, "little")
    return bytes(b)


def shuffle_inv(t, n, width, ch, delta):
    """plan 2, tflag 1: plane b * ch + c holds byte b of channel c's values; delta adds each value to the previous one
    of its channel, modulo 2^(8 width)"""
    u = width * ch; rows = n // u; mask = (1 << (8 * width)) - 1
    out = bytearray(n)
    for c in range(ch):
        prev = 0
        for r in range(rows):
            v = 0
            for b in range(width): v |= t[(b * ch + c) * rows + r] << (8 * b)
            if delta: v = (prev + v) & mask
            prev = v
            o = (r * ch + c) * width
            out[o:o + width] = v.to_bytes(width, "little")
    out[rows * u:] = t[rows * u:n]
    return bytes(out)


# ---- line classes (FORMAT.md 20.1)
def lines_inv(t, n):
    need(len(t) >= 14 and t[0] == 1, "lines header")
    K = t[1]; need(1 <= K <= 8, "lines class count")
    L, tail, other = struct.unpack_from("<III", t, 2)
    hdr = 14 + 8 * K; need(len(t) >= hdr, "lines classes")
    klen, kcnt = [], []
    for k in range(K):
        a, b = struct.unpack_from("<II", t, 14 + 8 * k)
        need(1 <= a <= 4096, "lines class length"); klen.append(a); kcnt.append(b)
    covered = sum(a * b for a, b in zip(klen, kcnt)); lines = sum(kcnt)
    need(L <= n and lines <= L and hdr + L + other + tail + covered == len(t) and covered + lines + other + tail == n, "lines sizes")
    cls = t[hdr:hdr + L]; op = hdr + L; oe = op + other; tp = oe; cp = tp + tail
    out = bytearray(); places = [[] for _ in range(K)]
    for c in cls:
        if c:
            need(c <= K and len(places[c - 1]) < kcnt[c - 1], "lines class id")
            need(n - len(out) >= klen[c - 1] + 1, "lines output")
            places[c - 1].append(len(out)); out += bytes(klen[c - 1]) + b"\n"
        else:
            e = t.find(b"\n", op, oe); need(e >= 0, "lines other line")
            need(e + 1 - op <= n - len(out), "lines output")
            out += t[op:e + 1]; op = e + 1
    need(all(len(places[k]) == kcnt[k] for k in range(K)), "lines class counts")
    need(op == oe and n - len(out) == tail, "lines tail")
    out += t[tp:tp + tail]
    for k in range(K):
        m = kcnt[k]
        for j in range(klen[k]):
            col = t[cp + j * m:cp + (j + 1) * m]
            for r, pos in enumerate(places[k]): out[pos + j] = col[r]
        cp += klen[k] * m
    return bytes(out)


# ---- word dictionary (FORMAT.md 20.2)
def word_inv(t, n):
    need(len(t) >= 6, "word header")
    np_ = t[0]; need(1 <= np_ <= 64 and len(t) >= np_ + 6, "word prefixes")
    pre = list(t[1:1 + np_]); need(len(set(pre)) == np_, "word prefix twice")
    capb = t[1 + np_]; need(capb not in pre, "word cap byte is a prefix")
    nw = struct.unpack_from("<I", t, 2 + np_)[0]; need(nw <= np_ * 256 and nw <= 64 * 256, "word count")
    p = 6 + np_; words = []
    for _ in range(nw):
        e = t.find(b"\n", p); need(e >= 0 and 3 <= e - p <= 48, "word entry")
        words.append(t[p:e]); p = e + 1
    pidx = {b: k for k, b in enumerate(pre)}
    out = bytearray()
    while p < len(t):
        s = p
        while p < len(t) and t[p] != capb and t[p] not in pidx: p += 1
        need(p - s <= n - len(out), "word output"); out += t[s:p]
        if p >= len(t): break
        cap = t[p] == capb
        if cap:
            p += 1; need(p < len(t) and t[p] in pidx, "word cap byte not before a code")
        need(len(t) - p >= 2, "word code"); code = pidx[t[p]] << 8 | t[p + 1]; p += 2
        need(code < nw, "word code above the dictionary"); w = bytearray(words[code])
        need(len(w) <= n - len(out), "word output")
        if cap: w[0] &= 0xDF
        out += w
    need(len(out) == n, "word length")
    return bytes(out)


# ---- log templates (FORMAT.md 20.3)
def unzz(z): return (z >> 1) ^ -(z & 1)


def get_ints(t, p, m):
    need(len(t) - p >= 2, "log ints header")
    mode, w = t[p], t[p + 1]; p += 2
    need(mode <= 1 and 1 <= w <= 8 and len(t) - p >= m * w, "log ints")
    planes = [t[p + b * m:p + (b + 1) * m] for b in range(w)]
    v = []; acc = 0
    for i in range(m):
        z = 0
        for b in range(w): z |= planes[b][i] << (8 * b)
        x = unzz(z)
        if mode: acc = (acc + x) & M64; v.append(acc)
        else: v.append(x & M64)
    return v, p + m * w


M64 = (1 << 64) - 1


def log_inv(t, n):
    need(len(t) >= 21 and t[0] == 1, "log header")
    L, T, tail, dlen, rlen = struct.unpack_from("<5I", t, 1)
    need(L <= n and T <= L and tail <= n, "log counts")
    lid, p = get_ints(t, 21, L)
    need(all(x <= T for x in lid), "log template id")
    cnt = [0] * (T + 1)
    for x in lid: cnt[x] += 1
    need(len(t) - p >= dlen + rlen + tail, "log sections")
    dict_ = t[p:p + dlen]; rare = t[p + dlen:p + dlen + rlen]; tailb = t[p + dlen + rlen:p + dlen + rlen + tail]
    p += dlen + rlen + tail
    tpl = [None]; ncols = 0; d = 0
    for _ in range(T):
        e = dict_.find(b"\n", d); need(e >= 0, "log template")
        parts = dict_[d:e].split(b"\x01")
        tpl.append((parts, list(range(ncols, ncols + len(parts) - 1)))); ncols += len(parts) - 1; d = e + 1
    need(d == dlen, "log dictionary: bytes left")
    need(ncols * 5 <= len(t) - p, "log columns")
    cols = []
    for tt in range(1, T + 1):
        for _ in tpl[tt][1]:
            m = cnt[tt]
            need(p < len(t), "log column type"); ty = t[p]; p += 1
            if ty == 0:
                need(len(t) - p >= 4, "log text column"); size = struct.unpack_from("<I", t, p)[0]; p += 4
                need(size <= len(t) - p, "log text column size"); cols.append(["t", t[p:p + size], 0]); p += size
            elif ty == 1:
                need(p < len(t), "log skeleton"); g = t[p]; p += 1; need(1 <= g <= 12, "log skeleton runs")
                pieces = []
                for _ in range(g + 1):
                    need(p < len(t), "log piece"); l = t[p]; p += 1; need(l <= len(t) - p, "log piece"); pieces.append(t[p:p + l]); p += l
                runs = []
                for _ in range(g):
                    need(p < len(t), "log pad"); pad = t[p]; p += 1; need(pad <= 18, "log pad")
                    need(len(t) - p >= 2, "log run"); w = t[p + 1]
                    need(1 <= w <= 8 and t[p] <= 1 and len(t) - p - 2 >= m * w, "log run")
                    v, p = get_ints(t, p, m)
                    lim = 10 ** pad if 1 <= pad < 18 else 10 ** 18
                    need(all(x < lim for x in v), "log run value")
                    runs.append((pad, v))
                cols.append(["s", pieces, runs, 0])
            else:
                raise Corrupt("log column type %d" % ty)
    need(p == len(t), "log: bytes left")
    out = bytearray(); rp = 0
    for x in lid:
        if x == 0:
            e = rare.find(b"\n", rp); need(e >= 0, "log rare line"); need(e + 1 - rp <= n - len(out), "log output")
            out += rare[rp:e + 1]; rp = e + 1; continue
        parts, cidx = tpl[x]
        for k, lit in enumerate(parts):
            need(len(lit) <= n - len(out), "log output"); out += lit
            if k == len(cidx): break
            C = cols[cidx[k]]
            if C[0] == "t":
                e = C[1].find(b"\n", C[2]); need(e >= 0, "log text value")
                need(e - C[2] <= n - len(out), "log output"); out += C[1][C[2]:e]; C[2] = e + 1
            else:
                i = C[3]; C[3] += 1
                for j, pc in enumerate(C[1]):
                    need(len(pc) <= n - len(out), "log output"); out += pc
                    if j == len(C[2]): break
                    pad, v = C[2][j]
                    ds = str(v[i]).encode().rjust(pad, b"0")
                    need(len(ds) <= n - len(out), "log output"); out += ds
        need(len(out) < n, "log output"); out.append(10)
    need(rp == len(rare), "log rare lines left")
    need(all(C[2] == len(C[1]) for C in cols if C[0] == "t"), "log text values left")
    need(n - len(out) == tail, "log tail")
    return bytes(out + tailb)


def json_inv(t, n, k):
    """tflag 10: the log stream decodes to n + k bytes; every newline is removed"""
    need(k <= n, "json newline count")
    d = log_inv(t, n + k).replace(b"\n", b"")
    need(len(d) == n, "json length")
    return d


# ---- the graph program (SPEC.md, Kolmo graph blocks): only the leaves are needed here
def graph_leaves(p):
    need(len(p) >= 3 and p[0] == ord("G") and p[1] == 1, "graph header")
    nn, i = uv64(p, 2)
    for _ in range(nn):
        i += 1
        nin, i = uv64(p, i)
        for _ in range(nin): _, i = uv64(p, i)
        _, i = uv64(p, i)
        pl, i = uv64(p, i); i += pl
    nl, i = uv64(p, i)
    for _ in range(nl):
        sid, i = uv64(p, i)
        cc = p[i]; i += 1
        rl, i = uv64(p, i)
        zl, i = uv64(p, i)
        yield sid, cc, rl, p[i:i + zl]
        i += zl
    need(i == len(p), "graph: bytes left")


# ---- the corpus walk (container layer: FORMAT.md 3-7)
def frames_of(f):
    if f[:4] == b"CZNS":
        p = 12
        while True:
            ln = struct.unpack_from("<Q", f, p)[0]; p += 8
            if not ln: return
            yield f[p:p + ln]; p += ln
    else:
        yield f


def blocks_of(f):
    nb = struct.unpack_from("<I", f, 20)[0]
    po, ro = 32 + 16 * nb, 0
    for k in range(nb):
        raw, comp, ck, codec, tflag = struct.unpack_from("<IIIBB", f, 32 + 16 * k)
        yield raw, ck, codec, tflag, f[po:po + comp], ro
        po += comp; ro += raw


META = {3: 4, 4: 4, 5: 4, 6: 4, 7: 4, 9: 4, 10: 8, 12: 4}


def check_block(raw, ck, codec, tflag, pay, orig, rt, stats):
    t, a, b, c = pay[:4]
    if codec == 12:
        for sid, cc, rl, z in graph_leaves(pay[4:]):
            if cc in NATIVE:
                need(len(decode(cc, z, rl)) == rl, "leaf length")
                stats["leaf %d" % cc] += 1
        return
    if codec not in NATIVE: return
    meta = 4 * (c + 1) if (tflag and t == 1) else META.get(tflag, 0)
    body = pay[4 + meta:]
    dlen = struct.unpack_from("<I", pay, 4)[0] if META.get(tflag) else raw
    d = decode(codec, body, dlen)
    need(len(d) == dlen, "decoded length")
    if tflag == 0:
        need(orig is None or d == orig, "decoded bytes differ from the original")
        kind = "block %d, checked against the original" % codec
    elif tflag == 1 and t == 2:
        d = shuffle_inv(d, raw, a, b, c)
        need(rt or (khash64(d, 0x637a6e31) & 0xFFFFFFFF) == ck, "shuffle inverse: checksum")
        kind = "block %d, shuffle inverse checked" % codec
    elif tflag in (5, 9, 10, 12):
        if tflag == 5: d = log_inv(d, raw)
        elif tflag == 9: d = word_inv(d, raw)
        elif tflag == 12: d = lines_inv(d, raw)
        else: d = json_inv(d, raw, struct.unpack_from("<I", pay, 8)[0])
        need(orig is None or d == orig, "inverse transform: bytes differ from the original")
        kind = "block %d, tflag %d inverse checked against the original" % (codec, tflag)
    elif tflag == 8:
        d = x86_inv(d)
        need(rt or (khash64(d, 0x637a6e31) & 0xFFFFFFFF) == ck, "x86 inverse: checksum")
        kind = "block %d, x86 inverse checked" % codec
    elif tflag == 14:
        d = a64_inv(d, a | (b & 15) << 8)
        need(rt or (khash64(d, 0x637a6e31) & 0xFFFFFFFF) == ck, "ARM64 inverse: checksum")
        kind = "block %d, ARM64 inverse checked" % codec
    else:
        kind = "block %d, tflag %d: transformed bytes decoded" % (codec, tflag)
    stats[kind] += 1


def mutations():
    """one stream per refusal rule of the prose; each must raise Corrupt"""
    yield "klz offset 0", (28, bytes([0x31, ord("a"), 0, 0, 0x00]), 11)
    yield "klz offset past the start", (28, bytes([0x31, ord("a"), 2, 0, 0x00]), 11)
    yield "klz match past the end", (28, bytes([0x31, ord("a"), 1, 0, 0x00]), 5)
    yield "klz end token with match bits", (28, bytes([0x31, ord("a"), 1, 0, 0x08]), 11)
    yield "klz bytes after the end", (28, bytes([0x31, ord("a"), 1, 0, 0x00, 0]), 11)
    yield "klz truncated", (28, bytes([0x31, ord("a"), 1]), 11)
    yield "khuf mode 3", (27, bytes([3, 4, 1, 2, 3, 4]), 4)
    yield "khuf chunk longer than the output", (27, bytes([1, 5, 7]), 4)
    yield "khuf zero-length chunk", (27, bytes([1, 0, 7, 1, 4, 7]), 4)
    yield "khuf one-symbol table", (27, bytes([2, 4, 3, 0, 65, 1]), 4)
    yield "khuf incomplete code", (27, bytes([2, 4, 6, 1, 65, 1, 66, 2, 0, 0, 0, 0]), 4)
    yield "klzh2 literal stream in rANS", (32, bytes([0, 4, 1, 1, 0]), 4)
    yield "klzh2 mode 2", (32, bytes([0, 4, 0, 6, 1, 4, 1, 2, 3, 4, 2, 0, 0, 0, 0, 0, 0]), 4)
    yield "klzh2 too many sequences", (32, bytes([3, 0]), 4)
    yield "krans frequencies not 4096", (-1, bytes([1, 0x80, 0x10, 1]), 4)
    yield "krans state below 2^16", (-1, bytes([1, 0x80, 0x10, 0x80, 0x10]) + bytes(32 + 7), 4)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "v12")
    for name, (codec, data, n) in mutations():
        try:
            krans(data, n) if codec == -1 else decode(codec, data, n)
            sys.exit("coder reference accepted a broken stream: " + name)
        except Corrupt:
            pass
    from collections import Counter
    stats = Counter(); bad = []; nframes = 0
    rels = sorted(d for d in os.listdir(root) if d.startswith("r") and os.path.isdir(os.path.join(root, d)))
    entries = []
    for line in open(os.path.join(root, "MANIFEST")):
        if line.strip():
            fr, og = line.split()[:2]; entries.append((fr, og))
    for rel in rels:
        for e in json.load(open(os.path.join(root, rel, "frames.json")))["frames"]:
            entries.append((rel + "/" + e["file"], rel + "/" + e["input"]))
    seen = set()
    for fr, og in entries:
        if fr in seen: continue
        seen.add(fr)
        o = open(os.path.join(root, og), "rb").read()
        f = open(os.path.join(root, fr), "rb").read()
        nframes += 1
        try:
            ro = 0
            for g in frames_of(f):
                orig_seg = o[ro:ro + struct.unpack_from("<Q", g, 12)[0]]; ro += len(orig_seg)
                if struct.unpack_from("<I", g, 20)[0] == 1 and g[32 + 12] == 20:
                    po = 48
                    comp = struct.unpack_from("<I", g, 36)[0]
                    g = g[po + 4:po + comp]; orig_seg = None   # video: the nested frame holds the kvid layout
                rt = g[10] & 1
                for raw, ck, codec, tflag, pay, r0 in blocks_of(g):
                    check_block(raw, ck, codec, tflag, pay, orig_seg[r0:r0 + raw] if orig_seg is not None else None, rt, stats)
        except (Corrupt, IndexError, struct.error) as e:
            bad.append("%s: %s" % (fr, e))
    for b in bad: print(b)
    for k in sorted(stats): print("  %5d %s" % (stats[k], k))
    if bad: sys.exit("CODER REFERENCE FAILED (%d of %d frames)" % (len(bad), nframes))
    print("CODER REFERENCE PASSED (%d frames)" % nframes)


if __name__ == "__main__":
    main()
