/* KLZMA: Kolmo's own LZMA-class coder (codec 24). Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.
   Format and design in klzma.h. Written for Kolmo; no code from liblzma or the LZMA SDK. Prices use integer
   arithmetic only, so the parse (and the output) is the same on every CPU and compiler. */
#include "klzma.h"
#include "par.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define KM_PB 11
#define KM_ONE (1u << KM_PB)
#define KM_MOVE 5
#define KM_TOP (1u << 24)
#define KM_MAXLEN 273
#define KM_OPT 4096
#define KM_NONE 0xFFFFFFFFu
#define KM_INF 0x3FFFFFFFu
typedef uint16_t kp;

typedef struct { kp choice, choice2, low[16][8], mid[16][8], high[256]; } km_len;
typedef struct {
    kp is_match[12][16], is_rep[12], is_rep0[12], is_rep1[12], is_rep2[12], is_rep0_long[12][16];
    kp slot[4][64], spec[14][32], align[16];
    km_len len, rlen;
} km_probs;
typedef struct { km_probs p; kp *lit; int lc, lp, pb; } km_model;

static int km_model_init(km_model *m, int lc, int lp, int pb) {
    kp *a = (kp *)&m->p; for (size_t i = 0; i < sizeof m->p / sizeof(kp); i++) a[i] = KM_ONE / 2;
    m->lc = lc; m->lp = lp; m->pb = pb;
    size_t nl = (size_t)0x300 << (lc + lp);
    m->lit = malloc(nl * sizeof(kp)); if (!m->lit) return -1;
    for (size_t i = 0; i < nl; i++) m->lit[i] = KM_ONE / 2;
    return 0;
}
static inline int st_lit(int s) { return s < 4 ? 0 : s < 10 ? s - 3 : s - 6; }
static inline int st_match(int s) { return s < 7 ? 7 : 10; }
static inline int st_rep(int s) { return s < 7 ? 8 : 11; }
static inline int st_short(int s) { return s < 7 ? 9 : 11; }
static inline uint32_t km_slot(uint32_t d) { if (d < 4) return d; int h = 31 - __builtin_clz(d); return 2u * (uint32_t)h + ((d >> (h - 1)) & 1u); }
static inline kp *km_litp(const km_model *m, uint32_t pos, unsigned prev) {
    return m->lit + 0x300u * ((((pos & ((1u << m->lp) - 1)) << m->lc)) + (prev >> (8 - m->lc)));
}

/* ---------------- range encoder ---------------- */
typedef struct { uint64_t low; uint32_t range; uint8_t cache; uint64_t csize; uint8_t *p, *e; int err; } km_rce;
static inline void rce_shift(km_rce *r) {
    if ((uint32_t)r->low < 0xFF000000u || (r->low >> 32)) {
        uint8_t carry = (uint8_t)(r->low >> 32), t = r->cache;
        do { if (r->p < r->e) *r->p++ = (uint8_t)(t + carry); else r->err = 1; t = 0xFF; } while (--r->csize);
        r->cache = (uint8_t)(r->low >> 24);
    }
    r->csize++;
    r->low = (r->low & 0x00FFFFFFu) << 8;
}
static inline void rce_bit(km_rce *r, kp *p, unsigned b) {
    uint32_t bound = (r->range >> KM_PB) * *p;
    if (!b) { r->range = bound; *p += (KM_ONE - *p) >> KM_MOVE; }
    else { r->low += bound; r->range -= bound; *p -= *p >> KM_MOVE; }
    while (r->range < KM_TOP) { r->range <<= 8; rce_shift(r); }
}
static inline void rce_direct(km_rce *r, uint32_t v, int nb) {
    while (nb--) { r->range >>= 1; if ((v >> nb) & 1) r->low += r->range; while (r->range < KM_TOP) { r->range <<= 8; rce_shift(r); } }
}
static void rce_tree(km_rce *r, kp *pr, int nb, uint32_t v) { uint32_t m = 1; for (int i = nb - 1; i >= 0; i--) { unsigned b = (v >> i) & 1; rce_bit(r, &pr[m], b); m = m << 1 | b; } }
static void rce_rtree(km_rce *r, kp *pr, int nb, uint32_t v) { uint32_t m = 1; for (int i = 0; i < nb; i++) { unsigned b = v & 1; v >>= 1; rce_bit(r, &pr[m], b); m = m << 1 | b; } }
static void rce_len(km_rce *r, km_len *L, uint32_t l, uint32_t ps) {   /* l = length - 2 */
    if (l < 8) { rce_bit(r, &L->choice, 0); rce_tree(r, L->low[ps], 3, l); return; }
    rce_bit(r, &L->choice, 1);
    if (l < 16) { rce_bit(r, &L->choice2, 0); rce_tree(r, L->mid[ps], 3, l - 8); return; }
    rce_bit(r, &L->choice2, 1); rce_tree(r, L->high, 8, l - 16);
}
static void rce_lit(km_rce *r, kp *pr, unsigned b, int matched, unsigned mb) {
    unsigned sym = 1;
    if (matched) {
        for (int i = 7; i >= 0; i--) {
            unsigned bit = (b >> i) & 1, mbit = (mb >> i) & 1;
            rce_bit(r, &pr[0x100 + (mbit << 8) + sym], bit); sym = sym << 1 | bit;
            if (bit != mbit) { for (i--; i >= 0; i--) { bit = (b >> i) & 1; rce_bit(r, &pr[sym], bit); sym = sym << 1 | bit; } break; }
        }
    } else for (int i = 7; i >= 0; i--) { unsigned bit = (b >> i) & 1; rce_bit(r, &pr[sym], bit); sym = sym << 1 | bit; }
}

/* ---------------- prices (1/16 bit, integer arithmetic only) ---------------- */
static uint32_t km_price_of(uint32_t w) {   /* about 16 * -log2(w / 2048), w in 1..2047 */
    uint32_t e = 0; uint64_t m = w; while (m < (1u << 15)) { m <<= 1; e++; }
    uint32_t fr = 0;
    for (int k = 0; k < 4; k++) { m = (m * m) >> 15; fr <<= 1; if (m >= (1u << 16)) { m >>= 1; fr |= 1; } }
    return 11 * 16 - ((15 - e) * 16 + fr);
}

/* ---------------- match finder: hash chains with 2-, 3- and 4-byte heads ---------------- */
typedef struct { uint32_t len, dist; } km_m;
struct km_mt;
typedef struct {
    const uint8_t *in; uint32_t n; uint32_t *h4, *chain, *h3, *h2, *son; int hlog, depth, bt; uint32_t nice;
    struct km_mt *mt; int mterr;   /* the threaded 4-byte finder (km_mt), or NULL; mterr: a worker failed (no output) */
} km_mf;
static inline uint32_t km_r32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t km_r64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline uint32_t km_count(const uint8_t *a, const uint8_t *b, uint32_t lim) {
    uint32_t l = 0;
    while (l + 8 <= lim) { uint64_t x = km_r64(a + l) ^ km_r64(b + l); if (x) return l + ((uint32_t)__builtin_ctzll(x) >> 3); l += 8; }
    while (l < lim && a[l] == b[l]) l++;
    return l;
}
#define KM_H2(p) ((uint32_t)(p)[0] | (uint32_t)(p)[1] << 8)
#define KM_H3(p) (((km_r32(p) & 0xFFFFFFu) * 2654435761u) >> 16)
#define KM_H4(M, p) ((km_r32(p) * 2654435761u) >> (32 - (M)->hlog))
/* binary-tree match finder (levels with bt set): each 4-byte hash bucket is a binary search tree of the earlier
 * positions ordered by their suffixes; inserting position p walks the tree from its root, reports every match longer
 * than the last, and re-roots the tree at p (left subtree: smaller suffixes, right: larger). The walk is cut after
 * `depth` nodes, and at the nice length the matched node's subtrees are taken over whole. son[2p], son[2p+1]: the
 * left and right children of p. */
static uint32_t km_bt(km_mf *M, uint32_t p, uint32_t lim, uint32_t best, km_m *out, uint32_t nm, int collect) {
    const uint8_t *s = M->in + p; uint32_t h = KM_H4(M, s), cur = M->h4[h];
    M->h4[h] = p;
    uint32_t *lt = &M->son[2 * (size_t)p], *gt = &M->son[2 * (size_t)p + 1], l0 = 0, l1 = 0;
    for (int d = M->depth;; d--) {
        if (cur == KM_NONE || d == 0) { *lt = *gt = KM_NONE; break; }
        const uint8_t *q = M->in + cur; uint32_t l = l0 < l1 ? l0 : l1;
        if (q[l] == s[l]) {
            l += 1 + km_count(s + l + 1, q + l + 1, lim - l - 1);
            if (collect && l > best) { best = l; out[nm].len = l; out[nm].dist = p - cur; nm++; }
            if (l >= lim) { *lt = M->son[2 * (size_t)cur]; *gt = M->son[2 * (size_t)cur + 1]; break; }
        }
        if (q[l] < s[l]) { *lt = cur; lt = &M->son[2 * (size_t)cur + 1]; cur = *lt; l0 = l; }
        else { *gt = cur; gt = &M->son[2 * (size_t)cur]; cur = *gt; l1 = l; }
    }
    return nm;
}
/* ---- the threaded 4-byte finder (same matches, so the same output, at any thread count) ----
 * As KLZH2's ko_mt (klz.c): the binary tree (or hash chain) of a 4-byte bucket holds only the positions of that bucket,
 * and inserting position p reads and writes only the head of p's bucket and the nodes (chain links) of positions in it;
 * son[] and chain[] are indexed by the absolute position, so there is no window to wrap. The buckets are split among K
 * workers (bucket h to worker h % K), each inserting every one of its positions in increasing order exactly as the
 * one-thread finder does, so every tree and chain evolves as on one thread. What a walk finds does not depend on
 * whether the parse asked for it (km_bt's collect and best only filter what is reported), so a worker records for
 * each position the matches longer than the last (from 1 byte for trees, 2 for chains, a chain stopping at the first
 * that reaches the nice or the largest length) and the parse keeps those longer than its 2- and 3-byte candidates:
 * exactly the list the one-thread walk reports with that starting length. The 2- and 3-byte heads and the nice-length
 * extension stay on the parse thread. Workers run up to KM_MTR chunks of KM_MTC positions ahead of the parse. */
#define KM_MTC (64u << 10)
#define KM_MTR 4
#define KM_MTMAXK 16
#ifndef KM_MT_MIN
#define KM_MT_MIN (1u << 20)   /* the threaded finder from inputs of this size (it starts K threads per call) */
#endif
typedef struct { km_m *m; size_t len, cap; } km_ar;
typedef struct km_mt {
    km_mf *M; uint32_t np, nch; int K;   /* np: positions with 4 bytes; K: bucket classes (bucket h is class h % K) */
    kolmo_cp *cp;                        /* the pipeline running class c's chunks in order on any thread (algo/par.h) */
    km_ar ar[KM_MTMAXK][KM_MTR];   /* per class and slot: for each of its positions {offset in chunk, count}, the matches */
    uint32_t cj, cur[KM_MTMAXK];   /* the parse: its chunk (complete) and read position per class */
} km_mt;
/* a worker: insert p (p + 4 <= n) into its tree or chain; out: the matches longer than the last */
static uint32_t km_ins(km_mf *M, uint32_t p, km_m *out) {
    const uint8_t *s = M->in + p; uint32_t maxl = M->n - p; if (maxl > KM_MAXLEN) maxl = KM_MAXLEN;
    const uint32_t lim = maxl < M->nice ? maxl : M->nice;
    if (M->bt) return km_bt(M, p, lim, 0, out, 0, 1);
    uint32_t h = KM_H4(M, s), c = M->h4[h], best = 1, nm = 0; M->chain[p] = c; M->h4[h] = p;
    for (int d = 0; d < M->depth && c != KM_NONE && best < lim; d++) {   /* km_find's chain walk from best = 1 */
        const uint8_t *q = M->in + c;
        if (q[best] == s[best]) {
            uint32_t l = km_count(s, q, maxl);
            if (l > best) { best = l; out[nm].len = l; out[nm].dist = p - c; nm++; }
        }
        c = M->chain[c];
    }
    return nm;
}
/* chunk j for the classes in mask (on a helper, or on the parse when no helper took them); each class writes only
 * its own arena */
static int km_mt_run(void *ctx, unsigned j, unsigned mask) {
    km_mt *Q = ctx; km_mf *M = Q->M; const uint32_t K = (uint32_t)Q->K, dmax = (uint32_t)M->depth + 2;
    const int sl = (int)(j % KM_MTR);
    for (uint32_t t = 0; t < K; t++) if (mask >> t & 1) Q->ar[t][sl].len = 0;
    const uint32_t c0 = j * KM_MTC, c1 = c0 + KM_MTC < Q->np ? c0 + KM_MTC : Q->np;
    for (uint32_t p = c0; p < c1; p++) {
        const uint32_t t = KM_H4(M, M->in + p) % K;
        if (!(mask >> t & 1)) continue;
        km_ar *A = &Q->ar[t][sl];
        if (A->len + dmax + 1 > A->cap) {
            size_t nc = A->cap ? 2 * A->cap : (size_t)KM_MTC; while (nc < A->len + dmax + 1) nc *= 2;
            km_m *g = realloc(A->m, nc * sizeof *g); if (!g) return 1;
            A->m = g; A->cap = nc;
        }
        km_m *e = A->m + A->len;
        const uint32_t nm = km_ins(M, p, e + 1);
        e->len = p - c0; e->dist = nm; A->len += 1 + nm;
    }
    return 0;
}
static void km_mt_end(km_mt *Q) {
    if (!Q) return;
    kolmo_cp_end(Q->cp);
    for (int r = 0; r < KM_MTR; r++) for (int t = 0; t < KM_MTMAXK; t++) free(Q->ar[t][r].m);
    free(Q);
}
/* K classes for M (tables initialised, nothing inserted), `passes` helpers always running and the others on tokens of
 * b; NULL when the helpers cannot start: the parse then runs on one thread */
static km_mt *km_mt_start(km_mf *M, int K, int passes, kolmo_budget *b) {
    km_mt *Q = calloc(1, sizeof *Q); if (!Q) return NULL;
    Q->M = M; Q->K = K; Q->np = M->n >= 4 ? M->n - 3 : 0; Q->nch = (Q->np + KM_MTC - 1) / KM_MTC; Q->cj = UINT32_MAX;
    Q->cp = kolmo_cp_start(K, Q->nch, KM_MTR, passes, b, km_mt_run, Q);
    if (!Q->cp) { free(Q); return NULL; }
    return Q;
}
/* the parse: p's matches from its worker (p + 4 <= n, positions taken in increasing order), those longer than best */
static uint32_t km_mt_take(km_mf *M, uint32_t p, km_m *out, uint32_t nm, uint32_t best) {
    km_mt *Q = M->mt; const uint32_t j = p / KM_MTC; const int sl = (int)(j % KM_MTR);
    if (j != Q->cj) {   /* a new chunk: release the earlier ones; run or wait for this one */
        if (kolmo_cp_need(Q->cp, j)) { M->mterr = 1; return nm; }
        Q->cj = j; memset(Q->cur, 0, sizeof Q->cur);
    }
    const uint32_t t = KM_H4(M, M->in + p) % (uint32_t)Q->K, k = p - j * KM_MTC;
    const km_m *a = Q->ar[t][sl].m; uint32_t c = Q->cur[t];
    while (a[c].len < k) c += 1 + a[c].dist;   /* the worker's positions before p (skipped by the parse) */
    const uint32_t cnt = a[c].dist; const km_m *e = a + c + 1; Q->cur[t] = c + 1 + cnt;
    for (uint32_t i = 0; i < cnt; i++) if (e[i].len > best) out[nm++] = e[i];   /* ascending: the walk's report from best */
    return nm;
}
static void km_skip(km_mf *M, uint32_t p) {
    const uint8_t *s = M->in + p;
    if (p + 4 <= M->n) {
        M->h2[KM_H2(s)] = p; M->h3[KM_H3(s)] = p;
        if (M->mt) return;   /* the workers insert p */
        if (M->bt) { uint32_t lim = M->n - p; if (lim > M->nice) lim = M->nice; km_bt(M, p, lim, 0, NULL, 0, 0); }
        else { uint32_t h = KM_H4(M, s); M->chain[p] = M->h4[h]; M->h4[h] = p; }
    }
    else if (p + 2 <= M->n) M->h2[KM_H2(s)] = p;
}
static uint32_t km_find(km_mf *M, uint32_t p, km_m *out) {
    const uint8_t *s = M->in + p; uint32_t maxl = M->n - p, best = 1, nm = 0;
    if (maxl > KM_MAXLEN) maxl = KM_MAXLEN;
    if (maxl < 2) return 0;
    if (p + 4 > M->n) {   /* the last bytes: 2-byte head only */
        uint32_t c = M->h2[KM_H2(s)]; M->h2[KM_H2(s)] = p;
        if (c != KM_NONE) { uint32_t l = km_count(s, M->in + c, maxl); if (l >= 2) { out[nm].len = l; out[nm].dist = p - c; nm++; } }
        return nm;
    }
    uint32_t c2 = M->h2[KM_H2(s)], c3 = M->h3[KM_H3(s)];
    M->h2[KM_H2(s)] = p; M->h3[KM_H3(s)] = p;
    if (c2 != KM_NONE && p - c2 < (1u << 16)) { uint32_t l = km_count(s, M->in + c2, maxl); if (l > best) { best = l; out[nm].len = l; out[nm].dist = p - c2; nm++; } }
    if (best < maxl && c3 != KM_NONE && c3 != c2) { uint32_t l = km_count(s, M->in + c3, maxl); if (l > best) { best = l; out[nm].len = l; out[nm].dist = p - c3; nm++; } }
    if (M->bt) {
        uint32_t lim = maxl < M->nice ? maxl : M->nice;
        nm = M->mt ? km_mt_take(M, p, out, nm, best) : km_bt(M, p, lim, best, out, nm, 1);
        if (nm && out[nm - 1].len == lim && lim < maxl) {   /* at the nice length: the full length of that match */
            km_m *t = &out[nm - 1]; t->len = lim + km_count(s + lim, s + lim - t->dist, maxl - lim);
        }
        return nm;
    }
    if (M->mt) return best < maxl && best < M->nice ? km_mt_take(M, p, out, nm, best) : nm;   /* the chain walk below runs only then */
    uint32_t h = KM_H4(M, s), c = M->h4[h]; M->chain[p] = c; M->h4[h] = p;
    for (int d = 0; d < M->depth && c != KM_NONE && best < maxl && best < M->nice; d++) {
        const uint8_t *q = M->in + c;
        if (q[best] == s[best]) {
            uint32_t l = km_count(s, q, maxl);
            if (l > best) { best = l; out[nm].len = l; out[nm].dist = p - c; nm++; }
        }
        c = M->chain[c];
    }
    return nm;
}

/* ---------------- encoder ---------------- */
/* kind: 0 literal, 1 short rep, 2-5 rep 0-3, 6 match, 7 literal then rep 0 (len); the composite paths of the
 * strong levels (review40 #93): 8 match (len, dist), literal, rep 0 (len2); 9 rep `dist` (len), literal, rep 0 (len2) */
typedef struct { uint32_t price, prev, dist; uint16_t len, len2; uint8_t kind, state; uint32_t reps[4]; } km_node;
typedef struct { uint8_t kind; uint32_t len, dist; } km_dec;
typedef struct {
    km_model m; km_rce rc; km_mf mf; const uint8_t *in; uint32_t n;
    uint16_t ptab[KM_ONE >> 4];
    uint32_t lenp[16][KM_MAXLEN + 1], rlenp[16][KM_MAXLEN + 1], slotp[4][64], distp[4][128], alignp[16];
    int len_cnt, dist_cnt, state, combo, refresh; uint32_t reps[4];   /* combo: kinds 8 and 9; refresh: price period */
    km_node opt[KM_OPT + 2 * KM_MAXLEN + 4];   /* a composite path reaches up to 2 * KM_MAXLEN + 1 past the window */ km_m M[KM_MAXLEN + 8], cache[KM_MAXLEN + 8]; uint32_t cache_pos, cache_nm;
    km_dec D[2 * KM_OPT + 4];
} km_enc;
#define P0(p) E->ptab[(p) >> 4]
#define P1(p) E->ptab[(KM_ONE - (p)) >> 4]
#define PB(p, b) ((b) ? P1(p) : P0(p))
static uint32_t km_tree_price(const km_enc *E, const kp *pr, int nb, uint32_t v) { uint32_t s = 0, m = 1; for (int i = nb - 1; i >= 0; i--) { unsigned b = (v >> i) & 1; s += PB(pr[m], b); m = m << 1 | b; } return s; }
static uint32_t km_rtree_price(const km_enc *E, const kp *pr, int nb, uint32_t v) { uint32_t s = 0, m = 1; for (int i = 0; i < nb; i++) { unsigned b = v & 1; v >>= 1; s += PB(pr[m], b); m = m << 1 | b; } return s; }
static void km_len_prices(km_enc *E, const km_len *L, uint32_t T[16][KM_MAXLEN + 1]) {
    uint32_t c0 = P0(L->choice), c1 = P1(L->choice), d0 = c1 + P0(L->choice2), d1 = c1 + P1(L->choice2), hi[256];
    for (uint32_t l = 0; l < 256; l++) hi[l] = d1 + km_tree_price(E, L->high, 8, l);
    for (int ps = 0; ps < (1 << E->m.pb); ps++) {
        for (uint32_t l = 0; l < 8; l++) T[ps][l + 2] = c0 + km_tree_price(E, L->low[ps], 3, l);
        for (uint32_t l = 0; l < 8; l++) T[ps][l + 10] = d0 + km_tree_price(E, L->mid[ps], 3, l);
        for (uint32_t l = 16; l < KM_MAXLEN - 1; l++) T[ps][l + 2] = hi[l - 16];
    }
}
static void km_dist_prices(km_enc *E) {
    for (int ls = 0; ls < 4; ls++) {
        for (uint32_t s = 0; s < 64; s++) E->slotp[ls][s] = km_tree_price(E, E->m.p.slot[ls], 6, s);
        for (uint32_t d = 0; d < 128; d++) {
            uint32_t s = km_slot(d), pr = E->slotp[ls][s];
            if (s >= 4) { int nb = (int)(s >> 1) - 1; uint32_t base = (2u | (s & 1)) << nb; pr += km_rtree_price(E, E->m.p.spec[s], nb, d - base); }
            E->distp[ls][d] = pr;
        }
    }
    for (uint32_t a = 0; a < 16; a++) E->alignp[a] = km_rtree_price(E, E->m.p.align, 4, a);
}
static inline uint32_t km_dist_price(const km_enc *E, uint32_t d, uint32_t len) {
    uint32_t ls = len - 2 < 3 ? len - 2 : 3;
    if (d < 128) return E->distp[ls][d];
    uint32_t s = km_slot(d); int nb = (int)(s >> 1) - 1;
    return E->slotp[ls][s] + ((uint32_t)(nb - 4) << 4) + E->alignp[d & 15];
}
static uint32_t km_lit_price(const km_enc *E, uint32_t pos) {
    const uint8_t *in = E->in; unsigned b = in[pos], prev = pos ? in[pos - 1] : 0, sym = 1; uint32_t s = 0;
    const kp *pr = km_litp(&E->m, pos, prev);
    for (int i = 7; i >= 0; i--) { unsigned bit = (b >> i) & 1; s += PB(pr[sym], bit); sym = sym << 1 | bit; }
    return s;
}
static uint32_t km_mlit_price(const km_enc *E, uint32_t pos, unsigned mb) {
    const uint8_t *in = E->in; unsigned b = in[pos], prev = pos ? in[pos - 1] : 0, sym = 1; uint32_t s = 0;
    const kp *pr = km_litp(&E->m, pos, prev);
    for (int i = 7; i >= 0; i--) {
        unsigned bit = (b >> i) & 1, mbit = (mb >> i) & 1;
        s += PB(pr[0x100 + (mbit << 8) + sym], bit); sym = sym << 1 | bit;
        if (bit != mbit) { for (i--; i >= 0; i--) { bit = (b >> i) & 1; s += PB(pr[sym], bit); sym = sym << 1 | bit; } break; }
    }
    return s;
}
static inline uint32_t km_rep_price(const km_enc *E, uint32_t r, int st, uint32_t ps) {   /* is_rep0.. bits of rep r (long) */
    const km_probs *p = &E->m.p;
    if (r == 0) return P0(p->is_rep0[st]) + P1(p->is_rep0_long[st][ps]);
    uint32_t s = P1(p->is_rep0[st]);
    if (r == 1) return s + P0(p->is_rep1[st]);
    return s + P1(p->is_rep1[st]) + PB(p->is_rep2[st], r == 3);
}

/* one window of the optimal parse from pos; fills E->D, returns the number of decisions */
static uint32_t km_optimal(km_enc *E, uint32_t pos) {
    km_node *o = E->opt; const uint8_t *in = E->in; const uint32_t n = E->n, nice = E->mf.nice, pbm = (1u << E->m.pb) - 1;
    const km_probs *P = &E->m.p;
    o[0].price = 0; o[0].state = (uint8_t)E->state; memcpy(o[0].reps, E->reps, sizeof E->reps);
    uint32_t reach = 0, i;
    for (i = 0;; i++) {
        const uint32_t abs = pos + i;
        if (i > 0 && (i >= reach || abs >= n)) break;
        if (i >= KM_OPT) break;
        if (i > 0) {   /* the node's state and repeat distances, from its best predecessor */
            km_node *q = &o[o[i].prev], *c = &o[i]; int st = q->state; uint32_t r[4]; memcpy(r, q->reps, sizeof r);
            switch (c->kind) {
                case 0: st = st_lit(st); break;
                case 1: st = st_short(st); break;
                case 6: st = st_match(st); r[3] = r[2]; r[2] = r[1]; r[1] = r[0]; r[0] = c->dist; break;
                case 7: st = st_rep(st_lit(st)); break;
                case 8: st = st_rep(st_lit(st_match(st))); r[3] = r[2]; r[2] = r[1]; r[1] = r[0]; r[0] = c->dist; break;
                case 9: { uint32_t k = c->dist, d = r[k]; for (; k > 0; k--) r[k] = r[k - 1]; r[0] = d; st = st_rep(st_lit(st_rep(st))); break; }
                default: { uint32_t k = c->kind - 2u, d = r[k]; for (; k > 0; k--) r[k] = r[k - 1]; r[0] = d; st = st_rep(st); }
            }
            c->state = (uint8_t)st; memcpy(c->reps, r, sizeof r);
        }
        uint32_t nm;
        if (E->cache_pos == abs) { nm = E->cache_nm; memcpy(E->M, E->cache, nm * sizeof(km_m)); }
        else nm = km_find(&E->mf, abs, E->M);
        E->cache_pos = KM_NONE;
        const km_node *c = &o[i]; const int st = c->state; const uint32_t ps = abs & pbm, maxl = n - abs < KM_MAXLEN ? n - abs : KM_MAXLEN;
        uint32_t rl[4] = {0, 0, 0, 0}, rbest = 0, rk = 0;
        for (uint32_t k = 0; k < 4; k++) {
            uint32_t d = c->reps[k] + 1;
            if (d <= abs && maxl >= 2 && in[abs] == in[abs - d] && in[abs + 1] == in[abs + 1 - d]) { rl[k] = km_count(in + abs, in + abs - d, maxl); if (rl[k] > rbest) { rbest = rl[k]; rk = k; } }
        }
        const uint32_t longest = nm ? E->M[nm - 1].len : 0;
        if (rbest >= nice || longest >= nice) {
            if (i == 0) {
                if (rbest >= nice && rbest + 1 >= longest) { E->D[0].kind = (uint8_t)(2 + rk); E->D[0].len = rbest; E->D[0].dist = 0; }
                else { E->D[0].kind = 6; E->D[0].len = longest; E->D[0].dist = E->M[nm - 1].dist - 1; }
                for (uint32_t k = 1; k < E->D[0].len; k++) km_skip(&E->mf, abs + k);
                return 1;
            }
            E->cache_pos = abs; E->cache_nm = nm; memcpy(E->cache, E->M, nm * sizeof(km_m));
            break;
        }
#define KM_RELAX(J, PR, KIND, LEN, DIST) KM_RELAX2(J, PR, KIND, LEN, DIST, 0)
#define KM_RELAX2(J, PR, KIND, LEN, DIST, L2) do { uint32_t j_ = (J), p_ = (PR); \
            if (j_ > reach) { for (uint32_t k_ = reach + 1; k_ <= j_; k_++) o[k_].price = KM_INF; reach = j_; } \
            if (p_ < o[j_].price) { o[j_].price = p_; o[j_].prev = i; o[j_].kind = (KIND); o[j_].len = (uint16_t)(LEN); o[j_].dist = (DIST); o[j_].len2 = (uint16_t)(L2); } } while (0)
/* after a match or rep of its full length L at distance D: a literal that differs from the byte D back, then rep 0 of
 * D again for the following 2+ bytes (only the longest such rep is priced), as LZMA-class optimisers do */
#define KM_COMBO(L, D, S1, BASE, KIND, DF) do { const uint32_t L_ = (L), q_ = abs + L_, dd_ = (D) + 1; \
            if (E->combo && q_ + 3 <= n && in[q_] != in[q_ - dd_] && in[q_ + 1] == in[q_ + 1 - dd_] && in[q_ + 2] == in[q_ + 2 - dd_]) { \
                uint32_t lim_ = n - q_ - 1; if (lim_ > nice) lim_ = nice; if (lim_ > KM_MAXLEN) lim_ = KM_MAXLEN; \
                const uint32_t l2_ = km_count(in + q_ + 1, in + q_ + 1 - dd_, lim_); \
                const int s1_ = (S1), s2_ = st_lit(s1_); const uint32_t pq_ = q_ & pbm, pq1_ = (q_ + 1) & pbm; \
                const uint32_t pr_ = (BASE) + P0(P->is_match[s1_][pq_]) + km_mlit_price(E, q_, in[q_ - dd_]) \
                    + P1(P->is_match[s2_][pq1_]) + P1(P->is_rep[s2_]) + km_rep_price(E, 0, s2_, pq1_) + E->rlenp[pq1_][l2_]; \
                KM_RELAX2(i + L_ + 1 + l2_, pr_, KIND, L_, DF, l2_); } } while (0)
        const uint32_t m0 = P0(P->is_match[st][ps]), m1 = P1(P->is_match[st][ps]);
        /* literal */
        { uint32_t lp = st >= 7 ? km_mlit_price(E, abs, in[abs - c->reps[0] - 1]) : km_lit_price(E, abs);
          KM_RELAX(i + 1, c->price + m0 + lp, 0, 1, 0);
          /* a literal, then a repeat of the last distance from the next byte (only the full length is priced) */
          const uint32_t d0 = c->reps[0] + 1;
          if (maxl >= 3 && abs + 1 >= d0 && (d0 > abs || in[abs] != in[abs - d0]) && in[abs + 1] == in[abs + 1 - d0] && in[abs + 2] == in[abs + 2 - d0]) {
              uint32_t l2 = km_count(in + abs + 1, in + abs + 1 - d0, (maxl - 1 < nice ? maxl - 1 : nice));
              if (l2 >= 2) {
                  const int s2 = st_lit(st); const uint32_t p2 = (abs + 1) & pbm;
                  KM_RELAX(i + 1 + l2, c->price + m0 + lp + P1(P->is_match[s2][p2]) + P1(P->is_rep[s2]) + km_rep_price(E, 0, s2, p2) + E->rlenp[p2][l2], 7, l2, 0);
              }
          } }
        if (maxl == 0) continue;
        const uint32_t rep1p = c->price + m1 + P1(P->is_rep[st]);
        /* short rep: one byte at the last distance */
        if (c->reps[0] + 1 <= abs && in[abs] == in[abs - c->reps[0] - 1])
            KM_RELAX(i + 1, rep1p + P0(P->is_rep0[st]) + P0(P->is_rep0_long[st][ps]), 1, 1, 0);
        /* repeat matches */
        for (uint32_t k = 0; k < 4; k++) if (rl[k] >= 2) {
            uint32_t b = rep1p + km_rep_price(E, k, st, ps);
            for (uint32_t l = 2; l <= rl[k]; l++) KM_RELAX(i + l, b + E->rlenp[ps][l], 2 + k, l, 0);
            KM_COMBO(rl[k], c->reps[k], st_rep(st), b + E->rlenp[ps][rl[k]], 9, k);
        }
        /* matches */
        if (nm) {
            uint32_t b = c->price + m1 + P0(P->is_rep[st]), l = 2;
            for (uint32_t k = 0; k < nm; k++) {
                uint32_t d = E->M[k].dist - 1;
                for (; l <= E->M[k].len; l++) KM_RELAX(i + l, b + E->lenp[ps][l] + km_dist_price(E, d, l), 6, l, d);
                { const uint32_t L = E->M[k].len < maxl ? E->M[k].len : maxl; KM_COMBO(L, d, st_match(st), b + E->lenp[ps][L] + km_dist_price(E, d, L), 8, d); }
            }
        }
#undef KM_RELAX
#undef KM_RELAX2
#undef KM_COMBO
    }
    /* back from node i to node 0 */
    uint32_t nd = 0;
    for (uint32_t j = i; j > 0; j = o[j].prev) nd += o[j].kind == 7 ? 2 : o[j].kind >= 8 ? 3 : 1;
    uint32_t k = nd;
    for (uint32_t j = i; j > 0; j = o[j].prev) {
        if (o[j].kind >= 8) { k--; E->D[k].kind = 2; E->D[k].len = o[j].len2; E->D[k].dist = 0; k--; E->D[k].kind = 0; E->D[k].len = 1; E->D[k].dist = 0;
            k--; if (o[j].kind == 8) { E->D[k].kind = 6; E->D[k].dist = o[j].dist; } else { E->D[k].kind = (uint8_t)(2 + o[j].dist); E->D[k].dist = 0; } E->D[k].len = o[j].len; }
        else if (o[j].kind == 7) { k--; E->D[k].kind = 2; E->D[k].len = o[j].len; E->D[k].dist = 0; k--; E->D[k].kind = 0; E->D[k].len = 1; E->D[k].dist = 0; }
        else { k--; E->D[k].kind = o[j].kind; E->D[k].len = o[j].len; E->D[k].dist = o[j].dist; }
    }
    return nd;
}

/* per level: finder depth, nice length, binary tree; and from level 6 the composite paths (kinds 8, 9) with prices
 * refreshed every 32 coded lengths or matches instead of 128 (review40 #93: level 8, the one `-9` uses, has nice 273) */
static const struct { int depth, nice, bt, strong; } KM_LV[10] = {{0, 0, 0, 0}, {4, 16, 0, 0}, {8, 32, 0, 0}, {16, 48, 1, 0}, {24, 64, 1, 0},
    {32, 64, 1, 0}, {48, 96, 1, 1}, {64, 128, 1, 1}, {96, 273, 1, 1}, {152, 273, 1, 1}};
size_t kolmo_klzma_bound(size_t n) { return n + n / 64 + 1024; }
size_t kolmo_klzma_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level) { return kolmo_klzma_compress_mt(in, n, out, cap, level, 1); }
size_t kolmo_klzma_compress_mt(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level, int threads) {
    return kolmo_klzma_compress_el(in, n, out, cap, level, threads, threads - 1, NULL);
}
size_t kolmo_klzma_compress_el(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level, int threads, int passes, kolmo_budget *budget) {
    int lc = 3, lp = 0, pb = 2;
    if (level >= 1000) { int q = level / 10; lc = (q / 100) % 10 - 1; lp = (q / 10) % 10; pb = q % 10; level %= 10; }
    if (level < 1) level = 1;
    if (level > 9) level = 9;
    if (lc < 0 || lc > 8 || lp > 4 || pb > 4 || lc + lp > 4 || n >= 0xFFFFFF00u || cap < 16) return 0;
    km_enc *E = calloc(1, sizeof *E); size_t r = 0;
    if (!E) return 0;
    E->in = in; E->n = (uint32_t)n;
    int hlog = 16; while (hlog < 24 && ((size_t)1 << hlog) < n) hlog++;
    km_mf *M = &E->mf; M->in = in; M->n = (uint32_t)n; M->hlog = hlog; M->depth = KM_LV[level].depth; M->nice = (uint32_t)KM_LV[level].nice; M->bt = KM_LV[level].bt;
    E->combo = KM_LV[level].strong; E->refresh = KM_LV[level].strong ? 32 : 128;
    M->h4 = malloc(sizeof(uint32_t) << hlog); M->h3 = malloc(sizeof(uint32_t) << 16); M->h2 = malloc(sizeof(uint32_t) << 16); if (M->bt) M->son = malloc(sizeof(uint32_t) * 2 * (n + 1)); else M->chain = malloc(sizeof(uint32_t) * (n + 1));
    if (!M->h4 || !M->h3 || !M->h2 || !(M->bt ? (void *)M->son : (void *)M->chain) || km_model_init(&E->m, lc, lp, pb)) goto out;
    memset(M->h4, 0xFF, sizeof(uint32_t) << hlog); memset(M->h3, 0xFF, sizeof(uint32_t) << 16); memset(M->h2, 0xFF, sizeof(uint32_t) << 16);
    {   /* K workers beside the parse (not with two threads: one worker beside the parse gained nothing in KLZH2) */
        int K = threads >= 3 ? threads - 1 : 0; if (K > KM_MTMAXK) K = KM_MTMAXK;
        if (K > 0 && n >= KM_MT_MIN) M->mt = km_mt_start(M, K, passes, budget);
    }
    for (uint32_t i = 0; i < (KM_ONE >> 4); i++) E->ptab[i] = (uint16_t)km_price_of((i << 4) + 8);
    out[0] = (uint8_t)(lc + 9 * (lp + 5 * pb));
    km_rce *R = &E->rc; R->low = 0; R->range = 0xFFFFFFFFu; R->cache = 0; R->csize = 1; R->p = out + 1; R->e = out + cap;
    E->cache_pos = KM_NONE; E->len_cnt = E->dist_cnt = 1 << 30;
    km_probs *P = &E->m.p; const uint32_t pbm = (1u << pb) - 1;
    uint32_t pos = 0;
    while (pos < n) {
        if (E->len_cnt >= E->refresh) { km_len_prices(E, &P->len, E->lenp); km_len_prices(E, &P->rlen, E->rlenp); E->len_cnt = 0; }
        if (E->dist_cnt >= E->refresh) { km_dist_prices(E); E->dist_cnt = 0; }
        uint32_t nd = km_optimal(E, pos);
        for (uint32_t k = 0; k < nd; k++) {
            const km_dec *d = &E->D[k]; const int st = E->state; const uint32_t ps = pos & pbm;
            if (d->kind == 0) {
                rce_bit(R, &P->is_match[st][ps], 0);
                rce_lit(R, km_litp(&E->m, pos, pos ? in[pos - 1] : 0), in[pos], st >= 7, st >= 7 ? in[pos - E->reps[0] - 1] : 0);
                E->state = st_lit(st); pos++; continue;
            }
            rce_bit(R, &P->is_match[st][ps], 1);
            if (d->kind == 6) {
                rce_bit(R, &P->is_rep[st], 0); rce_len(R, &P->len, d->len - 2, ps);
                uint32_t dd = d->dist, s = km_slot(dd), ls = d->len - 2 < 3 ? d->len - 2 : 3;
                rce_tree(R, P->slot[ls], 6, s);
                if (s >= 4) {
                    int nb = (int)(s >> 1) - 1; uint32_t x = dd - ((2u | (s & 1)) << nb);
                    if (s < 14) rce_rtree(R, P->spec[s], nb, x);
                    else { rce_direct(R, x >> 4, nb - 4); rce_rtree(R, P->align, 4, x & 15); }
                }
                E->reps[3] = E->reps[2]; E->reps[2] = E->reps[1]; E->reps[1] = E->reps[0]; E->reps[0] = dd;
                E->state = st_match(st); E->len_cnt++; E->dist_cnt++;
            } else {
                rce_bit(R, &P->is_rep[st], 1);
                if (d->kind == 1) { rce_bit(R, &P->is_rep0[st], 0); rce_bit(R, &P->is_rep0_long[st][ps], 0); E->state = st_short(st); pos++; continue; }
                uint32_t rk = d->kind - 2u;
                if (rk == 0) { rce_bit(R, &P->is_rep0[st], 0); rce_bit(R, &P->is_rep0_long[st][ps], 1); }
                else {
                    rce_bit(R, &P->is_rep0[st], 1);
                    if (rk == 1) rce_bit(R, &P->is_rep1[st], 0); else { rce_bit(R, &P->is_rep1[st], 1); rce_bit(R, &P->is_rep2[st], rk == 3); }
                    uint32_t dd = E->reps[rk]; for (; rk > 0; rk--) E->reps[rk] = E->reps[rk - 1]; E->reps[0] = dd;
                }
                rce_len(R, &P->rlen, d->len - 2, ps); E->state = st_rep(st); E->len_cnt++;
            }
            pos += d->len;
        }
        if (R->err || M->mterr) goto out;
    }
    for (int k = 0; k < 5; k++) rce_shift(R);
    if (!R->err && (size_t)(R->p - out) < n + 16) r = (size_t)(R->p - out);
out:
    km_mt_end(M->mt);
    free(M->h4); free(M->h3); free(M->h2); free(M->chain); free(M->son); free(E->m.lit); free(E);
    return r;
}

/* ---------------- decoder ---------------- */
#define KD_NORM do { if (range < KM_TOP) { range <<= 8; code = (code << 8) | (ip < ie ? *ip : 0u); ip++; } } while (0)
#define KD_BIT(PR, B) do { kp *p_ = (PR); uint32_t bd_ = (range >> KM_PB) * *p_; \
        if (code < bd_) { range = bd_; *p_ += (KM_ONE - *p_) >> KM_MOVE; B = 0; } else { code -= bd_; range -= bd_; *p_ -= *p_ >> KM_MOVE; B = 1; } KD_NORM; } while (0)
/* branch-free bit for the trees (literal and length bits are hard to predict); the same arithmetic as KD_BIT */
#define KD_BITB(PR, B) do { kp *p_ = (PR); uint32_t pv_ = *p_, bd_ = (range >> KM_PB) * pv_, m_ = 0u - (uint32_t)(code >= bd_); \
        range = (bd_ & ~m_) | ((range - bd_) & m_); code -= bd_ & m_; \
        *p_ = (kp)(pv_ + (((KM_ONE - pv_) >> KM_MOVE) & ~m_) - ((pv_ >> KM_MOVE) & m_)); B = m_ & 1u; KD_NORM; } while (0)
#define KD_TREE(PR, NB, V) do { uint32_t m_ = 1; for (int i_ = 0; i_ < (NB); i_++) { unsigned b_; KD_BITB(&(PR)[m_], b_); m_ = m_ << 1 | b_; } V = m_ - (1u << (NB)); } while (0)
#define KD_RTREE(PR, NB, V) do { uint32_t m_ = 1, v_ = 0; for (int i_ = 0; i_ < (NB); i_++) { unsigned b_; KD_BITB(&(PR)[m_], b_); m_ = m_ << 1 | b_; v_ |= b_ << i_; } V = v_; } while (0)
#define KD_LEN(L, PS, V) do { unsigned c_; KD_BIT(&(L)->choice, c_); \
        if (!c_) { KD_TREE((L)->low[PS], 3, V); } else { KD_BIT(&(L)->choice2, c_); \
            if (!c_) { KD_TREE((L)->mid[PS], 3, V); V += 8; } else { KD_TREE((L)->high, 8, V); V += 16; } } } while (0)
int kolmo_klzma_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 6) return -1;
    int pr = in[0], lc = pr % 9, lp = (pr / 9) % 5, pb = pr / 45;
    if (pb > 4 || lc + lp > 4) return -1;
    km_model m; if (km_model_init(&m, lc, lp, pb)) return -1;
    km_probs *P = &m.p; const uint8_t *ip = in + 1, *ie = in + cn; uint32_t range = 0xFFFFFFFFu, code = 0; int rc = -1;
    for (int k = 0; k < 5; k++) code = (code << 8) | *ip++;
    uint32_t pos = 0, reps[4] = {0, 0, 0, 0}; int st = 0; const uint32_t pbm = (1u << pb) - 1, lpm = (1u << lp) - 1;
    while (pos < n) {
        if (ip > ie + 4) goto out;   /* past what the end check below accepts: truncated or forged, stop now */
        const uint32_t ps = pos & pbm; unsigned b;
        KD_BIT(&P->is_match[st][ps], b);
        if (!b) {   /* literal */
            unsigned prev = pos ? out[pos - 1] : 0, sym = 1;
            kp *lpr = m.lit + 0x300u * (((pos & lpm) << lc) + (prev >> (8 - lc)));
            if (st >= 7) {
                /* branch-free form of the encoder's rule (rce_lit): tree 0x100 + 0x100 * match bit + sym while the bits
                 * agree, tree sym after the first difference (off drops to 0 there) */
                unsigned mb = out[pos - reps[0] - 1], off = 0x100;
                for (int i = 0; i < 8; i++) {
                    mb <<= 1; unsigned mbit = mb & off, bit; KD_BITB(&lpr[off + mbit + sym], bit); sym = sym << 1 | bit; off &= mbit ^ (bit - 1u);
                }
            } else for (int i = 0; i < 8; i++) { unsigned bit; KD_BITB(&lpr[sym], bit); sym = sym << 1 | bit; }
            out[pos++] = (uint8_t)sym; st = st_lit(st); continue;
        }
        uint32_t len;
        KD_BIT(&P->is_rep[st], b);
        if (!b) {   /* match */
            KD_LEN(&P->len, ps, len); len += 2;
            uint32_t s, ls = len - 2 < 3 ? len - 2 : 3, d;
            KD_TREE(P->slot[ls], 6, s);
            if (s < 4) d = s;
            else {
                int nb = (int)(s >> 1) - 1; d = (2u | (s & 1)) << nb;
                if (s < 14) { uint32_t x; KD_RTREE(P->spec[s], nb, x); d += x; }
                else {
                    uint32_t x = 0;
                    for (int k = 0; k < nb - 4; k++) { range >>= 1; unsigned bit = code >= range; if (bit) code -= range; x = x << 1 | bit; KD_NORM; }
                    uint32_t a; KD_RTREE(P->align, 4, a); d += (x << 4) + a;
                }
            }
            reps[3] = reps[2]; reps[2] = reps[1]; reps[1] = reps[0]; reps[0] = d; st = st_match(st);
        } else {
            KD_BIT(&P->is_rep0[st], b);
            if (!b) {
                KD_BIT(&P->is_rep0_long[st][ps], b);
                if (!b) {   /* short rep */
                    if (reps[0] >= pos) goto out;   /* reps[0] + 1 wrapped to 0 for 0xFFFFFFFF */
                    out[pos] = out[pos - reps[0] - 1]; pos++; st = st_short(st); continue;
                }
            } else {
                uint32_t d; KD_BIT(&P->is_rep1[st], b);
                if (!b) { d = reps[1]; reps[1] = reps[0]; }
                else { KD_BIT(&P->is_rep2[st], b); if (!b) { d = reps[2]; } else { d = reps[3]; reps[3] = reps[2]; } reps[2] = reps[1]; reps[1] = reps[0]; }
                reps[0] = d;
            }
            KD_LEN(&P->rlen, ps, len); len += 2; st = st_rep(st);
        }
        if (reps[0] >= pos || len > n - pos) goto out;   /* not dist > pos: reps[0] = 0xFFFFFFFF made dist 0 */
        uint32_t dist = reps[0] + 1;
        uint8_t *o = out + pos; const uint8_t *s = o - dist;
        if (dist >= 8 && n - pos - len >= 8) { for (uint32_t k = 0; k < len; k += 8) memcpy(o + k, s + k, 8); }
        else for (uint32_t k = 0; k < len; k++) o[k] = s[k];
        pos += len;
    }
    rc = ip <= ie + 4 ? 0 : -1;
out:
    free(m.lit); return rc;
}
