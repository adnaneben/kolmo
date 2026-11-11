/* Engine experiment 1b: a working encoder and decoder for the decode-cheap variant of experiment 1, timed end to
 * end against zstd level 3.
 *
 * Matches come from zstd's level-N search (ZSTD_generateSequences, literal compression off; zstd still entropy-codes
 * its sequences inside that call, so the search time charged here is an upper bound). Coding is ours:
 *   literals   one Huffman stream per block, plain or minus-the-byte-at-the-last-offset (picked by histogram), or,
 *              where a record period is found, up to 16 streams by position in the record
 *   sequences  offset / literal-run / match-length symbols in Huffman streams split by context, extra bits raw
 * Huffman streams use zstd's coder with table reuse across blocks. Every file is decoded and compared.
 *
 * build: cc -O3 -march=native -Ivendor/zstd -Ivendor/zstd/common research/engine/exp1b.c build/libvendor.a -lm -o /tmp/exp1b
 * run:   LEVEL=2 /tmp/exp1b files...                                                                              */
#define ZSTD_STATIC_LINKING_ONLY
#include "zstd.h"
#include "huf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned u32; typedef unsigned long long u64;
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int HFLAGS = 0;
#define BLK (HUF_BLOCKSIZE_MAX)
#define BL(x) (31 - __builtin_clz(x))

typedef struct { HUF_CElt ct[HUF_CTABLE_SIZE_ST(255)]; HUF_repeat rep; } cstate;
typedef struct { HUF_DTable dt[HUF_DTABLE_SIZE(HUF_TABLELOG_MAX)]; } dstate;
static u64 WK[HUF_WORKSPACE_SIZE_U64 + 8]; static u32 DWK[HUF_DECOMPRESS_WORKSPACE_SIZE_U32 + 8];
/* one stream: header (mode 1 byte, raw length 3, coded length 3) then bytes */
static u8 *put_stream(cstate *cs, const u8 *src, size_t n, u8 *o, int maxsym) {
    int mode = 0; size_t r = n;
    if (n >= 24) {
        cstate bak = *cs; HUF_repeat rep = cs->rep;
        r = n < 256 ? HUF_compress1X_repeat(o + 7, HUF_compressBound(n), src, n, maxsym, 11, WK, sizeof WK, cs->ct, &rep, HFLAGS)
                    : HUF_compress4X_repeat(o + 7, HUF_compressBound(n), src, n, maxsym, 11, WK, sizeof WK, cs->ct, &rep, HFLAGS);
        int reused = rep != HUF_repeat_none;
        if (HUF_isError(r) || r == 0 || r + 2 + (n >> 6) >= n) { *cs = bak; r = n; mode = 0; }
        else if (r == 1) { *cs = bak; o[7] = src[0]; mode = 1; }
        else { if (!reused) cs->rep = HUF_repeat_check; mode = reused ? 3 : 2; }
    }
    if (mode == 0) memcpy(o + 7, src, n);
    o[0] = (u8)mode; o[1] = (u8)n; o[2] = (u8)(n >> 8); o[3] = (u8)(n >> 16); o[4] = (u8)r; o[5] = (u8)(r >> 8); o[6] = (u8)(r >> 16);
    return o + 7 + r;
}
static const u8 *get_stream(dstate *ds, const u8 *c, u8 *dst, u32 *np) {
    int mode = c[0]; u32 n = c[1] | c[2] << 8 | c[3] << 16, r = c[4] | c[5] << 8 | c[6] << 16; c += 7; *np = n;
    if (mode == 0) memcpy(dst, c, n); else if (mode == 1) memset(dst, c[0], n);
    else { size_t e = mode == 2 ? (n < 256 ? HUF_decompress1X_DCtx_wksp(ds->dt, dst, n, c, r, DWK, sizeof DWK, HFLAGS) : HUF_decompress4X_hufOnly_wksp(ds->dt, dst, n, c, r, DWK, sizeof DWK, HFLAGS))
                                : (n < 256 ? HUF_decompress1X_usingDTable(dst, n, c, r, ds->dt, HFLAGS) : HUF_decompress4X_usingDTable(dst, n, c, r, ds->dt, HFLAGS));
           if (HUF_isError(e)) { fprintf(stderr, "stream decode failed\n"); exit(2); } }
    return c + r;
}
/* sequence symbols: FSE (as zstd uses for sequences), table sent with each stream */
static u8 *put_fse(const u8 *src, size_t n, u8 *o) {
    int mode = 0; size_t r = n;
    if (n >= 16) {
        unsigned cnt[64] = {0}, ms = 0, big = 0; for (size_t i = 0; i < n; i++) cnt[src[i] & 63]++;
        for (unsigned x = 0; x < 64; x++) if (cnt[x]) { ms = x; if (cnt[x] > big) big = cnt[x]; }
        if (big == n) { o[7] = src[0]; r = 1; mode = 1; }
        else {
            short norm[64]; static FSE_CTable ct[FSE_CTABLE_SIZE_U32(9, 63)]; static unsigned wk[FSE_BUILD_CTABLE_WORKSPACE_SIZE_U32(63, 9) + 8];
            unsigned tl = FSE_optimalTableLog(9, n, ms); size_t e = FSE_normalizeCount(norm, tl, cnt, n, ms, n >= 2048);
            size_t h = FSE_isError(e) ? e : FSE_writeNCount(o + 7, 256, norm, ms, tl);
            if (!FSE_isError(h) && !FSE_isError(FSE_buildCTable_wksp(ct, norm, ms, tl, wk, sizeof wk))) {
                size_t c = FSE_compress_usingCTable(o + 7 + h, n, src, n, ct);
                if (!FSE_isError(c) && c && h + c < n) { r = h + c; mode = 2; }
            }
        }
    }
    if (mode == 0) memcpy(o + 7, src, n);
    o[0] = (u8)mode; o[1] = (u8)n; o[2] = (u8)(n >> 8); o[3] = (u8)(n >> 16); o[4] = (u8)r; o[5] = (u8)(r >> 8); o[6] = (u8)(r >> 16);
    return o + 7 + r;
}
static const u8 *get_fse(const u8 *c, u8 *dst, u32 *np) {
    int mode = c[0]; u32 n = c[1] | c[2] << 8 | c[3] << 16, r = c[4] | c[5] << 8 | c[6] << 16; c += 7; *np = n;
    static unsigned wk[FSE_DECOMPRESS_WKSP_SIZE_U32(9, 255) + 8];
    if (mode == 0) memcpy(dst, c, n); else if (mode == 1) memset(dst, c[0], n);
    else if (FSE_isError(FSE_decompress_wksp_bmi2(dst, n, c, r, 9, wk, sizeof wk, HFLAGS != 0))) { fprintf(stderr, "fse decode failed\n"); exit(2); }
    return c + r;
}
static int NOCTX = 0;
static double ent(const u32 *h, u64 n) { double e = 0; if (!n) return 0; for (int s = 0; s < 256; s++) if (h[s]) e -= h[s] * log2((double)h[s] / n); return e; }
static void cluster(u32 (*h)[256], int M, int K, u8 *map) {            /* greedy merge, least entropy increase first */
    u64 n[64] = {0}; int alive[64] = {0}, id[64], live = 0; double E[64]; u32 t[256];
    for (int c = 0; c < M; c++) { for (int s = 0; s < 256; s++) n[c] += h[c][s]; id[c] = c; alive[c] = n[c] > 0; live += alive[c]; if (alive[c]) E[c] = ent(h[c], n[c]); }
    while (live > K) {
        double best = 1e300; int ba = -1, bb = -1;
        for (int a = 0; a < M; a++) if (alive[a]) for (int b = a + 1; b < M; b++) if (alive[b]) { for (int s = 0; s < 256; s++) t[s] = h[a][s] + h[b][s]; double d = ent(t, n[a] + n[b]) - E[a] - E[b]; if (d < best) { best = d; ba = a; bb = b; } }
        for (int s = 0; s < 256; s++) h[ba][s] += h[bb][s]; n[ba] += n[bb]; E[ba] = ent(h[ba], n[ba]); alive[bb] = 0; live--;
        for (int c = 0; c < M; c++) if (id[c] == bb) id[c] = ba;
    }
    int next = 0, num[64]; for (int c = 0; c < M; c++) num[c] = alive[c] ? next++ : 0;
    for (int c = 0; c < M; c++) map[c] = (u8)num[id[c]];
}
enum { CO = 4, CL = 8, CM = 4, NSTR = CO + CL + CM + 2 };
static inline int ctx_l(int pcl) { if (NOCTX) return 0; return pcl < 4 ? pcl : pcl < 8 ? 4 : pcl < 16 ? 5 : pcl < 20 ? 6 : 7; }
static inline void wcopy(u8 *d, const u8 *s, size_t n) { u8 *e = d + n; do { memcpy(d, s, 16); d += 16; s += 16; } while (d < e); }

/* ---- encoder: sequences (from zstd's search) + source -> our frame ---- */
static size_t encode(const u8 *src, size_t n, const ZSTD_Sequence *sq, size_t ns, u8 *out, int P, const u8 *pmap, int KP) {
    static cstate cl_[16]; memset(cl_, 0, sizeof cl_);
    static u8 lit[16][BLK + 64], sub[BLK + 64], st[NSTR][BLK / 3 + 64]; u8 *xb = malloc(BLK * 2 + 64);
    u8 *o = out; size_t pos = 0, si = 0; u32 rp[3] = {1, 4, 8}; int pcl = 0, poc = 0; u32 rep0 = 0;
    while (si < ns) {
        u32 nl[16] = {0}, cn[NSTR] = {0}, hp[256] = {0}, hs[256] = {0}, nq = 0; u64 acc = 0; int nb = 0; u8 *x = xb; size_t tot = 0;
        for (;; si++) {
            if (si >= ns) { o[0] = o[1] = o[2] = 0; break; }
            u32 ll = sq[si].litLength, ml = sq[si].matchLength, off = sq[si].offset;
            if (P > 1) { u32 ph = (u32)(pos % P); for (u32 j = 0; j < ll; j++) { int c = pmap[ph]; lit[c][nl[c]++] = src[pos + j]; if (++ph == (u32)P) ph = 0; } }
            else { memcpy(lit[0] + nl[0], src + pos, ll);
                   if (rep0) for (u32 j = 0; j < ll; j++) { u8 v = src[pos + j], d = (u8)(v - src[pos + j - rep0]); sub[nl[0] + j] = d; hp[v]++; hs[d]++; }
                   else for (u32 j = 0; j < ll; j++) { u8 v = src[pos + j]; sub[nl[0] + j] = v; hp[v]++; hs[v]++; }
                   nl[0] += ll; }
            pos += ll; tot += ll;
            if (!ml) { /* block end: ll was the tail */ o[0] = (u8)ll; o[1] = (u8)(ll >> 8); o[2] = (u8)(ll >> 16); si++; break; }
            int osym; u32 ob = 0;
            if (off == rp[0]) osym = 0; else if (off == rp[1]) { osym = 1; u32 t = rp[1]; rp[1] = rp[0]; rp[0] = t; } else if (off == rp[2]) { osym = 2; u32 t = rp[2]; rp[2] = rp[1]; rp[1] = rp[0]; rp[0] = t; }
            else { ob = BL(off); osym = 3 + ob; rp[2] = rp[1]; rp[1] = rp[0]; rp[0] = off; }
            u32 mv = ml - 3; int lc = ll < 16 ? (int)ll : 12 + BL(ll), mc = mv < 32 ? (int)mv : 27 + BL(mv);
            int xl = ctx_l(pcl), xo = NOCTX ? 0 : !!ll * 2 + (poc < 3), xm = NOCTX ? 0 : osym == 0 ? 0 : osym < 3 ? 1 : ob < 12 ? 2 : 3;
            st[CO + xl][cn[CO + xl]++] = (u8)lc; st[xo][cn[xo]++] = (u8)osym; st[CO + CL + xm][cn[CO + CL + xm]++] = (u8)mc;
            #define PUTB(v, k) do { acc |= (u64)(v) << nb; nb += (k); while (nb >= 8) { *x++ = (u8)acc; acc >>= 8; nb -= 8; } } while (0)
            if (ll >= 16) { int k = BL(ll); PUTB(ll - (1u << k), k); }
            if (osym >= 3) { if (ob >= 4) { int a = CO + CL + CM + (NOCTX ? 0 : ob >= 12); st[a][cn[a]++] = (u8)(off & 15); if (ob > 4) { u32 v = (off - (1u << ob)) >> 4; int k = ob - 4; if (k > 24) { PUTB(v & 0xFFFFFF, 24); v >>= 24; k -= 24; } PUTB(v, k); } } else if (ob) PUTB(off - (1u << ob), ob); }
            if (mv >= 32) { int k = BL(mv); PUTB(mv - (1u << k), k); }
            pos += ml; tot += ml; rep0 = off; pcl = lc; poc = osym; nq++;
        }
        if (nb) *x++ = (u8)acc;
        o[3] = (u8)nq; o[4] = (u8)(nq >> 8); o[5] = (u8)(nq >> 16); u32 xn = (u32)(x - xb); o[6] = (u8)xn; o[7] = (u8)(xn >> 8); o[8] = (u8)(xn >> 16);
        int lmode = 0; if (P <= 1) lmode = ent(hs, nl[0]) < ent(hp, nl[0]) * 0.99; o[9] = (u8)lmode; o += 10;
        if (P > 1) for (int c = 0; c < KP; c++) o = put_stream(&cl_[c], lit[c], nl[c], o, 255);
        else o = put_stream(&cl_[lmode], lmode ? sub : lit[0], nl[0], o, 255);
        for (int c = 0; c < NSTR; c++) o = put_fse(st[c], cn[c], o);
        memcpy(o, xb, xn); o += xn;
    }
    free(xb); (void)n;
    return (size_t)(o - out);
}

/* ---- decoder ---- */
static size_t decode(const u8 *in, size_t zn, u8 *out, int P, const u8 *pmap, int KP) {
    static dstate dl[16]; for (int c = 0; c < 16; c++) dl[c].dt[0] = (u32)HUF_TABLELOG_MAX * 0x01000001;
    static u8 lit[16][BLK + 64], st[NSTR][BLK / 3 + 64];
    const u8 *q = in, *end = in + zn; u8 *o = out; u32 rp[3] = {1, 4, 8}; int pcl = 0, poc = 0; u32 rep0 = 0;
    while (q < end) {
        u32 tail = q[0] | q[1] << 8 | q[2] << 16, nq = q[3] | q[4] << 8 | q[5] << 16, xn = q[6] | q[7] << 8 | q[8] << 16; int lmode = q[9]; q += 10; u32 nn;
        const u8 *lp[16], *sp[NSTR];
        if (P > 1) for (int c = 0; c < KP; c++) { q = get_stream(&dl[c], q, lit[c], &nn); lp[c] = lit[c]; }
        else { q = get_stream(&dl[lmode], q, lit[0], &nn); lp[0] = lit[0]; }
        for (int c = 0; c < NSTR; c++) { q = get_fse(q, st[c], &nn); sp[c] = st[c]; }
        const u8 *x = q; q += xn; u64 acc = 0; int nb = 0;
        #define GETB(v, k) do { while (nb < (k)) { acc |= (u64)*x++ << nb; nb += 8; } (v) = (u32)(acc & ((1ull << (k)) - 1)); acc >>= (k); nb -= (k); } while (0)
        for (u32 i = 0; i <= nq; i++) {
            u32 ll, ml = 0, off = 0; int lc = 0, osym = 0; u32 ob = 0, e;
            if (i == nq) ll = tail;
            else {
                lc = *sp[CO + ctx_l(pcl)]++; if (lc < 16) ll = (u32)lc; else { int k = lc - 12; GETB(e, k); ll = (1u << k) + e; }
                osym = *sp[NOCTX ? 0 : !!ll * 2 + (poc < 3)]++;
                if (osym == 0) off = rp[0]; else if (osym == 1) { off = rp[1]; rp[1] = rp[0]; rp[0] = off; } else if (osym == 2) { off = rp[2]; rp[2] = rp[1]; rp[1] = rp[0]; rp[0] = off; }
                else { ob = (u32)osym - 3; if (ob >= 4) { u32 low = *sp[CO + CL + CM + (NOCTX ? 0 : ob >= 12)]++, hi = 0; if (ob > 4) { int k = (int)ob - 4; if (k > 24) { u32 h2; GETB(hi, 24); GETB(h2, k - 24); hi |= h2 << 24; } else GETB(hi, k); } off = (1u << ob) + (hi << 4 | low); if (ob >= 4) off = ((1u << ob) & ~15u) + (hi << 4) + low; }
                       else { e = 0; if (ob) GETB(e, (int)ob); off = (1u << ob) + e; }
                       rp[2] = rp[1]; rp[1] = rp[0]; rp[0] = off; }
                int mc = *sp[CO + CL + (NOCTX ? 0 : osym == 0 ? 0 : osym < 3 ? 1 : ob < 12 ? 2 : 3)]++; u32 mv; if (mc < 32) mv = (u32)mc; else { int k = mc - 27; GETB(e, k); mv = (1u << k) + e; } ml = mv + 3;
            }
            if (P > 1) { u32 ph = (u32)((o - out) % P); for (u32 j = 0; j < ll; j++) { *o++ = *lp[pmap[ph]]++; if (++ph == (u32)P) ph = 0; } }
            else if (!lmode || !rep0) { if (ll) wcopy(o, lp[0], ll); lp[0] += ll; o += ll; }
            else { if (rep0 >= 16) { for (u32 j = 0; j < ll; j += 16) for (int t = 0; t < 16; t++) o[j + t] = (u8)(lp[0][j + t] + o[(ptrdiff_t)(j + t) - rep0]); } else for (u32 j = 0; j < ll; j++) o[j] = (u8)(lp[0][j] + o[(ptrdiff_t)j - rep0]); lp[0] += ll; o += ll; }
            if (i < nq) { if (off >= 16) wcopy(o, o - off, ml); else for (u32 j = 0; j < ml; j++) o[j] = o[(ptrdiff_t)j - off]; o += ml; rep0 = off; pcl = lc; poc = osym; }
        }
    }
    return (size_t)(o - out);
}

int main(int argc, char **argv) {
    HFLAGS = __builtin_cpu_supports("bmi2") ? HUF_flags_bmi2 : 0;
    NOCTX = getenv("NOCTX") != NULL;
    const int LEVEL = getenv("LEVEL") ? atoi(getenv("LEVEL")) : 2, REPS = getenv("REPS") ? atoi(getenv("REPS")) : 5;
    printf("file,n,zstd3,z3_enc_ms,z3_dec_ms,zstdN,zN_enc_ms,ours,parse_ms,code_ms,dec_ms,P,ok\n");
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
        u8 *src = malloc(n + 64); if (fread(src, 1, n, f) != n) return 1; fclose(f);
        const char *name = strrchr(argv[a], '/'); name = name ? name + 1 : argv[a];
        ZSTD_CCtx *cc = ZSTD_createCCtx(), *c2 = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(n) + n / 8 + (1 << 20); u8 *z = malloc(cap), *back = malloc(n + 256), *mine = malloc(cap);
        double t3e = 1e9, t3d = 1e9, tNe = 1e9, tp = 1e9, tc = 1e9, td = 1e9; size_t A3 = 0, AN = 0, M = 0, ns = 0; int ok = 1;
        size_t sb = ZSTD_sequenceBound(n); ZSTD_Sequence *sq = malloc(sb * sizeof *sq);
        int P = 1, KP = 1; u8 pmap[64] = {0};
        for (int r = 0; r < REPS; r++) {
            double t0 = now(); A3 = ZSTD_compressCCtx(cc, z, cap, src, n, 3); double t1 = now(); if (t1 - t0 < t3e) t3e = t1 - t0;
            t0 = now(); ZSTD_decompress(back, n, z, A3); t1 = now(); if (t1 - t0 < t3d) t3d = t1 - t0;
            t0 = now(); AN = ZSTD_compressCCtx(cc, z, cap, src, n, LEVEL); t1 = now(); if (t1 - t0 < tNe) tNe = t1 - t0;
            /* ours: search */
            t0 = now(); ZSTD_CCtx_reset(c2, ZSTD_reset_session_and_parameters); ZSTD_CCtx_setParameter(c2, ZSTD_c_compressionLevel, LEVEL); ZSTD_CCtx_setParameter(c2, ZSTD_c_literalCompressionMode, ZSTD_ps_disable);
            ns = ZSTD_generateSequences(c2, sq, sb, src, n); t1 = now(); if (t1 - t0 < tp) tp = t1 - t0;
            if (ZSTD_isError(ns)) { fprintf(stderr, "%s: %s\n", name, ZSTD_getErrorName(ns)); ok = 0; break; }
            /* ours: record period from the literals of the first sequences (part of the coding time), then coding */
            t0 = now();
            { static u32 hp[64][256]; double bestc = 1e300, c1 = 0; size_t pos = 0, cnt = 0; P = 1;
              static u32 lpos[400000]; static u8 lval[400000]; size_t m = 0;
              for (size_t i = 0; i < ns && m < 400000; i++) { u32 ll = sq[i].litLength; for (u32 j = 0; j < ll && m < 400000; j += 3) { lpos[m] = (u32)(pos + j); lval[m++] = src[pos + j]; } pos += ll + sq[i].matchLength; }
              cnt = m;
              for (int p = 1; p <= 64 && cnt > 20000; p++) { memset(hp, 0, (size_t)p * sizeof hp[0]); for (size_t i = 0; i < cnt; i++) hp[lpos[i] % p][lval[i]]++;
                  double c = p * 800.0 * (double)cnt / 400000; for (int q = 0; q < p; q++) { u64 t = 0; for (int s = 0; s < 256; s++) t += hp[q][s]; c += ent(hp[q], t); }
                  if (p == 1) c1 = c; if (c < bestc * 0.99) { bestc = c; P = p; } }
              if (bestc > c1 * 0.95) P = 1;
              if (P > 1) { memset(hp, 0, sizeof hp); for (size_t i = 0; i < cnt; i++) hp[lpos[i] % P][lval[i]]++; KP = P < 16 ? P : 16; cluster(hp, P, KP, pmap); } }
            M = encode(src, n, sq, ns, mine, P, pmap, KP); t1 = now(); if (t1 - t0 < tc) tc = t1 - t0;
            t0 = now(); size_t dn = decode(mine, M, back, P, pmap, KP); t1 = now(); if (t1 - t0 < td) td = t1 - t0;
            if (dn != n || memcmp(back, src, n)) { fprintf(stderr, "%s: round trip differs (%zu of %zu)\n", name, dn, n); ok = 0; break; }
        }
        printf("%s,%zu,%zu,%.2f,%.2f,%zu,%.2f,%zu,%.2f,%.2f,%.2f,%d,%d\n", name, n, A3, t3e * 1e3, t3d * 1e3, AN, tNe * 1e3, M + (P > 1 ? 70 : 6), tp * 1e3, tc * 1e3, td * 1e3, P, ok); fflush(stdout);
        free(src); free(z); free(back); free(mine); free(sq); ZSTD_freeCCtx(cc); ZSTD_freeCCtx(c2);
    }
    return 0;
}
