/* KLZ and KLZH: Kolmo's own LZ77 coders. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0. Formats in klz.h.
   Written for Kolmo: hash-table and hash-chain match finders with lazy evaluation (the classic LZ77 parse of the
   literature; the skipping rule on misses and the gain rule of lazy matching are the usual heuristics), byte-aligned
   sequences for KLZ, and Kolmo's Huffman coder (khuf) for the KLZH streams. No code from LZ4 or zstd. */
#include "klz.h"
#include "khuf.h"
#include <stdlib.h>
#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "klz: little-endian hosts only (x86-64, AArch64)"
#endif
static inline uint32_t kz_r32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t kz_r64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline void kz_w64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline int kz_hb(uint32_t v) { return 31 - __builtin_clz(v); }   /* v > 0 */
static size_t kz_put_uv(uint8_t *p, uint64_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
static int kz_get_uv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int sh = 0;
    while (*i < n && sh < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << sh; if (!(c & 128)) { *v = r; return 0; } sh += 7; }
    return -1;
}
/* bytes equal from a and b (b < a), a stopping at e */
static inline size_t kz_count(const uint8_t *a, const uint8_t *b, const uint8_t *e) {
    const uint8_t *s = a;
    while (a + 8 <= e) { uint64_t x = kz_r64(a) ^ kz_r64(b); if (x) return (size_t)(a - s) + ((size_t)__builtin_ctzll(x) >> 3); a += 8; b += 8; }
    while (a < e && *a == *b) { a++; b++; }
    return (size_t)(a - s);
}
static inline uint32_t kz_h4(const uint8_t *p, int lg) { return (kz_r32(p) * 2654435761u) >> (32 - lg); }
static inline uint32_t kz_h5(const uint8_t *p, int lg) { return (uint32_t)(((kz_r64(p) << 24) * 889523592379ull) >> (64 - lg)); }

/* match copy: op is the output position, the match starts off bytes back, ml bytes, the output ends at oe */
static inline void kz_match(uint8_t *op, size_t off, size_t ml, uint8_t *oe) {
    const uint8_t *m = op - off; uint8_t *e = op + ml;
    if (off >= 16 && (size_t)(oe - e) >= 16) { while (op < e) { memcpy(op, m, 16); op += 16; m += 16; } return; }
    if (off < 8) {   /* short period: copy one multiple of the period of at least 8 bytes, then 8 bytes at a time */
        size_t per = off * ((8 + off - 1) / off);
        while (op < e && (size_t)(op - (e - ml)) < per) *op++ = *m++;
        m = op - per;
    }
    if ((size_t)(oe - e) >= 8) { while (op < e) { memcpy(op, m, 8); op += 8; m += 8; } return; }
    while (op < e) *op++ = *m++;
}

/* ---------------- the parse ---------------- */
typedef struct { uint32_t ll, ml, off; } kz_seq;
typedef struct { int hlog, clog, depth, lazy, skip, nice; } kz_par;
typedef struct { const uint8_t *in; size_t n; uint32_t *head, *chain; uint32_t cmask, ins; kz_par P; } kz_mf;
#define KZ_NONE 0xFFFFFFFFu

static inline void kz_insert(kz_mf *M, uint32_t p) {
    uint32_t h = kz_h4(M->in + p, M->P.hlog);
    if (M->chain) M->chain[p & M->cmask] = M->head[h];
    M->head[h] = p;
}
static inline void kz_update(kz_mf *M, uint32_t target) {   /* insert every position before target */
    uint32_t e = M->n >= 4 ? (uint32_t)(M->n - 3) : 0; if (target > e) target = e;
    for (uint32_t p = M->ins; p < target; p++) kz_insert(M, p);
    if (target > M->ins) M->ins = target;
}
/* longest match at pos (inserting pos); returns the length (0 if under 4) and sets *off */
static size_t kz_find(kz_mf *M, uint32_t pos, uint32_t *off) {
    const uint8_t *ip = M->in + pos, *ie = M->in + M->n; size_t maxl = M->n - pos, best = 3;
    if (maxl < 4) return 0;
    kz_update(M, pos);
    uint32_t h = kz_h4(ip, M->P.hlog), cand = M->head[h];
    uint32_t win = M->chain ? M->cmask : KLZ_WINDOW; if (win > KLZ_WINDOW) win = KLZ_WINDOW;
    if (M->chain) M->chain[pos & M->cmask] = cand;
    M->head[h] = pos; if (M->ins <= pos) M->ins = pos + 1;
    const uint32_t v = kz_r32(ip);
    for (int d = 0; d < M->P.depth && cand != KZ_NONE && cand < pos && pos - cand <= win; d++) {
        const uint8_t *c = M->in + cand;
        if (c[best] == ip[best] && kz_r32(c) == v) {
            size_t l = 4 + kz_count(ip + 4, c + 4, ie);
            if (l > best) { best = l; *off = pos - cand; if (l >= (size_t)M->P.nice || l >= maxl) break; }
        }
        if (!M->chain) break;
        uint32_t nx = M->chain[cand & M->cmask];
        if (nx >= cand) break;   /* overwritten slot */
        cand = nx;
    }
    return best >= 4 ? best : 0;
}
static inline int kz_gain(size_t l, uint32_t off, uint32_t rep0) { return (int)l * 4 - (off == rep0 ? 0 : kz_hb(off + 1)); }

static size_t kz_parse(const uint8_t *in, size_t n, const kz_par *P, kz_seq *S, size_t *anchor_out, int *err) {
    kz_mf M = {in, n, NULL, NULL, 0, 0, *P}; size_t ns = 0;
    M.head = malloc(sizeof(uint32_t) << P->hlog);
    if (P->clog) { M.chain = malloc(sizeof(uint32_t) << P->clog); M.cmask = (1u << P->clog) - 1; }
    if (!M.head || (P->clog && !M.chain)) { free(M.head); free(M.chain); *err = 1; return 0; }
    memset(M.head, 0xFF, sizeof(uint32_t) << P->hlog);
    size_t pos = 0, anchor = 0; uint32_t rep0 = 0; const uint8_t *ie = in + n; size_t miss = 0;
    while (pos + 8 <= n) {
        uint32_t off = 0, roff = rep0; size_t rl = 0, ml;
        if (rep0 && pos >= rep0 && kz_r32(in + pos) == kz_r32(in + pos - rep0)) rl = 4 + kz_count(in + pos + 4, in + pos + 4 - rep0, ie);
        ml = kz_find(&M, (uint32_t)pos, &off);
        if (rl && (!ml || kz_gain(rl, roff, rep0) >= kz_gain(ml, off, rep0))) { ml = rl; off = roff; }
        if (!ml) {
            miss++; size_t step = P->skip ? 1 + (miss >> P->skip) : 1;
            pos += step; continue;
        }
        miss = 0;
        for (int k = 0; k < P->lazy && pos + 9 <= n; k++) {   /* lazy evaluation: is a match one byte later better? */
            uint32_t o2 = 0; size_t l2 = kz_find(&M, (uint32_t)pos + 1, &o2), r2 = 0;
            if (rep0 && pos + 1 >= rep0 && kz_r32(in + pos + 1) == kz_r32(in + pos + 1 - rep0)) r2 = 4 + kz_count(in + pos + 5, in + pos + 5 - rep0, ie);
            if (r2 && (!l2 || kz_gain(r2, rep0, rep0) >= kz_gain(l2, o2, rep0))) { l2 = r2; o2 = rep0; }
            if (l2 && kz_gain(l2, o2, rep0) > kz_gain(ml, off, rep0) + 4) { pos++; ml = l2; off = o2; } else break;
        }
        while (pos > anchor && pos > off && in[pos - 1] == in[pos - 1 - off]) { pos--; ml++; }   /* extend backwards */
        S[ns].ll = (uint32_t)(pos - anchor); S[ns].ml = (uint32_t)ml; S[ns].off = off; ns++;
        rep0 = off; pos += ml; anchor = pos;
        if (M.chain && P->depth > 1) { if (pos > M.ins) { uint32_t from = (uint32_t)(pos - ml) + 1; if (from > M.ins) M.ins = from; kz_update(&M, (uint32_t)pos); } }
        else if (pos >= 2 && pos + 4 <= n) { kz_insert(&M, (uint32_t)pos - 2); M.ins = (uint32_t)pos; }
    }
    free(M.head); free(M.chain);
    *anchor_out = anchor; *err = 0; return ns;
}
/* the fast parse (KLZH levels 1-3): two hash tables, 8-byte and 5-byte keys, one candidate each, greedy, a repeat
 * check one byte ahead, and a step that grows with the length of the current literal run; only the start and the
 * end of each match are inserted. minm: shortest match taken from the 5-byte table (measured: 6 instead of 4 is
 * 3.2% smaller on the structured set, 0.8% larger on Silesia; 4 is kept, a per-block choice is a tuning item) */
static inline uint32_t kz_h8(const uint8_t *p, int lg) { return (uint32_t)((kz_r64(p) * 0xCF1BBCDCB7A56463ull) >> (64 - lg)); }
static size_t kz_parse_dfast(const uint8_t *in, size_t n, int hl, int hs, int skip, int minm, kz_seq *S, size_t *anchor_out, int *err) {
    uint32_t *tl = calloc((size_t)1 << hl, 4), *ts = calloc((size_t)1 << hs, 4); size_t ns = 0;
    if (!tl || !ts) { free(tl); free(ts); *err = 1; return 0; }
    const uint8_t *ie = in + n; size_t pos = 1, anchor = 0; uint32_t rep0 = 0;
    while (pos + 9 <= n) {
        size_t ml, start; uint32_t off;
        const uint32_t hL = kz_h8(in + pos, hl), hS = kz_h5(in + pos, hs), cl = tl[hL], cs = ts[hS];
        tl[hL] = ts[hS] = (uint32_t)pos;
        if (rep0 && pos + 1 > rep0 && kz_r32(in + pos + 1) == kz_r32(in + pos + 1 - rep0)) {   /* repeat, one byte ahead */
            start = pos + 1; off = rep0; ml = 4 + kz_count(in + start + 4, in + start + 4 - off, ie);
        } else if (cl < pos && pos - cl <= KLZ_WINDOW && kz_r64(in + cl) == kz_r64(in + pos)) {
            start = pos; off = (uint32_t)(pos - cl); ml = 8 + kz_count(in + pos + 8, in + cl + 8, ie);
        } else if (cs < pos && pos - cs <= KLZ_WINDOW && kz_r32(in + cs) == kz_r32(in + pos)) {
            const uint32_t h1 = kz_h8(in + pos + 1, hl), c1 = tl[h1]; tl[h1] = (uint32_t)pos + 1;
            if (c1 < pos + 1 && pos + 1 - c1 <= KLZ_WINDOW && kz_r64(in + c1) == kz_r64(in + pos + 1)) {   /* a long match one byte later */
                start = pos + 1; off = (uint32_t)(pos + 1 - c1); ml = 8 + kz_count(in + start + 8, in + c1 + 8, ie);
            } else {
                start = pos; off = (uint32_t)(pos - cs); ml = 4 + kz_count(in + pos + 4, in + cs + 4, ie);
                if ((int)ml < minm) { pos += 1 + ((pos - anchor) >> skip); continue; }   /* short matches cost more than their literals */
            }
        } else { pos += 1 + ((pos - anchor) >> skip); continue; }
        while (start > anchor && start > off && in[start - 1] == in[start - 1 - off]) { start--; ml++; }
        S[ns].ll = (uint32_t)(start - anchor); S[ns].ml = (uint32_t)ml; S[ns].off = off; ns++;
        rep0 = off; pos = start + ml; anchor = pos;
        if (pos + 9 <= n) {
            size_t a = start + 2; tl[kz_h8(in + a, hl)] = (uint32_t)a; ts[kz_h5(in + a, hs)] = (uint32_t)a;
            tl[kz_h8(in + pos - 2, hl)] = (uint32_t)(pos - 2); ts[kz_h5(in + pos - 1, hs)] = (uint32_t)(pos - 1);
        }
    }
    free(tl); free(ts); *anchor_out = anchor; *err = 0; return ns;
}
static const kz_par KLZ_LV[4] = {{0}, {14, 0, 1, 0, 6, 64}, {16, 0, 1, 0, 7, 64}, {17, 20, 16, 1, 0, 128}};
/* KLZH: levels 1-3 the fast parse (hlog = long table, clog = short table, skip), 4-9 hash chains */
static const kz_par KLZH_LV[10] = {{0}, {16, 15, 0, 0, 6, 0}, {17, 16, 0, 0, 7, 0}, {18, 17, 0, 0, 8, 0}, {17, 18, 4, 0, 0, 64},
    {17, 19, 8, 1, 0, 64}, {18, 20, 16, 1, 0, 128}, {18, 21, 32, 1, 0, 128}, {19, 22, 64, 2, 0, 256}, {20, 23, 256, 2, 0, 512}};

/* ---------------- KLZ: byte-aligned format ---------------- */
size_t kolmo_klz_bound(size_t n) { return n + n / 255 + 32; }
static uint8_t *kz_emit(uint8_t *op, const uint8_t *lit, size_t ll, size_t off, size_t ml) {
    uint8_t *tok = op++; unsigned t = ll < 7 ? (unsigned)ll : 7u;
    if (ll >= 7) { size_t r = ll - 7; while (r >= 255) { *op++ = 255; r -= 255; } *op++ = (uint8_t)r; }
    memcpy(op, lit, ll); op += ll;
    if (ml) {
        size_t m = ml - 4; t |= (unsigned)(m < 15 ? m : 15) << 3;
        *op++ = (uint8_t)off; *op++ = (uint8_t)(off >> 8); if (off > 0xFFFF) { t |= 128; *op++ = (uint8_t)(off >> 16); }
        if (m >= 15) { size_t r = m - 15; while (r >= 255) { *op++ = 255; r -= 255; } *op++ = (uint8_t)r; }
    }
    *tok = (uint8_t)t; return op;
}
#define KZ_ROOM(LL) ((size_t)(oe - op) >= (LL) + (LL) / 255 + 16)   /* room for a sequence with LL literals */
size_t kolmo_klz_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level) {
    if (n >= 0xFFFFFFF0u || cap < 16) return 0;
    uint8_t *const oe = out + cap;
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    uint8_t *op = out;
    if (level >= 3) {   /* chains and lazy evaluation */
        kz_seq *S = malloc((n / 4 + 2) * sizeof *S); if (!S) return 0;
        size_t anchor; int err; size_t ns = kz_parse(in, n, &KLZ_LV[level], S, &anchor, &err);
        if (err) { free(S); return 0; }
        const uint8_t *ip = in;
        for (size_t k = 0; k < ns; k++) { if (!KZ_ROOM(S[k].ll)) { free(S); return 0; } op = kz_emit(op, ip, S[k].ll, S[k].off, S[k].ml); ip += S[k].ll + S[k].ml; }
        if (!KZ_ROOM(n - anchor)) { free(S); return 0; }
        op = kz_emit(op, ip, n - anchor, 0, 0);
        free(S); return (size_t)(op - out);
    }
    const int lg = KLZ_LV[level].hlog, trig = KLZ_LV[level].skip;
    uint32_t *ht = calloc((size_t)1 << lg, sizeof(uint32_t)); if (!ht) return 0;
    size_t ip = 0, anchor = 0; const size_t limit = n > 12 ? n - 12 : 0; const uint8_t *ie = in + n;
    while (ip < limit) {
        size_t cand, att = (size_t)1 << trig;
        for (;;) {   /* look for a 4-byte match; the step grows after every 2^trig misses */
            uint32_t h = kz_h5(in + ip, lg); cand = ht[h]; ht[h] = (uint32_t)ip;
            if (cand < ip && ip - cand <= KLZ_WINDOW && kz_r32(in + cand) == kz_r32(in + ip)) break;
            ip += att++ >> trig;
            if (ip >= limit) goto done;
        }
        while (ip > anchor && cand > 0 && in[ip - 1] == in[cand - 1]) { ip--; cand--; }
        size_t ml = 4 + kz_count(in + ip + 4, in + cand + 4, ie);
        if (!KZ_ROOM(ip - anchor)) { free(ht); return 0; }
        op = kz_emit(op, in + anchor, ip - anchor, ip - cand, ml);
        ip += ml; anchor = ip;
        if (ip < limit) { ht[kz_h5(in + ip - 2, lg)] = (uint32_t)(ip - 2); if (level >= 2) ht[kz_h5(in + ip - 1, lg)] = (uint32_t)(ip - 1); }
    }
done:
    if (!KZ_ROOM(n - anchor)) { free(ht); return 0; }
    op = kz_emit(op, in + anchor, n - anchor, 0, 0);
    free(ht); return (size_t)(op - out);
}
int kolmo_klz_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    const uint8_t *ip = in, *ie = in + cn; uint8_t *op = out, *oe = out + n;
    for (;;) {
        if (ip >= ie) return -1;
        unsigned t = *ip++; size_t ll = t & 7, off, ml;
        if (ie - ip >= 48 && oe - op >= 96) {   /* fast path: whole-register copies, few bounds tests */
            if (ll == 7) { unsigned b; do { if (ip >= ie) return -1; b = *ip++; ll += b; } while (b == 255); }
            if (ll <= 32) { memcpy(op, ip, 16); memcpy(op + 16, ip + 16, 16); }   /* ip + 32 <= ie: at most one extension byte */
            else { if (ll > (size_t)(ie - ip) || ll > (size_t)(oe - op)) return -1; memcpy(op, ip, ll); }
            op += ll; ip += ll;
            if (op == oe) return (t >> 3) == 0 && ip == ie ? 0 : -1;
            if (ie - ip < 3) return -1;
            off = (size_t)ip[0] | (size_t)ip[1] << 8 | ((size_t)ip[2] << 16 & (size_t)(0 - (t >> 7)));
            ip += 2 + (t >> 7); ml = (t >> 3) & 15;
            if (ml < 15 && off >= 8 && off <= (size_t)(op - out) && oe - op >= 32) {   /* short match: 3 x 8 bytes (period >= 8) */
                const uint8_t *m = op - off; memcpy(op, m, 8); memcpy(op + 8, m + 8, 8); memcpy(op + 16, m + 16, 8); op += ml + 4; continue;
            }
        } else {
            if (ll == 7) { unsigned b; do { if (ip >= ie) return -1; b = *ip++; ll += b; } while (b == 255); }
            if (ll > (size_t)(ie - ip) || ll > (size_t)(oe - op)) return -1;
            if (ll <= 16 && ie - ip >= 16 && oe - op >= 16) memcpy(op, ip, 16); else memcpy(op, ip, ll);
            op += ll; ip += ll;
            if (op == oe) return (t >> 3) == 0 && ip == ie ? 0 : -1;
            if (ie - ip < 2 + (t >> 7)) return -1;
            off = (size_t)ip[0] | (size_t)ip[1] << 8; ip += 2;
            if (t & 128) off |= (size_t)*ip++ << 16;
            ml = (t >> 3) & 15;
        }
        if (ml == 15) { unsigned b; do { if (ip >= ie) return -1; b = *ip++; ml += b; } while (b == 255); }
        ml += 4;
        if (!off || off > (size_t)(op - out) || ml > (size_t)(oe - op)) return -1;
        kz_match(op, off, ml, oe); op += ml;
    }
}

/* ---------------- KLZH: entropy-coded streams ---------------- */
static inline unsigned kz_lcode(uint32_t v, uint32_t *ex, int *nb) {
    if (v < 16) { *nb = 0; *ex = 0; return v; }
    int h = kz_hb(v); *nb = h - 1; *ex = v & ((1u << (h - 1)) - 1);
    return 16u + 2u * (unsigned)(h - 4) + ((v >> (h - 1)) & 1u);
}
typedef struct { uint64_t b; int c; uint8_t *p, *e; } kz_bw;
static inline int kz_bput(kz_bw *w, uint32_t v, int nb) {   /* nb <= 30 */
    w->b |= (uint64_t)v << w->c; w->c += nb;
    if (w->c >= 32) { if (w->p + 8 > w->e) return -1; kz_w64(w->p, w->b); w->p += w->c >> 3; w->b >>= w->c & ~7; w->c &= 7; }
    return 0;
}
size_t kolmo_klzh_bound(size_t n) { return n + n / 64 + 1024; }
size_t kolmo_klzh_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level) {
    if (n < 16 || n >= 0xFFFFFFF0u || cap < 64) return 0;
    if (level < 1) level = 1;
    if (level > 9) level = 9;
    kz_seq *S = malloc((n / 4 + 2) * sizeof *S); size_t anchor; int err;
    if (!S) return 0;
    const kz_par *LV = &KLZH_LV[level];
    size_t ns = level <= 3 ? kz_parse_dfast(in, n, LV->hlog, LV->clog, LV->skip, 4, S, &anchor, &err) : kz_parse(in, n, LV, S, &anchor, &err);
    uint8_t *lit = malloc(n + 16), *cd = malloc(3 * (ns + 1)), *xb = malloc(ns * 12 + 64), *tmp = malloc(kolmo_khuf_bound(n) + 64);
    size_t r = 0;
    if (err || !lit || !cd || !xb || !tmp) goto out;
    uint8_t *llc = cd, *mlc = cd + ns, *ofc = cd + 2 * ns; size_t nl = 0; const uint8_t *ip = in;
    uint32_t rep[3] = {1, 4, 8}; kz_bw w = {0, 0, xb, xb + ns * 12 + 64};
    for (size_t k = 0; k < ns; k++) {
        uint32_t ex; int nb;
        memcpy(lit + nl, ip, S[k].ll); nl += S[k].ll; ip += S[k].ll + S[k].ml;
        llc[k] = (uint8_t)kz_lcode(S[k].ll, &ex, &nb); if (kz_bput(&w, ex, nb)) goto out;
        mlc[k] = (uint8_t)kz_lcode(S[k].ml - 4, &ex, &nb); if (kz_bput(&w, ex, nb)) goto out;
        uint32_t o = S[k].off;
        if (o == rep[0]) ofc[k] = 0;
        else if (o == rep[1]) { ofc[k] = 1; rep[1] = rep[0]; rep[0] = o; }
        else if (o == rep[2]) { ofc[k] = 2; rep[2] = rep[1]; rep[1] = rep[0]; rep[0] = o; }
        else { int h = kz_hb(o); ofc[k] = (uint8_t)(3 + h); if (kz_bput(&w, o - (1u << h), h)) goto out; rep[2] = rep[1]; rep[1] = rep[0]; rep[0] = o; }
    }
    memcpy(lit + nl, ip, n - anchor); nl += n - anchor;
    if (w.p + 8 > w.e) goto out;
    kz_w64(w.p, w.b); w.p += (w.c + 7) >> 3;
    size_t o = kz_put_uv(out, ns); o += kz_put_uv(out + o, nl);
    const uint8_t *st[4] = {lit, llc, mlc, ofc}; size_t sn[4] = {nl, ns, ns, ns};
    for (int q = 0; q < 4; q++) {
        size_t z = sn[q] ? kolmo_khuf_compress_chunks(st[q], sn[q], tmp, kolmo_khuf_bound(n) + 64) : 0;
        if (sn[q] && !z) goto out;
        if (cap - o < z + 10) goto out;
        o += kz_put_uv(out + o, z); memcpy(out + o, tmp, z); o += z;
    }
    size_t xl = (size_t)(w.p - xb);
    if (cap - o < xl || o + xl >= n) goto out;
    memcpy(out + o, xb, xl); r = o + xl;
out:
    free(S); free(lit); free(cd); free(xb); free(tmp); return r;
}

typedef struct { uint64_t b; int c; const uint8_t *p, *e; } kz_br;
static inline void kz_refill(kz_br *r) {
    if (r->p + 8 <= r->e) { r->b |= kz_r64(r->p) << r->c; r->p += (63 - r->c) >> 3; r->c |= 56; }
    else while (r->c <= 56) { uint64_t v = r->p < r->e ? *r->p : 0; r->b |= v << r->c; r->p++; r->c += 8; }
}
static inline uint32_t kz_bget(kz_br *r, int nb) { uint32_t v = (uint32_t)(r->b & ((1ull << nb) - 1)); r->b >>= nb; r->c -= nb; return v; }
/* length code c -> base value and extra-bit count (codes 0..71; see klz.h) */
static void kz_ltab(uint32_t base[72], uint8_t nb[72]) {
    for (unsigned c = 0; c < 72; c++) {
        if (c < 16) { base[c] = c; nb[c] = 0; continue; }
        int h = 4 + (int)(c - 16) / 2; base[c] = (1u << h) | ((c & 1u) << (h - 1)); nb[c] = (uint8_t)(h - 1);
    }
}
int kolmo_klzh_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0; uint64_t ns, nl;
    if (kz_get_uv(in, cn, &i, &ns) || kz_get_uv(in, cn, &i, &nl) || ns > n / 4 + 1 || nl > n) return -1;
    uint8_t *lit = malloc(nl + 32), *cd = malloc(3 * ns + 1); int rc = -1;
    if (!lit || !cd) goto out;
    uint8_t *st[4] = {lit, cd, cd + ns, cd + 2 * ns}; size_t sn[4] = {nl, ns, ns, ns};
    for (int q = 0; q < 4; q++) {
        uint64_t z; if (kz_get_uv(in, cn, &i, &z) || z > cn - i) goto out;
        if (sn[q] ? kolmo_khuf_decompress_chunks(in + i, z, st[q], sn[q]) : z != 0) goto out;
        i += z;
    }
    kz_br br = {0, 0, in + i, in + cn};
    const uint8_t *lp = lit, *le = lit + nl, *llc = st[1], *mlc = st[2], *ofc = st[3];
    uint32_t LB[72]; uint8_t LN[72]; kz_ltab(LB, LN);
    { unsigned ml = 0, mo = 0;                                  /* codes in range: one pass, maxima only (decmap lane) */
      for (size_t k = 0; k < ns; k++) { unsigned a = llc[k] > mlc[k] ? llc[k] : mlc[k]; ml = a > ml ? a : ml; mo = ofc[k] > mo ? ofc[k] : mo; }
      if (ml >= 72 || mo > 26) goto out; }
    uint8_t *op = out, *oe = out + n; uint32_t rep[3] = {1, 4, 8};
    for (size_t k = 0; k < ns; k++) {
        uint32_t ll, ml, off; unsigned oc = ofc[k], a = llc[k], b = mlc[k];
        /* after a refill at least 56 bits are buffered; the next refill only when the extra bits left need it */
        kz_refill(&br); ll = LB[a] + kz_bget(&br, LN[a]);
        if (br.c < LN[b]) kz_refill(&br);
        ml = LB[b] + kz_bget(&br, LN[b]) + 4;
        if (oc >= 3) { int h = (int)oc - 3; if (br.c < h) kz_refill(&br); off = (1u << h) | kz_bget(&br, h); rep[2] = rep[1]; rep[1] = rep[0]; rep[0] = off; }
        else if (oc == 0) off = rep[0];
        else if (oc == 1) { off = rep[1]; rep[1] = rep[0]; rep[0] = off; }
        else { off = rep[2]; rep[2] = rep[1]; rep[1] = rep[0]; rep[0] = off; }
        if (ll > (size_t)(le - lp) || ll > (size_t)(oe - op)) goto out;
        if (ll <= 16 && oe - op >= 16) memcpy(op, lp, 16); else memcpy(op, lp, ll);   /* lit has 32 bytes of slack */
        op += ll; lp += ll;
        if (off > (size_t)(op - out) || ml > (size_t)(oe - op)) goto out;
        if (off >= 16 && ml <= 32 && oe - op >= 32 + 16) { memcpy(op, op - off, 16); memcpy(op + 16, op - off + 16, 16); }
        else kz_match(op, off, ml, oe);
        op += ml;
    }
    if ((size_t)(le - lp) != (size_t)(oe - op)) goto out;
    memcpy(op, lp, (size_t)(le - lp));
    if ((size_t)(br.p - in) - (size_t)(br.c >> 3) > cn) goto out;
    rc = 0;
out:
    free(lit); free(cd); return rc;
}
