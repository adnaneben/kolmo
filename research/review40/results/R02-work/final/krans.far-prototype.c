/* Kolmo context rANS (krans): static rANS with static context modelling, the coder of graph leaf codec 33.
   Format, method and references: krans.h. Compiled on its own (NATSRC in the Makefile), no dependency on the rest
   of the library. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0. */
#include "krans.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define KR_SB 12
#define KR_M (1u << KR_SB)
#define KR_L (1u << 16)
#define KR_EMAX 1020                    /* four lags of magnitude 255 */
#define KR_MAXT 255
#define KR_REST 14                      /* class of the symbol that takes the rest of 2^12 */
#define KR_CTX_MIN 2048                 /* encoder: chunks below this are coded order 0 */
#define KR_CTX_M 256                    /* format: a chunk shorter than this has one context (set-up work stays below the chunk's bytes) */
#define KR_COLD __attribute__((cold))   /* set-up and header code: compiled for size */

static const uint16_t kr_edges1[16] = {1, 2, 4, 6, 8, 11, 14, 18, 23, 29, 37, 47, 60, 80, 110, 160};
static const uint16_t kr_edges2[8] = {1, 3, 6, 11, 18, 29, 47, 80};
static const uint16_t kr_edgesb[5] = {1, 3, 6, 12, 25};
static const uint8_t kr_mbits[4] = {3, 4, 6, 12};

/* ---- the context function (shared by the encoder and the decoder) ---- */
typedef struct {
    unsigned ls, P, mb, eq, bq, mk, pf, nlag; uint32_t lag[4];          /* the header fields */
    unsigned N, mbits; uint32_t maxlag;                                  /* derived */
    uint8_t mag[256]; uint16_t qe[KR_EMAX + 1], qb[256], pm[8];          /* qe: bin * NB * P; qb: bin * P; pm: prev mask by phase */
    uint16_t qbp[8][256];                                                /* (qb & pm[phase]) + phase: the previous-byte term and the phase in one lookup */
} kr_ctx;
static unsigned kr_bin(unsigned v, const uint16_t *edges, unsigned n) { unsigned b = 0; while (b < n && edges[b] <= v) b++; return b; }
static KR_COLD int kr_ctx_init(kr_ctx *c) {
    if (c->ls > 5 || c->P < 1 || c->P > 8 || c->mb > 3 || c->eq > 3 || c->mk > 3 || c->nlag > 4) return -1;
    if (c->mk == 3 && c->P != 2) return -1;                              /* 16-bit words: high byte, low byte */
    if ((c->eq == 0) != (c->nlag == 0) || (c->pf && (!c->bq || c->P < 2))) return -1;
    const unsigned ne = c->eq == 1 ? 17 : c->eq == 2 ? 9 : c->eq == 3 ? 48 : 1, nb = c->bq == 1 ? 256 : c->bq == 2 ? 6 : c->bq == 3 ? 11 : 1;
    c->N = ne * nb * c->P; c->mbits = kr_mbits[c->mb]; c->maxlag = 1;
    if (c->N > KRANS_NCTX_MAX) return -1;
    for (unsigned q = 0; q < c->nlag; q++) {
        if (c->lag[q] < 1 || c->lag[q] > KRANS_CHUNK_MAX || (c->mk == 3 && (c->lag[q] & 1))) return -1;
        if (c->lag[q] + (c->mk == 3) > c->maxlag) c->maxlag = c->lag[q] + (c->mk == 3);
    }
    if (c->N == 1) return 0;                                             /* one context: the tables below are never read */
    for (unsigned b = 0; b < 256; b++) {
        c->mag[b] = (uint8_t)(c->mk == 0 ? (b + 1) >> 1 : c->mk == 1 ? (b < 128 ? b : 256 - b) : b);       /* mk 3: the previous byte as it is */
        const unsigned neg = c->mk == 0 ? b & 1 : c->mk == 1 ? b >> 7 : 0, sb = kr_bin(c->mag[b], kr_edgesb, 5);   /* bq 3: bin and sign */
        const unsigned v = c->bq == 1 ? b : c->bq == 2 ? sb : c->bq == 3 ? (sb ? 2 * sb - 1 + neg : 0) : 0;
        c->qb[b] = (uint16_t)(v * c->P);
    }
    for (unsigned e = 0; e <= KR_EMAX; e++)
        c->qe[e] = (uint16_t)((c->eq == 1 ? kr_bin(e, kr_edges1, 16) : c->eq == 2 ? kr_bin(e, kr_edges2, 8) : c->eq == 3 ? (e < 95 ? e : 95) >> 1 : 0) * nb * c->P);
    for (unsigned ph = 0; ph < 8; ph++) c->pm[ph] = c->pf && ph == 0 ? 0 : 0xFFFF;
    for (unsigned ph = 0; ph < c->P; ph++) for (unsigned b = 0; b < 256; b++) c->qbp[ph][b] = (uint16_t)((c->qb[b] & c->pm[ph]) + ph);
    return 0;
}
/* mk 3: the class of the 16-bit zigzag word at w (high byte first): 0 for magnitude 0, else its half-octave 1..31 */
static inline unsigned kr_wcls(const uint8_t *w) {
    const uint32_t m = (((uint32_t)w[0] << 8 | w[1]) + 1) >> 1; if (!m) return 0;
    const unsigned k = 31u - (unsigned)__builtin_clz(m);
    return 2 * k + 1 + (k ? (m >> (k - 1)) & 1 : 0);
}
/* context of byte i of a segment in phase ph: any i (lags before the segment start count 0) */
static inline unsigned kr_ctx_at(const kr_ctx *c, const uint8_t *seg, size_t i, unsigned ph) {
    unsigned e = 0;
    if (c->mk == 3) { if (i >= ph) for (unsigned q = 0; q < c->nlag; q++) if (c->lag[q] <= i - ph) e += kr_wcls(seg + (i - ph) - c->lag[q]); }
    else for (unsigned q = 0; q < c->nlag; q++) if (c->lag[q] <= i) e += c->mag[seg[i - c->lag[q]]];
    return (unsigned)c->qe[e] + (c->qb[i ? seg[i - 1] : 0] & c->pm[ph]) + ph;
}
/* the same for i >= maxlag, q = the byte's address */
static inline unsigned kr_ctx_in(const kr_ctx *c, const uint8_t *q, unsigned ph) {
    unsigned e = 0;
    if (c->mk == 3) for (unsigned l = 0; l < c->nlag; l++) e += kr_wcls(q - ph - c->lag[l]);
    else for (unsigned l = 0; l < c->nlag; l++) e += c->mag[*(q - c->lag[l])];
    return (unsigned)c->qe[e] + (c->qb[q[-1]] & c->pm[ph]) + ph;
}

static inline uint32_t kr_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint32_t kr_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static int kr_uv_get(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0;
    for (unsigned s = 0; s < 35; s += 7) { if (*i >= n) return -1; const uint8_t b = p[(*i)++]; r |= (uint64_t)(b & 127) << s; if (b < 128) { *v = r; return 0; } }
    return -1;
}
static size_t kr_uv_put(uint8_t *p, uint64_t v) { size_t k = 0; for (;; v >>= 7) { p[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) return k; } }
static inline unsigned kr_bitlen(uint32_t v) { return v ? 32u - (unsigned)__builtin_clz(v) : 0; }

/* ================================================= decoder ================================================= */
typedef struct { uint8_t sym[KR_M]; struct { uint16_t f, c; } fc[256]; } kr_dtab;   /* slot -> symbol; symbol -> frequency, start */
typedef struct { const uint8_t *p, *e; uint64_t acc; unsigned nb; int bad; } kr_br;
static inline uint32_t kr_get(kr_br *r, unsigned n) {                     /* n <= 16 */
    while (r->nb < n) { if (r->p < r->e) r->acc |= (uint64_t)*r->p++ << r->nb; else r->bad = 1; r->nb += 8; }
    const uint32_t v = (uint32_t)r->acc & ((1u << n) - 1); r->acc >>= n; r->nb -= n; return v;
}
static int kr_dec_table(kr_br *r, unsigned mbits, kr_dtab *t) {
    uint16_t f[256]; const unsigned lo = kr_get(r, 8), hi = kr_get(r, 8); int pc = 0, rest = -1; uint32_t sum = 0;
    if (lo > hi) return -1;
    for (unsigned s = lo; s <= hi; s++) {
        int e;
        if (!kr_get(r, 1)) e = pc;
        else if (!kr_get(r, 1)) e = pc - 1;
        else if (!kr_get(r, 1)) e = pc + 1;
        else if (!kr_get(r, 1)) e = pc - 2;
        else if (!kr_get(r, 1)) e = pc + 2;
        else {
            e = (int)kr_get(r, 4);
            if (e == KR_REST) { if (rest >= 0) return -1; rest = (int)s; f[s] = 1; continue; }
        }
        if (e < 0 || e > 12) return -1;
        pc = e; f[s] = 0;
        if (e) { const unsigned nm = (unsigned)e - 1 < mbits ? (unsigned)e - 1 : mbits; f[s] = (uint16_t)((1u << (e - 1)) | kr_get(r, nm) << ((unsigned)e - 1 - nm)); sum += f[s]; }
    }
    if (r->bad || rest < 0 || sum >= KR_M || !f[lo] || !f[hi]) return -1;
    f[rest] = (uint16_t)(KR_M - sum);
    uint32_t c = 0;
    for (unsigned s = lo; s <= hi; s++) if (f[s]) { t->fc[s].f = f[s]; t->fc[s].c = (uint16_t)c; memset(t->sym + c, (int)s, f[s]); c += f[s]; }
    return 0;
}
#define KR_SYM(X, TB, O) do { const uint32_t sl_ = (X) & (KR_M - 1); const kr_dtab *t_ = (TB); const uint8_t s_ = t_->sym[sl_]; (O) = s_; \
        (X) = (uint32_t)t_->fc[s_].f * ((X) >> KR_SB) + sl_ - t_->fc[s_].c; } while (0)
#define KR_FILL(X) do { const uint32_t k_ = 0u - (uint32_t)((X) < KR_L), y_ = ((X) << 16) | kr_rd16(p); (X) ^= ((X) ^ y_) & k_; p += k_ & 2; } while (0)   /* no branch; two readable bytes at p */
#define KR_FILL_CK(X) do { if ((X) < KR_L) { if (pe - p < 2) return -1; (X) = ((X) << 16) | kr_rd16(p); p += 2; } } while (0)
typedef struct { uint32_t x[32]; uint8_t *sb[32]; unsigned sp[32]; const uint8_t *p, *pe; } kr_dst;   /* states, segment starts and phases, the words */
/* byte i of every segment by the checked path: any position (segment starts), any number of words left */
static __attribute__((noinline)) int kr_round_ck(const kr_ctx *c, const kr_dtab *const *ct, kr_dst *d, size_t i, unsigned S) {
    const uint8_t *p = d->p, *const pe = d->pe;
    for (unsigned k = 0; k < S; k++) {
        const unsigned cx = c->N == 1 ? 0 : kr_ctx_at(c, d->sb[k], i, (unsigned)((d->sp[k] + i) % c->P));
        KR_SYM(d->x[k], ct[cx], d->sb[k][i]); KR_FILL_CK(d->x[k]);
    }
    d->p = p; return 0;
}
/* bytes i.. of every segment (i >= maxlag) for as long as 2 S bytes of words are left; returns the position reached.
 * S, NL (the number of lags), WORD (mk 3) and O0 (one context) are constants at the call sites of kr_dec_syms, so
 * each call compiles to its own loop with the states in registers; other shapes take the general call. */
static inline __attribute__((always_inline)) size_t kr_run(const kr_ctx *c, const kr_dtab *const *ct, kr_dst *d, size_t i, const size_t sl,
                                                           const unsigned S, const unsigned NL, const int WORD, const int O0) {
    uint32_t x[32]; uint8_t *sb[32]; unsigned sp[32]; size_t lg[4] = {1, 1, 1, 1}; const uint8_t *p = d->p, *const pe = d->pe;
    for (unsigned k = 0; k < S; k++) { x[k] = d->x[k]; sb[k] = d->sb[k]; sp[k] = d->sp[k]; }
    for (unsigned l = 0; l < NL; l++) lg[l] = c->lag[l];
    if (O0) {
        const kr_dtab *t = ct[0];
        for (; i < sl && (size_t)(pe - p) >= 2 * (size_t)S; i++) for (unsigned k = 0; k < S; k++) { KR_SYM(x[k], t, sb[k][i]); KR_FILL(x[k]); }   /* measured faster than the unrolled form (GCC 13, x86-64) */
    } else {
        const unsigned P = c->P; unsigned pi = (unsigned)(i % P);
        const uint8_t *mag = c->mag; const uint16_t *qe = c->qe; const uint16_t (*qbp)[256] = c->qbp;
#define KR_ALL(ONE) do { if (S == 8) { ONE(0); ONE(1); ONE(2); ONE(3); ONE(4); ONE(5); ONE(6); ONE(7); } else for (unsigned k = 0; k < S; k++) ONE(k); } while (0)
#define KR_ONE(K) do { uint8_t *q = sb[K] + i; unsigned ph = sp[K] + pi, e = 0; if (ph >= P) ph -= P; \
            if (WORD) for (unsigned l = 0; l < NL; l++) e += kr_wcls(q - ph - lg[l]); \
            else for (unsigned l = 0; l < NL; l++) e += mag[*(q - lg[l])]; \
            const unsigned cx = (unsigned)qe[e] + qbp[ph][q[-1]]; \
            KR_SYM(x[K], ct[cx], *q); KR_FILL(x[K]); } while (0)
        for (; i < sl && (size_t)(pe - p) >= 2 * (size_t)S; i++) { KR_ALL(KR_ONE); if (++pi == P) pi = 0; }
#undef KR_ONE
#undef KR_ALL
    }
    for (unsigned k = 0; k < S; k++) d->x[k] = x[k];
    d->p = p; return i;
}
/* mk 3 with the lags 2, s, s + 2, s - 2 (left, up, up-left, up-right word) and every segment starting on a word:
 * whole words from an even i (i >= maxlag) for as long as 4 S bytes of words are left. Both bytes of a word share
 * e, and of its four classes three are carried over from the word before, so a word costs two class computations
 * (the up-right word and itself) instead of eight. Returns the position reached. */
static size_t kr_run_words(const kr_ctx *c, const kr_dtab *const *ct, kr_dst *d, size_t i, const size_t sl) {
    enum { S = 8 };
    uint32_t x[S]; uint8_t *sb[S]; unsigned cl[S], cul[S], cu[S]; const uint8_t *p = d->p, *const pe = d->pe;
    const size_t s = c->lag[1]; const uint16_t *qe = c->qe, *qb = c->qb; const unsigned m0 = c->pm[0];
    for (unsigned k = 0; k < S; k++) {
        x[k] = d->x[k]; sb[k] = d->sb[k]; const uint8_t *q = sb[k] + i;
        cl[k] = kr_wcls(q - 2); cul[k] = kr_wcls(q - s - 2); cu[k] = kr_wcls(q - s);
    }
    unsigned cur[S], e[S];                                                  /* the decode order stays byte i of every segment, then byte i + 1 */
#define KR_HI(K) do { uint8_t *q = sb[K] + i; cur[K] = kr_wcls(q - s + 2); e[K] = qe[cl[K] + cul[K] + cu[K] + cur[K]]; \
        KR_SYM(x[K], ct[e[K] + (qb[q[-1]] & m0)], q[0]); KR_FILL(x[K]); } while (0)
#define KR_LO(K) do { uint8_t *q = sb[K] + i; KR_SYM(x[K], ct[e[K] + qb[q[0]] + 1], q[1]); KR_FILL(x[K]); \
        cl[K] = kr_wcls(q); cul[K] = cu[K]; cu[K] = cur[K]; } while (0)
    for (; i + 1 < sl && (size_t)(pe - p) >= 4 * (size_t)S; i += 2) {
        KR_HI(0); KR_HI(1); KR_HI(2); KR_HI(3); KR_HI(4); KR_HI(5); KR_HI(6); KR_HI(7);
        KR_LO(0); KR_LO(1); KR_LO(2); KR_LO(3); KR_LO(4); KR_LO(5); KR_LO(6); KR_LO(7);
    }
#undef KR_HI
#undef KR_LO
    for (unsigned k = 0; k < S; k++) d->x[k] = x[k];
    d->p = p; return i;
}
/* Four lags of byte magnitudes (mk 0..2) of which the last three are far (the row above an image residual: up,
 * up-left, up-right): their sum is computed for a batch of positions at a time, a plain loop over decoded bytes that
 * the compiler vectorises, and the symbol loop adds the near lag. A batch is no longer than the nearest far lag, so
 * every byte it reads is decoded. i >= maxlag; runs for as long as 2 S bytes of words are left; returns the position. */
#define KR_FB 2048
static __attribute__((noinline)) size_t kr_run_far(const kr_ctx *c, const kr_dtab *const *ct, kr_dst *d, size_t i, const size_t sl) {
    enum { S = 8 };
    uint32_t x[S]; uint8_t *sb[S]; unsigned sp[S]; uint16_t U[S][KR_FB + 40];                                   /* rows 4176 bytes apart: not the same cache set for every state */ const uint8_t *p = d->p, *const pe = d->pe;
    const size_t l0 = c->lag[0], l1 = c->lag[1], l2 = c->lag[2], l3 = c->lag[3];
    size_t B = l1 < l2 ? l1 : l2; if (l3 < B) B = l3;
    if (B > KR_FB) B = KR_FB;
    const uint8_t *mag = c->mag; const uint16_t *qe = c->qe; const uint16_t (*qbp)[256] = c->qbp; const unsigned P = c->P, mk = c->mk;
    for (unsigned k = 0; k < S; k++) { x[k] = d->x[k]; sb[k] = d->sb[k]; sp[k] = d->sp[k]; }
    while (i < sl && (size_t)(pe - p) >= 2 * (size_t)S) {
        const size_t b = sl - i < B ? sl - i : B; size_t j;
        for (unsigned k = 0; k < S; k++) {
            const uint8_t *restrict a1 = sb[k] + i - l1, *restrict a2 = sb[k] + i - l2, *restrict a3 = sb[k] + i - l3; uint16_t *restrict u = U[k];
            if (mk == 0) for (j = 0; j < b; j++) u[j] = (uint16_t)(((a1[j] + 1u) >> 1) + ((a2[j] + 1u) >> 1) + ((a3[j] + 1u) >> 1));
            else if (mk == 1) for (j = 0; j < b; j++) u[j] = (uint16_t)((a1[j] < 128 ? a1[j] : 256u - a1[j]) + (a2[j] < 128 ? a2[j] : 256u - a2[j]) + (a3[j] < 128 ? a3[j] : 256u - a3[j]));
            else for (j = 0; j < b; j++) u[j] = (uint16_t)(a1[j] + a2[j] + a3[j]);
        }
        unsigned pi = (unsigned)(i % P);
#define KR_ONE(K) do { uint8_t *q = sb[K] + i + j; unsigned ph = sp[K] + pi; if (ph >= P) ph -= P; \
            const unsigned cx = (unsigned)qe[U[K][j] + mag[*(q - l0)]] + qbp[ph][q[-1]]; \
            KR_SYM(x[K], ct[cx], *q); KR_FILL(x[K]); } while (0)
        for (j = 0; j < b && (size_t)(pe - p) >= 2 * (size_t)S; j++) {
            KR_ONE(0); KR_ONE(1); KR_ONE(2); KR_ONE(3); KR_ONE(4); KR_ONE(5); KR_ONE(6); KR_ONE(7);
            if (++pi == P) pi = 0;
        }
#undef KR_ONE
        i += j;
        if (j < b) break;
    }
    for (unsigned k = 0; k < S; k++) d->x[k] = x[k];
    d->p = p; return i;
}
/* the symbols of one chunk: p[0..pe) = states and words, o[0..m) the output, ct[context] = its table */
static int kr_dec_syms(const kr_ctx *c, const kr_dtab *const *ct, const uint8_t *p, const uint8_t *pe, uint8_t *o, size_t m) {
    const unsigned S = 1u << c->ls, P = c->P; const size_t sl = m / S, head = c->N == 1 ? 0 : c->maxlag < sl ? c->maxlag : sl;
    kr_dst d;
    if ((size_t)(pe - p) < 4 * (size_t)S) return -1;
    for (unsigned k = 0; k < S; k++) { d.x[k] = kr_rd32(p); p += 4; if (d.x[k] < KR_L) return -1; d.sb[k] = o + (size_t)k * sl; d.sp[k] = (unsigned)(((size_t)k * sl) % P); }
    d.p = p; d.pe = pe;
    const int shape = c->N == 1 ? (S == 8) : S == 8 && c->nlag == 4 ? (c->mk == 3 ? 4 : 3) : S == 8 && c->nlag == 0 ? 2 : 5;
    const int wordrun = shape == 4 && !(sl & 1) && c->lag[0] == 2 && c->lag[1] >= 4 && c->lag[2] == c->lag[1] + 2 && c->lag[3] == c->lag[1] - 2;
    const int farrun = shape == 3 && c->lag[1] >= 16 && c->lag[2] >= 16 && c->lag[3] >= 16;
    for (size_t i = 0; i < sl;) {
        if (i < head || (size_t)(d.pe - d.p) < 2 * (size_t)S) { if (kr_round_ck(c, ct, &d, i, S)) return -1; i++; continue; }
        if (farrun) { i = kr_run_far(c, ct, &d, i, sl); continue; }
        if (wordrun) {                                                      /* whole words from an even position, else one checked byte */
            if (!(i & 1) && i + 1 < sl && (size_t)(d.pe - d.p) >= 4 * (size_t)S) i = kr_run_words(c, ct, &d, i, sl);
            else { if (kr_round_ck(c, ct, &d, i, S)) return -1; i++; }
            continue;
        }
        switch (shape) {                                                    /* at least one byte per segment is decoded */
            case 0: i = kr_run(c, ct, &d, i, sl, S, 0, 0, 1); break;          /* order 0 */
            case 1: i = kr_run(c, ct, &d, i, sl, 8, 0, 0, 1); break;
            case 2: i = kr_run(c, ct, &d, i, sl, 8, 0, 0, 0); break;          /* previous byte and phase (order 1) */
            case 3: i = kr_run(c, ct, &d, i, sl, 8, 4, 0, 0); break;          /* four neighbours (image residuals) */
            case 4: i = kr_run(c, ct, &d, i, sl, 8, 4, 1, 0); break;          /* four neighbouring 16-bit words */
            default: i = kr_run(c, ct, &d, i, sl, S, c->nlag, c->mk == 3, 0); break;
        }
    }
    {   /* the bytes left over belong to the last segment */
        uint8_t *seg = d.sb[S - 1]; const size_t len = m - (size_t)(S - 1) * sl; uint32_t y = d.x[S - 1];
        p = d.p;
        for (size_t i = sl; i < len; i++) {
            const unsigned cx = c->N == 1 ? 0 : kr_ctx_at(c, seg, i, (unsigned)((d.sp[S - 1] + i) % P));
            KR_SYM(y, ct[cx], seg[i]); KR_FILL_CK(y);
        }
        d.x[S - 1] = y;
    }
    for (unsigned k = 0; k < S; k++) if (d.x[k] != KR_L) return -1;         /* back where the encoder started */
    return p == pe ? 0 : -1;
}
int kolmo_krans_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0, o = 0; kr_dtab *T = NULL; unsigned tcap = 0; int rc = -1;
    kr_ctx *c = malloc(sizeof *c); const kr_dtab **ct = malloc(KRANS_NCTX_MAX * sizeof *ct); uint8_t *cmap = malloc(KRANS_NCTX_MAX);
    if (!c || !ct || !cmap) goto out;
    while (o < n) {
        uint64_t m, z; if (i >= cn) goto out;
        const uint8_t kind = in[i++];
        if (kr_uv_get(in, cn, &i, &m) || m == 0 || m > KRANS_CHUNK_MAX || m > n - o) goto out;
        if (kind == 0) { if (m > cn - i) goto out; memcpy(out + o, in + i, (size_t)m); i += (size_t)m; o += (size_t)m; continue; }
        if (kind == 1) { if (i >= cn) goto out; memset(out + o, in[i++], (size_t)m); o += (size_t)m; continue; }
        if (kind != 2 || cn - i < 3) goto out;
        const uint8_t b0 = in[i++], b1 = in[i++];
        memset(c, 0, offsetof(kr_ctx, N));
        c->ls = b0 & 7; c->P = ((b0 >> 3) & 7) + 1u; c->mb = b0 >> 6;
        c->eq = b1 & 3; c->bq = (b1 >> 2) & 3; c->mk = (b1 >> 4) & 3; c->pf = (b1 >> 6) & 1;
        if (b1 >> 7) goto out;
        if (c->eq) {
            if (i >= cn) goto out;
            c->nlag = in[i++]; if (c->nlag < 1 || c->nlag > 4) goto out;
            for (unsigned q = 0; q < c->nlag; q++) { uint64_t v; if (kr_uv_get(in, cn, &i, &v) || v < 1 || v > KRANS_CHUNK_MAX) goto out; c->lag[q] = (uint32_t)v; }
        }
        if (kr_ctx_init(c) || (c->N > 1 && m < KR_CTX_M) || i >= cn) goto out;
        const unsigned nt = in[i++] + 1u;
        if (nt > KR_MAXT || nt > c->N || nt > 1 + m / 16) goto out;
        if (nt > tcap) { kr_dtab *q = realloc(T, (size_t)nt * sizeof *T); if (!q) goto out; T = q; tcap = nt; }
        kr_br r = {in + i, in + cn, 0, 0, 0}; const unsigned w = kr_bitlen(nt - 1);
        for (unsigned q = 0, pv = 0; q < c->N; q++) { if (w && !kr_get(&r, 1)) { pv = kr_get(&r, w); if (pv >= nt) goto out; } cmap[q] = (uint8_t)pv; }
        for (unsigned t = 0; t < nt; t++) if (kr_dec_table(&r, c->mbits, &T[t])) goto out;
        if (r.bad || r.acc) goto out;                                       /* the padding bits are zero */
        i = (size_t)(r.p - in);
        if (kr_uv_get(in, cn, &i, &z) || z > cn - i || (z & 1)) goto out;
        for (unsigned q = 0; q < c->N; q++) ct[q] = &T[cmap[q]];
        if (kr_dec_syms(c, ct, in + i, in + i + z, out + o, (size_t)m)) goto out;
        i += (size_t)z; o += (size_t)m;
    }
    rc = i == cn ? 0 : -1;
out:
    free(T); free(c); free(ct); free(cmap); return rc;
}

/* ================================================= encoder ================================================= */
typedef struct { uint64_t acc, bits; unsigned nb; uint8_t *p, *e; } kr_bw;     /* p NULL: count only */
static void kr_put(kr_bw *w, uint32_t v, unsigned n) {
    w->bits += n; if (!w->p) return;
    w->acc |= (uint64_t)v << w->nb; w->nb += n;
    while (w->nb >= 8) { if (w->p < w->e) *w->p++ = (uint8_t)w->acc; w->acc >>= 8; w->nb -= 8; }
}
static inline void kr_put_flush(kr_bw *w) { const unsigned pad = (unsigned)(-(int64_t)w->bits & 7); if (pad) kr_put(w, 0, pad); }

typedef struct { uint64_t rcp; uint32_t bias; uint16_t cmpl, f; } kr_esym;
typedef struct {
    kr_ctx c; unsigned T; double bits;                 /* bits: header + ideal payload + states, the encoder's score */
    uint8_t *cmap; uint16_t *f; uint8_t *rest;          /* N entries; T x 256 frequencies; the rest symbol of each table */
    uint8_t *hdr; size_t hn;                            /* b0 .. end of the bit stream */
} kr_plan;
typedef struct {
    size_t mcap, hcap; unsigned maxT, lim; int oom;   /* lim: tables allowed in this chunk */
    kr_plan pl[2];
    uint16_t *ctx; const kr_plan *ctx_of;               /* the context of every byte under plan ctx_of (N > 1) */
    uint32_t *hist; size_t histn;                       /* contexts x 256 counts */
    uint32_t *ch; uint64_t ctot[KR_MAXT];               /* tables x 256 counts */
    double *cphi;                                       /* tables x 256: x log2 x of ch (clustering) */
    double *phi; int phi_ok;                            /* x log2 x for x < 4096 */
    float *len;                                         /* tables x 256 code lengths in bits (absent: huge) */
    uint64_t *ord; uint8_t *cmap2, *used; uint16_t *words; kr_esym *es;
    uint32_t h0[256];
} kr_enc;
static KR_COLD void kr_enc_free(kr_enc *E) {
    for (int q = 0; q < 2; q++) { free(E->pl[q].cmap); free(E->pl[q].f); free(E->pl[q].rest); free(E->pl[q].hdr); }
    free(E->ctx); free(E->hist); free(E->ch); free(E->cphi); free(E->phi); free(E->len); free(E->ord); free(E->cmap2); free(E->used); free(E->words); free(E->es);
}
/* buffers for chunks of up to mcap bytes and maxT tables; ctxs = 0: order 0 only (one context, no context arrays) */
static KR_COLD int kr_enc_init(kr_enc *E, size_t mcap, unsigned maxT, int ctxs) {
    const size_t ncap = ctxs ? KRANS_NCTX_MAX : 1;
    memset(E, 0, sizeof *E); E->mcap = mcap; E->maxT = maxT; E->hcap = ncap * 2 + 64 + (size_t)maxT * 700;
    for (int q = 0; q < 2; q++) {
        kr_plan *p = &E->pl[q];
        p->cmap = malloc(ncap); p->f = malloc((size_t)maxT * 256 * sizeof(uint16_t)); p->rest = malloc(maxT); p->hdr = malloc(E->hcap);
        if (!p->cmap || !p->f || !p->rest || !p->hdr) return -1;
    }
    E->ch = malloc((size_t)maxT * 256 * sizeof(uint32_t)); E->cphi = malloc((size_t)maxT * 256 * sizeof(double)); E->len = malloc((size_t)maxT * 256 * sizeof(float));
    E->ord = malloc(ncap * sizeof(uint64_t)); E->cmap2 = malloc(ncap); E->used = malloc(ncap); E->words = malloc((mcap + 64) * sizeof(uint16_t));
    E->es = malloc((size_t)maxT * 256 * sizeof(kr_esym)); E->ctx = ctxs ? malloc((mcap + 1) * sizeof(uint16_t)) : NULL;
    if (ctxs && !E->ctx) return -1;
    return E->ch && E->cphi && E->len && E->ord && E->cmap2 && E->used && E->words && E->es ? 0 : -1;
}
static inline double kr_phi(const kr_enc *E, uint64_t x) { return x < 4096 ? E->phi[x] : (double)x * log2((double)x); }

/* nearest frequency with at most mbits bits below its top bit, v >= 1 */
static inline uint32_t kr_quant(double v, unsigned mbits) {
    const uint32_t iv = v >= KR_M - 1 ? KR_M - 1 : (uint32_t)v;
    const unsigned e = kr_bitlen(iv), sh = e - 1 > mbits ? e - 1 - mbits : 0;
    const uint32_t lo = (iv >> sh) << sh, hi = lo + (1u << sh);
    return hi < KR_M && v - (double)lo > (double)hi - v ? hi : lo;
}
/* frequencies of one table from its counts: every present symbol >= 1, coded ones representable, the most frequent
 * symbol takes the rest. The others are scaled so the rest lands where the same scale puts the top symbol. */
static void kr_norm(const uint32_t *cnt, uint64_t tot, unsigned mbits, uint16_t *f, uint8_t *rest) {
    unsigned big = 0, nz = 0;
    for (unsigned s = 0; s < 256; s++) { f[s] = 0; if (cnt[s]) { nz++; if (cnt[s] > cnt[big] || !cnt[big]) big = s; } }
    *rest = (uint8_t)big;
    if (nz < 2) { f[big] = KR_M; return; }
    const double base = (double)KR_M / (double)tot; double scale = 1.0; uint32_t sum = 0;
    for (int it = 0;; it++) {
        sum = 0;
        for (unsigned s = 0; s < 256; s++) if (cnt[s] && s != big) { double v = cnt[s] * base * scale; if (v < 1) v = 1; f[s] = (uint16_t)kr_quant(v, mbits); sum += f[s]; }
        const double rb = cnt[big] * base * scale;
        if (it >= 4) { if (sum < KR_M) break; scale *= 0.97; continue; }   /* always ends: at a small scale the others are all 1 */
        scale *= (double)KR_M / ((double)sum + rb);
    }
    f[big] = (uint16_t)(KR_M - sum);
}
/* the table part of the header for frequencies f (0 = absent) */
static KR_COLD void kr_put_table(kr_bw *w, const uint16_t *f, unsigned rest, unsigned mbits) {
    unsigned lo = 0, hi = 255; while (!f[lo]) lo++;
    while (!f[hi]) hi--;
    kr_put(w, lo, 8); kr_put(w, hi, 8);
    int pc = 0;
    for (unsigned s = lo; s <= hi; s++) {
        if (s == rest) { kr_put(w, 31, 5); kr_put(w, KR_REST, 4); continue; }
        const int e = (int)kr_bitlen(f[s]), d = e - pc;
        if (d == 0) kr_put(w, 0, 1); else if (d == -1) kr_put(w, 1, 2); else if (d == 1) kr_put(w, 3, 3); else if (d == -2) kr_put(w, 7, 4);
        else if (d == 2) kr_put(w, 15, 5); else { kr_put(w, 31, 5); kr_put(w, (uint32_t)e, 4); }
        if (e) { const unsigned nm = (unsigned)e - 1 < mbits ? (unsigned)e - 1 : mbits; kr_put(w, ((uint32_t)f[s] >> ((unsigned)e - 1 - nm)) & ((1u << nm) - 1), nm); }
        pc = e;
    }
}
static KR_COLD void kr_put_header(kr_bw *w, const kr_plan *pl) {
    const kr_ctx *c = &pl->c;
    kr_put(w, c->ls | (c->P - 1) << 3 | c->mb << 6, 8); kr_put(w, c->eq | c->bq << 2 | c->mk << 4 | c->pf << 6, 8);
    if (c->eq) {
        kr_put(w, c->nlag, 8);
        for (unsigned q = 0; q < c->nlag; q++) { uint8_t t[8]; const size_t k = kr_uv_put(t, c->lag[q]); for (size_t a = 0; a < k; a++) kr_put(w, t[a], 8); }
    }
    kr_put(w, pl->T - 1, 8);
    const unsigned wd = kr_bitlen(pl->T - 1);
    if (wd) for (unsigned q = 0, pv = 0; q < c->N; q++) { if (pl->cmap[q] == pv) kr_put(w, 1, 1); else { pv = pl->cmap[q]; kr_put(w, 0, 1); kr_put(w, pv, wd); } }
    for (unsigned t = 0; t < pl->T; t++) kr_put_table(w, pl->f + (size_t)t * 256, pl->rest[t], c->mbits);
    kr_put_flush(w);
}
/* tables, code lengths and the exact score of a plan from its context map: counts per table (empty tables are
 * dropped and the map renumbered), frequencies, then header bits + sum of counts x code lengths */
static double kr_eval(kr_enc *E, kr_plan *pl, const uint32_t *hist) {
    const unsigned N = pl->c.N; unsigned T = pl->T; uint8_t ren[KR_MAXT];
    memset(E->ch, 0, (size_t)T * 256 * sizeof(uint32_t)); memset(E->ctot, 0, T * sizeof(uint64_t));
    for (unsigned q = 0; q < N; q++) {
        const uint32_t *h = hist + (size_t)q * 256; uint32_t *d = E->ch + (size_t)pl->cmap[q] * 256; uint64_t t = 0;
        for (unsigned s = 0; s < 256; s++) { d[s] += h[s]; t += h[s]; }
        E->ctot[pl->cmap[q]] += t; E->used[q] = t != 0;
    }
    unsigned nt = 0;
    for (unsigned t = 0; t < T; t++) {
        ren[t] = (uint8_t)nt; if (!E->ctot[t]) { ren[t] = 0; continue; }
        if (nt != t) { memcpy(E->ch + (size_t)nt * 256, E->ch + (size_t)t * 256, 256 * sizeof(uint32_t)); E->ctot[nt] = E->ctot[t]; }
        nt++;
    }
    if (nt != T) { for (unsigned q = 0; q < N; q++) pl->cmap[q] = ren[pl->cmap[q]]; pl->T = T = nt; }
    for (unsigned q = 0; q < N; q++) if (!E->used[q]) pl->cmap[q] = q ? pl->cmap[q - 1] : 0;   /* unused contexts cost one bit */
    double bits = 0;
    for (unsigned t = 0; t < T; t++) {
        const uint32_t *h = E->ch + (size_t)t * 256; uint16_t *f = pl->f + (size_t)t * 256; float *ln = E->len + (size_t)t * 256;
        kr_norm(h, E->ctot[t], pl->c.mbits, f, &pl->rest[t]);
        for (unsigned s = 0; s < 256; s++) { ln[s] = f[s] ? (float)(KR_SB - log2((double)f[s])) : 1e30f; if (h[s]) bits += (double)h[s] * ln[s]; }
    }
    kr_bw w = {0}; kr_put_header(&w, pl);
    return bits + (double)w.bits + 32.0 * (1u << pl->c.ls);
}
static int kr_cmp64(const void *a, const void *b) { const uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? 1 : x > y ? -1 : 0; }
/* contexts -> tables, greedily from the most used context down: a context joins the table where the extra code
 * length is smallest, or opens a table when that costs less than the table's own bits (tbit per symbol) */
static void kr_cluster(kr_enc *E, kr_plan *pl, const uint32_t *hist, double tbit) {
    const unsigned N = pl->c.N; unsigned na = 0, T = 0;
    memset(pl->cmap, 0, N);
    for (unsigned q = 0; q < N; q++) {
        const uint32_t *h = hist + (size_t)q * 256; uint64_t t = 0;
        for (unsigned s = 0; s < 256; s++) t += h[s];
        if (t) E->ord[na++] = t << 12 | q;
    }
    qsort(E->ord, na, sizeof(uint64_t), kr_cmp64);
    for (unsigned a = 0; a < na; a++) {
        const unsigned q = (unsigned)(E->ord[a] & 4095); const uint64_t C = E->ord[a] >> 12; const uint32_t *h = hist + (size_t)q * 256;
        uint8_t ss[256]; uint32_t sc[256]; double sp[256]; unsigned ns = 0;
        for (unsigned s = 0; s < 256; s++) if (h[s]) { ss[ns] = (uint8_t)s; sc[ns] = h[s]; sp[ns] = kr_phi(E, h[s]); ns++; }
        double best = T < E->lim ? ns * tbit + 25.0 : 1e300; int bk = -1; const double phiC = kr_phi(E, C);
        for (unsigned k = 0; k < T; k++) {
            const uint32_t *kh = E->ch + (size_t)k * 256; const double *kp = E->cphi + (size_t)k * 256; const uint64_t K = E->ctot[k];
            double d = kr_phi(E, K + C) - kr_phi(E, K) - phiC; unsigned nw = 0;
            for (unsigned b = 0; b < ns; b++) { const uint32_t ks = kh[ss[b]]; if (ks) d -= kr_phi(E, (uint64_t)ks + sc[b]) - kp[ss[b]] - sp[b]; else nw++; }
            d += nw * tbit;
            if (d < best) { best = d; bk = (int)k; }
        }
        if (bk < 0) { bk = (int)T++; memset(E->ch + (size_t)bk * 256, 0, 256 * sizeof(uint32_t)); memset(E->cphi + (size_t)bk * 256, 0, 256 * sizeof(double)); E->ctot[bk] = 0; }
        uint32_t *kh = E->ch + (size_t)bk * 256; double *kp = E->cphi + (size_t)bk * 256;
        for (unsigned b = 0; b < ns; b++) { kh[ss[b]] += sc[b]; kp[ss[b]] = kr_phi(E, kh[ss[b]]); }
        E->ctot[bk] += C; pl->cmap[q] = (uint8_t)bk;
    }
    pl->T = T ? T : 1;
}
/* every context to the table that codes it shortest with the tables as built; returns the number moved */
static unsigned kr_reassign(kr_enc *E, kr_plan *pl, const uint32_t *hist) {
    const unsigned N = pl->c.N, T = pl->T; unsigned moved = 0;
    for (unsigned q = 0; q < N; q++) {
        const uint32_t *h = hist + (size_t)q * 256; uint8_t ss[256]; float sc[256]; unsigned ns = 0;
        for (unsigned s = 0; s < 256; s++) if (h[s]) { ss[ns] = (uint8_t)s; sc[ns] = (float)h[s]; ns++; }
        if (!ns) continue;
        unsigned bk = pl->cmap[q]; float best = 0;
        { const float *ln = E->len + (size_t)bk * 256; for (unsigned b = 0; b < ns; b++) best += sc[b] * ln[ss[b]]; }
        for (unsigned k = 0; k < T; k++) {
            if (k == pl->cmap[q]) continue;
            const float *ln = E->len + (size_t)k * 256; float d = 0;
            for (unsigned b = 0; b < ns && d < best; b++) d += sc[b] * ln[ss[b]];
            if (d < best) { best = d; bk = k; }
        }
        if (bk != pl->cmap[q]) { pl->cmap[q] = (uint8_t)bk; moved++; }
    }
    return moved;
}
static void kr_fill_ctx(const kr_ctx *c, const uint8_t *in, size_t m, uint16_t *ctx) {
    const unsigned S = 1u << c->ls; const size_t sl = m / S;
    for (unsigned k = 0; k < S; k++) {
        const size_t b = (size_t)k * sl, len = k == S - 1 ? m - b : sl, head = c->maxlag < len ? c->maxlag : len;
        const uint8_t *seg = in + b; unsigned ph = (unsigned)(b % c->P); size_t i = 0;
        for (; i < head; i++) { ctx[b + i] = (uint16_t)kr_ctx_at(c, seg, i, ph); if (++ph == c->P) ph = 0; }
        for (; i < len; i++) { ctx[b + i] = (uint16_t)kr_ctx_in(c, seg + i, ph); if (++ph == c->P) ph = 0; }
    }
}
/* build a plan whose context fields are set: contexts, counts, tables, header. Returns 0, or -1 when the fields
 * are not valid or memory ran out (E->oom) */
static int kr_plan_build(kr_enc *E, kr_plan *pl, const uint8_t *in, size_t m, int refine) {
    kr_ctx *c = &pl->c; const uint32_t *hist = E->h0;
    if (kr_ctx_init(c) || (c->N > 1 && m < KR_CTX_M)) return -1;
    if (c->N > 1) {
        if (c->N > E->histn) { free(E->hist); E->hist = malloc((size_t)c->N * 256 * sizeof(uint32_t)); E->histn = E->hist ? c->N : 0; if (!E->hist) { E->oom = 1; return -1; } }
        memset(E->hist, 0, (size_t)c->N * 256 * sizeof(uint32_t));
        kr_fill_ctx(c, in, m, E->ctx); E->ctx_of = pl;
        for (size_t j = 0; j < m; j++) E->hist[(size_t)E->ctx[j] * 256 + in[j]]++;
        hist = E->hist;
        if (!E->phi_ok) {
            if (!E->phi && !(E->phi = malloc(4096 * sizeof(double)))) { E->oom = 1; return -1; }
            E->phi[0] = 0; for (unsigned v = 1; v < 4096; v++) E->phi[v] = v * log2((double)v);
            E->phi_ok = 1;
        }
        kr_cluster(E, pl, hist, 3.0 + 0.5 * (c->mbits > 6 ? 6 : c->mbits));
    } else { pl->cmap[0] = 0; pl->T = 1; }
    pl->bits = kr_eval(E, pl, hist);
    for (int it = 0; it < refine && pl->T > 1; it++) {
        const unsigned T0 = pl->T; memcpy(E->cmap2, pl->cmap, c->N);
        if (!kr_reassign(E, pl, hist)) break;
        const double b2 = kr_eval(E, pl, hist);
        if (b2 >= pl->bits) { memcpy(pl->cmap, E->cmap2, c->N); pl->T = T0; pl->bits = kr_eval(E, pl, hist); break; }
        pl->bits = b2;
    }
    kr_bw w = {0, 0, 0, pl->hdr, pl->hdr + E->hcap}; kr_put_header(&w, pl);
    pl->hn = (size_t)(w.bits >> 3);
    return pl->hn <= E->hcap ? 0 : -1;
}
static inline uint64_t kr_mulhi(uint64_t a, uint64_t b) { return (uint64_t)(((unsigned __int128)a * b) >> 64); }
/* the coded chunk of plan pl after the kind byte and m: header, z, states and words. Returns its size or 0 (cap). */
static size_t kr_emit(kr_enc *E, const kr_plan *pl, const uint8_t *in, size_t m, uint8_t *out, size_t cap) {
    const kr_ctx *c = &pl->c; const unsigned S = 1u << c->ls; const size_t sl = m / S;
    if (c->N > 1 && E->ctx_of != pl) { kr_fill_ctx(c, in, m, E->ctx); E->ctx_of = pl; }   /* a later trial overwrote them */
    for (unsigned t = 0; t < pl->T; t++) {
        const uint16_t *f = pl->f + (size_t)t * 256; kr_esym *e = E->es + (size_t)t * 256; uint32_t cum = 0;
        for (unsigned s = 0; s < 256; s++) {
            if (!f[s]) continue;
            e[s].f = f[s]; e[s].cmpl = (uint16_t)(KR_M - f[s]);
            if (f[s] == 1) { e[s].rcp = ~0ull; e[s].bias = cum + KR_M - 1; }      /* mulhi gives x - 1: x + bias + (x - 1)(M - 1) = x M + cum */
            else { e[s].rcp = ~0ull / f[s] + 1; e[s].bias = cum; }                /* ceil(2^64 / f): exact quotient for 32-bit x */
            cum += f[s];
        }
    }
    uint16_t *we = E->words + E->mcap + 64, *w = we; uint32_t x[32];
    for (unsigned k = 0; k < S; k++) x[k] = KR_L;
    const uint16_t *ctx = E->ctx; const uint8_t *cmap = pl->cmap; const int one = c->N == 1;
#define KR_ENC(X, J) do { const kr_esym *y_ = E->es + (size_t)(one ? 0 : cmap[ctx[J]]) * 256 + in[J]; \
        if (((X) >> 20) >= y_->f) { *--w = (uint16_t)(X); (X) >>= 16; } \
        (X) = (X) + y_->bias + (uint32_t)kr_mulhi((X), y_->rcp) * y_->cmpl; } while (0)
    { uint32_t y = x[S - 1]; for (size_t j = m; j-- > (size_t)S * sl;) KR_ENC(y, j); x[S - 1] = y; }
    for (size_t i = sl; i-- > 0;) for (unsigned k = S; k-- > 0;) KR_ENC(x[k], (size_t)k * sl + i);
#undef KR_ENC
    const size_t nw = (size_t)(we - w), z = 4 * (size_t)S + 2 * nw; uint8_t zb[8]; const size_t zn = kr_uv_put(zb, z);
    if (cap < pl->hn + zn + z) return 0;
    memcpy(out, pl->hdr, pl->hn); memcpy(out + pl->hn, zb, zn);
    uint8_t *o = out + pl->hn + zn;
    for (unsigned k = 0; k < S; k++) { o[0] = (uint8_t)x[k]; o[1] = (uint8_t)(x[k] >> 8); o[2] = (uint8_t)(x[k] >> 16); o[3] = (uint8_t)(x[k] >> 24); o += 4; }
    for (size_t q = 0; q < nw; q++) { o[0] = (uint8_t)w[q]; o[1] = (uint8_t)(w[q] >> 8); o += 2; }
    return pl->hn + zn + z;
}
static unsigned kr_default_ls(size_t m) { return m >= 32768 ? 3 : m >= 4096 ? 2 : m >= 512 ? 1 : 0; }
/* one chunk. Returns the bytes written, 0 when cap is too small or memory ran out. */
static KR_COLD size_t kr_chunk(kr_enc *E, const kolmo_krans_params *pp, const uint8_t *in, size_t m, uint8_t *out, size_t cap) {
    uint8_t hb[8]; size_t hn = kr_uv_put(hb, m); unsigned nz = 0;
    memset(E->h0, 0, sizeof E->h0);
    for (size_t j = 0; j < m; j++) E->h0[in[j]]++;
    for (unsigned s = 0; s < 256; s++) nz += E->h0[s] != 0;
    if (nz == 1) { if (cap < 2 + hn) return 0; out[0] = 1; memcpy(out + 1, hb, hn); out[1 + hn] = in[0]; return 2 + hn; }
    kr_plan *best = NULL, *cur = &E->pl[0];
    E->ctx_of = NULL;
    E->lim = 1 + m / 16 < E->maxT ? (unsigned)(1 + m / 16) : E->maxT;
#define KR_TRY(REFINE) do { if (!kr_plan_build(E, cur, in, m, (REFINE))) { if (!best || cur->bits < best->bits) { best = cur; cur = &E->pl[best == &E->pl[0]]; } } \
        else if (E->oom) return 0; } while (0)
#define KR_SET(LS, PP, MB, EQ, BQ, MK, PF, NL) do { memset(&cur->c, 0, offsetof(kr_ctx, N)); cur->c.ls = (LS); cur->c.P = (PP); cur->c.mb = (MB); cur->c.eq = (EQ); \
        cur->c.bq = (BQ); cur->c.mk = (MK); cur->c.pf = (PF); cur->c.nlag = (NL); } while (0)
    unsigned ls = kr_default_ls(m); const unsigned mb = 0, step = pp->step ? pp->step : 1; const int ref = pp->effort >= 2 ? 2 : 1;
    if (pp->force && (pp->P > 1 || pp->eq || pp->bq) && !E->ctx) return 0;
    if (pp->force) {
        KR_SET(pp->ls, pp->P, pp->mb, pp->eq, pp->bq, pp->mk, pp->pf, pp->nlag); memcpy(cur->c.lag, pp->lag, sizeof cur->c.lag);
        KR_TRY(ref);
    } else {
        KR_SET(ls, 1, mb, 0, 0, 0, 0, 0); KR_TRY(0);                                    /* order 0 */
        const int img = pp->stride && step <= 8 && pp->stride % step == 0 && pp->stride >= 2 * step && m >= 2 * (size_t)pp->stride && (!pp->words || (step == 2 && !(m & 1)));
        if (img) while (ls && (m >> ls) < 8 * (size_t)pp->stride) ls--;      /* a segment's first row has no row above: at least 8 rows each */
        if (img && m >= KR_CTX_MIN) {
            /* image residuals: the magnitudes of the left, upper, upper-left and upper-right neighbours of the same
             * channel, with the sign and magnitude of the previous byte (the previous channel of the pixel; the left
             * neighbour of a one-channel image) */
            const unsigned pf = step > 1;
#define KR_IMG(EQ, BQ, PF) do { KR_SET(ls, step, mb, (EQ), (BQ), 0, (PF), 4); cur->c.lag[0] = step; cur->c.lag[1] = pp->stride; \
                cur->c.lag[2] = pp->stride + step; cur->c.lag[3] = pp->stride - step; KR_TRY(ref); } while (0)
            if (pp->words) {
                /* 16-bit residuals as (high, low) pairs: the half-octave classes of the four neighbouring words; the
                 * low byte also under its high byte. First the two bytes under their own tables alone. */
                KR_SET(ls, 2, mb, 0, 0, 0, 0, 0); KR_TRY(0);
                for (unsigned eq = 1; eq <= (pp->effort >= 2 ? 3u : 1u); eq += 2) {
                    KR_SET(ls, 2, mb, eq, 2, 3, 1, 4); cur->c.lag[0] = 2; cur->c.lag[1] = pp->stride; cur->c.lag[2] = pp->stride + 2; cur->c.lag[3] = pp->stride - 2;
                    KR_TRY(ref);
                }
            } else {
                KR_IMG(1, 3, pf);
                if (pp->effort >= 2) { if (pf) KR_IMG(1, 3, 0); KR_IMG(2, 3, pf); }
            }
#undef KR_IMG
        } else if (m >= KR_CTX_MIN) {
            KR_SET(ls, 1, mb, 0, 1, 0, 0, 0); KR_TRY(ref);                               /* order 1 */
        }
    }
#undef KR_TRY
#undef KR_SET
    size_t z = 0;
    if (best && best->bits / 8 + 1 + hn < (double)m && cap >= 1 + hn) { out[0] = 2; memcpy(out + 1, hb, hn); z = kr_emit(E, best, in, m, out + 1 + hn, cap - 1 - hn); if (z) z += 1 + hn; }
    if (!z || z >= 1 + hn + m) { if (cap < 1 + hn + m) return 0; out[0] = 0; memcpy(out + 1, hb, hn); memcpy(out + 1 + hn, in, m); z = 1 + hn + m; }
    return z;
}
size_t kolmo_krans_bound(size_t n) { return n + (n / 4096 + 1) * 6 + 16; }
size_t kolmo_krans_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, const kolmo_krans_params *pp) {
    static const kolmo_krans_params def = {0}; if (!pp) pp = &def;
    const unsigned step = pp->step ? pp->step : 1;
    size_t chunk = pp->chunk ? pp->chunk : pp->stride ? (2u << 20) : (128u << 10), o = 0;
    if (chunk > KRANS_CHUNK_MAX) chunk = KRANS_CHUNK_MAX;
    if (chunk < 4096) chunk = 4096;
    if (pp->stride && pp->stride % step == 0) {                 /* whole rows per segment where the chunk allows it */
        const size_t unit = (size_t)pp->stride << kr_default_ls(chunk);
        if (chunk >= 2 * unit) chunk = chunk / unit * unit;
    }
    if (chunk > n) chunk = n;
    if (!n) return 0;
    const int ctxs = pp->force || chunk >= KR_CTX_MIN;
    kr_enc E; unsigned maxT = pp->max_tables ? pp->max_tables : 64; if (maxT > KR_MAXT) maxT = KR_MAXT;
    if (maxT > 1 + chunk / 16) maxT = (unsigned)(1 + chunk / 16);
    if (!ctxs) maxT = 1;
    if (kr_enc_init(&E, chunk, maxT, ctxs)) { kr_enc_free(&E); return 0; }
    for (size_t i = 0; i < n;) {
        const size_t mm = n - i < chunk ? n - i : chunk;
        const size_t z = kr_chunk(&E, pp, in + i, mm, out + o, cap - o);
        if (!z) { o = 0; break; }
        o += z; i += mm;
    }
    kr_enc_free(&E); return o;
}
