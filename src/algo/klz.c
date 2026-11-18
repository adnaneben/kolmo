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
    if ((size_t)(oe - e) >= 16) {   /* room for whole-register copies past the end */
        if (off >= 32) { while (op < e && (size_t)(oe - op) >= 32) { memcpy(op, m, 16); memcpy(op + 16, m + 16, 16); op += 32; m += 32; } if (op < e) memcpy(op, m, 16); return; }
        if (off >= 16) { while (op < e) { memcpy(op, m, 16); op += 16; m += 16; } return; }
        if (off < 8) {   /* the first 8 bytes one by one in two halves, then a source at least 8 bytes back */
            static const uint8_t inc[8] = {0, 1, 2, 1, 0, 4, 4, 4}; static const int8_t adj[8] = {0, 0, 0, 1, 4, -1, -2, -3};
            op[0] = m[0]; op[1] = m[1]; op[2] = m[2]; op[3] = m[3];
            m += inc[off]; memcpy(op + 4, m, 4); m += adj[off]; op += 8;
        }
        while (op < e) { memcpy(op, m, 8); op += 8; m += 8; }
        return;
    }
    while (op < e) *op++ = *m++;
}

/* ---------------- the parse ---------------- */
typedef struct { uint32_t ll, ml, off; } kz_seq;
typedef struct { int hlog, clog, depth, lazy, skip, nice; } kz_par;
typedef struct { const uint8_t *in; size_t n; uint32_t *head, *chain; uint32_t cmask, ins; kz_par P; uint32_t win; } kz_mf;
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
    uint32_t win = M->chain ? M->cmask : M->win; if (win > M->win) win = M->win;
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

static size_t kz_parse(const uint8_t *in, size_t n, const kz_par *P, uint32_t win, kz_seq *S, size_t *anchor_out, int *err) {
    kz_mf M = {in, n, NULL, NULL, 0, 0, *P, win}; size_t ns = 0;
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
        } else if (cl < pos && pos - cl <= KLZH_WINDOW && kz_r64(in + cl) == kz_r64(in + pos)) {
            start = pos; off = (uint32_t)(pos - cl); ml = 8 + kz_count(in + pos + 8, in + cl + 8, ie);
        } else if (cs < pos && pos - cs <= KLZH_WINDOW && kz_r32(in + cs) == kz_r32(in + pos)) {
            const uint32_t h1 = kz_h8(in + pos + 1, hl), c1 = tl[h1]; tl[h1] = (uint32_t)pos + 1;
            if (c1 < pos + 1 && pos + 1 - c1 <= KLZH_WINDOW && kz_r64(in + c1) == kz_r64(in + pos + 1)) {   /* a long match one byte later */
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
static const kz_par KLZ_LV[4] = {{0}, {0}, {0}, {17, 20, 16, 1, 0, 128}};   /* level 3; levels 1-2: kz_fast */
/* KLZH: levels 1-3 the fast parse (hlog = long table, clog = short table, skip), 4-9 hash chains */
static const kz_par KLZH_LV[10] = {{0}, {16, 15, 0, 0, 6, 0}, {17, 16, 0, 0, 7, 0}, {18, 17, 0, 0, 8, 0}, {17, 18, 4, 0, 0, 64},
    {17, 19, 8, 1, 0, 64}, {18, 20, 16, 1, 0, 128}, {18, 21, 32, 1, 0, 128}, {19, 22, 64, 2, 0, 256}, {20, 23, 256, 2, 0, 512}};

/* ---------------- KLZ: byte-aligned streams (format in klz.h) ---------------- */
size_t kolmo_klz_bound(size_t n) { return n + n / 255 + 64; }
static size_t kz_uvlen(uint64_t v) { size_t k = 1; while (v >= 128) { v >>= 7; k++; } return k; }
static void kz_put_uvw(uint8_t *p, uint64_t v, size_t w) {   /* v in exactly w bytes (non-minimal when needed) */
    for (size_t k = 0; k < w; k++, v >>= 7) p[k] = (uint8_t)((v & 127) | (k + 1 < w ? 128 : 0));
}
/* the encoder's streams: literals go straight to the output after the header, the others to scratch */
typedef struct { uint8_t *lp, *le, *tp, *fp, *xp; const uint8_t *ilim; } kz_o;   /* ilim: input end - 16 */
static inline void kz_ext(kz_o *o, size_t v) { if (v < 255) *o->xp++ = (uint8_t)v; else { *o->xp++ = 255; o->xp += kz_put_uv(o->xp, v - 255); } }
/* one sequence: ll literals from lit, then a match (1 <= off <= 65535, ml >= 4); 0 when the literals do not fit */
static inline int kz_seq_out(kz_o *o, const uint8_t *lit, size_t ll, size_t off, size_t ml, int safe) {
    if ((size_t)(o->le - o->lp) < ll + 32) return 0;
    if (ll <= 16 && (safe || lit <= o->ilim)) memcpy(o->lp, lit, 16); else memcpy(o->lp, lit, ll);
    o->lp += ll;
    size_t m = ml - 4; *o->tp++ = (uint8_t)((ll < 15 ? ll : 15) | (m < 15 ? m : 15) << 4);
    o->fp[0] = (uint8_t)off; o->fp[1] = (uint8_t)(off >> 8); o->fp += 2;
    if (ll >= 15) kz_ext(o, ll - 15);
    if (m >= 15) kz_ext(o, m - 15);
    return 1;
}
/* the greedy parse of levels 1 and 2 (wave 4): one hash table of 6-byte keys (hb), the step grows after every 2^trig
 * misses (the classic skipping heuristic), the position after each match tried at once; level 2 also inserts the
 * byte before. The table holds the low 16 bits of positions: with 16-bit offsets that is enough to find the
 * candidate (the most recent position with these bits, at most 65535 back), and the table is half the size of one
 * of positions (2^15 entries, 64 KiB at level 1). After a literal run of KZ_LR bytes a match must have KZ_ML bytes
 * (in data that hardly compresses, short matches cost a sequence each at decode for a byte or two).
 * Measured (benchmark/w4/lz/RESULTS.md): 6-byte keys against 5 at the same table, 2^15 entries against 2^14, the
 * literal-run rule, a repeat-offset probe at the next byte (no gain), a decaying step after a match (no gain). */
#define KZ_LR 64
#define KZ_ML 7
#define kz_hf(p, lg) ((uint32_t)(((kz_r64(p) << (64 - 8 * hb)) * 889523592379ull) >> (64 - (lg))))
static inline __attribute__((always_inline)) int kz_fast(const uint8_t *in, size_t n, kz_o *O, int lg, int trig, int two, int hb, const uint8_t **anchor_out) {
    const uint8_t *ip = in, *anchor = in, *const ie = in + n;
    if (n < 20) { *anchor_out = in; return 1; }
    /* the stream pointers as plain locals (registers); the extension stream, rarely written, stays in O */
    uint8_t *lp = O->lp, *tp = O->tp, *fp = O->fp; uint8_t *const lle = O->le - 32;
    uint16_t *ht = calloc((size_t)1 << lg, sizeof(uint16_t)); if (!ht) return -1;
    const uint8_t *const limit = ie - 16;   /* matches start at most here: literals copy 16 bytes safely (safe = 1) */
#define KZ_CAND(P, E) ((P) - (size_t)(uint16_t)((size_t)((P) - in) - (E)))
#define KZ_OK(P, M) ((M) < (P) && kz_r32(M) == kz_r32(P))   /* candidates are never before in */
    ip++;
    unsigned att = 1u << trig;
    for (;;) {
        const uint8_t *m; size_t ml;
        {   /* look for a 4-byte match; the hash of the next position is computed before the test of this one */
            const uint8_t *fip = ip; unsigned step = 1; uint32_t fh = kz_hf(ip, lg);
            do {
                uint32_t h = fh; ip = fip; fip += step; step = att++ >> trig;
                if (fip > limit) goto done;
                m = KZ_CAND(ip, ht[h]); fh = kz_hf(fip, lg); ht[h] = (uint16_t)(ip - in);
            } while (!KZ_OK(ip, m) || ((size_t)(ip - anchor) >= KZ_LR && (kz_r64(ip) ^ kz_r64(m)) << (64 - 8 * KZ_ML)));
        }
        while (ip > anchor && m > in && ip[-1] == m[-1]) { ip--; m--; }
        att = 1u << trig;
        for (;;) {
            ml = 4 + kz_count(ip + 4, m + 4, ie);
            {   /* the sequence: literals (16 bytes at once: a match starts at most 16 bytes before the end), token, offset */
                const size_t ll = (size_t)(ip - anchor), off = (size_t)(ip - m), mm = ml - 4;
                if (ll > (size_t)(lle - lp)) { free(ht); return 0; }
                memcpy(lp, anchor, 16); if (ll > 16) memcpy(lp + 16, anchor + 16, ll - 16);
                lp += ll;
                *tp++ = (uint8_t)((ll < 15 ? ll : 15) | (mm < 15 ? mm : 15) << 4);
                fp[0] = (uint8_t)off; fp[1] = (uint8_t)(off >> 8); fp += 2;
                if (__builtin_expect(ll >= 15 || mm >= 15, 0)) { if (ll >= 15) kz_ext(O, ll - 15); if (mm >= 15) kz_ext(O, mm - 15); }
            }
            ip += ml; anchor = ip;
            if (ip > limit) goto done;
            if (two) ht[kz_hf(ip - 3, lg)] = (uint16_t)(ip - 3 - in);
            ht[kz_hf(ip - 2, lg)] = (uint16_t)(ip - 2 - in);
            uint32_t h = kz_hf(ip, lg); m = KZ_CAND(ip, ht[h]); ht[h] = (uint16_t)(ip - in);   /* a match right after the match */
            if (!KZ_OK(ip, m)) break;
        }
        ip++;
    }
#undef KZ_CAND
#undef KZ_OK
done:
    free(ht); O->lp = lp; O->tp = tp; O->fp = fp; *anchor_out = anchor; return 1;
}
size_t kolmo_klz_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level) {
    if (n >= 0xFFFFFFF0u) return 0;
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    const size_t w = kz_uvlen(n), H = 3 * w, ms = n / 4 + 2;
    if (cap < H + 64) return 0;
    uint8_t *sc = malloc(ms * 15); if (!sc) return 0;   /* tokens, offsets (2 per token), extensions (up to 12) */
    kz_o o = {out + H, out + cap, sc, sc + ms, sc + 3 * ms, n >= 16 ? in + n - 16 : in - 1};
    const uint8_t *anchor = in; int ok = 1;
    if (level >= 3) {   /* chains and lazy evaluation */
        kz_seq *S = malloc(ms * sizeof *S); size_t an; int err;
        if (!S) { free(sc); return 0; }
        size_t ns = kz_parse(in, n, &KLZ_LV[level], KLZ_WIN, S, &an, &err);
        if (err) ok = 0;
        for (size_t k = 0; ok && k < ns; k++) { ok = kz_seq_out(&o, anchor, S[k].ll, S[k].off, S[k].ml, 0); anchor += S[k].ll + S[k].ml; }
        free(S);
    } else if (level == 1) ok = kz_fast(in, n, &o, 15, 6, 0, 6, &anchor);
    else ok = kz_fast(in, n, &o, 16, 6, 1, 6, &anchor);
    size_t r = 0;
    if (ok == 1) {   /* the last literals, then the header and the other streams */
        size_t ll = (size_t)(in + n - anchor), nt = (size_t)(o.tp - sc), nx = (size_t)(o.xp - (sc + 3 * ms));
        if ((size_t)(o.le - o.lp) >= ll + 3 * nt + nx) {
            memcpy(o.lp, anchor, ll); o.lp += ll;
            size_t nl = (size_t)(o.lp - (out + H));
            kz_put_uvw(out, nt, w); kz_put_uvw(out + w, nl, w); kz_put_uvw(out + 2 * w, nx, w);
            memcpy(o.lp, sc, nt); o.lp += nt; memcpy(o.lp, sc + ms, 2 * nt); o.lp += 2 * nt;
            memcpy(o.lp, sc + 3 * ms, nx); o.lp += nx;
            r = (size_t)(o.lp - out);
        }
    }
    free(sc); return r;
}
/* length extension from the ext stream; SIZE_MAX when malformed */
static inline size_t kz_xget(const uint8_t **xp, const uint8_t *xe) {
    if (*xp >= xe) return (size_t)-1;
    size_t v = *(*xp)++;
    if (v == 255) {
        uint64_t u; size_t i = 0;
        if (kz_get_uv(*xp, (size_t)(xe - *xp), &i, &u) || u > 0xFFFFFFFFu) return (size_t)-1;
        *xp += i; v = 255 + (size_t)u;
    }
    return v;
}
/* match of ml <= 18 bytes, off >= 1, with room for 32 bytes */
static inline void kz_m18(uint8_t *op, const uint8_t *m, size_t off) {
    if (off >= 8) { memcpy(op, m, 8); memcpy(op + 8, m + 8, 8); memcpy(op + 16, m + 16, 2); return; }
    static const uint8_t inc[8] = {0, 1, 2, 1, 0, 4, 4, 4}; static const int8_t adj[8] = {0, 0, 0, 1, 4, -1, -2, -3};
    op[0] = m[0]; op[1] = m[1]; op[2] = m[2]; op[3] = m[3];
    m += inc[off]; memcpy(op + 4, m, 4); m += adj[off];
    memcpy(op + 8, m, 8); memcpy(op + 16, m + 8, 8);
}
/* the decoder's fast loop, kept apart so that its values stay in registers: at most k sequences (the budget: 32 bytes
 * of output and 16 bytes of input per sequence fit, with 64 and 32 bytes of margin), each short (at most 14 literals
 * and 18 match bytes: fixed-size copies) or with one extension byte per long length (copied in 32-byte steps and
 * charged to the budget: (ll + ml) / 32 + ll / 16 slots more). Stops at the first sequence it does not take. A
 * literal pointer past the literals is caught by the caller at the end (the output then does not add up). Returns
 * -1 on a bad offset. */
typedef struct { const uint8_t *tp, *fp, *lp, *xp, *xe; uint8_t *op; } kz_dst;
static __attribute__((noinline)) int kz_dec_fast(kz_dst *S, size_t k, uint8_t *const out) {
    const uint8_t *tp = S->tp, *tl = tp + k, *fp = S->fp, *lp = S->lp; uint8_t *op = S->op; int rc = 0;
    while (tp < tl) {
        const size_t t = *tp, off = (size_t)fp[0] | (size_t)fp[1] << 8; size_t ll = t & 15, ml = (t >> 4) + 4;
        if (__builtin_expect(ll == 15 || ml == 19, 0)) {
            const uint8_t *x = S->xp; size_t a = 0, b = 0;
            if (ll == 15) { if (x >= S->xe || *x == 255) break; a = *x++; }
            if (ml == 19) { if (x >= S->xe || *x == 255) break; b = *x++; }
            ll += a; ml += b;
            const size_t ch = (ll + ml) / 32 + ll / 16;
            if (ch >= (size_t)(tl - tp)) break;
            S->xp = x; tl -= ch;
            { const uint8_t *s0 = lp; uint8_t *d = op, *de = op + ll; do { memcpy(d, s0, 16); memcpy(d + 16, s0 + 16, 16); d += 32; s0 += 32; } while (d < de); }
            op += ll; lp += ll;
            if (off - 1 >= (size_t)(op - out)) { rc = -1; break; }
            kz_match(op, off, ml, op + ml + 64); op += ml; tp++; fp += 2;
            continue;
        }
        memcpy(op, lp, 16); op += ll; lp += ll;
        if (off - 1 >= (size_t)(op - out)) { rc = -1; break; }
        kz_m18(op, op - off, off);
        op += ml; tp++; fp += 2;
    }
    S->tp = tp; S->fp = fp; S->lp = lp; S->op = op;
    return rc;
}
int kolmo_klz_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0; uint64_t ns, nl, nx;
    if (kz_get_uv(in, cn, &i, &ns) || kz_get_uv(in, cn, &i, &nl) || kz_get_uv(in, cn, &i, &nx)) return -1;
    if (nl > n || ns > n / 4 + 1 || nx > cn || cn - i != nl + 3 * ns + nx) return -1;
    const uint8_t *lp = in + i, *const le = lp + nl, *tp = le, *const te = tp + ns, *fp = te;   /* literals, tokens, */
    const uint8_t *xp = fp + 2 * ns, *const xe = xp + nx, *const ie = in + cn;                  /* offsets, extensions */
    uint8_t *op = out, *const oe = out + n;
    while (tp < te) {
        size_t ko = (size_t)(oe - op) >= 64 ? ((size_t)(oe - op) - 64) / 32 : 0, ki = (size_t)(ie - lp) >= 32 ? ((size_t)(ie - lp) - 32) / 16 : 0;
        size_t k = ko < ki ? ko : ki; if (k > (size_t)(te - tp)) k = (size_t)(te - tp);
        if (k) {
            kz_dst S = {tp, fp, lp, xp, xe, op};
            if (kz_dec_fast(&S, k, out)) return -1;
            int moved = S.tp != tp; tp = S.tp; fp = S.fp; lp = S.lp; xp = S.xp; op = S.op;
            if (moved) continue;   /* budget used, or stopped at a sequence: recompute (the general path if again) */
        }
        /* one sequence with every test: long lengths, or near the end of the output or of the input */
        const size_t t = *tp++, off = (size_t)fp[0] | (size_t)fp[1] << 8; size_t ll = t & 15, ml = t >> 4; fp += 2;
        if (lp > le) return -1;
        if (ll == 15) { size_t x = kz_xget(&xp, xe); if (x == (size_t)-1) return -1; ll += x; }
        if (ml == 15) { size_t x = kz_xget(&xp, xe); if (x == (size_t)-1) return -1; ml += x; }
        ml += 4;
        if (ll > (size_t)(le - lp) || ll > (size_t)(oe - op) || ml > (size_t)(oe - op) - ll) return -1;
        if (ll >= 256) memcpy(op, lp, ll);
        else if ((size_t)(ie - lp) - ll >= 16 && (size_t)(oe - op) - ll >= 16) {   /* whole-register literal copies */
            const uint8_t *s0 = lp; uint8_t *d = op, *de = op + ll; do { memcpy(d, s0, 16); d += 16; s0 += 16; } while (d < de);
        } else memcpy(op, lp, ll);
        op += ll; lp += ll;
        if (off - 1 >= (size_t)(op - out)) return -1;
        kz_match(op, off, ml, oe);
        op += ml;
    }
    if (lp > le || (size_t)(le - lp) != (size_t)(oe - op) || xp != xe) return -1;
    memcpy(op, lp, (size_t)(le - lp));
    return 0;
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
    size_t ns = level <= 3 ? kz_parse_dfast(in, n, LV->hlog, LV->clog, LV->skip, 4, S, &anchor, &err) : kz_parse(in, n, LV, KLZH_WINDOW, S, &anchor, &err);
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
