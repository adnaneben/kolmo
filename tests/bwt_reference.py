#!/usr/bin/env python3
"""Independent decoder for Kolmo's block-sorting stream (codec 18), FORMAT.md section 22, in plain Python.

usage: python3 tests/bwt_reference.py [--max-bytes N] [tests/v12]

Decodes every corpus block and graph leaf coded with codec 18 (LZP, inverse BWT, QLFC with the static or the adaptive
model) and compares blocks without a transform with the original. The QLFC tables and constants are the normative data
file tests/format_tables/qlfc.json. --max-bytes skips blocks larger than N bytes (make check uses 300000; the full
run is `make check-bwt-reference`, several minutes).
"""
import json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coder_reference as R  # noqa: E402  (frame walk, Corrupt, khash64, transforms of FORMAT.md 19-20)

need, Corrupt = R.need, R.Corrupt
TB = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "format_tables", "qlfc.json")))
STRETCH, SQUASH = TB["bsc_stretch_table"], TB["bsc_squash_table"]
RANK_STATE, RUN_STATE = TB["model_rank_state_table"], TB["model_run_state_table"]
K = TB["constants"]


def i16(x): x &= 0xFFFF; return x - 0x10000 if x & 0x8000 else x
def i32(x): x &= 0xFFFFFFFF; return x - (1 << 32) if x & 0x80000000 else x


# ---- range decoder (FORMAT.md 22.3)
class RC:
    def __init__(self, p, a, e):
        self.p, self.i, self.e = p, a, e
        self.range = 0xFFFFFFFF; self.code = 0
        for _ in range(3): self.code = ((self.code << 16) | self.in16()) & 0xFFFFFFFF

    def in16(self):
        if self.i + 2 <= self.e: v = self.p[self.i] | self.p[self.i + 1] << 8; self.i += 2; return v
        if self.i < self.e: v = self.p[self.i]; self.i = self.e; return v
        return 0

    def bit(self, prob):
        if self.range < 0x10000:
            self.range = (self.range << 16) & 0xFFFFFFFF; self.code = ((self.code << 16) | self.in16()) & 0xFFFFFFFF
        r = (self.range >> 12) * prob & 0xFFFFFFFF
        if self.code >= r: self.range = (self.range - r) & 0xFFFFFFFF; self.code = (self.code - r) & 0xFFFFFFFF; return 1
        self.range = r; return 0

    def word(self):
        w = 0
        for _ in range(32): w = (w * 2 + self.bit(2048)) & 0xFFFFFFFF
        return w


# ---- counters and mixers (FORMAT.md 22.4)
def up0(arr, i, th, ar): arr[i] = i16(arr[i] + (((4096 - th - arr[i]) * ar) >> 12))
def up1(arr, i, th, ar): arr[i] = i16(arr[i] - (((arr[i] - th) * ar) >> 12))


class Mixer:
    __slots__ = ("s0", "s1", "s2", "mixed", "index", "map", "w0", "w1", "w2")

    def __init__(self):
        self.w0 = self.w1 = 2048 << 5; self.w2 = 0; self.s0 = self.s1 = self.s2 = 0; self.mixed = 0; self.index = 0
        self.map = [SQUASH[2048 + (p - 8) * 256] for p in range(17)]

    def mix(self, p0, p1, p2):
        self.s0, self.s1, self.s2 = STRETCH[p0], STRETCH[p1], STRETCH[p2]
        sp = i16(i32(self.s0 * self.w0 + self.s1 * self.w1 + self.s2 * self.w2) >> 17)
        sp = -2047 if sp < -2047 else 2047 if sp > 2047 else sp
        self.index = (sp + 2048) >> 8
        m = self.map; mapped = m[self.index] + (((m[self.index + 1] - m[self.index]) * (sp & 255)) >> 8)
        self.mixed = (3 * SQUASH[2048 + sp] + mapped) >> 2
        return self.mixed

    def learn(self, eps, lr):
        self.w0 = i32(self.w0 - (i32(lr[0] * eps * self.s0) >> 16))
        self.w1 = i32(self.w1 - (i32(lr[1] * eps * self.s1) >> 16))
        self.w2 = i32(self.w2 - (i32(lr[2] * eps * self.s2) >> 16))

    def update(self, bit, lr, th, ar):
        if bit:
            up1(self.map, self.index, th, ar); up1(self.map, self.index + 1, th, ar); self.learn(self.mixed - 1, lr)
        else:
            up0(self.map, self.index, th, ar); up0(self.map, self.index + 1, th, ar); self.learn(self.mixed - 4095, lr)


def C(prefix, part):
    """thresholds and rates of one counter set: (th0, ar0, th1, ar1)"""
    return tuple(K["%s_%s_%s" % (prefix[0], prefix[1], part[:2]) + s] for s in ("TH0", "AR0", "TH1", "AR1")) if False else \
        (K["%s_%s_TH0" % (prefix, part)], K["%s_%s_AR0" % (prefix, part)], K["%s_%s_TH1" % (prefix, part)], K["%s_%s_AR1" % (prefix, part)])


def LR(prefix, part): return (K["%s_%s_LR0" % (prefix, part)], K["%s_%s_LR1" % (prefix, part)], K["%s_%s_LR2" % (prefix, part)])


# ---- QLFC (FORMAT.md 22.5)
def bsr(x): return x.bit_length() - 1


def qlfc_header(rc):
    used = [0] * 256; prev = -1; maxrank = 7; last = 255; mtf = [0] * 256
    for rank in range(256):
        cur = 0
        for b in range(7, -1, -1):
            b0 = b1 = 0
            for c in range(cur << (b + 1), (cur + 1) << (b + 1)):
                if c == prev or not used[c]:
                    if c & (1 << b): b1 = 1
                    else: b0 = 1
                    if b0 and b1: break
            if b0 and b1: cur = cur * 2 + rc.bit(2048)
            else: cur = cur * 2 + (1 if b1 else 0)
        mtf[rank] = cur
        if cur == prev:
            maxrank = bsr(rank - 1) if rank > 1 else 0; last = rank
            for r in range(rank + 1, 256): mtf[r] = cur
            break
        prev = cur; used[cur] = 1
    return mtf, maxrank, last


def qlfc_block(p, a, e, cap, adaptive):
    rc = RC(p, a, e)
    n = i32(rc.word())
    need(0 <= n <= cap, "qlfc size")
    mtf, maxrank, lastrank = qlfc_header(rc)
    P = "M" if adaptive else "F"
    k = {nm: C(P + "_RANK", nm) for nm in ("TS", "TC", "TP", "ES", "EC", "EP", "MS", "MC", "MP", "PS", "PC", "PP")}
    k.update({"r" + nm: C(P + "_RUN", nm) for nm in ("TS", "TC", "TP", "ES", "EC", "EP", "MS", "MC", "MP")})
    l = {nm: LR(P + "_RANK", nm) for nm in ("TM", "EM", "MM", "PM")}
    l.update({"r" + nm: LR(P + "_RUN", nm) for nm in ("TM", "EM", "MM")})
    mt = {nm: C(P + "_RANK", nm) for nm in ("TM", "EM", "MM", "PM")} if adaptive else {}
    if adaptive: mt.update({"r" + nm: C(P + "_RUN", nm) for nm in ("TM", "EM", "MM")})
    # model arrays, all 2048 at the start; names follow FORMAT.md 22.4
    def arr(n_): return [2048] * n_
    rS, rC, rP = arr(256), arr(256), arr(1)
    reS, reC, reP = arr(256 * 8), arr(256 * 8), arr(8)
    rmS, rmC, rmP = [arr(256 * 128) for _ in range(8)], [arr(256 * 128) for _ in range(8)], [arr(128) for _ in range(8)]
    rpS, rpC, rpP = arr(256 * 256), arr(256 * 256), arr(256)
    uS, uC, uP = arr(256), arr(256), arr(1)
    ueS, ueC, ueP = arr(256 * 32), arr(256 * 32), arr(32)
    umS, umC, umP = [arr(256 * 32) for _ in range(32)], [arr(256 * 32) for _ in range(32)], [arr(32) for _ in range(32)]
    if adaptive:
        mxR = [Mixer() for _ in range(256)]; mxRE = [[Mixer() for _ in range(8)] for _ in range(8)]
        mxRM = [Mixer() for _ in range(8)]; mxRP = [Mixer() for _ in range(256)]; mxU = [Mixer() for _ in range(256)]
        mxUE = [[Mixer() for _ in range(32)] for _ in range(32)]; mxUM = [Mixer() for _ in range(32)]

    def dec(mx, lr, mtk, cs, ci):
        """one bit from three counters (arrays, indexes) cs/ci; returns the bit, updating the counters"""
        (a0, i0), (a1, i1), (a2, i2) = cs  # order: char, state, static (FORMAT.md 22.5)
        if adaptive:
            b = rc.bit(mx.mix(a0[i0], a1[i1], a2[i2]))
            mx.update(b, lr, mtk[2 if b else 0], mtk[3 if b else 1])
        else:
            b = rc.bit((a0[i0] * lr[0] + a1[i1] * lr[1] + a2[i2] * lr[2]) >> 5)
        return b

    def upd(b, cs, ks):
        for (a_, i_), (th0, ar0, th1, ar1) in zip(cs, ks):
            if b: up1(a_, i_, th1, ar1)
            else: up0(a_, i_, th0, ar0)

    def upd_bit(b, cs, ks):   # qc_update_bit: the same results as up0/up1 except for the rounding of bit 0
        for (a_, i_), (th0, ar0, th1, ar1) in zip(cs, ks):
            d = a_[i_] * ar1 - th1 * ar1 if b else a_[i_] * ar0 - ((4096 - th0) * ar0 - 4095)
            a_[i_] = i16(a_[i_] - (d >> 12))
    UPB = upd_bit if not adaptive else upd
    ctxR0 = ctxR4 = ctxRun = avg = 0
    rh, uh = [0] * 256, [0] * 256
    out = bytearray()
    while len(out) < n:
        ch = mtf[0]; hist = rh[ch]
        st = RANK_STATE[(ctxRun << 11) | (ctxR4 << 3) | hist]
        rank = 1
        if avg < 32:
            cs = ((rC, ch), (rS, st), (rP, 0))
            b = dec(mxR[ch] if adaptive else None, l["TM"], mt.get("TM"), cs, None)
            upd(b, (cs[1], cs[0], cs[2]), (k["TS"], k["TC"], k["TP"]))
            if b:
                mx = mxRE[max(hist, 1)][1] if adaptive else None
                brs = 1
                while True:
                    if brs == maxrank: break
                    cs = ((reC, ch * 8 + brs - 1), (reS, st * 8 + brs - 1), (reP, brs - 1))
                    b = dec(mx, l["EM"], mt.get("EM"), cs, None)
                    upd(b, (cs[1], cs[0], cs[2]), (k["ES"], k["EC"], k["EP"]))
                    if not b: break
                    brs += 1
                    need(brs <= 7, "qlfc rank exponent")
                    if adaptive: mx = mxRE[max(hist, brs)][brs]
                rh[ch] = brs
                mx = mxRM[brs] if adaptive else None
                for _ in range(brs):
                    cs = ((rmC[brs], ch * 128 + rank), (rmS[brs], st * 128 + rank), (rmP[brs], rank))
                    b = dec(mx, l["MM"], mt.get("MM"), cs, None)
                    UPB(b, (cs[1], cs[0], cs[2]), (k["MS"], k["MC"], k["MP"]))
                    rank = rank * 2 + b
            else:
                rh[ch] = 0
        else:
            rank = 0; ctx = 1
            for _ in range(maxrank + 1):
                cs = ((rpC, ch * 256 + ctx), (rpS, st * 256 + ctx), (rpP, ctx))
                b = dec(mxRP[ctx] if adaptive else None, l["PM"], mt.get("PM"), cs, None)
                UPB(b, (cs[1], cs[0], cs[2]), (k["PS"], k["PC"], k["PP"]))
                ctx = ctx * 2 + b; rank = rank * 2 + b
            need(rank != 0, "qlfc escape rank 0")
            rh[ch] = bsr(rank)
        need(rank <= lastrank, "qlfc rank above the table")
        del mtf[0]; mtf.insert(rank, ch)
        avg = (avg * 124 + rank * 4) >> 7
        rank -= 1
        hist = uh[ch]
        st = RUN_STATE[(ctxR0 << 10) | (ctxRun << 6) | (min(rank, 7) << 3) | min(hist, 7)]
        cs = ((uC, ch), (uS, st), (uP, 0))
        b = dec(mxU[ch] if adaptive else None, l["rTM"], mt.get("rTM"), cs, None)
        upd(b, (cs[1], cs[0], cs[2]), (k["rTS"], k["rTC"], k["rTP"]))
        if b:
            mx = mxUE[max(hist, 1)][1] if adaptive else None
            run = brs = 1
            while True:
                cs = ((ueC, ch * 32 + brs - 1), (ueS, st * 32 + brs - 1), (ueP, brs - 1))
                b = dec(mx, l["rEM"], mt.get("rEM"), cs, None)
                upd(b, (cs[1], cs[0], cs[2]), (k["rES"], k["rEC"], k["rEP"]))
                if not b: break
                brs += 1
                need(brs <= 30, "qlfc run exponent")
                if adaptive: mx = mxUE[max(hist, brs)][brs]
            uh[ch] = (uh[ch] + 3 * brs + 3) >> 2
            mx = mxUM[brs] if adaptive else None; ctx = 1
            for _ in range(brs):
                cs = ((umC[brs], ch * 32 + ctx), (umS[brs], st * 32 + ctx), (umP[brs], ctx))
                b = dec(mx, l["rMM"], mt.get("rMM"), cs, None)
                UPB(b, (cs[1], cs[0], cs[2]), (k["rMS"], k["rMC"], k["rMP"]))
                run = run * 2 + b
                ctx = ctx * 2 + b if brs <= 5 else ctx + 1
            ctxR0 = ((ctxR0 << 1) | (rank == 0)) & 7
            ctxR4 = ((ctxR4 << 2) | min(rank, 3)) & 255
            ctxRun = ((ctxRun << 1) | (run < 3)) & 15
            need(run <= n - len(out), "qlfc run past the end")
            out += bytes([ch]) * run
        else:
            uh[ch] = (uh[ch] + 2) >> 2
            ctxR0 = ((ctxR0 << 1) | (rank == 0)) & 7
            ctxR4 = ((ctxR4 << 2) | min(rank, 3)) & 255
            ctxRun = ((ctxRun << 1) | 1) & 15
            out.append(ch)
    return bytes(out)


def qlfc(p, cap, adaptive):
    need(len(p) >= 1, "qlfc empty")
    nb = p[0]
    if nb == 1: return qlfc_block(p, 1, len(p), cap, adaptive)
    need(nb != 0 and len(p) >= 1 + 8 * nb, "qlfc block table")
    ip, out = 1 + 8 * nb, bytearray()
    blocks = []
    for b in range(nb):
        o, c = struct.unpack_from("<ii", p, 1 + 8 * b)
        need(o >= 0 and c >= 0 and ip + c <= len(p) and len(out) + o <= cap, "qlfc block entry")
        blocks.append((ip, c, o)); ip += c; out += bytes(0)
    tot = 0; res = bytearray()
    for a, c, o in blocks:
        need(tot + o <= cap, "qlfc size"); tot += o
        d = bytes(p[a:a + c]) if c == o else qlfc_block(p, a, a + c, o, adaptive)
        need(len(d) == o, "qlfc block size")
        res += d
    return bytes(res)


# ---- inverse BWT (FORMAT.md 22.6)
def unbwt(t, idx, rows=None):
    """t: the BWT of the text without its end marker, which sits at row idx (1..n) of the full BWT. rows, if given,
    receives the row of every suffix (used to tell whether the sampled indexes match the text)"""
    n = len(t)
    need(1 <= idx <= n, "bwt primary index")
    L = t[:idx] + b"\0" + t[idx:]          # row idx holds the marker; its byte value is never read
    cnt = [0] * 256
    for c in t: cnt[c] += 1
    base = [0] * 256; s_ = 1                  # F row 0 is the marker, the smallest
    for c in range(256): base[c] = s_; s_ += cnt[c]
    lf = [0] * (n + 1); seen = base[:]
    for i in range(n + 1):
        if i == idx: continue
        c = L[i]; lf[i] = seen[c]; seen[c] += 1
    out = bytearray(n); r = 0
    for k in range(n - 1, -1, -1):           # row 0's L byte is the last text byte
        out[k] = L[r]; r = lf[r]
        if rows is not None: rows[k] = r      # r is now the row of the suffix starting at k
    return bytes(out)


# ---- LZP (FORMAT.md 22.7)
def lzp_block(p, a, e, cap, hbits, minlen):
    need(e - a >= 4, "lzp block size"); need(cap >= 4, "lzp output")
    mask = (1 << hbits) - 1; table = {}
    out = bytearray(p[a:a + 4]); i = a + 4
    ctx = out[3] | out[2] << 8 | out[1] << 16 | out[0] << 24
    while i < e:
        h = ((ctx >> 15) ^ ctx ^ (ctx >> 3)) & mask
        v = table.get(h, 0); table[h] = len(out)
        if p[i] == 0xF2 and v > 0:
            i += 1; need(i < e, "lzp flag at the end")
            if p[i] != 255:
                ln = minlen
                while True:
                    need(i < e, "lzp length"); x = p[i]; i += 1; ln += x
                    if x != 254: break
                need(ln <= cap - len(out), "lzp match past the end")
                for _ in range(ln): out.append(out[v]); v += 1
                ctx = out[-1] | out[-2] << 8 | out[-3] << 16 | out[-4] << 24
            else:
                i += 1; need(len(out) < cap, "lzp output"); out.append(0xF2); ctx = ((ctx << 8) | 0xF2) & 0xFFFFFFFF
        else:
            need(len(out) < cap, "lzp output"); out.append(p[i]); ctx = ((ctx << 8) | p[i]) & 0xFFFFFFFF; i += 1
    return bytes(out)


def lzp(p, cap, hbits, minlen):
    need(len(p) >= 1 and 10 <= hbits <= 28 and 4 <= minlen <= 255, "lzp parameters")
    nb = p[0]
    if nb == 1: return lzp_block(p, 1, len(p), cap, hbits, minlen)
    need(nb != 0 and len(p) >= 1 + 8 * nb, "lzp block table")
    ip, op, res = 1 + 8 * nb, 0, bytearray()
    for b in range(nb):
        o, c = struct.unpack_from("<ii", p, 1 + 8 * b)
        need(o >= 0 and c >= 0 and ip + c <= len(p) and op + o <= cap, "lzp block entry")
        d = bytes(p[ip:ip + c]) if c == o else lzp_block(p, ip, ip + c, o, hbits, minlen)
        need(len(d) == o, "lzp block size")
        res += d; ip += c; op += o
    return bytes(res)


# ---- the stream (FORMAT.md 22.2)
def samples_match(p, n):
    """True when the sampled indexes of the stream agree with the text (FORMAT.md 22.6); for tests"""
    flags = p[0]; q = 3 if flags & 4 else 1
    idx = struct.unpack_from("<I", p, q)[0]; rlog, k = p[q + 4], p[q + 5]
    samples = struct.unpack_from("<%dI" % k, p, q + 6)
    t = qlfc(p[q + 6 + 4 * k:], n, (flags & 3) == 2); rows = [0] * len(t); unbwt(t, idx, rows)
    return all(rows[(i + 1) << rlog] == x for i, x in enumerate(samples))


def bwt_stream(p, n):
    need(n >= 2 and len(p) >= 7, "bwt stream size")
    flags = p[0]; coder = flags & 3; use_lzp = (flags >> 2) & 1; q = 1
    need(flags >> 3 == 0 and coder in (1, 2), "bwt flags")
    if use_lzp:
        hb, ml = p[1], p[2]; q = 3; need(10 <= hb <= 28 and ml >= 16, "bwt lzp parameters")
    need(len(p) - q >= 6, "bwt header")
    idx = struct.unpack_from("<I", p, q)[0]; rlog, k = p[q + 4], p[q + 5]; q += 6
    need(1 <= rlog <= 30 and len(p) - q >= 4 * k + 1, "bwt samples")
    samples = struct.unpack_from("<%dI" % k, p, q); q += 4 * k
    t = qlfc(p[q:], n, coder == 2)
    lz = len(t)
    need((use_lzp or lz == n) and lz >= 2, "bwt length")
    need((lz - 1) >> rlog == k, "bwt sample count")
    need(1 <= idx <= lz and all(1 <= x <= lz for x in samples), "bwt indexes")
    d = unbwt(t, idx)
    if use_lzp:
        d = lzp(d, n, hb, ml); need(len(d) == n, "bwt lzp length")
    return d


def main():
    R.BWT = bwt_stream; R.NATIVE = R.NATIVE + (18,)
    args = sys.argv[1:]; maxb = None
    if args[:1] == ["--max-bytes"]: maxb = int(args[1]); args = args[2:]
    root = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), "v12")
    from collections import Counter
    stats = Counter(); bad = []; seen = set()
    entries = [l.split()[:2] for l in open(os.path.join(root, "MANIFEST")) if l.strip()]
    for rel in sorted(d for d in os.listdir(root) if d.startswith("r") and os.path.isdir(os.path.join(root, d))):
        entries += [[rel + "/" + e["file"], rel + "/" + e["input"]] for e in json.load(open(os.path.join(root, rel, "frames.json")))["frames"]]
    for fr, og in entries:
        if fr in seen: continue
        seen.add(fr)
        f = open(os.path.join(root, fr), "rb").read(); o = open(os.path.join(root, og), "rb").read(); ro = 0
        try:
            for g in R.frames_of(f):
                seg = o[ro:ro + struct.unpack_from("<Q", g, 12)[0]]; ro += len(seg)
                if struct.unpack_from("<I", g, 20)[0] == 1 and g[44] == 20:
                    g = g[52:48 + struct.unpack_from("<I", g, 36)[0]]; seg = None
                for raw, ck, codec, tflag, pay, r0 in R.blocks_of(g):
                    if codec == 12:
                        for sid, cc, rl, z in R.graph_leaves(pay[4:]):
                            if cc == 18 and (maxb is None or rl <= maxb):
                                need(len(bwt_stream(z, rl)) == rl, "leaf length"); stats["leaf 18"] += 1
                        continue
                    if codec != 18 or (maxb is not None and raw > maxb): continue
                    t, a, b, c = pay[:4]
                    if tflag == 0:
                        d = bwt_stream(pay[4:], raw)
                        need(seg is None or d == seg[r0:r0 + raw], "bytes differ from the original")
                        stats["block 18, checked against the original"] += 1
                    else:
                        R.check_block(raw, ck, 18, tflag, pay, seg[r0:r0 + raw] if seg is not None else None, g[10] & 1, stats)
        except (Corrupt, IndexError, struct.error) as e:
            bad.append("%s: %s" % (fr, e))
    for b in bad: print(b)
    for k_ in sorted(stats): print("  %5d %s" % (stats[k_], k_))
    if bad: sys.exit("BWT REFERENCE FAILED (%d frames)" % len(bad))
    print("BWT REFERENCE PASSED (%d blocks and leaves%s)" % (sum(stats.values()), "" if maxb is None else ", up to %d bytes" % maxb))


if __name__ == "__main__":
    main()
