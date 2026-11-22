/* KLZ and KLZH: Kolmo's own LZ77 coders. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0. Formats in klz.h.
   Written for Kolmo: hash-table and hash-chain match finders with lazy evaluation (the classic LZ77 parse of the
   literature; the skipping rule on misses and the gain rule of lazy matching are the usual heuristics), byte-aligned
   sequences for KLZ, and Kolmo's Huffman coder (khuf) for the KLZH streams. No code from LZ4 or zstd. */
#include "klz.h"
#include "krans.h"
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
typedef struct { const uint8_t *in; size_t n; uint32_t *head, *chain; uint32_t cmask, ins; kz_par P; int h5; } kz_mf;   /* h5: 5-byte hash keys */
#define KZ_HASH(M, p) ((M)->h5 ? kz_h5(p, (M)->P.hlog) : kz_h4(p, (M)->P.hlog))
#define KZ_NONE 0xFFFFFFFFu

static inline void kz_insert(kz_mf *M, uint32_t p) {
    uint32_t h = KZ_HASH(M, M->in + p);
    if (M->chain) M->chain[p & M->cmask] = M->head[h];
    M->head[h] = p;
}
static inline void kz_update(kz_mf *M, uint32_t target) {   /* insert every position before target */
    uint32_t e = M->h5 ? (M->n >= 8 ? (uint32_t)(M->n - 7) : 0) : M->n >= 4 ? (uint32_t)(M->n - 3) : 0; if (target > e) target = e;
    for (uint32_t p = M->ins; p < target; p++) kz_insert(M, p);
    if (target > M->ins) M->ins = target;
}
/* longest match at pos (inserting pos); returns the length (0 if under 4) and sets *off */
static size_t kz_find(kz_mf *M, uint32_t pos, uint32_t *off) {
    const uint8_t *ip = M->in + pos, *ie = M->in + M->n; size_t maxl = M->n - pos, best = 3;
    if (maxl < 4) return 0;
    kz_update(M, pos);
    uint32_t h = KZ_HASH(M, ip), cand = M->head[h];
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
static inline void kz_mtf(uint32_t r[3], uint32_t off) {   /* the KLZH repeat offsets' move to front */
    if (off == r[0]) return;
    if (off != r[1]) r[2] = r[1];
    r[1] = r[0]; r[0] = off;
}
/* KLZH2 level 3: the fast parse with the decoder's three repeat offsets tried one byte ahead, and both tables of 2^20
 * entries (fewer for short blocks). Measured on the screening binaries at 4 MiB blocks against kz_parse_dfast (tables
 * 2^18 / 2^17, skip 8): three repeats -0.1%, 2^20 tables -2.1% (x-ray -11%: 2^17 short slots for 4 MiB of 16-bit
 * samples overwrite each other), the step after every 2^9 literals instead of 2^8 a further -1.3% on x-ray; about 10%
 * slower. A 3-byte match at rep0 where nothing else is found: ooffice -0.5%, x-ray +0.1%, no slower. */
static size_t kz_parse_dfast3(const uint8_t *in, size_t n, int lg, int skip, kz_seq *S, size_t *anchor_out, int *err) {
    uint32_t *tl = calloc((size_t)1 << lg, 4), *ts = calloc((size_t)1 << lg, 4); size_t ns = 0;
    if (!tl || !ts) { free(tl); free(ts); *err = 1; return 0; }
    const uint8_t *ie = in + n; size_t pos = 1, anchor = 0; uint32_t rep[3] = {1, 4, 8};
    while (pos + 9 <= n) {
        size_t ml = 0, start = pos + 1; uint32_t off = 0;
        const uint32_t hL = kz_h8(in + pos, lg), hS = kz_h5(in + pos, lg), cl = tl[hL], cs = ts[hS];
        tl[hL] = ts[hS] = (uint32_t)pos;
        const uint32_t v1 = kz_r32(in + pos + 1);
        for (int k = 0; k < 3; k++) {   /* repeats, one byte ahead; a later one is taken when 2 bytes longer */
            const uint32_t r = rep[k];
            if (pos + 1 >= r && kz_r32(in + pos + 1 - r) == v1) {
                size_t l = 4 + kz_count(in + pos + 5, in + pos + 5 - r, ie);
                if (l > ml + 1) { ml = l; off = r; }
            }
        }
        if (ml) {
        } else if (cl < pos && pos - cl <= KLZ_WINDOW && kz_r64(in + cl) == kz_r64(in + pos)) {
            start = pos; off = (uint32_t)(pos - cl); ml = 8 + kz_count(in + pos + 8, in + cl + 8, ie);
        } else if (cs < pos && pos - cs <= KLZ_WINDOW && kz_r32(in + cs) == kz_r32(in + pos)) {
            const uint32_t h1 = kz_h8(in + pos + 1, lg), c1 = tl[h1]; tl[h1] = (uint32_t)pos + 1;
            if (c1 < pos + 1 && pos + 1 - c1 <= KLZ_WINDOW && kz_r64(in + c1) == kz_r64(in + pos + 1)) {   /* a long match one byte later */
                off = (uint32_t)(pos + 1 - c1); ml = 8 + kz_count(in + start + 8, in + c1 + 8, ie);
            } else { start = pos; off = (uint32_t)(pos - cs); ml = 4 + kz_count(in + pos + 4, in + cs + 4, ie); }
        } else if (pos + 1 >= rep[0] && ((kz_r32(in + pos + 1 - rep[0]) ^ v1) & 0xFFFFFFu) == 0) {   /* 3 bytes of rep0 */
            ml = 3; off = rep[0];
        } else { pos += 1 + ((pos - anchor) >> skip); continue; }
        while (start > anchor && start > off && in[start - 1] == in[start - 1 - off]) { start--; ml++; }
        S[ns].ll = (uint32_t)(start - anchor); S[ns].ml = (uint32_t)ml; S[ns].off = off; ns++;
        kz_mtf(rep, off); pos = start + ml; anchor = pos;
        if (pos + 9 <= n) {
            size_t a = start + 2; tl[kz_h8(in + a, lg)] = (uint32_t)a; ts[kz_h5(in + a, lg)] = (uint32_t)a;
            tl[kz_h8(in + pos - 2, lg)] = (uint32_t)(pos - 2); ts[kz_h5(in + pos - 1, lg)] = (uint32_t)(pos - 1);
        }
    }
    free(tl); free(ts); *anchor_out = anchor; *err = 0; return ns;
}
static const kz_par KLZ_LV[4] = {{0}, {14, 0, 1, 0, 6, 64}, {16, 0, 1, 0, 7, 64}, {17, 20, 16, 1, 0, 128}};
/* KLZH: levels 1-3 the fast parse (hlog = long table, clog = short table, skip), 4-9 hash chains */
static const kz_par KLZH_LV[10] = {{0}, {16, 15, 0, 0, 6, 0}, {17, 16, 0, 0, 7, 0}, {18, 17, 0, 0, 8, 0}, {17, 18, 4, 0, 0, 64},
    {18, 22, 8, 2, 0, 32}, {18, 22, 16, 2, 0, 64}, {18, 21, 32, 1, 0, 128}, {19, 22, 64, 2, 0, 256}, {20, 23, 256, 2, 0, 512}};

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
/* ---------------- the optimal parse (KLZH levels 7-9) ----------------
 * Forward dynamic programming over windows of up to KO_OPT positions, in the manner of the price-based parsers of the
 * literature (and of km_optimal in klzma.c): every position gets the matches of a binary-tree finder (one per length
 * that improves on the last) and the three repeat offsets of its own best path; every length of every candidate is
 * priced. Prices (1/256 bit) come from adaptive statistics of the literals and of the three code streams, rescaled
 * every 128 KiB of input (the khuf tables are per 128 KiB chunk) and refreshed as sequences are chosen. A node's price
 * includes the price of its pending literal length. A candidate of the nice length or longer is taken at once. */
#define KO_OPT 4096
#define KO_INF 0x3FFFFFFFu
#define KO_LLT 1024
#define KO_MLT 1024
#define KO_SPAN (128u << 10)
typedef struct { uint32_t len, off; } ko_m;
typedef struct {
    const uint8_t *in; uint32_t n, *head, *son, bmask, ins, nice; int hlog, depth;
    uint32_t *h3, *ldm, wmax; int ldmlog;   /* KLZH2: 3-byte heads, long-distance table (offsets up to wmax), or NULL */
} ko_mf;
/* skip: a position whose next node is already within skip of its price is not searched; KLZH2 only: wlog, the largest
 * offset 2^wlog - 1 (long-distance candidates past the tree's window), m3: 3-byte matches */
typedef struct { int hlog, bwlog, depth, nice, skip, wlog, m3; } ko_par;
typedef struct { uint32_t price, mlen, off, litlen, rep[3]; } ko_node;
typedef struct {
    uint32_t lit[256], ll[72], ml[72], of[33], sl, sll, sml, sof, pending;
    uint32_t pl[256], pll[72], pml[72], pof[33], llp[KO_LLT], mlp[KO_MLT];
    uint32_t mmin; int nof;   /* shortest match (4 KLZH, 3 KLZH2) and offset codes (32, 33) */
} ko_st;
#define KO_H4(p, lg) ((kz_r32(p) * 2654435761u) >> (32 - (lg)))
/* binary tree per 4-byte hash bucket over a cyclic window of bmask + 1 positions (as km_bt); inserts p, and when
 * collect is set reports in out every match longer than the last (from 4 bytes) */
static uint32_t ko_bt(ko_mf *M, uint32_t p, ko_m *out, int collect) {
    const uint8_t *s = M->in + p; uint32_t lim = M->n - p; if (lim > M->nice) lim = M->nice;
    uint32_t h = KO_H4(s, M->hlog), cur = M->head[h], low = p > M->bmask ? p - M->bmask : 0, best = 3, nm = 0;
    M->head[h] = p;
    uint32_t *lt = &M->son[2 * (size_t)(p & M->bmask)], *gt = lt + 1, l0 = 0, l1 = 0;
    for (int d = M->depth;; d--) {
        if (cur == KZ_NONE || cur < low || d == 0) { *lt = *gt = KZ_NONE; break; }
        const uint8_t *q = M->in + cur; uint32_t l = l0 < l1 ? l0 : l1, *cs = &M->son[2 * (size_t)(cur & M->bmask)];
        if (q[l] == s[l]) {
            l += 1 + (uint32_t)kz_count(s + l + 1, q + l + 1, s + lim);
            if (collect && l > best) { best = l; out[nm].len = l; out[nm].off = p - cur; nm++; }
            if (l >= lim) { *lt = cs[0]; *gt = cs[1]; break; }
        }
        if (q[l] < s[l]) { *lt = cur; lt = cs + 1; cur = *lt; l0 = l; }
        else { *gt = cur; gt = cs; cur = *gt; l1 = l; }
    }
    if (nm && out[nm - 1].len == lim && lim < M->n - p) { ko_m *t = &out[nm - 1]; t->len = lim + (uint32_t)kz_count(s + lim, s + lim - t->off, M->in + M->n); }
    return nm;
}
#define KO_H3(p) (((kz_r32(p) & 0xFFFFFFu) * 2654435761u) >> 18)
#define KO_LDMIN 32   /* long-distance candidates: 32-byte keys, one position in 8 by content */
static inline uint32_t ko_ldh(const uint8_t *p, int lg) {
    uint64_t h = kz_r64(p) * 0x9E3779B97F4A7C15ull ^ kz_r64(p + 8) * 0xC2B2AE3D27D4EB4Full ^ kz_r64(p + 16) * 0x165667B19E3779F9ull ^ kz_r64(p + 24) * 0xD6E8FEB86659FD93ull;
    return (uint32_t)(h >> (64 - lg - 3));   /* the low 3 bits select the positions kept */
}
/* KLZH2 extra candidates at p: a 3-byte match (from the latest position with the same 3 bytes, offsets below 2^14:
 * farther ones cost more than they save, and the parse took them on text) when it is shorter than the tree's first, and a long-distance match past the tree's window when it is longer than the tree's last */
static uint32_t ko_aux(ko_mf *M, uint32_t p, ko_m *out, uint32_t nm, int collect) {
    const uint8_t *s = M->in + p, *ie = M->in + M->n;
    if (M->h3) {
        uint32_t h = KO_H3(s), c = M->h3[h]; M->h3[h] = p;
        if (collect && c != KZ_NONE && p - c <= M->bmask && p - c < (1u << 14) && (kz_r32(M->in + c) & 0xFFFFFFu) == (kz_r32(s) & 0xFFFFFFu)) {
            uint32_t l = 3 + (uint32_t)kz_count(s + 3, M->in + c + 3, s + (M->n - p < M->nice ? M->n - p : M->nice));
            if (!nm || l < out[0].len) { memmove(out + 1, out, nm * sizeof *out); out[0].len = l; out[0].off = p - c; nm++; }
        }
    }
    if (M->ldm && p + KO_LDMIN <= M->n) {
        uint32_t h = ko_ldh(s, M->ldmlog);
        if (!(h & 7)) {
            h >>= 3; uint32_t c = M->ldm[h]; M->ldm[h] = p;
            if (collect && c != KZ_NONE && p - c > M->bmask && p - c <= M->wmax) {
                uint32_t l = (uint32_t)kz_count(s, M->in + c, ie);
                if (l >= KO_LDMIN && (!nm || l > out[nm - 1].len)) { out[nm].len = l; out[nm].off = p - c; nm++; }
            }
        }
    }
    return nm;
}
static uint32_t ko_find(ko_mf *M, uint32_t p, ko_m *out) {   /* p + 8 <= n, positions searched in increasing order */
    for (uint32_t q = M->ins; q < p; q++) { ko_bt(M, q, NULL, 0); if (M->h3 || M->ldm) ko_aux(M, q, out, 0, 0); }
    M->ins = p + 1;
    uint32_t nm = ko_bt(M, p, out, 1);
    return M->h3 || M->ldm ? ko_aux(M, p, out, nm, 1) : nm;
}
static inline uint32_t ko_w(uint32_t x) { x++; int h = kz_hb(x); return (uint32_t)h * 256 + (uint32_t)(((uint64_t)x << 8) >> h); }   /* ~256 (log2(x + 1) + 1) */
static inline uint32_t ko_cost(uint32_t f, uint32_t ws) { uint32_t c = ws - ko_w(f); return c < 256 ? 256 : c > 11 * 256 ? 11 * 256 : c; }
static void ko_refresh(ko_st *T) {
    uint32_t w = ko_w(T->sl); for (int s = 0; s < 256; s++) T->pl[s] = ko_cost(T->lit[s], w);
    if (T->mmin == 3) for (int s = 0; s < 256; s++) if (T->pl[s] < 2 * 256) T->pl[s] = 2 * 256;   /* KLZH2: see ko_init */
    w = ko_w(T->sll); for (int c = 0; c < 72; c++) T->pll[c] = ko_cost(T->ll[c], w);
    w = ko_w(T->sml); for (int c = 0; c < 72; c++) T->pml[c] = ko_cost(T->ml[c], w);
    w = ko_w(T->sof); for (int c = 0; c < T->nof; c++) T->pof[c] = ko_cost(T->of[c], w) + (c >= 3 ? (uint32_t)(c - 3) * 256 : 0);
    for (uint32_t v = 0; v < KO_LLT; v++) { uint32_t ex; int nb; unsigned c = kz_lcode(v, &ex, &nb); T->llp[v] = T->pll[c] + (uint32_t)nb * 256; }
    for (uint32_t v = 0; v < KO_MLT; v++) { uint32_t ex; int nb; unsigned c = kz_lcode(v, &ex, &nb); T->mlp[v] = T->pml[c] + (uint32_t)nb * 256; }
    T->pending = 0;
}
static uint32_t ko_scale(uint32_t *f, int k, int lg) {   /* f = 1 + f >> shift, the sum brought near 2^lg */
    uint32_t s = 0; for (int i = 0; i < k; i++) s += f[i];
    int sh = kz_hb(s + 1) - lg; if (sh < 0) sh = 0;
    s = 0; for (int i = 0; i < k; i++) { f[i] = 1 + (f[i] >> sh); s += f[i]; }
    return s;
}
static void ko_rescale(ko_st *T) {
    T->sl = ko_scale(T->lit, 256, 12); T->sll = ko_scale(T->ll, 72, 11); T->sml = ko_scale(T->ml, 72, 11); T->sof = ko_scale(T->of, T->nof, 11);
    ko_refresh(T);
}
/* KLZH2 prices a literal at 2 bits or more: the first statistics are those of the raw bytes, so a byte that later
 * matches mostly (the zero bytes of 64-bit integers) starts at under a bit, the parse leaves it to the literals and
 * its statistics stay low (population.i64: 70.3 KB at level 9 against 63.7 KB at level 3; 60.0 KB with the floor) */
static void ko_init(ko_st *T, const uint8_t *in, size_t n, int v2) {
    T->mmin = v2 ? 3 : 4; T->nof = v2 ? 33 : 32;
    static const uint8_t LL0[16] = {4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, ML0[16] = {6, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
        OF0[27] = {6, 2, 1, 1, 1, 1, 2, 3, 4, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1};
    uint32_t c[256] = {0}; size_t m = n < KO_SPAN ? n : KO_SPAN;
    for (size_t i = 0; i < m; i++) c[in[i]]++;
    for (int s = 0; s < 256; s++) T->lit[s] = 1 + (c[s] >> 4);
    for (int k = 0; k < 72; k++) { T->ll[k] = k < 16 ? LL0[k] : 1; T->ml[k] = k < 16 ? ML0[k] : 1; }
    for (int k = 0; k < T->nof; k++) T->of[k] = k < 27 ? OF0[k] : 1;
    T->sl = T->sll = T->sml = T->sof = 0;
    for (int s = 0; s < 256; s++) T->sl += T->lit[s];
    for (int k = 0; k < 72; k++) { T->sll += T->ll[k]; T->sml += T->ml[k]; }
    for (int k = 0; k < T->nof; k++) T->sof += T->of[k];
    ko_refresh(T);
}
static inline uint32_t ko_llp(const ko_st *T, uint32_t v) { if (v < KO_LLT) return T->llp[v]; uint32_t ex; int nb; unsigned c = kz_lcode(v, &ex, &nb); return T->pll[c] + (uint32_t)nb * 256; }
static inline uint32_t ko_mlp(const ko_st *T, uint32_t ml) { uint32_t v = ml - T->mmin; if (v < KO_MLT) return T->mlp[v]; uint32_t ex; int nb; unsigned c = kz_lcode(v, &ex, &nb); return T->pml[c] + (uint32_t)nb * 256; }
static inline unsigned ko_ofc(uint32_t off, const uint32_t r[3]) { return off == r[0] ? 0u : off == r[1] ? 1u : off == r[2] ? 2u : 3u + (unsigned)kz_hb(off); }
static inline void ko_rep(uint32_t r[3], uint32_t off) {   /* the encoder's move to front */
    if (off == r[0]) return;
    if (off != r[1]) r[2] = r[1];
    r[1] = r[0]; r[0] = off;
}
/* ---------------- the lazy parse (KLZH levels 5-6) ----------------
 * Hash chains as kz_parse, with the encoder's three repeat offsets tried at every position, a minimum length for new
 * offsets, and lazy evaluation of the next one or two positions. Gains in quarter bits per byte of length less the
 * approximate bits of the offset (a repeat costs a code, a new offset a code and its extra bits). */
static size_t kz_best(kz_mf *M, size_t pos, const uint32_t r[3], size_t minm, uint32_t *off, int *gain) {
    const uint8_t *in = M->in, *ie = in + M->n; size_t best = 0; int g = -(1 << 30);
    const uint32_t v = kz_r32(in + pos);
    for (int k = 0; k < 3; k++) {
        const uint32_t rk = r[k];
        if (pos >= rk && kz_r32(in + pos - rk) == v) {
            size_t l = 4 + kz_count(in + pos + 4, in + pos + 4 - rk, ie); int gk = (int)l * 4 - k - (k ? 1 : 0);
            if (gk > g) { g = gk; best = l; *off = rk; }
        }
    }
    uint32_t o = 0; size_t l = kz_find(M, (uint32_t)pos, &o);
    if (l >= minm) { int gm = (int)l * 4 - kz_hb(o + 1) - 2; if (gm > g) { g = gm; best = l; *off = o; } }
    *gain = g; return best;
}
static size_t kz_parse_lazy(const uint8_t *in, size_t n, const kz_par *P, size_t minm, kz_seq *S, size_t *anchor_out, int *err) {
    kz_mf M = {in, n, NULL, NULL, 0, 0, *P, minm >= 5}; size_t ns = 0;
    M.head = malloc(sizeof(uint32_t) << P->hlog); M.chain = malloc(sizeof(uint32_t) << P->clog); M.cmask = (1u << P->clog) - 1;
    if (!M.head || !M.chain) { free(M.head); free(M.chain); *err = 1; return 0; }
    memset(M.head, 0xFF, sizeof(uint32_t) << P->hlog);
    size_t pos = 0, anchor = 0; uint32_t rep[3] = {1, 4, 8};
    while (pos + 8 <= n) {
        uint32_t off = 0; int g; size_t ml = kz_best(&M, pos, rep, minm, &off, &g);
        if (!ml) { pos++; continue; }
        for (int k = 0; k < P->lazy && pos + 9 <= n; k++) {   /* is a match one byte later better? */
            uint32_t o2 = 0; int g2; size_t l2 = kz_best(&M, pos + 1, rep, minm, &o2, &g2);
            if (l2 && g2 > g + 1 + 3 * k) { pos++; ml = l2; off = o2; g = g2; } else break;
        }
        while (pos > anchor && pos > off && in[pos - 1] == in[pos - 1 - off]) { pos--; ml++; }   /* extend backwards */
        S[ns].ll = (uint32_t)(pos - anchor); S[ns].ml = (uint32_t)ml; S[ns].off = off; ns++;
        ko_rep(rep, off); pos += ml; anchor = pos;
        if (pos > M.ins) { uint32_t from = (uint32_t)(pos - ml) + 1; if (from > M.ins) M.ins = from; kz_update(&M, (uint32_t)pos); }
    }
    free(M.head); free(M.chain);
    *anchor_out = anchor; *err = 0; return ns;
}
/* the minimum length of a new offset for the lazy parse, chosen per block: four 32 KiB slices are parsed with 5, 6 and 7
 * and their streams' entropy estimated (tables favour 6-7; 4 is not tried: it would need 4-byte hash keys, and with
 * them the estimate chose it where it lost) */
static uint64_t kz_hcost(const uint32_t *f, int k) {
    uint32_t s = 0; for (int i = 0; i < k; i++) s += f[i];
    if (!s) return 0;
    uint64_t c = 0; const uint32_t ws = ko_w(s);
    for (int i = 0; i < k; i++) if (f[i]) c += (uint64_t)f[i] * (ws - ko_w(f[i])) + 4 * 256;   /* and about 4 bits of table */
    return c;
}
static uint64_t kz_est(const uint8_t *in, size_t n, const kz_seq *S, size_t ns, size_t anchor) {
    uint32_t lit[256] = {0}, ll[72] = {0}, ml[72] = {0}, of[32] = {0}, r[3] = {1, 4, 8}, ex; uint64_t xb = 0; size_t p = 0; int nb;
    for (size_t k = 0; k < ns; k++) {
        for (uint32_t i = 0; i < S[k].ll; i++) lit[in[p + i]]++;
        ll[kz_lcode(S[k].ll, &ex, &nb)]++; xb += (uint64_t)nb;
        ml[kz_lcode(S[k].ml - 4, &ex, &nb)]++; xb += (uint64_t)nb;
        unsigned c = ko_ofc(S[k].off, r); of[c]++; if (c >= 3) xb += c - 3;
        ko_rep(r, S[k].off); p += S[k].ll + S[k].ml;
    }
    for (size_t i = anchor; i < n; i++) lit[in[i]]++;
    return kz_hcost(lit, 256) + kz_hcost(ll, 72) + kz_hcost(ml, 72) + kz_hcost(of, 32) + xb * 256;
}
static size_t kz_minm(const uint8_t *in, size_t n, const kz_par *P, kz_seq *S) {
    const size_t sl = 32u << 10;
    if (n < 4 * sl) return 5;
    kz_par Q = *P; Q.hlog = 15; Q.clog = 15;
    uint64_t best = UINT64_MAX; size_t bm = 5;
    for (size_t m = 5; m <= 7; m++) {
        uint64_t t = 0;
        for (int q = 0; q < 4; q++) {
            const uint8_t *a = in + (size_t)q * (n - sl) / 3; size_t anc; int err;
            size_t ns = kz_parse_lazy(a, sl, &Q, m, S, &anc, &err);
            if (err) return 5;
            t += kz_est(a, sl, S, ns, anc);
        }
        if (t < best) { best = t; bm = m; }
    }
    return bm;
}
/* one sequence: literals in[anchor..start), then ml bytes at off; statistics and repeat offsets follow */
static void ko_emit(ko_st *T, const uint8_t *in, kz_seq *S, size_t *ns, size_t anchor, size_t start, uint32_t ml, uint32_t off, uint32_t r[3]) {
    uint32_t ll = (uint32_t)(start - anchor), ex; int nb;
    S[*ns].ll = ll; S[*ns].ml = ml; S[*ns].off = off; (*ns)++;
    for (size_t k = anchor; k < start; k++) T->lit[in[k]] += 2;
    T->sl += 2 * ll;
    T->ll[kz_lcode(ll, &ex, &nb)]++; T->sll++;
    T->ml[kz_lcode(ml - T->mmin, &ex, &nb)]++; T->sml++;
    T->of[ko_ofc(off, r)]++; T->sof++;
    ko_rep(r, off); T->pending++;
}
static size_t ko_parse_pass(const uint8_t *in, size_t n, const ko_par *P, ko_st *T, kz_seq *S, size_t *anchor_out, int *err) {
    const uint32_t nice = (uint32_t)P->nice; int bw = 4; while (bw < P->bwlog && ((size_t)1 << bw) < n) bw++;
    ko_mf M = {in, (uint32_t)n, NULL, NULL, (1u << bw) - 1, 0, nice, P->hlog, P->depth, NULL, NULL, 0, 0};
    M.head = malloc(sizeof(uint32_t) << P->hlog); M.son = malloc(sizeof(uint32_t) * 2 << bw);
    const int v2 = T->mmin == 3;
    if (v2 && P->m3) { M.h3 = malloc(sizeof(uint32_t) << 14); if (M.h3) memset(M.h3, 0xFF, sizeof(uint32_t) << 14); }
    if (v2 && P->wlog > bw && n > M.bmask + 1) {   /* a block longer than the tree's window */
        M.wmax = (uint32_t)((1ull << P->wlog) - 1); M.ldmlog = 22; while (M.ldmlog > 16 && ((size_t)8 << M.ldmlog) > n) M.ldmlog--;
        M.ldm = malloc(sizeof(uint32_t) << M.ldmlog); if (M.ldm) memset(M.ldm, 0xFF, sizeof(uint32_t) << M.ldmlog);
    }
    ko_node *o = malloc((KO_OPT + nice + 8) * sizeof *o); ko_m *mm = malloc((nice + 10) * sizeof *mm); uint32_t *path = malloc((KO_OPT + nice + 8) * sizeof *path);
    if (!M.head || !M.son || !o || !mm || !path || (v2 && P->m3 && !M.h3) || (M.wmax && !M.ldm)) {
        free(M.head); free(M.son); free(o); free(mm); free(path); free(M.h3); free(M.ldm); *err = 1; return 0;
    }
    memset(M.head, 0xFF, sizeof(uint32_t) << P->hlog);
    const uint8_t *ie = in + n; size_t pos = 0, anchor = 0, ns = 0, next_scale = KO_SPAN; uint32_t rep[3] = {1, 4, 8};
#define KO_RELAX(J, PR, ML, OFF, LL) do { uint32_t j_ = (J), p_ = (PR); \
        if (j_ > last) { for (uint32_t k_ = last + 1; k_ <= j_; k_++) o[k_].price = KO_INF; last = j_; } \
        if (p_ < o[j_].price) { o[j_].price = p_; o[j_].mlen = (ML); o[j_].off = (OFF); o[j_].litlen = (LL); } } while (0)
    while (pos + 8 <= n) {
        if (pos >= next_scale) { ko_rescale(T); next_scale = pos + KO_SPAN; }
        else if (T->pending >= 32) ko_refresh(T);
        uint32_t last = 0, end = 0, cur, fl = 0, foff = 0;
        o[0].litlen = (uint32_t)(pos - anchor); o[0].price = ko_llp(T, o[0].litlen); o[0].mlen = 0; memcpy(o[0].rep, rep, sizeof rep);
        for (cur = 0;; cur++) {
            if (cur > 0) {
                if (cur >= last) { end = last; break; }
                if (cur >= KO_OPT) { end = cur; break; }
                ko_node *c = &o[cur];
                if (c->mlen) { memcpy(c->rep, o[cur - c->mlen].rep, sizeof c->rep); ko_rep(c->rep, c->off); }
                else memcpy(c->rep, o[cur - 1].rep, sizeof c->rep);
            }
            const ko_node *c = &o[cur]; const size_t abs = pos + cur; const uint32_t base = c->price, L = c->litlen;
            if (abs >= n) continue;
            const uint32_t lb = base - ko_llp(T, L);
            if (P->skip && cur > 0 && cur + 1 < last && o[cur + 1].price <= base + (uint32_t)P->skip) continue;   /* unpromising position */
            if (abs + 8 <= n) {
                uint32_t nm = ko_find(&M, (uint32_t)abs, mm), rl[3] = {0, 0, 0}, rb = 0, rk = 0;
                for (int k = 0; k < 3; k++) {
                    uint32_t r = c->rep[k];
                    if (r <= abs && kz_r32(in + abs) == kz_r32(in + abs - r)) { rl[k] = 4 + (uint32_t)kz_count(in + abs + 4, in + abs + 4 - r, ie); if (rl[k] > rb) { rb = rl[k]; rk = (uint32_t)k; } }
                    else if (v2 && r <= abs && ((kz_r32(in + abs) ^ kz_r32(in + abs - r)) & 0xFFFFFFu) == 0) rl[k] = 3;
                }
                const uint32_t ml = nm ? mm[nm - 1].len : 0;
                if (rb >= nice || ml >= nice) {   /* long enough: take it now */
                    if (rb >= nice && (rb + 1 >= ml || rb >= ml)) { fl = rb; foff = c->rep[rk]; } else { fl = ml; foff = mm[nm - 1].off; }
                    end = cur; break;
                }
                KO_RELAX(cur + 1, lb + T->pl[in[abs]] + ko_llp(T, L + 1), 0, 0, L + 1);
                const uint32_t mb = base + T->llp[0];
                for (int k = 0; k < 3; k++)
                    for (uint32_t l = T->mmin; l <= rl[k]; l++) KO_RELAX(cur + l, mb + T->pof[k] + ko_mlp(T, l), l, c->rep[k], 0);
                uint32_t pl = T->mmin - 1;
                for (uint32_t i = 0; i < nm; i++) {
                    const uint32_t off = mm[i].off, ob = mb + T->pof[ko_ofc(off, c->rep)];
                    for (uint32_t l = pl + 1; l <= mm[i].len; l++) KO_RELAX(cur + l, ob + ko_mlp(T, l), l, off, 0);
                    pl = mm[i].len;
                }
            } else KO_RELAX(cur + 1, lb + T->pl[in[abs]] + ko_llp(T, L + 1), 0, 0, L + 1);
        }
        uint32_t np = 0;
        for (uint32_t j = end; j > 0;) { if (o[j].mlen) { path[np++] = j; j -= o[j].mlen; } else j--; }
        while (np) { uint32_t j = path[--np]; size_t st = pos + j - o[j].mlen; ko_emit(T, in, S, &ns, anchor, st, o[j].mlen, o[j].off, rep); anchor = st + o[j].mlen; }
        pos += end;
        if (fl) { ko_emit(T, in, S, &ns, anchor, pos, fl, foff, rep); pos += fl; anchor = pos; }
    }
#undef KO_RELAX
    free(M.head); free(M.son); free(o); free(mm); free(path); free(M.h3); free(M.ldm);
    *anchor_out = anchor; *err = 0; return ns;
}
static size_t ko_parse(const uint8_t *in, size_t n, const ko_par *P, int v2, kz_seq *S, size_t *anchor_out, int *err) {
    ko_st *T = malloc(sizeof *T); if (!T) { *err = 1; return 0; }
    ko_init(T, in, n, v2);
    size_t ns = ko_parse_pass(in, n, P, T, S, anchor_out, err);
    free(T); return ns;
}
/* KLZH levels 7-9 (their KLZH_LV entries are unused; 5-6 take KLZH_LV with kz_parse_lazy); wlog and m3 apply to KLZH2 */
static const ko_par KO_LV[10] = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {18, 22, 8, 32, 128, 30, 0}, {19, 23, 24, 64, 0, 30, 1}, {20, 24, 128, 512, 0, 30, 1}};

size_t kolmo_klzh_bound(size_t n) { return n + n / 64 + 1024; }
#define KZ_SEG (64u << 10)   /* KLZH2: the finest input span of the khuf tables (spans of KZ_SEG << j, kz_klzh_enc) */
#define KZ_MAXSEG 4096
#ifndef KZ_RANS_PCT
#define KZ_RANS_PCT 97   /* KLZH2: rANS only when it is below this % of the khuf size (it decodes about 2x slower per symbol) */
#endif
static size_t kz_klzh_enc(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level, int v2) {
    if (n < 16 || n >= 0xFFFFFFF0u || cap < 64) return 0;
    if (level < 1) level = 1;
    if (level > 9) level = 9;
    kz_seq *S = malloc((n / (v2 ? 3 : 4) + 2) * sizeof *S); size_t anchor; int err;
    if (!S) return 0;
    const kz_par *LV = &KLZH_LV[level]; const uint32_t mmin = v2 ? 3 : 4;
    ko_par KO9 = KO_LV[level]; if (v2 && level == 9) KO9.nice = 1024;   /* KLZH2: owid_energy_f64.npy -0.4%, 4% slower */
    int lg3 = 12; while (lg3 < 20 && ((size_t)1 << lg3) < n) lg3++;   /* KLZH2 level 3: about one slot per position */
    size_t ns = level == 3 && v2 ? kz_parse_dfast3(in, n, lg3, 9, S, &anchor, &err)
              : level <= 3 ? kz_parse_dfast(in, n, LV->hlog, LV->clog, LV->skip, 4, S, &anchor, &err)
              : level >= 7 ? ko_parse(in, n, &KO9, v2, S, &anchor, &err)
              : level >= 5 ? kz_parse_lazy(in, n, LV, kz_minm(in, n, LV, S), S, &anchor, &err) : kz_parse(in, n, LV, S, &anchor, &err);
    uint8_t *lit = malloc(n + 16), *cd = malloc(3 * (ns + 1)), *xb = malloc(ns * 12 + 64), *tmp = malloc(kolmo_khuf_bound(n) + 64), *trial = NULL;
    size_t r = 0;
    if (err || !lit || !cd || !xb || !tmp) goto out;
    uint8_t *llc = cd, *mlc = cd + ns, *ofc = cd + 2 * ns; size_t nl = 0; const uint8_t *ip = in;
    uint32_t rep[3] = {1, 4, 8}; kz_bw w = {0, 0, xb, xb + ns * 12 + 64};
    size_t sgl[KZ_MAXSEG + 1], sgs[KZ_MAXSEG + 1], nseg = 0;   /* KLZH2: literal and sequence counts at each KZ_SEG of input */
    for (size_t k = 0; k < ns; k++) {
        uint32_t ex; int nb;
        if (v2) {   /* the literal stream is cut at the input position itself, the code streams at the next sequence */
            const size_t p0 = (size_t)(ip - in), pe = p0 + S[k].ll + S[k].ml;
            for (; nseg < KZ_MAXSEG && nseg * KZ_SEG < pe; nseg++) {
                const size_t B = nseg * KZ_SEG;
                if (B <= p0) { sgl[nseg] = nl; sgs[nseg] = k; }
                else if (B < p0 + S[k].ll) { sgl[nseg] = nl + (B - p0); sgs[nseg] = k; }
                else { sgl[nseg] = nl + S[k].ll; sgs[nseg] = k + 1; }
            }
        }
        memcpy(lit + nl, ip, S[k].ll); nl += S[k].ll; ip += S[k].ll + S[k].ml;
        llc[k] = (uint8_t)kz_lcode(S[k].ll, &ex, &nb); if (kz_bput(&w, ex, nb)) goto out;
        mlc[k] = (uint8_t)kz_lcode(S[k].ml - mmin, &ex, &nb); if (kz_bput(&w, ex, nb)) goto out;
        uint32_t o = S[k].off;
        if (o == rep[0]) ofc[k] = 0;
        else if (o == rep[1]) { ofc[k] = 1; rep[1] = rep[0]; rep[0] = o; }
        else if (o == rep[2]) { ofc[k] = 2; rep[2] = rep[1]; rep[1] = rep[0]; rep[0] = o; }
        else { int h = kz_hb(o); ofc[k] = (uint8_t)(3 + h); if (kz_bput(&w, o - (1u << h), h)) goto out; rep[2] = rep[1]; rep[1] = rep[0]; rep[0] = o; }
    }
    for (; v2 && nseg < KZ_MAXSEG && nseg * KZ_SEG < n; nseg++) { sgl[nseg] = nl + (nseg * KZ_SEG - anchor); sgs[nseg] = ns; }
    memcpy(lit + nl, ip, n - anchor); nl += n - anchor;
    if (w.p + 8 > w.e) goto out;
    kz_w64(w.p, w.b); w.p += (w.c + 7) >> 3;
    size_t o = kz_put_uv(out, ns); o += kz_put_uv(out + o, nl);
    const uint8_t *st[4] = {lit, llc, mlc, ofc}; size_t sn[4] = {nl, ns, ns, ns};
    for (int q = 0; q < 4; q++) {
        size_t z = 0;
        if (v2 && nseg > 1 && sn[q]) {   /* KLZH2: khuf chunks with local tables, cut where the input crosses each span of
                                            KZ_SEG << j (the chunk framing allows any chunk size up to KHUF_BLOCK_MAX). Levels
                                            1-4 take 256 KiB spans; 5-9 the smallest of 64, 128, 256 and 512 KiB in each
                                            512 KiB of input (Silesia-like tuning set -0.15% at level 9, 20% slower at 3) */
            const size_t *b = q ? sgs : sgl, cap2 = kolmo_khuf_bound(n) + 64;
            const int j0 = level >= 5 ? 0 : 2, j1 = level >= 5 ? 3 : 2; const size_t R = (size_t)1 << j1;
            if (j0 < j1 && !trial && !(trial = malloc(cap2))) goto out;
            for (size_t g0 = 0; g0 < nseg && z != (size_t)-1; g0 += R) {
                size_t bz = (size_t)-1;
                for (int j = j0; j <= j1; j++) {
                    size_t y = 0; const size_t sp = (size_t)1 << j; uint8_t *dst = j0 < j1 ? trial : tmp + z;
                    for (size_t g = g0; g < g0 + R && g < nseg && y != (size_t)-1; g += sp) {
                        const size_t a = b[g], ge = g + sp, e = ge < nseg ? b[ge] : sn[q];
                        if (e > a) { size_t y1 = kolmo_khuf_compress_chunks(st[q] + a, e - a, dst + y, cap2 - z - y); y = y1 ? y + y1 : (size_t)-1; }
                    }
                    if (y < bz && z + y <= cap2) { bz = y; if (dst != tmp + z) memcpy(tmp + z, trial, y); }
                }
                z = bz == (size_t)-1 ? (size_t)-1 : z + bz;
            }
            if (z == (size_t)-1) z = 0;
        }
        if (!z && sn[q]) z = kolmo_khuf_compress_chunks(st[q], sn[q], tmp, kolmo_khuf_bound(n) + 64);
        if (sn[q] && !z) goto out;
        uint8_t mode = 0; const uint8_t *src = tmp;
        if (v2 && q && sn[q] && kolmo_krans_bound(sn[q]) <= kolmo_khuf_bound(n) + 64) {   /* KLZH2: rANS where it is smaller */
            uint8_t *t2 = malloc(kolmo_krans_bound(sn[q])); if (!t2) goto out;
            size_t z2 = kolmo_krans_compress_lim(st[q], sn[q], t2, kolmo_krans_bound(sn[q]), (size_t)((uint64_t)z * KZ_RANS_PCT / 100));
            if (z2 && (uint64_t)z2 * 100 < (uint64_t)z * KZ_RANS_PCT) { memcpy(tmp, t2, z2); z = z2; mode = 1; }
            free(t2);
        }
        if (cap - o < z + 11) goto out;
        if (v2) out[o++] = mode;
        o += kz_put_uv(out + o, z); memcpy(out + o, src, z); o += z;
    }
    size_t xl = (size_t)(w.p - xb);
    if (cap - o < xl || o + xl >= n) goto out;
    memcpy(out + o, xb, xl); r = o + xl;
out:
    free(S); free(lit); free(cd); free(xb); free(tmp); free(trial); return r;
}

size_t kolmo_klzh_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level) { return kz_klzh_enc(in, n, out, cap, level, 0); }
size_t kolmo_klzh2_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level) { return kz_klzh_enc(in, n, out, cap, level, 1); }

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
static int kz_klzh_dec(const uint8_t *in, size_t cn, uint8_t *out, size_t n, int v2) {
    size_t i = 0; uint64_t ns, nl;
    if (kz_get_uv(in, cn, &i, &ns) || kz_get_uv(in, cn, &i, &nl) || ns > n / (v2 ? 3 : 4) + 1 || nl > n) return -1;
    uint8_t *lit = malloc(nl + 32), *cd = malloc(3 * ns + 1); int rc = -1;
    if (!lit || !cd) goto out;
    uint8_t *st[4] = {lit, cd, cd + ns, cd + 2 * ns}; size_t sn[4] = {nl, ns, ns, ns};
    for (int q = 0; q < 4; q++) {
        unsigned mode = 0; if (v2) { if (i >= cn || in[i] > (q ? 1 : 0)) goto out; mode = in[i++]; }
        uint64_t z; if (kz_get_uv(in, cn, &i, &z) || z > cn - i) goto out;
        if (sn[q] ? (mode ? kolmo_krans_decompress(in + i, z, st[q], sn[q]) : kolmo_khuf_decompress_chunks(in + i, z, st[q], sn[q])) : z != 0) goto out;
        i += z;
    }
    kz_br br = {0, 0, in + i, in + cn};
    const uint8_t *lp = lit, *le = lit + nl, *llc = st[1], *mlc = st[2], *ofc = st[3];
    uint32_t LB[72]; uint8_t LN[72]; kz_ltab(LB, LN);
    { unsigned ml = 0, mo = 0;                                  /* codes in range: one pass, maxima only (decmap lane) */
      for (size_t k = 0; k < ns; k++) { unsigned a = llc[k] > mlc[k] ? llc[k] : mlc[k]; ml = a > ml ? a : ml; mo = ofc[k] > mo ? ofc[k] : mo; }
      if (ml >= 72 || mo > (v2 ? 32u : 26u)) goto out; }
    uint8_t *op = out, *oe = out + n; uint32_t rep[3] = {1, 4, 8}; const uint32_t mmin = v2 ? 3 : 4;
    for (size_t k = 0; k < ns; k++) {
        uint32_t ll, ml, off; unsigned oc = ofc[k], a = llc[k], b = mlc[k];
        /* after a refill at least 56 bits are buffered; the next refill only when the extra bits left need it */
        kz_refill(&br); ll = LB[a] + kz_bget(&br, LN[a]);
        if (br.c < LN[b]) kz_refill(&br);
        ml = LB[b] + kz_bget(&br, LN[b]) + mmin;
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
int kolmo_klzh_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) { return kz_klzh_dec(in, cn, out, n, 0); }
int kolmo_klzh2_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) { return kz_klzh_dec(in, cn, out, n, 1); }
