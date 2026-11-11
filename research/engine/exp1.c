/* Engine experiment 1: is there room above zstd level 3 if only the literals are coded differently?
 *
 * zstd's own level-3 match decisions are kept (ZSTD_generateSequences), so the search cost is a known quantity.
 * Only the bytes the matches leave over (the literals) are re-coded:
 *   K1     one Huffman stream per block: what zstd does (checks this harness against the real frame)
 *   O1Kn   literals split into n streams by the previous byte's class (classes learned per file), Huffman each
 *   POS    split by position modulo a detected record period (a context the decoder knows without decoding)
 *   COL    split by field number within the text line
 *   SUB    literal minus the byte at the last match offset
 *   BINO1  adaptive binary coding with the previous byte as context (the LZMA family)
 *   BINMIX adaptive binary coding mixing order 1, order 2, match byte, position and field (an upper bound)
 * Sizes are zstd's frame with its literal sections replaced. Decode cost is measured with a common sequence executor.
 *
 * build: cc -O3 -march=native -Ivendor/zstd -Ivendor/zstd/common research/engine/exp1.c build/libvendor.a -lm -o /tmp/exp1 */
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

/* ---- zstd frame: bytes spent on literal sections ---- */
static int parse_frame(const u8 *z, size_t zn, u64 *litbytes, u64 *nlit, u64 *rawblk) {
    size_t p = 4; u8 fhd = z[p++]; int ss = (fhd >> 5) & 1, did = fhd & 3, fcs = fhd >> 6;
    if (!ss) p++;
    p += did == 3 ? 4 : did; p += fcs == 0 ? ss : fcs == 1 ? 2 : fcs == 2 ? 4 : 8;
    *litbytes = *nlit = *rawblk = 0;
    for (;;) {
        if (p + 3 > zn) return -1;
        u32 bh = z[p] | z[p + 1] << 8 | z[p + 2] << 16; p += 3; int last = bh & 1, type = (bh >> 1) & 3; u32 bs = bh >> 3;
        if (type == 0) { *rawblk += bs; *litbytes += bs; *nlit += bs; p += bs; }
        else if (type == 1) { *litbytes += 1; *nlit += bs; p += 1; }
        else {
            const u8 *q = z + p; int lt = q[0] & 3, sf = (q[0] >> 2) & 3; u32 regen, comp, hs;
            if (lt < 2) {
                if (!(sf & 1)) { hs = 1; regen = q[0] >> 3; } else if (sf == 1) { hs = 2; regen = (q[0] | q[1] << 8) >> 4; } else { hs = 3; regen = (q[0] | q[1] << 8 | q[2] << 16) >> 4; }
                comp = lt == 0 ? regen : 1;
            } else {
                u64 v = q[0] | (u64)q[1] << 8 | (u64)q[2] << 16 | (u64)q[3] << 24 | (u64)q[4] << 32;
                if (sf < 2) { hs = 3; regen = (v >> 4) & 1023; comp = (v >> 14) & 1023; }
                else if (sf == 2) { hs = 4; regen = (v >> 4) & 16383; comp = (v >> 18) & 16383; }
                else { hs = 5; regen = (v >> 4) & 262143; comp = (v >> 22) & 262143; }
            }
            *litbytes += hs + comp; *nlit += regen; p += bs;
        }
        if (last) break;
    }
    return 0;
}

/* ---- one literal stream per class, Huffman with table reuse across blocks (as zstd does for its single stream) ---- */
typedef struct { HUF_CElt ct[HUF_CTABLE_SIZE_ST(255)]; HUF_repeat rep; } cstate;
static u64 WK[HUF_WORKSPACE_SIZE_U64 + 8];
static size_t sub_compress(cstate *cs, const u8 *src, size_t n, u8 *dst, size_t cap, int *mode) {
    if (!n) { *mode = 0; return 0; }
    if (n < 24) { memcpy(dst, src, n); *mode = 0; return n; }
    cstate bak = *cs; HUF_repeat rep = cs->rep;
    size_t r = n < 256 ? HUF_compress1X_repeat(dst, cap, src, n, 255, 11, WK, sizeof WK, cs->ct, &rep, HFLAGS)
                       : HUF_compress4X_repeat(dst, cap, src, n, 255, 11, WK, sizeof WK, cs->ct, &rep, HFLAGS);
    int reused = rep != HUF_repeat_none;
    if (HUF_isError(r) || r == 0 || r + 2 + (n >> 6) >= n) { *cs = bak; memcpy(dst, src, n); *mode = 0; return n; }
    if (r == 1) { *cs = bak; dst[0] = src[0]; *mode = 1; return 1; }
    if (!reused) cs->rep = HUF_repeat_check;
    *mode = reused ? 3 : 2; return r;
}
typedef struct { HUF_DTable dt[HUF_DTABLE_SIZE(HUF_TABLELOG_MAX)]; } dstate;
static u32 DWK[HUF_DECOMPRESS_WORKSPACE_SIZE_U32 + 8];
static int sub_decompress(dstate *ds, const u8 *c, size_t cn, u8 *dst, size_t n, int mode) {
    if (!n) return 0;
    if (mode == 0) { memcpy(dst, c, n); return 0; }
    if (mode == 1) { memset(dst, c[0], n); return 0; }
    size_t r;
    if (mode == 2) r = n < 256 ? HUF_decompress1X_DCtx_wksp(ds->dt, dst, n, c, cn, DWK, sizeof DWK, HFLAGS) : HUF_decompress4X_hufOnly_wksp(ds->dt, dst, n, c, cn, DWK, sizeof DWK, HFLAGS);
    else r = n < 256 ? HUF_decompress1X_usingDTable(dst, n, c, cn, ds->dt, HFLAGS) : HUF_decompress4X_usingDTable(dst, n, c, cn, ds->dt, HFLAGS);
    return HUF_isError(r) ? -1 : 0;
}

/* packed form of one scheme: per block, per class: mode, raw length, coded length, bytes */
typedef struct { u8 *buf; size_t len, cap; } pack_t;
static void pk(pack_t *P, const void *d, size_t n) { if (P->len + n > P->cap) { P->cap = (P->cap + n) * 2; P->buf = realloc(P->buf, P->cap); } memcpy(P->buf + P->len, d, n); P->len += n; }

/* Cost (and optionally the packed bytes) of coding val[] split by cls[] into K Huffman streams per block. */
static u64 cost_split(const u8 *val, const u8 *cls, int K, const u32 *blkend, u32 nb, pack_t *P) {
    cstate *cs = calloc(K, sizeof(cstate)); u8 *tmp = malloc(K * (size_t)(HUF_BLOCKSIZE_MAX + 64)), *o = malloc(HUF_compressBound(HUF_BLOCKSIZE_MAX) + 64);
    u32 *cnt = malloc(K * 4); u64 tot = 0; u32 a = 0;
    for (u32 b = 0; b < nb; b++) {
        u32 e = blkend[b]; memset(cnt, 0, K * 4);
        if (K == 1) { cnt[0] = e - a; memcpy(tmp, val + a, e - a); }
        else for (u32 i = a; i < e; i++) { int c = cls[i]; tmp[(size_t)c * (HUF_BLOCKSIZE_MAX + 64) + cnt[c]++] = val[i]; }
        tot += K > 1 ? (K + 7) / 8 : 0;                               /* which classes are present */
        for (int c = 0; c < K; c++) {
            int mode; size_t r = sub_compress(&cs[c], tmp + (size_t)c * (HUF_BLOCKSIZE_MAX + 64), cnt[c], o, HUF_compressBound(HUF_BLOCKSIZE_MAX) + 64, &mode);
            if (cnt[c]) tot += r + (K == 1 ? 4 : 3);                   /* stream header: mode and two lengths */
            if (P) { u32 h[3] = {(u32)mode, cnt[c], (u32)r}; pk(P, h, 12); pk(P, o, r); }
        }
        a = e;
    }
    free(cs); free(tmp); free(o); free(cnt);
    return tot;
}

/* Greedy merge of M contexts (histograms h[M][256]) into at most K classes, least entropy increase first. */
static double ent(const u32 *h, u64 n) { double e = 0; if (!n) return 0; for (int s = 0; s < 256; s++) if (h[s]) e -= h[s] * log2((double)h[s] / n); return e; }
static void cluster(u32 (*h)[256], int M, int K, u8 *map) {
    u64 *n = calloc(M, 8), tot = 0; int *alive = calloc(M, sizeof(int)), *id = malloc(M * sizeof(int)); double *E = malloc(M * sizeof(double));
    for (int c = 0; c < M; c++) { for (int s = 0; s < 256; s++) n[c] += h[c][s]; tot += n[c]; id[c] = c; }
    int rare = -1, live = 0;                                           /* contexts under 1/2000 of the data share one class */
    for (int c = 0; c < M; c++) {
        if (!n[c]) { id[c] = -1; continue; }
        if (n[c] * 2000 < tot) { if (rare < 0) { rare = c; alive[c] = 1; live++; } else { for (int s = 0; s < 256; s++) h[rare][s] += h[c][s]; n[rare] += n[c]; id[c] = rare; } }
        else { alive[c] = 1; live++; }
    }
    for (int c = 0; c < M; c++) if (alive[c]) E[c] = ent(h[c], n[c]);
    u32 t[256];
    while (live > K) {
        double best = 1e300; int ba = -1, bb = -1;
        for (int a = 0; a < M; a++) if (alive[a]) for (int b = a + 1; b < M; b++) if (alive[b]) {
            for (int s = 0; s < 256; s++) t[s] = h[a][s] + h[b][s];
            double d = ent(t, n[a] + n[b]) - E[a] - E[b];
            if (d < best) { best = d; ba = a; bb = b; }
        }
        for (int s = 0; s < 256; s++) h[ba][s] += h[bb][s];
        n[ba] += n[bb]; E[ba] = ent(h[ba], n[ba]); alive[bb] = 0; live--;
        for (int c = 0; c < M; c++) if (id[c] == bb) id[c] = ba;
    }
    int next = 0, *num = malloc(M * sizeof(int)); for (int c = 0; c < M; c++) num[c] = alive[c] ? next++ : -1;
    for (int c = 0; c < M; c++) map[c] = id[c] < 0 ? 0 : (u8)num[id[c]];
    free(n); free(alive); free(id); free(E); free(num);
}

/* ---- adaptive binary models (cost only) ---- */
static float LG[4097]; static float STR[4097];
static inline float squash(float x) { return 1.0f / (1.0f + expf(-x)); }
#define UPD(p, bit) ((p) += (((bit) ? 65535 - (p) : -(p)) >> 4))

/* ---- real binary range coder (LZMA style) for a measured decode rate ---- */
typedef struct { u64 low; u32 range; u8 cache; u64 csz; u8 *out; size_t n; } rce;
static void rc_shift(rce *r) { if ((u32)r->low < 0xFF000000u || (r->low >> 32)) { u8 c = r->cache; do { r->out[r->n++] = (u8)(c + (u8)(r->low >> 32)); c = 0xFF; } while (--r->csz); r->cache = (u8)(r->low >> 24); } r->csz++; r->low = (u32)r->low << 8; }
static inline void rc_enc(rce *r, u16 *p, int bit) { u32 b = (r->range >> 11) * *p; if (!bit) { r->range = b; *p += (2048 - *p) >> 5; } else { r->low += b; r->range -= b; *p -= *p >> 5; } while (r->range < (1u << 24)) { r->range <<= 8; rc_shift(r); } }

int main(int argc, char **argv) {
    HFLAGS = __builtin_cpu_supports("bmi2") ? HUF_flags_bmi2 : 0;
    const int LEVEL = getenv("LEVEL") ? atoi(getenv("LEVEL")) : 3;       /* whose parse is kept */
    for (int i = 1; i <= 4096; i++) { LG[i] = -log2f(i / 4096.0f); } LG[0] = 12;
    for (int i = 0; i <= 4096; i++) { float p = (i < 1 ? 0.5f : i > 4095 ? 4095.5f : i) / 4096.0f; STR[i] = logf(p / (1 - p)); }
    printf("file,n,zstd3,zstd_lit_bytes,nlit,K1,O1K4,O1K8,O1K16,P,POS,COL,SUB,SUBblk,BINO1,BINMIX,S0,SC,SA,nseq,t_zstd_dec_ms,t_base_ms,t_o1_ms,t_pos_ms,t_sub_ms,bin_dec_MBps_lit,chk\n");
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
        u8 *src = malloc(n + 16); if (fread(src, 1, n, f) != n) return 1; fclose(f);
        const char *name = strrchr(argv[a], '/'); name = name ? name + 1 : argv[a];
        ZSTD_CCtx *cc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(n); u8 *z = malloc(cap), *back = malloc(n + 64);
        size_t A = ZSTD_compressCCtx(cc, z, cap, src, n, LEVEL);
        double tz = 1e9; for (int r = 0; r < 5; r++) { double t0 = now(); ZSTD_decompress(back, n, z, A); double t1 = now() - t0; if (t1 < tz) tz = t1; }
        u64 L, nlz, rawb; parse_frame(z, A, &L, &nlz, &rawb);
        ZSTD_CCtx_reset(cc, ZSTD_reset_session_and_parameters); ZSTD_CCtx_setParameter(cc, ZSTD_c_compressionLevel, LEVEL);
        size_t sb = ZSTD_sequenceBound(n); ZSTD_Sequence *sq = malloc(sb * sizeof *sq); size_t ns = ZSTD_generateSequences(cc, sq, sb, src, n);
        if (ZSTD_isError(ns)) { fprintf(stderr, "%s: sequences: %s\n", name, ZSTD_getErrorName(ns)); continue; }
        /* literals: position, last match offset; blocks */
        u32 *lpos = malloc((n + 1) * 4), *lrep = malloc((n + 1) * 4), *blkend = malloc((ns + 2) * 4), *blkseq = malloc((ns + 2) * 4); u32 nl = 0, nb = 0; size_t pos = 0; u32 rep0 = 0;
        for (size_t i = 0; i < ns; i++) {
            u32 ll = sq[i].litLength; for (u32 j = 0; j < ll; j++) { lpos[nl] = (u32)(pos + j); lrep[nl] = rep0; nl++; } pos += ll;
            if (!sq[i].matchLength && !sq[i].offset) { blkend[nb] = nl; blkseq[nb] = (u32)i + 1; nb++; } else { pos += sq[i].matchLength; rep0 = sq[i].offset; }
        }
        if (!nb || blkseq[nb - 1] != ns) { blkend[nb] = nl; blkseq[nb] = (u32)ns; nb++; }
        int chk = pos == n && (nl > nlz ? nl - nlz : nlz - nl) * 1000 <= nlz + 1000; if (!chk) fprintf(stderr, "%s: pos %zu n %zu nl %u zstd-literals %llu raw-block-bytes %llu blocks %u\n", name, pos, n, nl, nlz, rawb, nb);
        u8 *val = malloc(nl + 1), *cls = calloc(nl + 1, 1), *vsub = malloc(nl + 1);
        for (u32 i = 0; i < nl; i++) { u32 p = lpos[i]; val[i] = src[p]; vsub[i] = lrep[i] && p >= lrep[i] ? (u8)(src[p] - src[p - lrep[i]]) : src[p]; }
        /* a block's literal count can exceed what one Huffman call takes only if blocks were merged; they are not */
        u64 cK1 = cost_split(val, cls, 1, blkend, nb, NULL);
        /* O1: classes of the previous byte */
        u32 (*h)[256] = calloc(256, sizeof *h); for (u32 i = 0; i < nl; i++) h[lpos[i] ? src[lpos[i] - 1] : 0][val[i]]++;
        u32 (*h2)[256] = malloc(256 * sizeof *h2); u8 map4[256], map8[256], map16[256];
        memcpy(h2, h, 256 * sizeof *h); cluster(h2, 256, 16, map16); memcpy(h2, h, 256 * sizeof *h); cluster(h2, 256, 8, map8); memcpy(h2, h, 256 * sizeof *h); cluster(h2, 256, 4, map4);
        u64 cO1[3]; const u8 *maps[3] = {map4, map8, map16}; const int Ks[3] = {4, 8, 16};
        for (int k = 0; k < 3; k++) { for (u32 i = 0; i < nl; i++) cls[i] = maps[k][lpos[i] ? src[lpos[i] - 1] : 0]; cO1[k] = cost_split(val, cls, Ks[k], blkend, nb, NULL) + 128; }
        /* POS: record period from a sample of literals */
        int P = 1; { u32 step = nl > 2000000 ? nl / 2000000 : 1; double bestc = 1e300, c1 = 0; u32 (*hp)[256] = malloc(64 * sizeof *hp);
            for (int p = 1; p <= 64; p++) { memset(hp, 0, (size_t)p * sizeof *hp); u64 *cn = calloc(p, 8); for (u32 i = 0; i < nl; i += step) { hp[lpos[i] % p][val[i]]++; }
                double c = 0; for (int q = 0; q < p; q++) { u64 t = 0; for (int s = 0; s < 256; s++) t += hp[q][s]; c += ent(hp[q], t); } free(cn);
                c += p * 8.0 * 100;                                     /* table cost per class */
                if (p == 1) c1 = c; if (c < bestc * 0.99) { bestc = c; P = p; } }
            if (bestc > c1 * 0.97) P = 1; free(hp); }
        u64 cPOS = 0; u8 pmap[64] = {0}; int KP = 1;
        if (P > 1) { u32 (*hp)[256] = calloc(64, sizeof *hp); for (u32 i = 0; i < nl; i++) hp[lpos[i] % P][val[i]]++; KP = P < 16 ? P : 16; cluster(hp, P, KP, pmap); free(hp);
            for (u32 i = 0; i < nl; i++) cls[i] = pmap[lpos[i] % P]; cPOS = cost_split(val, cls, KP, blkend, nb, NULL) + 64; }
        /* COL: field number in the line, for text */
        u64 cCOL = 0; u8 *fld = NULL; { size_t pr = 0, nlc = 0, dc[5] = {0}; const char D[5] = {',', '\t', ';', '|', ' '}; size_t m = n < (1u << 20) ? n : (1u << 20);
            for (size_t i = 0; i < m; i++) { u8 c = src[i]; pr += (c >= 32 && c < 127) || c == '\n' || c == '\t' || c == '\r'; nlc += c == '\n'; for (int d = 0; d < 5; d++) dc[d] += c == D[d]; }
            if (pr * 100 >= m * 95 && nlc * 2000 >= m) { int bd = 4; size_t bc = 0; for (int d = 0; d < 4; d++) if (dc[d] >= nlc && dc[d] > bc) { bd = d; bc = dc[d]; }
                fld = malloc(n + 1); u32 fi = 0; for (size_t i = 0; i < n; i++) { fld[i] = (u8)(fi < 63 ? fi : 63); if (src[i] == '\n') fi = 0; else if (src[i] == (u8)D[bd]) fi++; }
                u32 (*hp)[256] = calloc(64, sizeof *hp); for (u32 i = 0; i < nl; i++) hp[fld[lpos[i]]][val[i]]++; u8 cm[64]; cluster(hp, 64, 16, cm); free(hp);
                for (u32 i = 0; i < nl; i++) cls[i] = cm[fld[lpos[i]]]; cCOL = cost_split(val, cls, 16, blkend, nb, NULL) + 64; } }
        /* SUB: whole file, and the better of plain and sub per block (an estimate: table reuse chains are per variant) */
        memset(cls, 0, nl); u64 cSUB = cost_split(vsub, cls, 1, blkend, nb, NULL), cSUBb = 0;
        { u32 a0 = 0; for (u32 b = 0; b < nb; b++) { u32 e = blkend[b], one = e - a0; u64 x = cost_split(val + a0, cls, 1, &one, 1, NULL), y = cost_split(vsub + a0, cls, 1, &one, 1, NULL); cSUBb += (x < y ? x : y) + 1; a0 = e; } }
        /* adaptive binary: order 1 alone, and a mix */
        double bO1 = 0, bMIX = 0;
        { u16 *t1 = malloc(65536 * 2), *t0 = malloc(256 * 2), *t2 = malloc((1u << 24) * 2), *tm = malloc(2 * 65536 * 2), *tp = malloc(64 * 256 * 2), *tc = malloc(64 * 256 * 2);
          for (u32 i = 0; i < 65536; i++) t1[i] = 32768; for (u32 i = 0; i < 256; i++) t0[i] = 32768; for (u32 i = 0; i < (1u << 24); i++) t2[i] = 32768;
          for (u32 i = 0; i < 2 * 65536; i++) tm[i] = 32768; for (u32 i = 0; i < 64 * 256; i++) tp[i] = tc[i] = 32768;
          enum { NI = 6 }; float (*w)[NI] = malloc(256 * sizeof *w); for (int i = 0; i < 256; i++) for (int j = 0; j < NI; j++) w[i][j] = 0.3f;
          for (u32 i = 0; i < nl; i++) {
              u32 p = lpos[i]; u8 c = val[i], c1 = p ? src[p - 1] : 0, c2 = p > 1 ? src[p - 2] : 0, mb = lrep[i] && p >= lrep[i] ? src[p - lrep[i]] : 0; int hasmb = lrep[i] && p >= lrep[i], matched = hasmb;
              u32 ph = P > 1 ? p % P : 0; if (ph > 63) ph &= 63; u32 fi = fld ? fld[p] : 0; u32 node = 1;
              for (int k = 7; k >= 0; k--) {
                  int bit = (c >> k) & 1, mbit = (mb >> k) & 1;
                  u16 *q[NI] = {&t0[node], &t1[c1 * 256 + node], &t2[((u32)c2 << 16 | (u32)c1 << 8 | node)], &tm[(matched ? 65536 : 0) + (matched ? mb : c1) * 256 + node], &tp[ph * 256 + node], &tc[fi * 256 + node]};
                  bO1 += LG[(*q[1] >> 4) ? (bit ? *q[1] >> 4 : 4096 - (*q[1] >> 4)) : (bit ? 1 : 4095)];
                  float s[NI], dot = 0; for (int j = 0; j < NI; j++) { s[j] = STR[*q[j] >> 4]; dot += w[node][j] * s[j]; }
                  float pr = squash(dot); if (pr < 1.0f / 4096) pr = 1.0f / 4096; if (pr > 4095.0f / 4096) pr = 4095.0f / 4096;
                  bMIX += -log2f(bit ? pr : 1 - pr); float err = (bit - pr) * 0.02f; for (int j = 0; j < NI; j++) w[node][j] += err * s[j];
                  for (int j = 0; j < NI; j++) UPD(*q[j], bit);
                  if (matched && bit != mbit) matched = 0; node = node * 2 + bit;
              }
          }
          free(t1); free(t0); free(t2); free(tm); free(tp); free(tc); free(w); }
        /* ---- sequences: what contexts could save over one static table per field per block ----
         * symbols: offset (rep0..2 or the bit length of a new offset), literal-run code, match-length code. Costs are
         * entropies: S0 = one static table per field per block (zstd's design; calibrates against the real frame),
         * SC = static tables per context, SA = adaptive with the same contexts. Extra bits are charged in full except
         * the low four offset bits, which SC and SA model. */
        double S0 = 0, SC = 0, SA = 0;
        { enum { NO = 36, NL = 40, NM = 64, CO = 4, CL = 8, CM2 = 4 };
          static u32 so[NO], sl[NL], sm[NM], co[CO][NO], cl[CL][NL], cm[CM2][NM], al[16], ca[2][16];
          static u32 ao[CO][NO], aot[CO], all_[CL][NL], alt[CL], am[CM2][NM], amt[CM2], aa[2][16], aat[2];
          for (int c = 0; c < CO; c++) { aot[c] = NO; for (int x = 0; x < NO; x++) ao[c][x] = 1; } for (int c = 0; c < CL; c++) { alt[c] = NL; for (int x = 0; x < NL; x++) all_[c][x] = 1; }
          for (int c = 0; c < CM2; c++) { amt[c] = NM; for (int x = 0; x < NM; x++) am[c][x] = 1; } for (int c = 0; c < 2; c++) { aat[c] = 16; for (int x = 0; x < 16; x++) aa[c][x] = 1; }
          u32 rp[3] = {1, 4, 8}; int pcl = 0, poc = 0; size_t si = 0; double extra = 0, extra_lo = 0;
          #define BL(x) (31 - __builtin_clz(x))
          #define ENTB(h, N) do { u64 t_ = 0; for (int x = 0; x < (N); x++) t_ += (h)[x]; if (t_) { for (int x = 0; x < (N); x++) if ((h)[x]) { acc -= (h)[x] * log2((double)(h)[x] / t_); acc += 8; } } } while (0)
          #define ADP(tab, tot, N, sym) do { SA += -log2((double)(tab)[sym] / (tot)); (tab)[sym] += 24; (tot) += 24; if ((tot) > 60000) { (tot) = 0; for (int x = 0; x < (N); x++) { (tab)[x] = ((tab)[x] + 1) >> 1; (tot) += (tab)[x]; } } } while (0)
          for (u32 b = 0; b < nb; b++) {
              memset(so, 0, sizeof so); memset(sl, 0, sizeof sl); memset(sm, 0, sizeof sm); memset(co, 0, sizeof co); memset(cl, 0, sizeof cl); memset(cm, 0, sizeof cm); memset(al, 0, sizeof al); memset(ca, 0, sizeof ca);
              for (; si < blkseq[b]; si++) {
                  u32 ll = sq[si].litLength, ml = sq[si].matchLength, off = sq[si].offset; if (!ml) continue;
                  int osym, lowctx = 0; u32 low = 0, ob = 0;
                  if (off == rp[0]) osym = 0; else if (off == rp[1]) { osym = 1; u32 t = rp[1]; rp[1] = rp[0]; rp[0] = t; } else if (off == rp[2]) { osym = 2; u32 t = rp[2]; rp[2] = rp[1]; rp[1] = rp[0]; rp[0] = t; }
                  else { ob = BL(off); osym = 3 + ob; rp[2] = rp[1]; rp[1] = rp[0]; rp[0] = off; }
                  int lc = ll < 16 ? (int)ll : 12 + BL(ll), mv = ml - 3, mc = mv < 32 ? mv : 27 + BL(mv); if (lc >= NL) lc = NL - 1; if (mc >= NM) mc = NM - 1;
                  int xo = !!ll * 2 + (poc < 3), xl = pcl < 8 ? (pcl < 4 ? pcl : 4 + (pcl >= 8)) : 5 + (pcl >= 16) + (pcl >= 20), xm = osym == 0 ? 0 : osym < 3 ? 1 : ob < 12 ? 2 : 3; if (xl >= CL) xl = CL - 1;
                  so[osym]++; sl[lc]++; sm[mc]++; co[xo][osym]++; cl[xl][lc]++; cm[xm][mc]++;
                  if (osym >= 3) { if (ob >= 4) { low = off & 15; lowctx = ob >= 12; al[low]++; ca[lowctx][low]++; extra += ob - 4; extra_lo += 4; ADP(aa[lowctx], aat[lowctx], 16, low); } else extra += ob; }
                  if (ll >= 16) extra += BL(ll); if (mv >= 32) extra += BL(mv);
                  ADP(ao[xo], aot[xo], NO, osym); ADP(all_[xl], alt[xl], NL, lc); ADP(am[xm], amt[xm], NM, mc);
                  pcl = lc; poc = osym;
              }
              double acc = 0; ENTB(so, NO); ENTB(sl, NL); ENTB(sm, NM); S0 += acc;
              acc = 0; for (int c = 0; c < CO; c++) ENTB(co[c], NO); for (int c = 0; c < CL; c++) ENTB(cl[c], NL); for (int c = 0; c < CM2; c++) ENTB(cm[c], NM); for (int c = 0; c < 2; c++) ENTB(ca[c], 16); SC += acc;
          }
          S0 += extra + extra_lo; SC += extra; SA += extra; S0 /= 8; SC /= 8; SA /= 8; }
        /* ---- decode cost: a common executor with the literal schemes ---- */
        double tk[4] = {0, 0, 0, 0};
        for (int kind = 0; kind < 4; kind++) {
            int K = kind == 0 || kind == 3 ? 1 : kind == 1 ? 16 : KP; if (kind == 2 && P == 1) continue;
            const u8 *v = kind == 3 ? vsub : val;
            if (kind == 1) for (u32 i = 0; i < nl; i++) cls[i] = map16[lpos[i] ? src[lpos[i] - 1] : 0];
            else if (kind == 2) for (u32 i = 0; i < nl; i++) cls[i] = pmap[lpos[i] % P]; else memset(cls, 0, nl);
            pack_t Pk = {0}; cost_split(v, cls, K, blkend, nb, &Pk);
            u8 *lb = malloc((size_t)K * (HUF_BLOCKSIZE_MAX + 64)); dstate *ds = calloc(K, sizeof(dstate)); const u8 *lp[16]; double best = 1e9;
            for (int r = 0; r < 5; r++) {
                double t0 = now(); const u8 *q = Pk.buf; u8 *out = back; size_t si = 0; u32 rp = 0;
                for (int c = 0; c < K; c++) ds[c].dt[0] = (u32)HUF_TABLELOG_MAX * 0x01000001;
                for (u32 b = 0; b < nb; b++) {
                    for (int c = 0; c < K; c++) { u32 hd[3]; memcpy(hd, q, 12); q += 12; lp[c] = lb + (size_t)c * (HUF_BLOCKSIZE_MAX + 64); if (sub_decompress(&ds[c], q, hd[2], (u8 *)lp[c], hd[1], (int)hd[0])) { fprintf(stderr, "huf decode failed\n"); return 2; } q += hd[2]; }
                    for (; si < blkseq[b]; si++) {
                        u32 ll = sq[si].litLength, ml = sq[si].matchLength, off = sq[si].offset;
                        if (kind == 0) { memcpy(out, lp[0], ll); lp[0] += ll; out += ll; }
                        else if (kind == 1) { u8 pv = out > back ? out[-1] : 0; for (u32 j = 0; j < ll; j++) { int c = map16[pv]; pv = *lp[c]++; *out++ = pv; } }
                        else if (kind == 2) { u32 ph = (u32)((out - back) % P); for (u32 j = 0; j < ll; j++) { *out++ = *lp[pmap[ph]]++; if (++ph == (u32)P) ph = 0; } }
                        else { if (!rp) memcpy(out, lp[0], ll); else for (u32 j = 0; j < ll; j++) out[j] = (u8)(lp[0][j] + out[(ptrdiff_t)j - rp]); lp[0] += ll; out += ll; }
                        if (ml) { if (off >= ml) memcpy(out, out - off, ml); else for (u32 j = 0; j < ml; j++) out[j] = out[(ptrdiff_t)j - off]; out += ml; rp = off; }
                    }
                }
                double t1 = now() - t0; if (t1 < best) best = t1;
                if ((size_t)(out - back) != n || memcmp(back, src, n)) { fprintf(stderr, "%s: executor %d output differs\n", name, kind); chk = 0; break; }
            }
            tk[kind] = best; free(lb); free(ds); free(Pk.buf);
        }
        /* measured rate of a real binary decoder on the literals (context: previous byte) */
        double binrate = 0; { u8 *eb = malloc(nl + nl / 2 + 1024); rce R = {0, 0xFFFFFFFFu, 0, 1, eb, 0}; u16 *pt = malloc(65536 * 2); for (u32 i = 0; i < 65536; i++) pt[i] = 1024;
            for (u32 i = 0; i < nl; i++) { u32 c1 = lpos[i] ? src[lpos[i] - 1] : 0, node = 1; for (int k = 7; k >= 0; k--) { int bit = (val[i] >> k) & 1; rc_enc(&R, &pt[c1 * 256 + node], bit); node = node * 2 + bit; } }
            for (int i = 0; i < 5; i++) rc_shift(&R);
            double best = 1e9; u8 *dl = malloc(nl + 1);
            for (int r = 0; r < 3; r++) { for (u32 i = 0; i < 65536; i++) pt[i] = 1024; const u8 *ip = eb + 1; u32 code = 0, range = 0xFFFFFFFFu; for (int i = 0; i < 4; i++) code = code << 8 | *ip++;
                double t0 = now();
                for (u32 i = 0; i < nl; i++) { u32 c1 = lpos[i] ? src[lpos[i] - 1] : 0, node = 1; u16 *pp = &pt[c1 * 256];
                    do { u16 *p = &pp[node]; u32 bd = (range >> 11) * *p; if (code < bd) { range = bd; *p += (2048 - *p) >> 5; node = node * 2; } else { code -= bd; range -= bd; *p -= *p >> 5; node = node * 2 + 1; }
                         if (range < (1u << 24)) { range <<= 8; code = code << 8 | *ip++; } } while (node < 256);
                    dl[i] = (u8)node; }
                double t1 = now() - t0; if (t1 < best) best = t1; }
            if (memcmp(dl, val, nl)) { fprintf(stderr, "%s: binary decoder differs\n", name); chk = 0; }
            binrate = nl / 1e6 / best; free(eb); free(pt); free(dl); }
        printf("%s,%zu,%zu,%llu,%u,%llu,%llu,%llu,%llu,%d,%llu,%llu,%llu,%llu,%.0f,%.0f,%.0f,%.0f,%.0f,%zu,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%d\n", name, n, A, L, nl, cK1, cO1[0], cO1[1], cO1[2], P, cPOS, cCOL, cSUB, cSUBb,
               bO1 / 8, bMIX / 8, S0, SC, SA, ns, tz * 1e3, tk[0] * 1e3, tk[1] * 1e3, tk[2] * 1e3, tk[3] * 1e3, binrate, chk);
        fflush(stdout);
        free(src); free(z); free(back); free(sq); free(lpos); free(lrep); free(blkend); free(blkseq); free(val); free(cls); free(vsub); free(h); free(h2); free(fld); ZSTD_freeCCtx(cc);
    }
    return 0;
}
