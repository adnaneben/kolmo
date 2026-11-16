/* KRANS: static interleaved rANS (order 0 and order 1). See krans.h and KRANS.md.
 *
 * Coder: 32-bit states kept in [2^15, 2^31), 16-bit renormalisation, so a state reads at most one 16-bit word per
 * decoded symbol (the property that lets a SIMD decoder renormalise all lanes with one load and one permute).
 * Tables are static per block: the encoder counts, normalises to 2^prec, writes the table, codes. No adaptation. */
#define _GNU_SOURCE
#include "krans.h"

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define KR_X86 1
#endif
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define KR_NEON 1
#endif

#define KR_L (1u << 15)          /* lower bound of a normalised state */
#define KR_MINB 1024u
#define KR_MAXB (1u << 24)
#define KR_MAXTAB 257            /* order 1: up to 256 own contexts and one shared table */

enum { K_STORED = 0, K_RLE = 1, K_O0 = 2, K_O1 = 3 };

/* ---------------------------------------------------------------- small helpers */

static size_t put_uv(uint8_t *o, uint64_t v) {
    size_t k = 0;
    while (v >= 128) { o[k++] = (uint8_t)(v | 128); v >>= 7; }
    o[k++] = (uint8_t)v;
    return k;
}
static int get_uv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0;
    for (int sh = 0; sh < 64 && *i < n; sh += 7) {
        uint8_t c = p[(*i)++];
        r |= (uint64_t)(c & 127) << sh;
        if (!(c & 128)) { *v = r; return 0; }
    }
    return -1;
}
static inline uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }   /* little-endian hosts */
static inline uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static int ways_code(int w) { return w == 8 ? 0 : w == 16 ? 1 : w == 32 ? 2 : -1; }
static const int ways_of[4] = {8, 16, 32, 0};

static float kr_lg[(1 << 14) + 1];   /* log2(f) */
static pthread_once_t kr_once = PTHREAD_ONCE_INIT;
static void kr_init_tables(void);

/* ---------------------------------------------------------------- frequency normalisation */

/* Scale counts (present symbols syms[0..ns)) to sum 2^M, every present symbol >= 1. Rounds, then moves the
 * remaining difference one unit at a time to the symbol where it costs the fewest coded bits. */
static void kr_norm(const uint32_t *cnt, const uint8_t *syms, int ns, uint64_t tot, int M, uint32_t *f) {
    const uint32_t T = 1u << M;
    int64_t sum = 0; int big = syms[0];
    for (int i = 0; i < ns; i++) {
        int s = syms[i];
        uint64_t v = ((uint64_t)cnt[s] * T * 2 + tot) / (2 * tot);
        if (!v) v = 1;
        f[s] = (uint32_t)v; sum += v;
        if (cnt[s] > cnt[big]) big = s;
    }
    int64_t d = (int64_t)T - sum;
    if (d != 0 && (int64_t)f[big] > 16 * (d < 0 ? -d : d)) { f[big] = (uint32_t)((int64_t)f[big] + d); return; }
    while (d != 0) {
        int b = -1; double bc = 0;
        for (int i = 0; i < ns; i++) {
            int s = syms[i];
            if (d > 0) {          /* gain of +1 */
                double g = cnt[s] * log2((f[s] + 1.0) / f[s]);
                if (b < 0 || g > bc) { bc = g; b = s; }
            } else {
                if (f[s] <= 1) continue;
                double c = cnt[s] * log2((double)f[s] / (f[s] - 1.0));
                if (b < 0 || c < bc) { bc = c; b = s; }
            }
        }
        if (d > 0) { f[b]++; d--; } else { f[b]--; d++; }
    }
}

static double kr_bits(const uint32_t *cnt, const uint8_t *syms, int ns, const uint32_t *f, int M) {
    double b = 0;
    for (int i = 0; i < ns; i++) { int s = syms[i]; b += (double)cnt[s] * (M - kr_lg[f[s]]); }
    return b;
}

/* ---------------------------------------------------------------- table header */

/* Alphabet: byte r = number of runs (0..16) then r pairs (gap from the previous run's end, length - 1);
 * or 255 then a 32-byte bitmap when that is shorter. */
static size_t put_alpha(uint8_t *o, const uint8_t *syms, int ns) {
    int runs = 0;
    for (int i = 0; i < ns; i++) if (i == 0 || syms[i] != syms[i - 1] + 1) runs++;
    if (1 + 2 * runs <= 33) {
        size_t k = 1; int prev_end = 0; o[0] = (uint8_t)runs;
        for (int i = 0; i < ns;) {
            int j = i; while (j + 1 < ns && syms[j + 1] == syms[j] + 1) j++;
            o[k++] = (uint8_t)(syms[i] - prev_end); o[k++] = (uint8_t)(j - i);
            prev_end = syms[j] + 1; i = j + 1;
        }
        return k;
    }
    o[0] = 255; memset(o + 1, 0, 32);
    for (int i = 0; i < ns; i++) o[1 + (syms[i] >> 3)] |= (uint8_t)(1 << (syms[i] & 7));
    return 33;
}
static int get_alpha(const uint8_t *p, size_t n, size_t *i, uint8_t *syms, int *ns) {
    if (*i >= n) return -1;
    int r = p[(*i)++], k = 0;
    if (r == 255) {
        if (n - *i < 32) return -1;
        for (int s = 0; s < 256; s++) if (p[*i + (s >> 3)] >> (s & 7) & 1) syms[k++] = (uint8_t)s;
        *i += 32;
    } else {
        if (r > 16 || n - *i < (size_t)2 * r) return -1;
        int at = 0;
        for (int q = 0; q < r; q++) {
            int st = at + p[*i], len = p[*i + 1] + 1; *i += 2;
            if (q > 0 && st == at) return -1;          /* runs must be separated (canonical form) */
            if (st + len > 256) return -1;
            for (int s = st; s < st + len; s++) syms[k++] = (uint8_t)s;
            at = st + len;
        }
    }
    *ns = k;
    return 0;
}
/* One table: alphabet, then frequency - 1 (uv) for every present symbol but the last, whose frequency is implied. */
static size_t put_table(uint8_t *o, const uint8_t *syms, int ns, const uint32_t *f) {
    size_t k = put_alpha(o, syms, ns);
    for (int i = 0; i + 1 < ns; i++) k += put_uv(o + k, f[syms[i]] - 1);
    return k;
}
static size_t table_size(const uint8_t *syms, int ns, const uint32_t *f) {
    uint8_t tmp[33 + 256 * 3];
    return put_table(tmp, syms, ns, f);
}
static int get_table(const uint8_t *p, size_t n, size_t *i, int M, uint8_t *syms, int *ns, uint32_t *f) {
    if (get_alpha(p, n, i, syms, ns) || *ns == 0) return -1;
    uint32_t T = 1u << M, sum = 0;
    for (int q = 0; q + 1 < *ns; q++) {
        uint64_t v; if (get_uv(p, n, i, &v) || v >= T - sum - 1) return -1;
        f[syms[q]] = (uint32_t)v + 1; sum += (uint32_t)v + 1;
    }
    if (sum >= T) return -1;
    f[syms[*ns - 1]] = T - sum;
    return 0;
}

/* ---------------------------------------------------------------- encoder */

typedef struct { uint32_t xmax, rcp, bias, cmpl, sh; } esym;

/* Reciprocal: for 2^(l-1) < f <= 2^l and x < 2^31, floor(x / f) = (x * ceil(2^(31+l) / f)) >> (31 + l). */
static void esym_init(esym *e, uint32_t c, uint32_t f, int M) {
    uint32_t l = 0; while ((1u << l) < f) l++;
    e->xmax = f << (31 - M);
    e->rcp = (uint32_t)(((1ull << (31 + l)) + f - 1) / f);
    e->sh = 31 + l; e->bias = c; e->cmpl = (1u << M) - f;
}
static inline void enc_put(uint32_t *xs, const esym *e, uint16_t **pp) {
    uint32_t x = *xs, c = x >= e->xmax;          /* branchless: store the word anyway, keep it only if c */
    (*pp)[-1] = (uint16_t)x; *pp -= c; x >>= 16 * c;
    uint32_t q = (uint32_t)(((uint64_t)x * e->rcp) >> e->sh);
    *xs = x + e->bias + q * e->cmpl;
}

typedef struct {
    uint32_t *c1;          /* order-1 counts [256][256], kept zero between blocks */
    uint16_t *w; size_t wcap;
    uint8_t *tb;           /* table scratch */
    esym *es;              /* [KR_MAXTAB][256] */
} encws;

static void encws_free(encws *w) { free(w->c1); free(w->w); free(w->tb); free(w->es); memset(w, 0, sizeof *w); }
static int encws_init(encws *w, size_t block) {
    memset(w, 0, sizeof *w);
    w->c1 = calloc(256 * 256, 4); w->wcap = block + 64; w->w = malloc(w->wcap * 2);
    w->tb = malloc(KR_MAXTAB * (33 + 256 * 3) + 64); w->es = malloc(sizeof(esym) * KR_MAXTAB * 256);
    if (!w->c1 || !w->w || !w->tb || !w->es) { encws_free(w); return -1; }
    return 0;
}

static int present(const uint32_t *cnt, uint8_t *syms) {
    int ns = 0;
    for (int s = 0; s < 256; s++) if (cnt[s]) syms[ns++] = (uint8_t)s;
    return ns;
}

/* order-1 plan for one precision */
typedef struct {
    int M; double cost;      /* bytes estimate */
    uint8_t own[256];        /* context has its own table */
    int nown, has_shared;
    uint32_t fs[256];        /* shared table */
} o1plan;

static void o1_plan(encws *w, const uint32_t *h0, const uint32_t *ctot, int M, int N, o1plan *pl) {
    uint8_t syms[256], gs[256]; uint32_t f[256], g[256]; int ng = present(h0, gs);
    uint64_t tot0 = 0; for (int i = 0; i < ng; i++) tot0 += h0[gs[i]];
    kr_norm(h0, gs, ng, tot0, M, g);
    pl->M = M;
    double gl[256]; for (int i = 0; i < ng; i++) gl[gs[i]] = M - kr_lg[g[gs[i]]];
    uint32_t sc[256]; memset(sc, 0, sizeof sc);
    for (int c = 0; c < 256; c++) {
        pl->own[c] = 0;
        if (!ctot[c]) continue;
        const uint32_t *cc = w->c1 + 256 * c;
        int ns = present(cc, syms);
        double shared = 0; for (int i = 0; i < ns; i++) shared += cc[syms[i]] * gl[syms[i]];
        double own;
        if (ns == 1) own = 8.0 * 3;   /* alphabet only: 1 run (3 bytes) */
        else { kr_norm(cc, syms, ns, ctot[c], M, f); own = kr_bits(cc, syms, ns, f, M) + 8.0 * table_size(syms, ns, f); }
        if (own < shared) pl->own[c] = 1;
        else for (int i = 0; i < ns; i++) sc[syms[i]] += cc[syms[i]];
    }
    /* final shared table from the counts of the contexts that use it */
    int nsh = present(sc, gs); uint64_t tsh = 0; for (int i = 0; i < nsh; i++) tsh += sc[gs[i]];
    double bits = 0, bytes = 1; int nown = 0;
    pl->has_shared = nsh > 0;
    memset(pl->fs, 0, sizeof pl->fs);
    if (nsh) {
        if (nsh == 1) pl->fs[gs[0]] = 1u << M;
        else { kr_norm(sc, gs, nsh, tsh, M, pl->fs); bits += kr_bits(sc, gs, nsh, pl->fs, M); }
        bytes += table_size(gs, nsh, pl->fs);
    }
    for (int c = 0; c < 256; c++) {
        if (!pl->own[c]) continue;
        const uint32_t *cc = w->c1 + 256 * c;
        int ns = present(cc, syms);
        nown++;
        if (ns == 1) { bytes += 3; continue; }
        kr_norm(cc, syms, ns, ctot[c], M, f);
        bits += kr_bits(cc, syms, ns, f, M); bytes += table_size(syms, ns, f);
    }
    { int k = 0; uint8_t os[256]; for (int c = 0; c < 256; c++) if (pl->own[c]) os[k++] = (uint8_t)c; bytes += put_alpha(w->tb, os, k); }
    pl->nown = nown;
    pl->cost = bytes + bits / 8 + 4.0 * N;
}

static inline int o1_ctx(const uint8_t *in, size_t i, size_t Ls, int N) {
    if (Ls) { if (i < (size_t)N * Ls && i % Ls == 0) return 0; }
    else if (i == 0) return 0;
    return in[i - 1];
}

static inline __attribute__((always_inline)) void enc_o0(const uint8_t *in, size_t n, const esym *es, uint32_t *xio,
                                                        uint16_t **pp, const int N) {
    uint32_t x[32]; uint16_t *wp = *pp; size_t G = n / N;
    for (int j = 0; j < N; j++) x[j] = xio[j];
    for (size_t i = n; i > G * N;) { i--; enc_put(&x[i - G * N], &es[in[i]], &wp); }
    for (size_t g = G; g > 0;) {
        g--;
        const uint8_t *s = in + g * N;
        for (int j = N - 1; j >= 0; j--) enc_put(&x[j], &es[s[j]], &wp);
    }
    for (int j = 0; j < N; j++) xio[j] = x[j];
    *pp = wp;
}
static void tr_gather(const uint8_t *in, size_t Ls, size_t k0, int N, uint8_t (*st)[32]);
static inline __attribute__((always_inline)) void enc_o1(const uint8_t *in, size_t Ls, const esym *es, const int *tmap,
                                                        uint32_t *xio, uint16_t **pp, const int N) {
    uint32_t x[32]; uint16_t *wp = *pp;
    uint8_t st[32][32];
    for (int j = 0; j < N; j++) x[j] = xio[j];
    size_t k = Ls, kc = Ls - Ls % 32;
    while (k > kc) {                                   /* top steps that do not fill a 32-step chunk */
        k--;
        for (int j = N - 1; j >= 0; j--) {
            size_t i = (size_t)j * Ls + k;
            int cx = k ? in[i - 1] : 0;
            enc_put(&x[j], &es[256 * tmap[cx] + in[i]], &wp);
        }
    }
    while (k > 0) {                                    /* 32-step chunks, lanes gathered by a transpose */
        k -= 32;
        tr_gather(in, Ls, k, N, st);
        for (int q = 31; q >= 0; q--)
            for (int j = N - 1; j >= 0; j--) {
                int cx = q ? st[q - 1][j] : k ? in[(size_t)j * Ls + k - 1] : 0;
                enc_put(&x[j], &es[256 * tmap[cx] + st[q][j]], &wp);
            }
    }
    for (int j = 0; j < N; j++) xio[j] = x[j];
    *pp = wp;
}

/* Encodes one block; returns bytes written to out (header included), 0 on error. */
static size_t enc_block(const uint8_t *in, size_t n, uint8_t *out, size_t cap, const krans_params *p, encws *w,
                        krans_stats *st) {
    const int N = p->ways;
    uint32_t h0[256] = {0};
    for (size_t i = 0; i < n; i++) h0[in[i]]++;
    uint8_t syms[256]; int ns = present(h0, syms);
    if (cap < n + 12) return 0;
    st->blocks++;
    if (ns == 1) { out[0] = K_RLE; out[1] = 1; out[2] = in[0]; st->rle++; return 3; }

    double best = (double)n; int kind = K_STORED, bm = 0;
    uint32_t f0[256];
    int c0[2] = {12, 14};
    if (p->prec) c0[0] = c0[1] = p->prec;
    if (p->order != 1) {
        for (int k = 0; k < 2; k++) {
            if (k && c0[1] == c0[0]) break;
            uint32_t f[256]; kr_norm(h0, syms, ns, n, c0[k], f);
            double e = kr_bits(h0, syms, ns, f, c0[k]) / 8 + table_size(syms, ns, f) + 4.0 * N;
            /* the finer table decodes slower (14 bits: two gathers); take it only when it saves >= 1/256 */
            if (k && kind == K_O0 && e + e / 256 >= best) continue;
            if (e < best) { best = e; kind = K_O0; bm = c0[k]; memcpy(f0, f, sizeof f); }
        }
    }
    o1plan pl = {0}, tp;
    const size_t Ls = n / N;
    uint32_t ctot[256] = {0};
    if (p->order != 0) {
        for (size_t i = 0; i < n; i++) { int c = o1_ctx(in, i, Ls, N); w->c1[256 * c + in[i]]++; ctot[c]++; }
        int c1[2] = {10, 12};
        if (p->prec) c1[0] = c1[1] = p->prec;
        int have = 0;
        for (int k = 0; k < 2; k++) {
            if (k && c1[1] == c1[0]) break;
            o1_plan(w, h0, ctot, c1[k], N, &tp);
            /* 12-bit order-1 tables are 4x larger to build and keep in cache: take them only for >= 1/256 */
            if (!have || tp.cost + (k ? tp.cost / 256 : 0) < pl.cost) { pl = tp; have = 1; }
        }
        if (pl.cost < best || (p->order == 1 && pl.cost < n)) { best = pl.cost; kind = K_O1; bm = pl.M; }
    }

    size_t o = 0;
    uint8_t *tb = w->tb; size_t tl = 0;
    uint32_t x[32];
    for (int j = 0; j < N; j++) x[j] = KR_L;
    uint16_t *we = w->w + w->wcap, *wp = we;
    if (kind == K_O0) {
        tl = put_table(tb, syms, ns, f0);
        esym es[256]; uint32_t c = 0;
        for (int i = 0; i < ns; i++) { esym_init(&es[syms[i]], c, f0[syms[i]], bm); c += f0[syms[i]]; }
        if (N == 8) enc_o0(in, n, es, x, &wp, 8); else if (N == 16) enc_o0(in, n, es, x, &wp, 16); else enc_o0(in, n, es, x, &wp, 32);
    } else if (kind == K_O1) {
        const int M = bm; uint8_t os[256]; int no = 0;
        for (int cx = 0; cx < 256; cx++) if (pl.own[cx]) os[no++] = (uint8_t)cx;
        tl = put_alpha(tb, os, no);
        tb[tl++] = (uint8_t)pl.has_shared;
        int tmap[256];
        uint8_t ts[256]; uint32_t f[256];
        if (pl.has_shared) {
            int k = present(pl.fs, ts);
            tl += put_table(tb + tl, ts, k, pl.fs);   /* k == 1: alphabet only, frequency implied */
            uint32_t c = 0;
            for (int i = 0; i < k; i++) { esym_init(&w->es[256 * no + ts[i]], c, pl.fs[ts[i]], M); c += pl.fs[ts[i]]; }
        }
        for (int cx = 0; cx < 256; cx++) tmap[cx] = no;
        for (int t = 0; t < no; t++) {
            int cx = os[t]; const uint32_t *cc = w->c1 + 256 * cx;
            int k = present(cc, ts);
            if (k == 1) f[ts[0]] = 1u << M; else kr_norm(cc, ts, k, ctot[cx], M, f);
            tl += put_table(tb + tl, ts, k, f);
            uint32_t c = 0;
            for (int i = 0; i < k; i++) { esym_init(&w->es[256 * t + ts[i]], c, f[ts[i]], M); c += f[ts[i]]; }
            tmap[cx] = t;
        }
        st->o1_tables += no + pl.has_shared;
        const esym *es = w->es;
        for (size_t i = n; i > (size_t)N * Ls;) { i--; enc_put(&x[N - 1], &es[256 * tmap[o1_ctx(in, i, Ls, N)] + in[i]], &wp); }
        if (N == 8) enc_o1(in, Ls, es, tmap, x, &wp, 8); else if (N == 16) enc_o1(in, Ls, es, tmap, x, &wp, 16); else enc_o1(in, Ls, es, tmap, x, &wp, 32);
    }
    if (p->order != 0) for (int cx = 0; cx < 256; cx++) if (ctot[cx]) memset(w->c1 + 256 * cx, 0, 1024);

    size_t wl = (size_t)(we - wp) * 2;
    size_t pay = tl + 4 * (size_t)N + wl;
    if (kind == K_STORED || pay >= n) {
        out[0] = K_STORED; o = 1 + put_uv(out + 1, n); memcpy(out + o, in, n); st->stored++; return o + n;
    }
    out[0] = (uint8_t)(kind | ways_code(N) << 2 | (bm - 8) << 4);
    o = 1 + put_uv(out + 1, pay);
    memcpy(out + o, tb, tl); o += tl;
    for (int j = 0; j < N; j++) { wr32(out + o, x[j]); o += 4; }
    memcpy(out + o, wp, wl); o += wl;
    st->table_bytes += tl; st->prec_hist[bm]++;
    if (kind == K_O0) st->o0++; else st->o1++;
    return o;
}

/* ---------------------------------------------------------------- decoder tables */

typedef struct {
    int M;
    uint32_t *pk;     /* M <= 12: sym | (f-1) << 8 | bias << 20 */
    uint32_t *fb;     /* M > 12:  f | bias << 16 */
    uint8_t *sy;      /* M > 12:  symbol (padded by 4 bytes for 32-bit gathers) */
    uint32_t map[256];/* order 1: table base (index << M) for a context */
    size_t cap;       /* entries allocated */
} dtab;

static int dtab_reserve(dtab *t, size_t ent) {
    if (ent <= t->cap) return 0;
    free(t->pk); free(t->fb); free(t->sy);
    t->pk = malloc(ent * 4); t->fb = malloc(ent * 4); t->sy = malloc(ent + 64);
    if (!t->pk || !t->fb || !t->sy) { free(t->pk); free(t->fb); free(t->sy); t->pk = t->fb = NULL; t->sy = NULL; t->cap = 0; return -1; }
    t->cap = ent;
    return 0;
}
static void dtab_fill(dtab *t, size_t base, const uint8_t *syms, int ns, const uint32_t *f) {
    uint32_t c = 0;
    if (t->M <= 12) {
        for (int i = 0; i < ns; i++) {
            uint32_t s = syms[i], fr = f[s], e = s | (fr - 1) << 8;
            uint32_t *d = t->pk + base + c;
            for (uint32_t k = 0; k < fr; k++) d[k] = e | k << 20;
            c += fr;
        }
    } else {
        for (int i = 0; i < ns; i++) {
            uint32_t s = syms[i], fr = f[s];
            uint32_t *d = t->fb + base + c;
            for (uint32_t k = 0; k < fr; k++) d[k] = fr | k << 16;
            memset(t->sy + base + c, (int)s, fr);
            c += fr;
        }
    }
}

/* ---------------------------------------------------------------- scalar decoders */

#define KR_INLINE static inline __attribute__((always_inline))

/* one decode step of state X at table offset B; S receives the symbol */
#define KR_STEP(PK, X, B, S) do {                                                          \
        uint32_t sl_ = (X) & mask;                                                         \
        if (PK) { uint32_t e_ = t->pk[(B) + sl_], q_ = (X) >> M;                           \
                  (S) = (uint8_t)e_; (X) = ((e_ >> 8) & 0xfff) * q_ + q_ + (e_ >> 20); }   \
        else    { uint32_t e_ = t->fb[(B) + sl_];                                          \
                  (S) = t->sy[(B) + sl_]; (X) = (e_ & 0xffff) * ((X) >> M) + (e_ >> 16); }  \
    } while (0)
/* branchless: the renormalisation outcome is data-dependent and mispredicts about as often as it is taken */
#define KR_RENORM_FAST(X) do { uint32_t w_ = rd16(p), c_ = (X) < KR_L; (X) = (X) << (16 * c_) | (w_ & (0u - c_)); p += 2 * c_; } while (0)
#define KR_RENORM_SAFE(X) do { if ((X) < KR_L) { if (end - p < 2) return -1; (X) = (X) << 16 | rd16(p); p += 2; } } while (0)

KR_INLINE int dec_o0_scalar_t(const dtab *t, uint32_t *xio, const uint8_t **pp, const uint8_t *end, uint8_t *out,
                              size_t n, size_t g0, const int N, const int PK) {
    const int M = t->M; const uint32_t mask = (1u << M) - 1;
    const uint8_t *p = *pp; size_t G = n / N, g = g0;
    uint32_t x[32];                                   /* local copy: the byte stores below cannot alias it */
    for (int j = 0; j < N; j++) x[j] = xio[j];
    for (; g < G && end - p >= 2 * N; g++) {
        uint8_t *o = out + g * N;
        for (int j = 0; j < N; j++) { uint8_t s; KR_STEP(PK, x[j], 0, s); KR_RENORM_FAST(x[j]); o[j] = s; }
    }
    for (; g < G; g++) {
        uint8_t *o = out + g * N;
        for (int j = 0; j < N; j++) { uint8_t s; KR_STEP(PK, x[j], 0, s); KR_RENORM_SAFE(x[j]); o[j] = s; }
    }
    for (size_t i = G * N; i < n; i++) { int j = (int)(i - G * N); uint8_t s; KR_STEP(PK, x[j], 0, s); KR_RENORM_SAFE(x[j]); out[i] = s; }
    for (int j = 0; j < N; j++) xio[j] = x[j];
    *pp = p;
    return 0;
}
#define KR_SPECIALISE(FN, ...) do {                                                                  \
        int pk_ = t->M <= 12;                                                                         \
        switch (N) {                                                                                  \
        case 8:  return pk_ ? FN(__VA_ARGS__, 8, 1) : FN(__VA_ARGS__, 8, 0);                          \
        case 16: return pk_ ? FN(__VA_ARGS__, 16, 1) : FN(__VA_ARGS__, 16, 0);                        \
        default: return pk_ ? FN(__VA_ARGS__, 32, 1) : FN(__VA_ARGS__, 32, 0);                        \
        }                                                                                             \
    } while (0)
static int dec_o0_scalar(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n, size_t g0) {
    KR_SPECIALISE(dec_o0_scalar_t, t, x, pp, end, out, n, g0);
}

/* Order-1 lanes write to N segments whose starts are n/N apart -- a power of two for the usual block sizes, so
 * byte stores straight to the segments all land in one L1 set. Decoders stage KR_STAGE steps of all lanes and
 * write each lane's run with one copy. */
#define KR_STAGE 32
#if defined(__SSE2__)
#include <emmintrin.h>
/* 16x16 byte transpose: four rounds of "row 2i, 2i+1 = interleave(row i, row i+8)" rotate (row, column) bits by one */
static inline __attribute__((always_inline)) void tr16(const uint8_t *src, size_t sstride, uint8_t *dst, size_t dstride) {
    __m128i r[16], u[16];
    for (int i = 0; i < 16; i++) r[i] = _mm_loadu_si128((const __m128i *)(src + i * sstride));
    for (int round = 0; round < 4; round++) {
        for (int i = 0; i < 8; i++) { u[2 * i] = _mm_unpacklo_epi8(r[i], r[i + 8]); u[2 * i + 1] = _mm_unpackhi_epi8(r[i], r[i + 8]); }
        for (int i = 0; i < 16; i++) r[i] = u[i];
    }
    for (int i = 0; i < 16; i++) _mm_storeu_si128((__m128i *)(dst + i * dstride), r[i]);
}
#endif
/* encoder: st[q][j] = in[j * Ls + k0 + q] for q < 32 */
static void tr_gather(const uint8_t *in, size_t Ls, size_t k0, int N, uint8_t (*st)[32]) {
#if defined(__SSE2__)
    if (N >= 16) {
        for (int jb = 0; jb < N / 16; jb++)
            for (int qb = 0; qb < 2; qb++) tr16(in + (size_t)16 * jb * Ls + k0 + 16 * qb, Ls, &st[16 * qb][16 * jb], 32);
        return;
    }
#endif
    for (int j = 0; j < N; j++) for (int q = 0; q < 32; q++) st[q][j] = in[(size_t)j * Ls + k0 + q];
}
KR_INLINE void stage_flush(const uint8_t (*st)[32], uint8_t *out, size_t Ls, size_t k0, int steps, int N) {
#if defined(__SSE2__)
    if (steps == KR_STAGE && N >= 16) {
        for (int qb = 0; qb < 2; qb++)
            for (int jb = 0; jb < N / 16; jb++)
                tr16(&st[16 * qb][16 * jb], 32, out + (size_t)16 * jb * Ls + k0 + 16 * qb, Ls);
        return;
    }
#endif
    for (int j = 0; j < N; j++) {
        uint8_t tmp[KR_STAGE];
        for (int q = 0; q < KR_STAGE; q++) tmp[q] = st[q][j];
        memcpy(out + (size_t)j * Ls + k0, tmp, (size_t)steps);
    }
}

KR_INLINE int dec_o1_scalar_t(const dtab *t, uint32_t *xio, const uint8_t **pp, const uint8_t *end, uint8_t *out,
                              size_t n, size_t k0, const int N, const int PK) {
    const int M = t->M; const uint32_t mask = (1u << M) - 1;
    const uint8_t *p = *pp; size_t Ls = n / N, k = k0;
    uint32_t b[32], x[32];
    uint8_t st[KR_STAGE][32];
    for (int j = 0; j < N; j++) { x[j] = xio[j]; b[j] = t->map[k ? out[j * Ls + k - 1] : 0]; }
    for (; k + KR_STAGE <= Ls && end - p >= 2 * N * KR_STAGE; k += KR_STAGE) {
        for (int q = 0; q < KR_STAGE; q++)
            for (int j = 0; j < N; j++) { uint8_t s; KR_STEP(PK, x[j], b[j], s); KR_RENORM_FAST(x[j]); st[q][j] = s; b[j] = t->map[s]; }
        stage_flush((const uint8_t (*)[32])st, out, Ls, k, KR_STAGE, N);
    }
    for (; k < Ls; k++) {
        for (int j = 0; j < N; j++) { uint8_t s; KR_STEP(PK, x[j], b[j], s); KR_RENORM_SAFE(x[j]); out[j * Ls + k] = s; b[j] = t->map[s]; }
    }
    for (size_t i = (size_t)N * Ls; i < n; i++) {
        uint32_t bb = t->map[i ? out[i - 1] : 0]; uint8_t s;
        KR_STEP(PK, x[N - 1], bb, s); KR_RENORM_SAFE(x[N - 1]); out[i] = s;
    }
    for (int j = 0; j < N; j++) xio[j] = x[j];
    *pp = p;
    return 0;
}
static int dec_o1_scalar(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n, size_t k0) {
    KR_SPECIALISE(dec_o1_scalar_t, t, x, pp, end, out, n, k0);
}

/* SIMD front ends decode whole groups (order 0) or steps (order 1) while enough input remains for unchecked
 * loads, and return how many they did; the scalar decoder finishes from there with the same states. */
typedef size_t (*simd_fn)(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n);

/* ---------------------------------------------------------------- AVX2 decoders */
#ifdef KR_X86
static uint32_t kr_perm8[256][8] __attribute__((aligned(32)));   /* lane i takes word rank(i) among renormalising lanes */

#define AVX2 __attribute__((target("avx2,popcnt")))
#define AVX2_INLINE static inline __attribute__((always_inline, target("avx2,popcnt")))

/* decode step for 8 lanes: X states, OFF table offsets (order 1) or zero; returns symbols as 32-bit lanes */
AVX2_INLINE __m256i avx2_step(const dtab *t, __m256i *X, __m256i off, const uint8_t **pp, const int PK, __m128i shM,
                               __m256i maskv) {
    __m256i x = *X, sl = _mm256_add_epi32(_mm256_and_si256(x, maskv), off), q = _mm256_srl_epi32(x, shM), s;
    if (PK) {
        __m256i e = _mm256_i32gather_epi32((const int *)t->pk, sl, 4);
        s = _mm256_and_si256(e, _mm256_set1_epi32(0xff));
        __m256i fm1 = _mm256_and_si256(_mm256_srli_epi32(e, 8), _mm256_set1_epi32(0xfff));
        x = _mm256_add_epi32(_mm256_mullo_epi32(fm1, q), _mm256_add_epi32(q, _mm256_srli_epi32(e, 20)));
    } else {
        __m256i e = _mm256_i32gather_epi32((const int *)t->fb, sl, 4);
        s = _mm256_and_si256(_mm256_i32gather_epi32((const int *)t->sy, sl, 1), _mm256_set1_epi32(0xff));
        x = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_and_si256(e, _mm256_set1_epi32(0xffff)), q), _mm256_srli_epi32(e, 16));
    }
    __m256i need = _mm256_cmpgt_epi32(_mm256_set1_epi32((int)KR_L), x);
    int mk = _mm256_movemask_ps(_mm256_castsi256_ps(need));
    __m256i wv = _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i *)*pp));
    wv = _mm256_permutevar8x32_epi32(wv, _mm256_load_si256((const __m256i *)kr_perm8[mk]));
    *X = _mm256_blendv_epi8(x, _mm256_or_si256(_mm256_slli_epi32(x, 16), wv), need);
    *pp += 2 * __builtin_popcount((unsigned)mk);
    return s;
}

AVX2_INLINE size_t avx2_o0_t(const dtab *t, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n,
                             const int V, const int PK) {
    const int N = 8 * V; const uint8_t *p = *pp;
    __m256i X[4], S[4];
    const __m128i shM = _mm_cvtsi32_si128(t->M);
    const __m256i maskv = _mm256_set1_epi32((int)((1u << t->M) - 1)), zero = _mm256_setzero_si256();
    const __m256i order = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);
    for (int v = 0; v < V; v++) X[v] = _mm256_loadu_si256((const __m256i *)(x + 8 * v));
    size_t G = n / N, g = 0;
    for (; g < G && end - p >= 16 * V; g++) {
        for (int v = 0; v < V; v++) S[v] = avx2_step(t, &X[v], zero, &p, PK, shM, maskv);
        uint8_t *o = out + g * N;
        if (V == 4) {
            __m256i a = _mm256_packus_epi32(S[0], S[1]), b = _mm256_packus_epi32(S[2], S[3]);
            _mm256_storeu_si256((__m256i *)o, _mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, b), order));
        } else if (V == 2) {
            __m256i a = _mm256_packus_epi32(S[0], S[1]);
            __m256i b = _mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, a), order);
            _mm_storeu_si128((__m128i *)o, _mm256_castsi256_si128(b));
        } else {
            __m256i a = _mm256_packus_epi32(S[0], S[0]);
            __m256i b = _mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, a), order);
            _mm_storel_epi64((__m128i *)o, _mm256_castsi256_si128(b));
        }
    }
    for (int v = 0; v < V; v++) _mm256_storeu_si256((__m256i *)(x + 8 * v), X[v]);
    *pp = p;
    return g;
}
AVX2 static size_t avx2_o0(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n) {
    int pk = t->M <= 12;
    switch (N) {
    case 8:  return pk ? avx2_o0_t(t, x, pp, end, out, n, 1, 1) : avx2_o0_t(t, x, pp, end, out, n, 1, 0);
    case 16: return pk ? avx2_o0_t(t, x, pp, end, out, n, 2, 1) : avx2_o0_t(t, x, pp, end, out, n, 2, 0);
    default: return pk ? avx2_o0_t(t, x, pp, end, out, n, 4, 1) : avx2_o0_t(t, x, pp, end, out, n, 4, 0);
    }
}

AVX2_INLINE size_t avx2_o1_t(const dtab *t, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n,
                             const int V, const int PK) {
    const int N = 8 * V; const uint8_t *p = *pp;
    const size_t Ls = n / N;
    __m256i X[4], B[4], S[4];
    const __m128i shM = _mm_cvtsi32_si128(t->M);
    const __m256i maskv = _mm256_set1_epi32((int)((1u << t->M) - 1));
    const __m256i order = _mm256_setr_epi32(0, 4, 1, 5, 2, 6, 3, 7);
    uint8_t st[KR_STAGE][32] __attribute__((aligned(32)));
    for (int v = 0; v < V; v++) { X[v] = _mm256_loadu_si256((const __m256i *)(x + 8 * v)); B[v] = _mm256_set1_epi32((int)t->map[0]); }
    size_t k = 0;
    for (; k + KR_STAGE <= Ls && end - p >= 16 * V * KR_STAGE; k += KR_STAGE) {
        for (int q = 0; q < KR_STAGE; q++) {
            for (int v = 0; v < V; v++) {
                S[v] = avx2_step(t, &X[v], B[v], &p, PK, shM, maskv);
                B[v] = _mm256_i32gather_epi32((const int *)t->map, S[v], 4);
            }
            if (V == 4) {
                __m256i a = _mm256_packus_epi32(S[0], S[1]), b = _mm256_packus_epi32(S[2], S[3]);
                _mm256_store_si256((__m256i *)st[q], _mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, b), order));
            } else if (V == 2) {
                __m256i a = _mm256_packus_epi32(S[0], S[1]);
                _mm_store_si128((__m128i *)st[q], _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, a), order)));
            } else {
                __m256i a = _mm256_packus_epi32(S[0], S[0]);
                _mm_storel_epi64((__m128i *)st[q], _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(_mm256_packus_epi16(a, a), order)));
            }
        }
        stage_flush((const uint8_t (*)[32])st, out, Ls, k, KR_STAGE, N);
    }
    for (int v = 0; v < V; v++) _mm256_storeu_si256((__m256i *)(x + 8 * v), X[v]);
    *pp = p;
    return k;
}
AVX2 static size_t avx2_o1(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n) {
    int pk = t->M <= 12;
    switch (N) {
    case 8:  return pk ? avx2_o1_t(t, x, pp, end, out, n, 1, 1) : avx2_o1_t(t, x, pp, end, out, n, 1, 0);
    case 16: return pk ? avx2_o1_t(t, x, pp, end, out, n, 2, 1) : avx2_o1_t(t, x, pp, end, out, n, 2, 0);
    default: return pk ? avx2_o1_t(t, x, pp, end, out, n, 4, 1) : avx2_o1_t(t, x, pp, end, out, n, 4, 0);
    }
}
#endif

/* ---------------------------------------------------------------- NEON decoders (no gather: table loads per lane) */
#ifdef KR_NEON
static uint8_t kr_shuf4[16][16];   /* lane i (mask bit i set) takes 16-bit word rank(i): bytes 2r, 2r+1, then zeros */

static inline __attribute__((always_inline)) uint32x4_t neon_step(const dtab *t, uint32x4_t *X, const uint32_t *off,
                                                                 const uint8_t **pp, const int PK) {
    const int M = t->M; const uint32_t mask = (1u << M) - 1;
    uint32_t xs[4], e[4], sy[4];
    vst1q_u32(xs, *X);
    uint32x4_t x, q = vshlq_u32(*X, vdupq_n_s32(-M));
    if (PK) {
        for (int i = 0; i < 4; i++) e[i] = t->pk[off[i] + (xs[i] & mask)];
        uint32x4_t ev = vld1q_u32(e);
        uint32x4_t fm1 = vandq_u32(vshrq_n_u32(ev, 8), vdupq_n_u32(0xfff));
        x = vaddq_u32(vmlaq_u32(q, fm1, q), vshrq_n_u32(ev, 20));
        vst1q_u32(sy, vandq_u32(ev, vdupq_n_u32(0xff)));
    } else {
        for (int i = 0; i < 4; i++) { uint32_t sl = off[i] + (xs[i] & mask); e[i] = t->fb[sl]; sy[i] = t->sy[sl]; }
        uint32x4_t ev = vld1q_u32(e);
        x = vmlaq_u32(vshrq_n_u32(ev, 16), vandq_u32(ev, vdupq_n_u32(0xffff)), q);
    }
    uint32x4_t need = vcltq_u32(x, vdupq_n_u32(KR_L));
    static const uint32_t bits[4] = {1, 2, 4, 8};
    uint32_t mk = vaddvq_u32(vandq_u32(need, vld1q_u32(bits)));
    uint8x16_t raw = vcombine_u8(vld1_u8(*pp), vdup_n_u8(0));
    uint32x4_t wv = vreinterpretq_u32_u8(vqtbl1q_u8(raw, vld1q_u8(kr_shuf4[mk])));
    *X = vbslq_u32(need, vorrq_u32(vshlq_n_u32(x, 16), wv), x);
    *pp += 2 * __builtin_popcount(mk);
    return vld1q_u32(sy);
}
static inline __attribute__((always_inline)) size_t neon_o0_t(const dtab *t, int N, uint32_t *x, const uint8_t **pp,
                                                             const uint8_t *end, uint8_t *out, size_t n, const int PK) {
    const int V = N / 4; const uint8_t *p = *pp;
    uint32x4_t X[8]; static const uint32_t zero[4] = {0, 0, 0, 0};
    for (int v = 0; v < V; v++) X[v] = vld1q_u32(x + 4 * v);
    size_t G = n / N, g = 0;
    for (; g < G && end - p >= 8 * V; g++) {
        uint8_t *o = out + g * N;
        for (int v = 0; v < V; v++) {
            uint32x4_t s = neon_step(t, &X[v], zero, &p, PK);
            uint16x4_t h = vmovn_u32(s); uint8x8_t b = vmovn_u16(vcombine_u16(h, h));
            vst1_lane_u32((uint32_t *)(void *)(o + 4 * v), vreinterpret_u32_u8(b), 0);
        }
    }
    for (int v = 0; v < V; v++) vst1q_u32(x + 4 * v, X[v]);
    *pp = p;
    return g;
}
static size_t neon_o0(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n) {
    return t->M <= 12 ? neon_o0_t(t, N, x, pp, end, out, n, 1) : neon_o0_t(t, N, x, pp, end, out, n, 0);
}
static inline __attribute__((always_inline)) size_t neon_o1_t(const dtab *t, int N, uint32_t *x, const uint8_t **pp,
                                                             const uint8_t *end, uint8_t *out, size_t n, const int PK) {
    const int V = N / 4; const uint8_t *p = *pp; const size_t Ls = n / N;
    uint32x4_t X[8]; uint32_t off[32], sy[4];
    for (int v = 0; v < V; v++) X[v] = vld1q_u32(x + 4 * v);
    for (int j = 0; j < N; j++) off[j] = t->map[0];
    size_t k = 0;
    for (; k < Ls && end - p >= 8 * V; k++) {
        for (int v = 0; v < V; v++) {
            vst1q_u32(sy, neon_step(t, &X[v], off + 4 * v, &p, PK));
            for (int i = 0; i < 4; i++) { out[(size_t)(4 * v + i) * Ls + k] = (uint8_t)sy[i]; off[4 * v + i] = t->map[sy[i]]; }
        }
    }
    for (int v = 0; v < V; v++) vst1q_u32(x + 4 * v, X[v]);
    *pp = p;
    return k;
}
static size_t neon_o1(const dtab *t, int N, uint32_t *x, const uint8_t **pp, const uint8_t *end, uint8_t *out, size_t n) {
    return t->M <= 12 ? neon_o1_t(t, N, x, pp, end, out, n, 1) : neon_o1_t(t, N, x, pp, end, out, n, 0);
}
#endif

/* ---------------------------------------------------------------- dispatch */

static int kr_forced = KRANS_DEC_AUTO;

static void kr_init_tables(void) {
    kr_lg[0] = 0;
    for (int f = 1; f <= (1 << 14); f++) kr_lg[f] = (float)log2((double)f);
#ifdef KR_X86
    for (int m = 0; m < 256; m++) { int r = 0; for (int i = 0; i < 8; i++) kr_perm8[m][i] = (m >> i & 1) ? (uint32_t)r++ : 0; }
#endif
#ifdef KR_NEON
    for (int m = 0; m < 16; m++) {
        int r = 0;
        for (int i = 0; i < 4; i++) {
            if (m >> i & 1) { kr_shuf4[m][4 * i] = (uint8_t)(2 * r); kr_shuf4[m][4 * i + 1] = (uint8_t)(2 * r + 1); r++; }
            else kr_shuf4[m][4 * i] = kr_shuf4[m][4 * i + 1] = 0xff;
            kr_shuf4[m][4 * i + 2] = kr_shuf4[m][4 * i + 3] = 0xff;
        }
    }
#endif
}

int krans_decoder_available(int which) {
    switch (which) {
    case KRANS_DEC_AUTO: case KRANS_DEC_SCALAR: return 1;
#ifdef KR_X86
    case KRANS_DEC_AVX2: __builtin_cpu_init(); return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("popcnt");
#endif
#ifdef KR_NEON
    case KRANS_DEC_NEON: return 1;
#endif
    default: return 0;
    }
}
int krans_set_decoder(int which) {
    if (!krans_decoder_available(which)) return -1;
    kr_forced = which;
    return 0;
}
int krans_decoder_active(void) {
    if (kr_forced != KRANS_DEC_AUTO) return kr_forced;
    if (krans_decoder_available(KRANS_DEC_AVX2)) return KRANS_DEC_AVX2;
    if (krans_decoder_available(KRANS_DEC_NEON)) return KRANS_DEC_NEON;
    return KRANS_DEC_SCALAR;
}
const char *krans_decoder_name(int which) {
    static const char *nm[] = {"auto", "scalar", "avx2", "neon"};
    return which >= 0 && which < 4 ? nm[which] : "?";
}
static void pick_simd(int which, simd_fn *f0, simd_fn *f1) {
    *f0 = *f1 = NULL;
#ifdef KR_X86
    if (which == KRANS_DEC_AVX2) { *f0 = avx2_o0; *f1 = avx2_o1; }
#endif
#ifdef KR_NEON
    if (which == KRANS_DEC_NEON) { *f0 = neon_o0; *f1 = neon_o1; }
#endif
    (void)which;
}

/* ---------------------------------------------------------------- block decoder */

typedef struct { dtab t; simd_fn s0, s1; } decws;

static int dec_block(const uint8_t *pl, size_t plen, int mode, uint8_t *out, size_t n, decws *w) {
    int kind = mode & 3;
    if (kind == K_STORED) { if (mode != 0 || plen != n) return -1; memcpy(out, pl, n); return 0; }
    if (kind == K_RLE) { if (mode != K_RLE || plen != 1) return -1; memset(out, pl[0], n); return 0; }
    int wc = mode >> 2 & 3, M = (mode >> 4) + 8;
    if (wc == 3 || M < 10 || M > 14) return -1;
    const int N = ways_of[wc];
    dtab *t = &w->t; t->M = M;
    size_t i = 0; uint8_t syms[256]; int ns; uint32_t f[256];
    if (kind == K_O0) {
        if (get_table(pl, plen, &i, M, syms, &ns, f) || ns < 2) return -1;
        if (dtab_reserve(t, (size_t)1 << M)) return -1;
        dtab_fill(t, 0, syms, ns, f);
    } else {
        uint8_t os[256]; int no;
        if (get_alpha(pl, plen, &i, os, &no) || i >= plen) return -1;
        int hs = pl[i++];
        if (hs > 1 || no + hs == 0) return -1;
        size_t ent = (size_t)(no + hs) << M;
        if (dtab_reserve(t, ent)) return -1;
        uint32_t sb = (uint32_t)((size_t)no << M);
        if (hs) {
            if (get_table(pl, plen, &i, M, syms, &ns, f)) return -1;
            if (ns == 1) f[syms[0]] = 1u << M;
            dtab_fill(t, sb, syms, ns, f);
        }
        for (int c = 0; c < 256; c++) t->map[c] = hs ? sb : 0;   /* contexts without a table: shared (or table 0; garbage in, garbage out, but in bounds) */
        for (int q = 0; q < no; q++) {
            if (get_table(pl, plen, &i, M, syms, &ns, f)) return -1;
            if (ns == 1) f[syms[0]] = 1u << M;
            dtab_fill(t, (size_t)q << M, syms, ns, f);
            t->map[os[q]] = (uint32_t)q << M;
        }
    }
    if (plen - i < 4u * N) return -1;
    uint32_t x[32];
    for (int j = 0; j < N; j++) { x[j] = rd32(pl + i + 4 * j); if (x[j] < KR_L || x[j] >= (1u << 31)) return -1; }
    const uint8_t *p = pl + i + 4 * N, *end = pl + plen;
    int r;
    if (kind == K_O0) { size_t g0 = w->s0 ? w->s0(t, N, x, &p, end, out, n) : 0; r = dec_o0_scalar(t, N, x, &p, end, out, n, g0); }
    else { size_t k0 = w->s1 ? w->s1(t, N, x, &p, end, out, n) : 0; r = dec_o1_scalar(t, N, x, &p, end, out, n, k0); }
    if (r || p != end) return -1;
    for (int j = 0; j < N; j++) if (x[j] != KR_L) return -1;
    return 0;
}

/* ---------------------------------------------------------------- frame */

/* Frame: "KRN1", uv original size, uv block size, then the blocks in order. Block: mode byte, uv payload length,
 * payload. The raw length of a block is the block size, or what remains for the last one. Mode: bits 0-1 kind
 * (0 stored, 1 one repeated byte, 2 order 0, 3 order 1), bits 2-3 ways (8, 16, 32), bits 4-7 precision - 8. */

void krans_default_params(krans_params *p) {
    memset(p, 0, sizeof *p);
    p->order = 2; p->ways = 32; p->prec = 0; p->block = 256 << 10; p->threads = 1;
}
size_t krans_bound(size_t n, uint32_t block) {
    if (block < KR_MINB) block = KR_MINB;
    size_t nb = n / block + 1;
    return 4 + 20 + nb * 8 + n;
}

typedef struct {
    const uint8_t *in; size_t n; uint32_t bs; size_t nb;
    uint8_t *slots; size_t slot; size_t *len;
    const krans_params *p; krans_stats st; atomic_size_t *next; atomic_int *err;
} encjob;

static void *enc_thread(void *arg) {
    encjob *j = arg; encws w;
    if (encws_init(&w, j->bs)) { atomic_store(j->err, 1); return NULL; }
    for (;;) {
        size_t b = atomic_fetch_add(j->next, 1);
        if (b >= j->nb || atomic_load(j->err)) break;
        size_t off = b * (size_t)j->bs, m = j->n - off < j->bs ? j->n - off : j->bs;
        size_t z = enc_block(j->in + off, m, j->slots + b * j->slot, j->slot, j->p, &w, &j->st);
        if (!z) { atomic_store(j->err, 1); break; }
        j->len[b] = z;
    }
    encws_free(&w);
    return NULL;
}

static void stats_add(krans_stats *a, const krans_stats *b) {
    a->blocks += b->blocks; a->stored += b->stored; a->rle += b->rle; a->o0 += b->o0; a->o1 += b->o1;
    a->table_bytes += b->table_bytes; a->o1_tables += b->o1_tables;
    for (int i = 0; i < 16; i++) a->prec_hist[i] += b->prec_hist[i];
}

size_t krans_compress(const void *src, size_t n, void *dst, size_t cap, const krans_params *pp) {
    pthread_once(&kr_once, kr_init_tables);
    krans_params p; if (pp) p = *pp; else krans_default_params(&p);
    if (ways_code(p.ways) < 0 || p.block < KR_MINB || p.block > KR_MAXB || p.order < 0 || p.order > 2) return 0;
    if (p.prec && (p.prec < 10 || p.prec > 14)) return 0;
    if (cap < krans_bound(n, p.block)) return 0;
    if (p.threads < 1) p.threads = 1;
    const uint8_t *in = src; uint8_t *out = dst;
    size_t o = 4; memcpy(out, "KRN1", 4);
    o += put_uv(out + o, n); o += put_uv(out + o, p.block);
    size_t nb = (n + p.block - 1) / p.block;
    krans_stats st; memset(&st, 0, sizeof st);
    if (p.threads == 1 || nb < 2) {
        encws w; if (encws_init(&w, p.block)) return 0;
        for (size_t b = 0; b < nb; b++) {
            size_t off = b * (size_t)p.block, m = n - off < p.block ? n - off : p.block;
            size_t z = enc_block(in + off, m, out + o, cap - o, &p, &w, &st);
            if (!z) { encws_free(&w); return 0; }
            o += z;
        }
        encws_free(&w);
    } else {
        int T = p.threads > 256 ? 256 : p.threads;
        size_t slot = (size_t)p.block + 16;
        uint8_t *slots = malloc(nb * slot); size_t *len = calloc(nb, sizeof *len);
        encjob *jobs = calloc((size_t)T, sizeof *jobs); pthread_t *th = calloc((size_t)T, sizeof *th);
        atomic_size_t next = 0; atomic_int err = 0;
        if (!slots || !len || !jobs || !th) { free(slots); free(len); free(jobs); free(th); return 0; }
        int started = 0;
        for (int k = 0; k < T; k++) {
            jobs[k] = (encjob){in, n, p.block, nb, slots, slot, len, &p, {0}, &next, &err};
            if (k && pthread_create(&th[k], NULL, enc_thread, &jobs[k]) == 0) started++;
            else if (k) { th[k] = 0; jobs[k].nb = 0; }
        }
        enc_thread(&jobs[0]);
        for (int k = 1; k < T; k++) if (jobs[k].nb) pthread_join(th[k], NULL);
        (void)started;
        if (!atomic_load(&err)) for (size_t b = 0; b < nb; b++) { memcpy(out + o, slots + b * slot, len[b]); o += len[b]; }
        for (int k = 0; k < T; k++) stats_add(&st, &jobs[k].st);
        free(slots); free(len); free(jobs); free(th);
        if (atomic_load(&err)) return 0;
    }
    if (p.stats) *p.stats = st;
    return o;
}

int krans_frame_size(const void *src, size_t cn, uint64_t *n) {
    const uint8_t *s = src; size_t i = 4; uint64_t bs;
    if (cn < 6 || memcmp(s, "KRN1", 4)) return -1;
    if (get_uv(s, cn, &i, n) || get_uv(s, cn, &i, &bs) || bs < KR_MINB || bs > KR_MAXB) return -1;
    return 0;
}

typedef struct {
    const uint8_t *src; uint8_t *dst; size_t n; uint32_t bs; size_t nb;
    const size_t *poff; const size_t *plen; const uint8_t *mode;
    atomic_size_t *next; atomic_int *err; int which;
} decjob;

static void *dec_thread(void *arg) {
    decjob *j = arg; decws w; memset(&w, 0, sizeof w);
    pick_simd(j->which, &w.s0, &w.s1);
    for (;;) {
        size_t b = atomic_fetch_add(j->next, 1);
        if (b >= j->nb || atomic_load(j->err)) break;
        size_t off = b * (size_t)j->bs, m = j->n - off < j->bs ? j->n - off : j->bs;
        if (dec_block(j->src + j->poff[b], j->plen[b], j->mode[b], j->dst + off, m, &w)) { atomic_store(j->err, 1); break; }
    }
    free(w.t.pk); free(w.t.fb); free(w.t.sy);
    return NULL;
}

int krans_decompress(const void *src, size_t cn, void *dst, size_t n, int threads) {
    pthread_once(&kr_once, kr_init_tables);
    const uint8_t *s = src; uint8_t *out = dst;
    size_t i = 4; uint64_t fn, bs;
    if (cn < 6 || memcmp(s, "KRN1", 4)) return -1;
    if (get_uv(s, cn, &i, &fn) || get_uv(s, cn, &i, &bs) || bs < KR_MINB || bs > KR_MAXB || fn != n) return -1;
    size_t nb = (n + bs - 1) / bs;
    int which = krans_decoder_active();
    if (threads <= 1 || nb < 2) {
        decws w; memset(&w, 0, sizeof w); pick_simd(which, &w.s0, &w.s1);
        int r = 0;
        for (size_t b = 0; b < nb && !r; b++) {
            uint64_t pl; size_t off = b * bs, m = n - off < bs ? n - off : bs;
            if (i >= cn) { r = -1; break; }
            int mode = s[i++];
            if (get_uv(s, cn, &i, &pl) || pl > cn - i) { r = -1; break; }
            r = dec_block(s + i, (size_t)pl, mode, out + off, m, &w);
            i += pl;
        }
        free(w.t.pk); free(w.t.fb); free(w.t.sy);
        return r || i != cn ? -1 : 0;
    }
    size_t *poff = malloc(nb * sizeof *poff), *plen = malloc(nb * sizeof *plen); uint8_t *mode = malloc(nb);
    if (!poff || !plen || !mode) { free(poff); free(plen); free(mode); return -1; }
    int r = 0;
    for (size_t b = 0; b < nb; b++) {
        uint64_t pl;
        if (i >= cn) { r = -1; break; }
        mode[b] = s[i++];
        if (get_uv(s, cn, &i, &pl) || pl > cn - i) { r = -1; break; }
        poff[b] = i; plen[b] = (size_t)pl; i += pl;
    }
    if (!r && i != cn) r = -1;
    if (!r) {
        int T = threads > 256 ? 256 : threads;
        if ((size_t)T > nb) T = (int)nb;
        atomic_size_t next = 0; atomic_int err = 0;
        decjob j = {s, out, n, (uint32_t)bs, nb, poff, plen, mode, &next, &err, which};
        pthread_t th[256]; int ok[256] = {0};
        for (int k = 1; k < T; k++) ok[k] = pthread_create(&th[k], NULL, dec_thread, &j) == 0;
        dec_thread(&j);
        for (int k = 1; k < T; k++) if (ok[k]) pthread_join(th[k], NULL);
        r = atomic_load(&err) ? -1 : 0;
    }
    free(poff); free(plen); free(mode);
    return r;
}
