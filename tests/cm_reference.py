#!/usr/bin/env python3
"""Independent decoder for Kolmo's context-mixing coders, FORMAT.md section 24, in plain Python:
codec 7 (CM, models 1 to 4), codec 17 (CM with the image model) and codec 25 (CM over parts, CMP).

usage: python3 tests/cm_reference.py [--prefix N | --full] [tests/v12]

The coder models every bit of the output, so this decoder runs at about a kilobyte per second. By default (make check)
it decodes the vectors in tests/vectors/cm/ whole (about 40 s in all) and the first N = 1024 bytes of every codec 7, 17 and 25 stream in
the corpus (each part of a CMP stream). It compares each prefix with the original where the block has no transform, and
with tests/vectors/cm/prefixes.json (SHA-256 of the prefix) otherwise. --full decodes every stream whole and checks it
against the original through the transform inverses of tests/coder_reference.py (about 20 minutes; `make
check-cm-reference`). The mixer priors are the normative data file tests/format_tables/cm_prior.json.
"""
import hashlib, json, math, os, struct, sys
from operator import mul

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coder_reference as R  # noqa: E402  (frame walk, Corrupt, varints, transforms of FORMAT.md 19-20)

need, Corrupt = R.need, R.Corrupt
HERE = os.path.dirname(os.path.abspath(__file__))
PRIOR = json.load(open(os.path.join(HERE, "format_tables", "cm_prior.json")))
M = 0xFFFFFFFF


# ---- fixed tables (FORMAT.md 24.2)
def _tables():
    # bit histories: (zeros, ones, last bit), counts capped at 24, the opposite count above 2 halved (rounding up)
    ids = {(0, 0, 0): 0}; cnt = [(0, 0)]; q = [(0, 0)]; nxt = [[0, 0] for _ in range(256)]; k = 0
    while k < len(q):
        n0, n1 = q[k]
        for y in (0, 1):
            a, b = n0, n1
            if y:
                b = min(b + 1, 24)
                if a > 2: a = (a + 1) // 2
            else:
                a = min(a + 1, 24)
                if b > 2: b = (b + 1) // 2
            if (a, b, y) not in ids:
                ids[(a, b, y)] = len(cnt); cnt.append((a, b)); q.append((a, b))
            nxt[k][y] = ids[(a, b, y)]
        k += 1
    ns = len(cnt)
    n0 = [cnt[s][0] if s < ns else 0 for s in range(256)]; n1 = [cnt[s][1] if s < ns else 0 for s in range(256)]
    knot = [0] * 33
    for k in range(16): knot[k] = math.floor(4096.0 / (1.0 + math.exp((16 - k) * 0.5)))
    knot[16] = 2047
    for k in range(16): knot[32 - k] = 4095 - knot[k]

    def squash_i(x):
        if x >= 2048: return 4095
        if x <= -2048: return 1
        s, f = (x + 2048) >> 7, (x + 2048) & 127
        return (knot[s] * (128 - f) + knot[s + 1] * f + 64) >> 7
    sq = [min(max(squash_i(i - 2048), 1), 4095) for i in range(4096)]
    sqx = [min(max(math.floor(4096.0 / (1.0 + math.exp(-(i - 2048) / 256.0)) + 0.5), 1), 4095) for i in range(4096)]

    def inverse(f):   # the smallest x in [-2047, 2047] with f(x) >= p, else 2047
        st = [2047] * 4096; x = -2047
        for p in range(4096):
            while x <= 2047 and f(x) < p: x += 1
            st[p] = min(x, 2047)
        return st
    st = inverse(squash_i); stx = inverse(lambda x: sqx[x + 2048])
    rcp = [655360 // (10 * i + 16) for i in range(64)]
    srcp = [655360 // (10 * i + 15) for i in range(1024)]
    pri = [n0[s] + n1[s] for s in range(256)]
    dx = []
    for s in range(256):
        a, b = n0[s], n1[s]; n = min(a + b, 7)
        dx.append(0 if a and b else 128 * (n + 1) if b else -128 * (n + 1) if a else 0)
    return [x[0] for x in nxt], [x[1] for x in nxt], n0, n1, sq, st, sqx, stx, rcp, srcp, pri, dx


NX0, NX1, BN0, BN1, SQ_L, ST_L, SQ_X, ST_X, RCP, SRCP, PRI, DX = _tables()


def hmix(a):
    a ^= a >> 15; a = (a * 0x2C1B3C6D) & M; a ^= a >> 12; a = (a * 0x297A2D39) & M; return a ^ (a >> 15)


TV_LR, TV_LIM, TV_APM, TV_MC = (0, -1, 1, 2), (2, 1, 3, 4), (13, 12, 14, 15), (6, 5, 7, 8)
TV_LRK, TV_LRD, TV_SM = (20, 12, 28, 40), (13, 12, 14, 16), (255, 127, 511, 1023)
TV_FIN = ((2, 2, 2, 2), (4, 1, 1, 2), (1, 2, 2, 3), (3, 1, 2, 2))
MASK3 = ((1 << 30) - 1) & ~1


# ---- stream headers (FORMAT.md 24.1)
def cm_header(p, n):
    """codec 7: (variant, log2 buckets or size shift, exact tables, tune bytes, mask, offset of the coded bytes)"""
    need(len(p) >= 8, "cm: stream shorter than 8 bytes")
    need(n < (1 << 28), "cm: streams of 2^28 bytes and more are not covered here")
    n32 = struct.unpack_from("<I", p, 0)[0]; f = n32 >> 28; n32 &= 0x0FFFFFFF
    v, bb, lx, tune, mask, off = 0, 20, 0, (0, 0, 0), 0, 4
    if f == 8 and len(p) >= 16 and p[4] >> 4 == 9:
        need(p[7] <= 2, "cm: model 4 prior byte above 2")
        mask = struct.unpack_from("<I", p, 8)[0]
        need(not mask & ~MASK3, "cm: model 4 mask with bit 0 or 30-31")
        v, lx, bb, tune, off = 4, 1, (p[4] & 15) + 2, (p[5], p[6], p[7]), 12
    elif f == 8:
        mb = p[4] >> 4 if len(p) >= 9 else 0
        need(mb in (3, 5, 6, 7, 14, 15), "cm: model nibble %d" % mb)
        lx = 1 if mb >= 5 else 0
        v = 3 if mb & 3 == 3 else 1 if mb & 3 == 2 else 0
        bb = (p[4] & 15) + 15; off = 5
        if mb >= 14:
            need(len(p) >= 12 and p[7] <= 1, "cm: tuned stream: prior byte above 1")
            tune = (p[5], p[6], p[7]); off = 8
    elif f:
        bb, v = (f & 7) + 15, f >> 3
    need(n32 == n, "cm: size word differs from the block length")
    need(v == 4 or 16 <= bb <= 22, "cm: table size out of range")
    return v, bb, lx, tune, mask, off


def cm_decode(p, n, maxn=None):
    v, bb, lx, tune, mask, off = cm_header(p, n)
    return cm_run(p, off, n, v, bb, lx, tune, mask, None, maxn)


def cmi_decode(p, n, maxn=None):
    """codec 17: iw u32 | ih u32 | ich u8 (bit 7: exact tables) | size word | coded bytes"""
    need(len(p) >= 17, "cmi: stream shorter than 17 bytes")
    iw, ih = struct.unpack_from("<II", p, 0); ich, lx = p[8] & 127, p[8] >> 7
    n32 = struct.unpack_from("<I", p, 9)[0]; f = n32 >> 28; bb = (f & 7) + 15
    need(n < (1 << 28) and n32 & 0x0FFFFFFF == n and f & 8 and 16 <= bb <= 22, "cmi: size word")
    need(iw and 1 <= ich <= 4 and iw >= ich and ih <= n, "cmi: image geometry")
    return cm_run(p, 13, n, 2, bb, lx, (0, 0, 0), 0, (iw, ich, ih), maxn)


def cmp_parts(p, n):
    """codec 25: version 0 | parts uv | per part raw size uv, coded size uv | the parts' codec-7 streams"""
    need(len(p) >= 2 and p[0] == 0, "cmp: version")
    np_, i = R.uv64(p, 1)
    need(1 <= np_ <= 256, "cmp: part count")
    sizes = []; ro = 0
    for _ in range(np_):
        rn, i = R.uv64(p, i); need(rn and rn <= n - ro, "cmp: part size"); ro += rn
        cl, i = R.uv64(p, i); need(cl <= len(p), "cmp: coded size")
        sizes.append((rn, cl))
    need(ro == n, "cmp: parts do not add up to the block")
    parts = []
    for rn, cl in sizes:
        need(cl <= len(p) - i, "cmp: part past the end"); parts.append((p[i:i + cl], rn)); i += cl
    need(i == len(p), "cmp: bytes after the last part")
    return parts


def cmp_decode(p, n, maxn=None):
    return b"".join(cm_decode(z, rn, maxn) for z, rn in cmp_parts(p, n))


# ---- the coder (FORMAT.md 24.3-24.7)
def cm_run(p, off, n, V, bb, lx, tune, mask, img, maxn):
    SQT, STT = (SQ_X, ST_X) if lx else (SQ_L, ST_L)

    def SQ(x): return SQT[(2047 if x > 2047 else -2047 if x < -2047 else x) + 2048]
    # contexts: slot k holds context mid[k]; slot 0 is the order-1 direct counter
    if V == 4: mid = [0] + [i for i in range(1, 30) if mask >> i & 1]
    else: mid = list(range(30 if V == 3 else 21 if V else 16))
    nm = len(mid); ni = 2 * nm + 3; nie = ni + 1
    if V == 4:
        nl = [0] * nm
        for k in range(1, nm):
            i = mid[k]; l_ = n >> bb; l_ >>= 2 if i >= 25 else 1 if i >= 16 else 0
            nl[k] = min(max(l_, 1 << 12), 1 << 22)
        mb = 16
        while mb < 22 and (1 << mb) < n: mb += 1
        mmask = (1 << mb) - 1
    else:
        shi = [0] + [32 - (bb - 1 - (i >= 25) if i >= 16 and bb > 18 else bb) for i in mid[1:]]
        mmask = (1 << 22) - 1
    T = [dict() for _ in range(nm)]
    # learning settings (FORMAT.md 24.6)
    b0, b1, b2 = tune
    lr = 12 + TV_LR[b0 & 3]; lim = TV_LIM[b0 >> 2 & 3]; apm_sh = TV_APM[b0 >> 4 & 3]; mc_sh = TV_MC[b0 >> 6]
    lrk = TV_LRK[b1 & 3]; lrd = TV_LRD[b1 >> 2 & 3]; smlim = TV_SM[b1 >> 4 & 3]; fw0, fw1, fw2, fw3 = TV_FIN[b1 >> 6]
    rcp = [RCP[k * lim] for k in range(16)]
    # mixer weights: rows of 4 mixers (2 for model 1) and the final mixer, each row a copy of its template
    if V == 4:
        tpl = [[1024] * nie for _ in range(4)]
        if b2:
            pr = PRIOR["cm_prior4"][b2 - 1]
            jm = [-1] * ni
            for k in range(nm): jm[k] = mid[k]; jm[nm + 3 + k] = 33 + mid[k]
            for k in range(3): jm[nm + k] = 30 + k
            for j in range(ni):
                if jm[j] >= 0:
                    for r in range(4): tpl[r][j] = max(-32768, min(32767, (pr[r][jm[j]] * 4) >> 4))
    else:
        tpl = [[1 << 14] * ni for _ in range(4)]
        if b2 == 1 and V:
            pr = PRIOR["cm_prior"][1 if V >= 3 else 0]
            for r in range(4): tpl[r] = [pr[r][j] * 4 for j in range(ni)]
    if V == 4: tpl = [t[:nie] for t in tpl]
    wft = [1 << 14] * 4 + [0]
    if V and ((V == 4 and b2) or (V != 4 and b2 == 1)):
        wft = [x * 4 for x in (PRIOR["cm_prior4"][b2 - 1] if V == 4 else PRIOR["cm_prior"][1 if V >= 3 else 0])[4][:5]]
    W1, W2, W3, W4, WF = {}, {}, {}, {}, {}
    t1, t2, t3, t4 = tpl
    # state maps, direct counters, APMs, match-model counters
    sm = []
    for k in range(nm):
        row = [0] * 2048
        for j in range(2048):
            s = j & 255; a, b = BN0[s], BN1[s]
            row[j] = (((2 * b + 1) << 22) // (2 * (a + b) + 2)) << 10
        sm.append(row)
    o1 = [0x80000000] * 65536
    a0 = [SQ((j - 16) * 128) * 16 for j in range(33)]
    A1 = [a0[:] for _ in range(256)]; A2, A3 = {}, {}
    mc = [1 << 15] * 64
    # history
    buf = bytearray(n + 8); pos = 0; c0 = 1; bp = 0; c4 = c8 = 0
    word = word1 = word2 = 0
    H = [0] * 30
    rl_last = [0] * 256; rl_d = [0] * 256; rlen = rcnt = rcand = 0
    ind1 = [0] * 256; ind2 = [0] * 65536; line0 = line1 = 0
    mt = {}; mprev = {}; mptr = 0; mlen = 0; mrec = 0
    s_tok = s_ltok = s_key = s_lt = s_fidx = s_pfn = 0; s_fst = [0] * 32; s_pfst = [0] * 32; s_ls = s_pls = 0
    i_pred = i_cls = i_act = i_n = i_med = i_wn = 0
    cur = [None] * nm
    # arithmetic decoder (FORMAT.md 24.7)
    cn = len(p); ip = off; pad = 0; code = 0
    for _ in range(4):
        if ip < cn: code = (code << 8) | p[ip]; ip += 1
        else: code = (code << 8) | 255; pad += 1
    low, high = 0, M
    nout = n if maxn is None else min(n, maxn)
    x = [0] * nie
    for _byte in range(nout):
        need(pad <= 8, "cm: stream ends early")
        for _bit in range(8):
            # -- predict
            if bp == 0 or bp == 4:
                nib = ((c0 & 15) * 0x5BD1E995) & M if bp else 0
                for k in range(1, nm):
                    h = hmix((H[mid[k]] + nib) & M); t = T[k]
                    if V == 4:
                        li = (h * nl[k]) >> 32; L = t.get(li)
                        if L is None: L = t[li] = bytearray(64)
                        chk = h & 255
                        if L[0] == chk: j = 0
                        elif L[1] == chk: j = 1
                        elif L[2] == chk: j = 2
                        elif L[3] == chk: j = 3
                        else:
                            j = 0; bpr = PRI[L[4]]
                            for q in (1, 2, 3):
                                if PRI[L[4 + 15 * q]] < bpr: bpr = PRI[L[4 + 15 * q]]; j = q
                            L[4 + 15 * j:19 + 15 * j] = bytes(15); L[j] = chk
                        cur[k] = (L, 3 + 15 * j)
                    else:
                        idx = h >> shi[k]; chk = h & 0xFFFF
                        a = t.get(idx)
                        if a is None: a = t[idx] = [0] * 16
                        if a[0] >> 16 == chk: cur[k] = a; continue
                        b = t.get(idx ^ 1)
                        if b is None: b = t[idx ^ 1] = [0] * 16
                        if b[0] >> 16 == chk: cur[k] = b; continue
                        w = a if (a[0] & 0xFFFF) <= (b[0] & 0xFFFF) else b
                        w[0] = chk << 16
                        for q in range(1, 16): w[q] = 0x80000000
                        cur[k] = w
            node = (c0 & ((1 << bp) - 1)) | (1 << bp) if bp < 4 else (c0 & ((1 << (bp - 4)) - 1)) | (1 << (bp - 4))
            c1 = i_pred if V == 2 else c4 & 255
            oi = c0 | c1 << 8; ov = o1[oi]
            sst = [0] * nm
            s_ = ((ov >> 8) & 255) | bp << 8; sst[0] = s_
            x[0] = STT[ov >> 20]; x[nm + 3] = STT[sm[0][s_] >> 20]
            if V == 4:
                for k in range(1, nm):
                    L, base = cur[k]; sv = L[base + node]; s_ = sv | bp << 8; sst[k] = s_
                    x[nm + 3 + k] = STT[sm[k][s_] >> 20]; x[k] = DX[sv]
            else:
                for k in range(1, nm):
                    vv = cur[k][node]; s_ = ((vv >> 8) & 255) | bp << 8; sst[k] = s_
                    x[k] = STT[vv >> 20]; x[nm + 3 + k] = STT[sm[k][s_] >> 20]
            # match model input
            eb = -1
            if mlen:
                bm = buf[mptr]
                if (bm + 256) >> (8 - bp) == c0: eb = (bm >> (7 - bp)) & 1
            if eb >= 0:
                l_ = min(mlen, 31)
                if V >= 3 and mlen >= 16:
                    l_ = (16 if mlen < 24 else 17 if mlen < 32 else 18 if mlen < 48 else 19 if mlen < 64 else 20 if mlen < 96 else
                          21 if mlen < 128 else 22 if mlen < 192 else 23 if mlen < 256 else 24 if mlen < 384 else 25 if mlen < 512 else
                          26 if mlen < 768 else 27 if mlen < 1024 else 28 if mlen < 1536 else 29 if mlen < 2048 else 30 if mlen < 4096 else 31)
                mci = l_ * 2 + eb
                pm = min(max(mc[mci] >> 4, 1), 4095)
                x[nm] = STT[pm]; x[nm + 1] = l_ * 16 if eb else -l_ * 16
            else:
                mci = -1; x[nm] = 0; x[nm + 1] = 0
            x[nm + 2] = 64
            ml = 0 if mlen == 0 else 1 if mlen < 16 else 2 if mlen < 32 else 3
            if V == 2: cls = i_cls
            else:
                cls = 0 if 97 <= (c1 | 32) <= 122 else 1 if 48 <= c1 <= 57 else 2
            w1i = c0 + 256 * cls
            w2i = (ml * 8 + bp) * 4 + (i_act if V == 2 else c1 >> 6)
            r1 = W1.get(w1i)
            if r1 is None: r1 = W1[w1i] = t1[:]
            r2 = W2.get(w2i)
            if r2 is None: r2 = W2[w2i] = t2[:]
            if V == 0:
                d1 = sum(map(mul, r1, x)) >> 16; d2 = sum(map(mul, r2, x)) >> 16
                pr1 = SQ(d1); pr2 = SQ(d2); st = (d1 + d2) >> 1
            else:
                w3i = ((i_n if V == 2 else (c4 >> 8) & 255) << 3) | bp
                w4i = c0 | (i_med if V == 2 else c1) << 8
                wfi = c0 | ml << 8
                r3 = W3.get(w3i)
                if r3 is None: r3 = W3[w3i] = t3[:]
                r4 = W4.get(w4i)
                if r4 is None: r4 = W4[w4i] = t4[:]
                rf = WF.get(wfi)
                if rf is None: rf = WF[wfi] = wft[:]
                if V == 4:
                    d = []
                    for r in (r1, r2, r3, r4):
                        pp = list(map(mul, x, r))
                        d.append(sum([(pp[q] + pp[q + 1]) >> 8 for q in range(0, nie, 2)]) >> 4)
                else:
                    d = [sum(map(mul, r1, x)) >> 16, sum(map(mul, r2, x)) >> 16, sum(map(mul, r3, x)) >> 16, sum(map(mul, r4, x)) >> 16]
                d = [2047 if q > 2047 else -2047 if q < -2047 else q for q in d]
                sf = rf[4] * 256 + rf[0] * d[0] + rf[1] * d[1] + rf[2] * d[2] + rf[3] * d[3]
                xf = d + [256]
                pr1, pr2, pr3, pr4 = SQ(d[0]), SQ(d[1]), SQ(d[2]), SQ(d[3])
                st = sf >> 16; st = 2047 if st > 2047 else -2047 if st < -2047 else st
                prf = SQ(st)
            st = 2047 if st > 2047 else -2047 if st < -2047 else st
            mixed = SQ(st)
            s = st + 2048; lo = s >> 7; wt = s & 127
            a2i = (c0 | c1 << 8) & 0xFFFF
            ra1 = A1[c0]; ra2 = A2.get(a2i)
            if ra2 is None: ra2 = A2[a2i] = a0[:]
            p1 = (ra1[lo] * (128 - wt) + ra1[lo + 1] * wt) >> 11
            p2 = (ra2[lo] * (128 - wt) + ra2[lo + 1] * wt) >> 11
            if V == 0: pq = (mixed + p1 + p2) // 3
            else:
                a3i = hmix(((i_wn if V == 2 else c4 & 0xFFFF) * 0x9E3779B1 + c0 * 0x85EBCA6B) & M) >> 16
                ra3 = A3.get(a3i)
                if ra3 is None: ra3 = A3[a3i] = a0[:]
                p3 = (ra3[lo] * (128 - wt) + ra3[lo + 1] * wt) >> 11
                pq = (fw0 * mixed + fw1 * p1 + fw2 * p2 + fw3 * p3 + 4) >> 3
            pq = 1 if pq < 1 else 4095 if pq > 4095 else pq
            # -- decode the bit
            mid_ = low + (((high - low) * pq) >> 12)
            if code <= mid_: y = 1; high = mid_
            else: y = 0; low = mid_ + 1
            # -- update
            for k in range(nm):
                row = sm[k]; e = row[sst[k]]; cnt = e & 1023; pv = e >> 10
                pv += (((y << 22) - pv) * SRCP[cnt]) >> 16
                if pv > (1 << 22) - 1: pv = (1 << 22) - 1
                row[sst[k]] = pv << 10 | (cnt + 1 if cnt < smlim else cnt)
            # direct counters
            cv = ov >> 16; cc = ov & 15; cs = (ov >> 8) & 255
            cv += (((65535 if y else 0) - cv) * rcp[cc]) >> 16
            cv = 0 if cv < 0 else 65535 if cv > 65535 else cv
            o1[oi] = cv << 16 | (NX1[cs] if y else NX0[cs]) << 8 | (cc + 1 if cc < 15 else 15)
            if V == 4:
                for k in range(1, nm):
                    L, base = cur[k]; sv = L[base + node]; L[base + node] = NX1[sv] if y else NX0[sv]
            else:
                for k in range(1, nm):
                    bk = cur[k]; vv = bk[node]
                    cv = vv >> 16; cc = vv & 15; cs = (vv >> 8) & 255
                    cv += (((65535 if y else 0) - cv) * rcp[cc]) >> 16
                    cv = 0 if cv < 0 else 65535 if cv > 65535 else cv
                    bk[node] = cv << 16 | (NX1[cs] if y else NX0[cs]) << 8 | (cc + 1 if cc < 15 else 15)
                for k in range(1, nm):
                    bk = cur[k]
                    if bk[0] & 0xFFFF < 0xFFFF: bk[0] += 1
            if mci >= 0: mc[mci] += ((y << 16) - mc[mci]) >> mc_sh
            e1 = (y << 12) - pr1; e2 = (y << 12) - pr2
            if V == 0:
                for q in range(ni):
                    xq = x[q]
                    if xq: r1[q] += (xq * e1) >> lr; r2[q] += (xq * e2) >> lr
            else:
                kk = 64 - (pos >> lrd)
                if kk < lrk: kk = lrk
                e1 = (e1 * kk) >> 4; e2 = (e2 * kk) >> 4
                e3 = (((y << 12) - pr3) * kk) >> 4; e4 = (((y << 12) - pr4) * kk) >> 4; ef = (y << 12) - prf
                if V == 4:
                    sh = lr - 11; es = (e1 >> sh, e2 >> sh, e3 >> sh, e4 >> sh)
                    for r, ek in zip((r1, r2, r3, r4), es):
                        for q in range(ni):
                            xq = x[q]
                            if xq:
                                wv = r[q] + ((xq * ek + 16384) >> 15)
                                r[q] = -32768 if wv < -32768 else 32767 if wv > 32767 else wv
                else:
                    for q in range(ni):
                        xq = x[q]
                        if xq:
                            r1[q] += (xq * e1) >> lr; r2[q] += (xq * e2) >> lr; r3[q] += (xq * e3) >> lr; r4[q] += (xq * e4) >> lr
                for q in range(5): rf[q] += (xf[q] * ef) >> 12
                g = 65535 if y else 0
                ra3[lo] += ((g - ra3[lo]) * (128 - wt)) >> apm_sh; ra3[lo + 1] += ((g - ra3[lo + 1]) * wt) >> apm_sh
            g = 65535 if y else 0
            ra1[lo] += ((g - ra1[lo]) * (128 - wt)) >> apm_sh; ra1[lo + 1] += ((g - ra1[lo + 1]) * wt) >> apm_sh
            ra2[lo] += ((g - ra2[lo]) * (128 - wt)) >> apm_sh; ra2[lo + 1] += ((g - ra2[lo + 1]) * wt) >> apm_sh
            if eb >= 0 and eb != y:
                if V >= 3: mrec = 8 if mlen >= 16 else mrec - 1 if mrec > 0 else 0
                mlen = 0
            c0 = (c0 << 1) | y
            # -- renormalise
            while (low ^ high) < (1 << 24):
                low = (low << 8) & M; high = ((high << 8) & M) | 255
                if ip < cn: code = ((code << 8) & M) | p[ip]; ip += 1
                else: code = ((code << 8) & M) | 255; pad += 1
            bp += 1
            if bp < 8: continue
            # -- end of a byte: the contexts of the next one (FORMAT.md 24.4)
            c = c0 & 255; buf[pos] = c; pos += 1
            c8 = ((c8 << 8) | (c4 >> 24)) & M; c4 = ((c4 << 8) | c) & M; c0 = 1; bp = 0
            if 97 <= (c | 32) <= 122 or c >= 128: word = ((word + (c | 32) + 1) * 0x3D4D51CB) & M
            elif word: word2 = word1; word1 = word; word = 0
            H[1] = hmix((c4 & 0xFF) + 0x1000000)
            H[2] = hmix((c4 & 0xFFFF) + 0x2000000)
            H[3] = hmix(((c4 & 0xFFFFFF) * 0x9E3779B1 + 3) & M)
            H[4] = hmix((c4 * 0x9E3779B1 + 4) & M)
            H[5] = hmix((hmix(c4) + (c8 & 0xFFFF) * 0x85EBCA6B + 5) & M)
            H[6] = hmix((word * 0x27D4EB2F + word1 * 0x165667B1 + 6) & M)
            H[7] = hmix(((c4 & 0xFF00FF00) * 0x9E3779B1 + 7) & M)
            H[16] = hmix(((c4 & 0xFFFF0000) * 0x9E3779B1 + 16) & M)
            H[17] = hmix(((c4 & 0xFF00) * 0x85EBCA6B + 17) & M)
            dd = (pos - rl_last[c]) & M
            if rl_last[c] and dd == rl_d[c] and 2 <= dd < 65536:
                if dd == rcand:
                    if rcnt < 255: rcnt += 1
                    if rcnt >= 8 and rlen != dd: rlen = dd
                elif rcnt: rcnt -= 1
                else: rcand = dd; rcnt = 1
            rl_d[c] = dd; rl_last[c] = pos
            r = rlen
            ab = buf[pos - r] if r and pos >= r else 0
            ab2 = buf[pos - 2 * r] if r and pos >= 2 * r else 0
            H[18] = hmix(((ab << 16 | ab2 << 8 | (1 if r else 0)) * 0x9E3779B1 + r * 0x85EBCA6B + 18) & M)
            H[19] = hmix(((ab << 8 | (c4 & 0xFF)) * 0xC2B2AE35 + r * 0x9E3779B1 + 19) & M)
            col = min(pos % r, 4095) if r else 0
            H[20] = hmix(((col << 8 | (c4 & 0xFF)) * 0x2545F491 + r * 0x85EBCA6B + 20) & M)
            q1 = (c4 >> 8) & 0xFF; q2 = (c4 >> 8) & 0xFFFF
            ind1[q1] = ((ind1[q1] << 8) | c) & 0xFFFF; ind2[q2] = ((ind2[q2] << 8) | c) & 0xFFFF
            H[14] = hmix(((ind1[c] << 8 | c) * 0x9E3779B1 + 14) & M)
            H[15] = hmix(((ind2[c4 & 0xFFFF] << 16 | (c4 & 0xFFFF)) * 0x85EBCA6B + 15) & M)
            H[12] = hmix((word * 0x2545F491 + 12) & M)
            H[13] = hmix((word * 0x9E3779B1 + word1 * 0x85EBCA6B + word2 * 0xC2B2AE35 + 13) & M)
            if c == 10: line1 = line0; line0 = pos
            col = pos - line0; plen = line0 - line1
            above = buf[line1 + col] if col < plen else 0
            above2 = buf[line1 + col + 1] if col + 1 < plen else 0
            H[10] = hmix(((above << 8 | above2) * 0x9E3779B1 + (min(col, 255) << 20) + 10) & M)
            H[11] = hmix(((above << 16 | (c4 & 0xFFFF)) * 0x85EBCA6B + 11) & M)
            H[8] = hmix((hmix((c4 * 0x9E3779B1) & M) + (c8 & 0xFF) * 0x85EBCA6B + 8) & M)
            H[9] = hmix((hmix((c4 + 0x51ED27) & M) + (c8 & 0xFFFFFF) * 0xC2B2AE35 + 9) & M)
            if V >= 3:
                if 48 <= c <= 57 or 97 <= (c | 32) <= 122 or c in (95, 45, 46) or c >= 128:
                    s_tok = ((s_tok + c + 1) * 0x2F0B3A55) & M
                else:
                    if s_tok:
                        s_ltok = s_tok
                        if s_lt: s_key = s_tok
                        s_tok = 0
                    if c in (58, 61) and s_ltok: s_key = s_ltok
                    s_lt = 1 if c == 60 or (c == 47 and s_lt) else 0
                if c == 10:
                    s_pfn = s_fidx + 1; s_pfst = s_fst[:]; s_pls = s_ls; s_ls = pos
                    s_fidx = 0; s_fst[0] = 0; s_key = 0; s_ltok = 0
                elif c in (32, 44, 9, 59, 124):
                    if s_fidx < 31: s_fidx += 1; s_fst[s_fidx] = (pos - s_ls) & M
                ab = ab1 = 0
                if s_fidx < s_pfn:
                    a = s_pls + s_pfst[s_fidx] + (pos - s_ls) - s_fst[s_fidx]
                    if s_pls <= a < s_ls: ab = buf[a]; ab1 = buf[a + 1] if a + 1 < s_ls else 0
                c1b = c4 & 0xFF
                H[25] = hmix((s_key * 0x9E3779B1 + c1b * 0x85EBCA6B + 25) & M)
                H[26] = hmix((s_key * 0xC2B2AE35 + s_tok * 0x9E3779B1 + 26) & M)
                H[27] = hmix(((s_fidx << 16 | (c4 & 0xFFFF)) * 0x9E3779B1 + 27) & M)
                H[28] = hmix(((s_fidx + 1) * 0x85EBCA6B + s_tok * 0xC2B2AE35 + 28) & M)
                H[29] = hmix(((ab << 16 | ab1 << 8 | s_fidx) * 0x9E3779B1 + c1b * 0x2545F491 + 29) & M)
                H[21] = 21; H[22] = 22; hh = 0
                for k in range(1, min(pos, 20) + 1):
                    hh = ((hh + buf[pos - k] + 1) * 0x2F0B3A55) & M
                    if k == 12: H[21] = hmix((hh + 21) & M)
                    elif k == 20: H[22] = hmix((hh + 22) & M)
            if V == 2:
                iw, ich, ih = img; q = pos

                def IB(dd): return buf[q - dd] if q >= dd + ih else 0

                def CL(v_): return 0 if v_ < 0 else 255 if v_ > 255 else v_
                W, N, NW = IB(ich), IB(iw), IB(iw + ich)
                NE = IB(iw - ich) if iw > ich else 0
                WW, NN = IB(2 * ich), IB(2 * iw)
                NNE = IB(2 * iw - ich) if iw > ich else 0
                P1, P2, W1_, N1 = IB(1), IB(2), IB(ich + 1), IB(iw + 1)
                ci = (q - ih) % ich if q >= ih else 0
                grad = CL(W + N - NW); mn, mx = min(W, N), max(W, N)
                med = mn if NW >= mx else mx if NW <= mn else grad
                avg = (W + N + 1) >> 1
                a_ = abs(W - NW) + abs(N - NW) + abs(N - NE)
                act = 0 if a_ < 3 else 1 if a_ < 8 else 2 if a_ < 16 else 3 if a_ < 32 else 4 if a_ < 64 else 5 if a_ < 128 else 6 if a_ < 256 else 7
                icp = CL(W + P1 - W1_) if ci else grad
                icn = CL(N + P1 - N1) if ci else avg
                kc = (ci * 0x9E3779B1) & M

                def IH(i, v_): H[i] = hmix(((v_ & M) * 0x9E3779B1 + kc + i * 0x85EBCA6B) & M)
                IH(1, W << 8 | N); IH(2, N << 8 | NE); IH(3, W << 8 | NW); IH(4, grad << 3 | act); IH(5, med << 3 | act)
                IH(6, W << 16 | N << 8 | NW); IH(7, W << 16 | N << 8 | NE); IH(8, N << 8 | NN); IH(9, W << 8 | WW)
                IH(10, (CL(2 * W - WW) >> 2) << 6 | CL(2 * N - NN) >> 2); IH(11, avg << 6 | ((W - N + 256) & 511) >> 3)
                if ci:
                    IH(12, icp << 3 | act); IH(13, icp << 8 | P1); IH(14, P1 << 8 | W); IH(15, P1 << 8 | N)
                    IH(16, P1 << 8 | P2); IH(17, icn << 8 | W)
                else:
                    IH(12, CL(N + NE - NNE) << 3 | act); IH(13, CL(W + NE - N)); IH(14, NW << 8 | NE)
                    IH(15, ((W + NE) >> 1) << 6 | N >> 2)
                    IH(16, (W >> 2) << 18 | (N >> 2) << 12 | (NW >> 2) << 6 | NE >> 2); IH(17, WW << 8 | NN)
                IH(18, (grad >> 1) << 7 | med >> 1)
                IH(19, (W >> 3) << 20 | (N >> 3) << 15 | (NE >> 3) << 10 | (NW >> 3) << 5 | P1 >> 3)
                IH(20, (icp + icn + 1) >> 1)
                i_pred = icp; i_cls = min(ci, 2); i_act = act >> 1; i_n = N; i_med = med; i_wn = W << 8 | N
            # match model (FORMAT.md 24.5)
            if pos >= 6:
                hh = hmix(c4 ^ (((c8 & 0xFFFF) * 0x9E3779B1) & M)) & mmask
                if mlen:
                    mptr += 1
                    if mlen < 65535: mlen += 1
                else:
                    cand = mt.get(hh, 0); found = 0
                    if V >= 3:
                        best = 0; bc = 0; k = 0
                        while cand > 0 and k < 16:
                            lm = min(cand, 400); l_ = 0
                            while l_ < lm and buf[cand - 1 - l_] == buf[pos - 1 - l_]: l_ += 1
                            if l_ > best:
                                best = l_; bc = cand
                                if l_ >= 400: break
                            cand = mprev.get(cand, 0); k += 1
                        if best >= 6: mlen = best; mptr = bc; found = 1
                    elif cand > 0:
                        lm = min(cand, 400); l_ = 0
                        while l_ < lm and buf[cand - 1 - l_] == buf[pos - 1 - l_]: l_ += 1
                        if l_ >= 6: mlen = l_; mptr = cand; found = 1
                    if V >= 3 and not found and mrec > 0 and mptr + 1 < pos: mptr += 1; mlen = 1
                if V >= 3: mprev[pos] = mt.get(hh, 0)
                mt[hh] = pos
                if mlen and mptr >= pos: mlen = 0
            if V >= 3:
                ebb = buf[mptr] if mlen else 256
                lq = 0 if mlen == 0 else 1 if mlen < 8 else 2 if mlen < 12 else 3 if mlen < 16 else 4 if mlen < 24 else 5 if mlen < 32 else 6 if mlen < 64 else 7
                H[23] = hmix(((ebb << 3 | lq) * 0x9E3779B1 + (c4 & 0xFF) * 0x85EBCA6B + 23) & M)
                H[24] = hmix(((ebb << 3 | lq) * 0xC2B2AE35 + (c4 & 0xFFFF) * 0x9E3779B1 + 24) & M)
    if maxn is None or nout == n: need(pad <= 8, "cm: stream ends early")
    return bytes(buf[:nout])


# ---- the corpus walk
def decode_any(codec, p, n, maxn=None):
    if codec == 7: return cm_decode(p, n, maxn)
    if codec == 17: return cmi_decode(p, n, maxn)
    return cmp_decode(p, n, maxn)


def main():
    args = sys.argv[1:]; prefix = 1024
    if args[:1] == ["--full"]: prefix = None; args = args[1:]
    elif args[:1] == ["--prefix"]: prefix = int(args[1]); args = args[2:]
    write = args[:1] == ["--write-prefixes"]
    if write: args = args[1:]
    root = args[0] if args else os.path.join(HERE, "v12")
    from collections import Counter
    stats = Counter(); bad = []
    vec = os.path.join(HERE, "vectors", "cm")
    for fn in sorted(os.listdir(vec)):
        if not fn.endswith(".z"): continue
        codec = int(fn.split(".")[-2][1:]); orig = fn.split("~")[0]
        want = open(os.path.join(vec, orig), "rb").read()
        try:
            got = decode_any(codec, open(os.path.join(vec, fn), "rb").read(), len(want))
            need(got == want, "differs from " + orig)
            stats["vector, codec %d" % codec] += 1
        except Corrupt as e:
            bad.append("vectors/cm/%s: %s" % (fn, e))
    pfile = os.path.join(vec, "prefixes.json")
    known = {}
    if os.path.exists(pfile) and prefix:
        pj = json.load(open(pfile))
        if pj["prefix"] == prefix: known = pj["digests"]
    found = {}
    entries = [l.split()[:2] for l in open(os.path.join(root, "MANIFEST")) if l.strip()]
    for rel in sorted(d for d in os.listdir(root) if d.startswith("r") and os.path.isdir(os.path.join(root, d))):
        entries += [[rel + "/" + e["file"], rel + "/" + e["input"]] for e in json.load(open(os.path.join(root, rel, "frames.json")))["frames"]]
    seen = set()
    for fr, og in entries:
        if fr in seen: continue
        seen.add(fr)
        f = open(os.path.join(root, fr), "rb").read(); o = open(os.path.join(root, og), "rb").read(); ro = 0
        try:
            for g in R.frames_of(f):
                seg = o[ro:ro + struct.unpack_from("<Q", g, 12)[0]]; ro += len(seg)
                if struct.unpack_from("<I", g, 20)[0] == 1 and g[44] == 20:
                    g = g[52:48 + struct.unpack_from("<I", g, 36)[0]]; seg = None
                for bi, (raw, ck, codec, tflag, pay, r0) in enumerate(R.blocks_of(g)):
                    key = "%s#%d" % (fr, bi)
                    if codec == 12:
                        for li, (sid, cc, rl, z) in enumerate(R.graph_leaves(pay[4:])):
                            if cc in (7, 17, 25):
                                d = decode_any(cc, z, rl, prefix)
                                found["%s.%d" % (key, li)] = hashlib.sha256(d).hexdigest()
                                stats["leaf %d" % cc] += 1
                        continue
                    if codec not in (7, 17, 25): continue
                    t, a, b, c = pay[:4]
                    orig = seg[r0:r0 + raw] if seg is not None else None
                    if prefix is None:
                        R.NATIVE = (24, 27, 28, 29, 32, codec)
                        real = R.decode
                        R.decode = lambda cc, z, m, _c=codec: decode_any(cc, z, m) if cc == _c else real(cc, z, m)
                        try: R.check_block(raw, ck, codec, tflag, pay, orig, g[10] & 1, stats)
                        finally: R.decode = real
                        continue
                    meta = 4 * (c + 1) if (tflag and t == 1) else R.META.get(tflag, 0)
                    body = pay[4 + meta:]
                    dlen = struct.unpack_from("<I", pay, 4)[0] if R.META.get(tflag) else raw
                    if codec == 25:
                        ds = [cm_decode(z, rn, prefix) for z, rn in cmp_parts(body, dlen)]
                        h = hashlib.sha256(b"".join(ds)).hexdigest()
                        if tflag == 0:
                            op = 0; pieces = []
                            for (z, rn), dpart in zip(cmp_parts(body, dlen), ds): pieces.append(orig[op:op + len(dpart)]); op += rn
                            need(b"".join(pieces) == b"".join(ds), "cmp: prefix differs from the original")
                    else:
                        d = decode_any(codec, body, dlen, prefix); h = hashlib.sha256(d).hexdigest()
                        if tflag == 0: need(orig is None or orig[:len(d)] == d, "prefix differs from the original")
                    found[key] = h
                    if tflag == 0: stats["block %d, prefix checked against the original" % codec] += 1
                    elif not write:
                        need(known.get(key) == h, "prefix differs from tests/vectors/cm/prefixes.json")
                        stats["block %d, tflag %d, prefix checked against its digest" % (codec, tflag)] += 1
        except (Corrupt, IndexError, struct.error) as e:
            bad.append("%s: %s" % (fr, e))
    if write and prefix:
        json.dump({"prefix": prefix, "digests": found}, open(pfile, "w"), indent=1, sort_keys=True)
    for b in bad: print(b)
    for k_ in sorted(stats): print("  %5d %s" % (stats[k_], k_))
    if bad: sys.exit("CM REFERENCE FAILED (%d)" % len(bad))
    print("CM REFERENCE PASSED (%d streams%s)" % (sum(stats.values()), "" if prefix is None else ", first %d bytes of corpus streams" % prefix))


if __name__ == "__main__":
    main()
