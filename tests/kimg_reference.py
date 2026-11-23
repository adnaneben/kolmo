#!/usr/bin/env python3
"""Independent decoder for kimg (codec 31), FORMAT.md section 23, in plain Python.

usage: python3 tests/kimg_reference.py [tests/v12]

Decodes every corpus block with codec 31 and compares it with the original.
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coder_reference as R  # noqa: E402

need, Corrupt = R.need, R.Corrupt


def tdiv(a, b):   # C division, truncating toward zero
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


# ---- tables (FORMAT.md 23.2)
_T33 = [1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101, 1546, 2047,
        2549, 2994, 3348, 3607, 3785, 3901, 3975, 4024, 4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094]
SQ = []
for i in range(4096):
    d = i - 2048; w = d & 127; k = (d >> 7) + 16
    SQ.append(4095 if k >= 32 else (_T33[k] * (128 - w) + _T33[k + 1] * w + 64) >> 7)


def squash(d): return SQ[max(-2047, min(2047, d)) + 2048]


ST = [0] * 4096; _pi = 0
for x in range(-2047, 2048):
    v = squash(x)
    for j in range(_pi, v + 1): ST[j] = x
    _pi = v + 1
for j in range(_pi, 4096): ST[j] = 2047
DT = [int(65536.0 * 2 / (2 * i + 3)) for i in range(256)]
WT = [0] + [(1 << 44) // (s * s) for s in range(1, 16384)]; WT[0] = WT[1]
BUCKET_T = [1, 3, 6, 10, 16, 24, 36, 54, 80, 120, 200]


def bucket(e):
    b = 0
    while b < 11 and e >= BUCKET_T[b]: b += 1
    return b


def q8(v):
    v = abs(v)
    return 0 if v == 0 else 1 if v <= 1 else 2 if v <= 2 else 3 if v <= 4 else 4 if v <= 8 else 5 if v <= 16 else 6 if v <= 32 else 7


def sgn(v): return (v > 0) - (v < 0)


class RC:
    def __init__(self, p, a):
        self.p, self.i = p, a; self.x1, self.x2, self.x, self.err = 0, 0xFFFFFFFF, 0, 0
        for _ in range(4): self.x = ((self.x << 8) | self.byte()) & 0xFFFFFFFF

    def byte(self):
        if self.i < len(self.p): b = self.p[self.i]; self.i += 1; return b
        self.err += 1; return 0

    def get(self, p):   # p: 12-bit probability of a 1
        xm = (self.x1 + ((self.x2 - self.x1) >> 12) * p) & 0xFFFFFFFF
        bit = 1 if self.x <= xm else 0
        if bit: self.x2 = xm
        else: self.x1 = (xm + 1) & 0xFFFFFFFF
        while ((self.x1 ^ self.x2) & 0xFF000000) == 0:
            self.x1 = (self.x1 << 8) & 0xFFFFFFFF; self.x2 = ((self.x2 << 8) | 255) & 0xFFFFFFFF
            self.x = ((self.x << 8) | self.byte()) & 0xFFFFFFFF
        return bit


def ctr(lim=250): return [32768, 0, lim]   # probability (16 bits), count, limit


def upd(c, bit):
    t = 65535 if bit else 0
    c[0] = (c[0] + (((t - c[0]) * DT[c[1]]) >> 16)) & 0xFFFF
    if c[1] < c[2]: c[1] += 1


def unfold(u, P, V):
    m = min(P, V - P)
    if not u: return 0
    if u <= 2 * m:
        k = (u + 1) >> 1; return k if u & 1 else -k
    a = u - m
    return a if P < V - P else -a


# ---- the sample loop (FORMAT.md 23.3)
def run(ver, npx, iw, ich, phase, V, rc):
    need(iw and ich and ich <= 4 and iw % ich == 0 and phase < iw, "kimg geometry")
    NA, NODES = 12, 27
    t0 = [[[ctr() for _ in range(NODES)] for _ in range(NA)] for _ in range(4)]
    t1 = [[[[ctr() for _ in range(NODES)] for _ in range(8)] for _ in range(8)] for _ in range(4)]
    t2 = [[[[ctr() for _ in range(NODES)] for _ in range(6)] for _ in range(27)] for _ in range(4)]
    t3 = [[[[ctr() for _ in range(NODES)] for _ in range(8)] for _ in range(8)] for _ in range(4)]
    low = [[[ctr(255) for _ in range(9)] for _ in range(9)] for _ in range(4)]
    W0 = (1 << 16) * 3 // 10
    wts = [[[[W0, W0, W0, W0, 0] for _ in range(NODES)] for _ in range(NA)] for _ in range(4)]
    bsum = [[[0] * 9 for _ in range(NA)] for _ in range(4)]; bcnt = [[[0] * 9 for _ in range(NA)] for _ in range(4)]
    ZERO = [0] * 12
    err = [{}, {}]   # error memory per row parity, by byte column (sparse: iw may be large)
    px = [0] * npx
    prevres = 0; prevE = [0, 0, 0]
    xx, rr, cc = phase, 0, phase % ich
    s0 = 4 if ver >= 2 else 1

    def code(rows, w, node):
        st = [ST[rows[i][node][0] >> 4] for i in range(4)]; ws = w[node]
        dot = ws[4] * 256 + sum(ws[i] * st[i] for i in range(4))
        p = squash(dot >> 16); p = 1 if p < 1 else 4094 if p > 4094 else p
        bit = rc.get(p)
        e = ((bit << 12) - p)
        for i in range(4): ws[i] += (st[i] * e) >> 12
        ws[4] += (256 * e) >> 12
        for i in range(4): upd(rows[i][node], bit)
        return bit

    for k in range(npx):
        x, row, c = xx, rr, cc
        xx += 1
        if xx == iw: xx = 0; rr ^= 1; cc = 0
        else:
            cc += 1
            if cc == ich: cc = 0
        hasW = x >= ich and k >= ich; hasN = k >= iw; hasNW = hasN and x >= ich and k >= iw + ich
        hasNE = k + ich >= iw and x + ich < iw and k >= iw - ich; hasWW = x >= 2 * ich and k >= 2 * ich; hasNN = k >= 2 * iw
        N = px[k - iw] if hasN else (px[k - ich] if hasW else 0)
        Wv = px[k - ich] if hasW else N
        NW = px[k - iw - ich] if hasNW else N; NE = px[k - iw + ich] if hasNE else N
        WW = px[k - 2 * ich] if hasWW else Wv; NN = px[k - 2 * iw] if hasNN else N
        NNE = px[k - 2 * iw + ich] if hasNN and hasNE else NE
        P = [0] * 10
        P[0] = Wv + N - NW
        mn, mx = min(Wv, N), max(Wv, N)
        P[1] = mn if NW >= mx else mx if NW <= mn else Wv + N - NW
        P[2] = (3 * Wv + 3 * N - 2 * NW + NE - NN + WW + 2) >> 2
        P[3] = N + NE - NNE; P[4] = Wv + NE - N; P[5] = N; P[6] = Wv
        P[7] = P[0] + prevE[0]; P[8] = P[1] + prevE[1]; P[9] = P[2] + prevE[2]
        prow = row ^ 1
        g_ = lambda r_, x_: err[r_].get(x_, ZERO)
        eW = g_(row, x - ich) if hasW else ZERO; eN = g_(prow, x) if hasN else ZERO
        eNW = g_(prow, x - ich) if hasNW else ZERO; eNE = g_(prow, x + ich) if hasNE else ZERO
        eWW = g_(row, x - 2 * ich) if ver >= 2 and hasWW else ZERO
        num = den = 0
        for i in range(10):
            s = s0 + eW[i] + eN[i] + eNW[i] + eNE[i] + eWW[i]
            w = WT[min(s, 16383)]; num += w * P[i]; den += w
        pr0 = tdiv(num + (den >> 1), den); pr = pr0
        ee = 2 * eW[10] + 2 * eN[10] + eNW[10] + eNE[10]
        if c: ee += 2 * abs(prevres)
        a = bucket(ee >> 1)
        g1, g2 = N - NW, Wv - NW
        sc = sgn(g1) + 1 + 3 * (sgn(g2) + 1)
        bc = bcnt[c][a][sc]; bs = bsum[c][a][sc]
        if bc: pr += tdiv(bs + (bc >> 1) if bs >= 0 else bs - (bc >> 1), bc)
        Vc = V[c]; Pc = 0 if pr < 0 else Vc if pr > Vc else pr
        g3 = NE - N; tx = sgn(g1) + 1 + 3 * (sgn(g2) + 1) + 9 * (sgn(g3) + 1)
        rows = (t0[c][a], t1[c][q8(eW[10])][q8(eN[10])], t2[c][tx][a >> 1],
                t3[c][q8(prevres) if c else q8(eNW[10])][q8(eNE[10])])
        w = wts[c][a]
        if code(rows, w, 0): u = 0
        else:
            LM = 8 if Vc > 255 else 7; L = 0
            while L < LM and code(rows, w, 1 + L): L += 1
            u = 1 << L
            if L >= 1:
                u |= code(rows, w, 9 + L) << (L - 1)
                if L >= 2: u |= code(rows, w, 18 + L) << (L - 2)
                for b in range(L - 3, -1, -1):
                    cl = low[c][L][b]; p = cl[0] >> 4; p = 1 if p < 1 else 4094 if p > 4094 else p
                    bb = rc.get(p); upd(cl, bb); u |= bb << b
        need(u <= Vc and rc.err <= 8, "kimg sample")
        v = Pc + unfold(u, Pc, Vc); px[k] = v
        res = v - Pc
        bsum[c][a][sc] = bs + v - pr0; bcnt[c][a][sc] = bc + 1
        if bc + 1 >= (512 if ver >= 2 else 64):
            bsum[c][a][sc] = tdiv(bsum[c][a][sc], 2); bcnt[c][a][sc] = (bc + 1) // 2
        eo = err[row].setdefault(x, [0] * 12)
        for i in range(10):
            q = 0 if P[i] < 0 else Vc if P[i] > Vc else P[i]; eo[i] = abs(v - q) & 0xFFFF
        eo[10] = abs(res) & 0xFFFF
        if c + 1 < ich: prevE = [v - P[0], v - P[1], v - P[2]]; prevres = res
        else: prevE = [0, 0, 0]; prevres = 0
    return px


def kimg(p, n):
    need(len(p) >= 24 and p[0] in (1, 2) and p[2] <= 2 and p[3] == 0 and struct.unpack_from("<I", p, 20)[0] == 0, "kimg header")
    ich = p[1]; iw, phase, head, npx = struct.unpack_from("<4I", p, 4); t = p[2]
    need(1 <= ich <= 4 and iw and iw % ich == 0 and phase < iw and head <= n and npx <= n - head, "kimg geometry")
    need(not t or (ich >= 3 and phase % ich == 0 and npx % ich == 0), "kimg colour transform")
    tail = n - head - npx
    need(len(p) >= 24 + head + tail, "kimg stored bytes")
    V = [255, 510 if t else 255, 510 if t else 255, 255]
    s = run(p[0], npx, iw, ich, phase, V, RC(p, 24 + head + tail))
    out = bytearray(p[24:24 + head])
    if not t: out += bytes(s)
    else:
        px = bytearray(npx)
        for k in range(0, npx - ich + 1, ich):
            if t == 1: G = s[k]; Rr = s[k + 1] - 255 + G; B = s[k + 2] - 255 + G
            else:
                Y, Co, Cg = s[k], s[k + 1] - 255, s[k + 2] - 255; tt = Y - (Cg >> 1); G = Cg + tt; B = tt - (Co >> 1); Rr = B + Co
            need(0 <= Rr <= 255 and 0 <= G <= 255 and 0 <= B <= 255, "kimg colour range")
            px[k], px[k + 1], px[k + 2] = Rr, G, B
            for cc in range(3, ich): px[k + cc] = s[k + cc]
        out += px
    out += p[24 + head:24 + head + tail]
    return bytes(out)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "v12")
    n = 0; bad = []
    entries = [l.split()[:2] for l in open(os.path.join(root, "MANIFEST")) if l.strip()]
    for fr, og in entries:
        f = open(os.path.join(root, fr), "rb").read()
        if f[:4] != b"CZN1": continue
        o = open(os.path.join(root, og), "rb").read()
        for raw, ck, codec, tflag, pay, r0 in R.blocks_of(f):
            if codec != 31: continue
            try:
                need(kimg(pay[4:], raw) == o[r0:r0 + raw], "bytes differ from the original"); n += 1
            except (Corrupt, IndexError, struct.error) as e: bad.append("%s: %s" % (fr, e))
    for b in bad: print(b)
    if bad: sys.exit("KIMG REFERENCE FAILED")
    print("KIMG REFERENCE PASSED (%d kimg blocks decoded to their originals)" % n)


if __name__ == "__main__":
    main()
