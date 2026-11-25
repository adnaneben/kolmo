/* kfam core: the family-stream coder for FASTQ. Apache-2.0, Kolmo contributors.
 *
 * Included by src/kolmo.c (block codec 16, FQFAM) and by src/experimental/kfam.c (the standalone tool, which adds
 * the CSV and one-stream families). Every name starts with kf_ / KF_ so it can live inside kolmo.c.
 *
 * A FASTQ text (complete four-line records, "\n" endings, the plus line bare or repeating the name) is split into
 * five streams:
 *   0 layout     per read a flag byte (plus-line form; +2 when the length changes, then the length as a varint)
 *   1 names      per name token an operation (same / numeric delta / literal), the token index as context
 *   2 bases      2-bit symbols (A C G T; anything else codes as A and is listed in stream 3)
 *   3 exceptions per non-ACGT base: (site delta varint, byte)
 *   4 qualities  quality byte - 33, one per base (no separators: the layout gives the read lengths)
 * Each stream is coded by a small model of its own and a binary arithmetic coder with 16-bit probabilities. The
 * names and qualities are cut into chunks at read boundaries (KF_CHUNK symbols by default; bases KF_BCHUNK), each
 * chunk coded with fresh models, so a frame codes and decodes on many cores. Chunk boundaries depend only on the input.
 *
 * Frame:  "KFM2" | family u8 | original size uv | streams uv | per stream: kind u8, parameter length uv, parameter
 *         bytes | jobs uv | per job: stream uv, raw size uv, coded size uv | coded jobs in order.
 * A stream's raw bytes are its jobs' raw bytes in order. The quality stream's parameter is its symbol counts
 * (128-bit presence mask, then a varint per present symbol), from which coder and decoder build the same binary
 * code tree.
 *
 * Quality model: in the spirit of the published fqzcomp quality model (Bonfield and Mahoney, 2013), with contexts
 * from the previous three qualities, the position in the read, a running sum of quality changes in the read, the
 * mean quality so far and whether the base is not A/C/G/T; written for Kolmo from that description. Six context
 * tables are mixed by two mixers (selected by tree node with the change sum, and by tree node with the position),
 * a second-layer mixer combines them and an adaptive probability map refines the result.
 *
 * Determinism: integer arithmetic only, except the logistic tables, which are built from IEEE double +, *, /
 * only (no libm), so every platform gets the same tables. */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ logistic tables (12-bit domain) */
static int16_t kf_sqt[4096];        /* squash: stretch domain [-2048, 2047] -> probability in [1, 4095] */
static int16_t kf_stt[4096];        /* stretch: probability -> smallest x with squash(x) >= p */
static uint32_t kf_rate[1024];      /* adaptation rate of a counter after n updates: 65536 / (n + 1.6) */
static uint8_t kf_dbk[4096];        /* quality change sum -> bucket 0..15 */
static pthread_once_t kf_tab_once = PTHREAD_ONCE_INIT;
static double kf_exp(double x) {    /* e^x from basic operations: exp(x) = exp(x / 1024)^1024, Taylor series inside */
    double y = x / 1024.0, t = 1.0, s = 1.0;
    for (int k = 1; k <= 12; k++) { t = t * y / k; s = s + t; }
    for (int k = 0; k < 10; k++) s = s * s;
    return s;
}
static void kf_tab_init(void) {
    for (int i = 0; i < 4096; i++) {
        double v = 4096.0 / (1.0 + kf_exp(-(i - 2048) / 256.0));
        int q = (int)(v + 0.5); kf_sqt[i] = (int16_t)(q < 1 ? 1 : q > 4095 ? 4095 : q);
    }
    for (int p = 0; p < 4096; p++) {
        int lo = -2048, hi = 2048;
        while (lo < hi) { int mid = lo + ((hi - lo) >> 1); if (kf_sqt[mid + 2048] >= p) hi = mid; else lo = mid + 1; }
        kf_stt[p] = (int16_t)(lo > 2047 ? 2047 : lo);
    }
    for (int n = 0; n < 1024; n++) kf_rate[n] = (uint32_t)(65536.0 / (n + 1.6));
    for (int d = 0; d < 4096; d++) { int b = 0; while (b < 15 && (b + 1) * (b + 1) <= 2 * d) b++; kf_dbk[d] = (uint8_t)b; }   /* ~sqrt(2d) */
}
static inline int kf_sq(int x) { return kf_sqt[(x < -2048 ? -2048 : x > 2047 ? 2047 : x) + 2048]; }
static inline int kf_st(int p) { return kf_stt[p]; }
static inline uint32_t kf_hsh(uint32_t a, uint32_t b) { uint32_t h = a * 0x9E3779B1u ^ b * 0x85EBCA77u; h ^= h >> 15; h *= 0x2C1B3C6Du; return h ^ (h >> 13); }

/* ------------------------------------------------------------------ growable byte buffer and varints */
typedef struct { uint8_t *p; size_t n, cap; } kf_buf;
static int kf_put(kf_buf *b, const void *s, size_t n) {
    if (n > b->cap - b->n) {
        size_t c = b->cap ? b->cap : 4096; while (c - b->n < n) c *= 2;
        uint8_t *t = realloc(b->p, c); if (!t) return -1; b->p = t; b->cap = c;
    }
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n; return 0;
}
static inline int kf_byte(kf_buf *b, uint8_t v) {
    if (b->n < b->cap) { b->p[b->n++] = v; return 0; }
    return kf_put(b, &v, 1);
}
static int kf_var(kf_buf *b, uint64_t v) { while (v >= 128) { if (kf_byte(b, (uint8_t)(v | 128))) return -1; v >>= 7; } return kf_byte(b, (uint8_t)v); }
static int kf_getvar(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int s = 0;
    while (*i < n && s < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << s; if (!(c & 128)) { *v = r; return 0; } s += 7; }
    return -1;
}
static inline uint64_t kf_zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
static inline int64_t kf_unzz(uint64_t v) { return (int64_t)(v >> 1) ^ -(int64_t)(v & 1); }

/* ------------------------------------------------------------------ binary arithmetic coder, 16-bit probabilities */
typedef struct { uint32_t lo, hi; kf_buf out; int err; } kf_aenc;
typedef struct { uint32_t lo, hi, x; const uint8_t *in; size_t n, pos, over; } kf_adec;   /* over: bytes read past the end */
typedef struct { kf_aenc *e; kf_adec *d; } kf_coder;      /* one coder object for both directions */
static inline void kf_ae_bit(kf_aenc *e, int y, uint32_t p) {          /* p = P(y = 1) in [1, 65535] */
    uint32_t mid = e->lo + (uint32_t)(((uint64_t)(e->hi - e->lo) * p) >> 16);
    if (y) e->hi = mid; else e->lo = mid + 1;
    while ((e->lo ^ e->hi) < (1u << 24)) { if (kf_byte(&e->out, (uint8_t)(e->hi >> 24))) e->err = 1; e->lo <<= 8; e->hi = (e->hi << 8) | 0xffu; }
}
static void kf_ae_end(kf_aenc *e) { for (int k = 0; k < 4; k++) { if (kf_byte(&e->out, (uint8_t)(e->lo >> 24))) e->err = 1; e->lo <<= 8; } }
static inline uint8_t kf_ad_next(kf_adec *d) { if (d->pos < d->n) return d->in[d->pos++]; d->over++; return 0; }
static void kf_ad_init(kf_adec *d, const uint8_t *in, size_t n) {
    d->lo = 0; d->hi = 0xffffffffu; d->x = 0; d->in = in; d->n = n; d->pos = 0; d->over = 0;
    for (int k = 0; k < 4; k++) d->x = (d->x << 8) | kf_ad_next(d);
}
static inline int kf_ad_bit(kf_adec *d, uint32_t p) {
    uint32_t mid = d->lo + (uint32_t)(((uint64_t)(d->hi - d->lo) * p) >> 16);
    int y = d->x <= mid;
    if (y) d->hi = mid; else d->lo = mid + 1;
    while ((d->lo ^ d->hi) < (1u << 24)) { d->lo <<= 8; d->hi = (d->hi << 8) | 0xffu; d->x = (d->x << 8) | kf_ad_next(d); }
    return y;
}
static inline int kf_code16(kf_coder *c, int y, uint32_t p) {          /* p = P(1) in 16 bits */
    if (p < 32) p = 32; if (p > 65503) p = 65503;
    if (c->e) { kf_ae_bit(c->e, y, p); return y; }
    return kf_ad_bit(c->d, p);
}
static inline int kf_code(kf_coder *c, int y, int p12) { return kf_code16(c, y, (uint32_t)p12 << 4); }

/* ------------------------------------------------------------------ counters, mixers, adaptive probability maps */
typedef struct { uint16_t p, n; } kf_slot;                                       /* probability (16 bits), updates */
typedef struct { kf_slot *s; uint32_t mask; int limit; } kf_ctab;                  /* adaptive probabilities */
static int kf_ctab_new(kf_ctab *t, int bits, int limit) {
    size_t sz = (size_t)1 << bits; t->mask = (uint32_t)sz - 1; t->limit = limit;
    t->s = malloc(sz * sizeof *t->s);
    if (!t->s) return -1;
    for (size_t i = 0; i < sz; i++) { t->s[i].p = 32768; t->s[i].n = 0; }
    return 0;
}
static void kf_ctab_free(kf_ctab *t) { free(t->s); t->s = NULL; }
static inline int kf_ctab_p12(const kf_ctab *t, uint32_t i) { return t->s[i & t->mask].p >> 4; }
static inline void kf_ctab_upd(kf_ctab *t, uint32_t i, int y) {
    kf_slot *e = &t->s[i & t->mask]; int n = e->n, p = e->p;
    p += (int)(((int64_t)((y ? 65535 : 0) - p) * kf_rate[n]) >> 16);
    e->p = (uint16_t)(p < 1 ? 1 : p > 65535 ? 65535 : p);
    if (n < t->limit) e->n = (uint16_t)(n + 1);
}
enum { KF_MAXIN = 24 };
typedef struct { int32_t *w; int nin, nsets, x[KF_MAXIN], pr, set; } kf_mixer;   /* logistic mixing, weights selected by a context */
static int kf_mixer_new(kf_mixer *m, int nin, int nsets) {                         /* the last input is the bias */
    m->nin = nin; m->nsets = nsets; m->w = malloc(sizeof(int32_t) * (size_t)nin * nsets);
    if (!m->w) return -1;
    for (int i = 0; i < nin * nsets; i++) m->w[i] = i % nin == nin - 1 ? 0 : (1 << 16) / (nin - 1);
    return 0;
}
static inline int kf_mix(kf_mixer *m, int set) {
    m->set = set; const int32_t *w = m->w + (size_t)set * m->nin; int64_t dot = 0;
    for (int i = 0; i < m->nin; i++) dot += (int64_t)m->x[i] * w[i];
    return m->pr = kf_sq((int)(dot >> 16));
}
static inline int kf_mix_st(kf_mixer *m, int set) {                                /* as kf_mix, in the stretch domain */
    m->set = set; const int32_t *w = m->w + (size_t)set * m->nin; int64_t dot = 0;
    for (int i = 0; i < m->nin; i++) dot += (int64_t)m->x[i] * w[i];
    int s = (int)(dot >> 16); s = s < -2047 ? -2047 : s > 2047 ? 2047 : s;
    m->pr = kf_sq(s); return s;
}
static inline void kf_mix_upd(kf_mixer *m, int y, int lr) {
    int32_t *w = m->w + (size_t)m->set * m->nin; int err = ((y << 12) - m->pr) * lr;
    for (int i = 0; i < m->nin; i++) w[i] += (m->x[i] * err) >> 10;
}
static inline void kf_mix_upd14(kf_mixer *m, int y, int lr) {                     /* finer rate: lr / 16 of the above */
    int32_t *w = m->w + (size_t)m->set * m->nin; int err = ((y << 12) - m->pr) * lr;
    for (int i = 0; i < m->nin; i++) w[i] += (m->x[i] * err) >> 14;
}
/* adaptive probability map: per context 33 interpolated buckets over the stretch domain, 16-bit probabilities */
typedef struct { uint16_t *t; int idx; } kf_apm;
static int kf_apm_new(kf_apm *a, int n) {
    a->t = malloc(sizeof(uint16_t) * 33 * (size_t)n); if (!a->t) return -1;
    for (int c = 0; c < n; c++) for (int j = 0; j < 33; j++) a->t[c * 33 + j] = (uint16_t)(kf_sq((j - 16) * 128) * 16);
    return 0;
}
static inline int kf_apm_p(kf_apm *a, int s, int ctx) {                            /* s: stretch; returns 16-bit p */
    int u = s + 2048; if (u < 0) u = 0; if (u > 4095) u = 4095;
    int lo = u >> 7, w = u & 127; uint16_t *t = a->t + (size_t)ctx * 33 + lo;
    a->idx = ctx * 33 + lo + (w >> 6);
    return (t[0] * (128 - w) + t[1] * w) >> 7;
}
static inline void kf_apm_upd(kf_apm *a, int y, int rate) {
    int g = (y << 16) + (y << rate) - y - y; uint16_t *t = a->t + a->idx;
    *t = (uint16_t)(*t + ((g - *t) >> rate));
}

/* ------------------------------------------------------------------ the byte model (names, layout, exceptions) */
/* Orders 0 to 3 of the stream's own bytes, plus one context the family supplies (token index, byte of a number);
 * five counters mixed by a mixer chosen by the partial byte. Table sizes follow the stream size. */
typedef struct { kf_ctab t[5]; kf_mixer mx; uint32_t c4, h[5]; } kf_bmodel;
static void kf_bm_free(kf_bmodel *m) { for (int i = 0; i < 5; i++) kf_ctab_free(&m->t[i]); free(m->mx.w); m->mx.w = NULL; }
static int kf_bm_new(kf_bmodel *m, size_t len) {
    int b = 10; while (b < 22 && ((size_t)1 << b) < len * 2) b++;
    memset(m, 0, sizeof *m);
    /* counters adapt fast (rate floor 1/17): name fields drift (tiles, lanes); 255 was 30% larger on names */
    if (kf_ctab_new(&m->t[0], 16, 16) || kf_ctab_new(&m->t[1], 16, 16) || kf_ctab_new(&m->t[2], b, 16) || kf_ctab_new(&m->t[3], b, 16)
        || kf_ctab_new(&m->t[4], b, 16) || kf_mixer_new(&m->mx, 6, 256)) { kf_bm_free(m); return -1; }
    for (int i = 0; i < 6 * 256; i++) m->mx.w[i] = (1 << 16) / 6;      /* every input, the bias too, starts at 1/6 */
    return 0;
}
static int kf_bm_code(kf_bmodel *m, kf_coder *c, int byte, uint32_t xc) {   /* codes (or decodes) one byte; returns it */
    uint32_t c4 = m->c4;
    m->h[0] = (xc & 0xff) << 8; m->h[1] = (c4 & 0xff) << 8;
    m->h[2] = kf_hsh(c4 & 0xffff, 2); m->h[3] = kf_hsh(c4 & 0xffffff, 3); m->h[4] = kf_hsh(xc, (c4 & 0xff) | 4u << 8);
    int c0 = 1;
    for (int b = 7; b >= 0; b--) {
        uint32_t i[5] = {m->h[0] | c0, m->h[1] | c0, m->h[2] + c0 * 0x3C6EF372u, m->h[3] + c0 * 0x3C6EF372u, m->h[4] + c0 * 0x3C6EF372u};
        for (int k = 0; k < 5; k++) m->mx.x[k] = kf_st(kf_ctab_p12(&m->t[k], i[k]));
        m->mx.x[5] = 256;
        int p = kf_mix(&m->mx, c0);
        int y = kf_code(c, (byte >> b) & 1, p);
        for (int k = 0; k < 5; k++) kf_ctab_upd(&m->t[k], i[k], y);
        kf_mix_upd(&m->mx, y, 6);
        c0 = c0 << 1 | y;
    }
    int v = c0 & 0xff; m->c4 = c4 << 8 | (uint32_t)v; return v;
}

/* ------------------------------------------------------------------ DNA bases: 2-bit symbols */
/* Two order-k contexts (the last 6 and 12 bases) and three match models: the earlier occurrence of the last KF_MML
 * bases, read forwards; the earlier occurrence of their reverse complement, read backwards (reads come from both
 * strands); and the last occurrence of the last KF_LML bases within KF_LW bases (overlapping neighbour reads, as in
 * sorted or amplicon data). A match model's input is the learned reliability of its predicted bit for its match
 * length and recent misses; a mismatch (a variant or a read error) keeps the alignment, and a new candidate replaces
 * a match shorter than its k-mer. The order-12 context is hashed by its older 11 bases into a 64-byte bucket holding
 * the 4 x 3 counters of the newest base, so the next base's bucket is known one base ahead and is prefetched; the
 * k-mer index is bucketed the same way. (More orders - 2, 4, 8, 16, 22 - made the output larger on all three test
 * runs, and slower.) */
#if defined(__GNUC__) || defined(__clang__)
#define KF_PREFETCH(p) __builtin_prefetch(p)
#else
#define KF_PREFETCH(p) ((void)0)
#endif
/* KF_DNA_DIRECT: the orders indexed directly (the first ones) */
enum { KF_DNA_ORD = 2, KF_DNA_DIRECT = 1, KF_DIN = KF_DNA_ORD + 4, KF_MML = 14, KF_LML = 8, KF_LW = 1024, KF_DLR = 4 };
static const int kf_dna_k[KF_DNA_ORD] = {6, 12};
typedef struct {
    kf_ctab t[KF_DNA_ORD], mt; kf_mixer mx; uint64_t hist, fk, rk; uint32_t *h, hmask, bk[KF_DNA_ORD];
    size_t fp, rp, lp; uint32_t fl, rl, fm, rm, ll, lm;   /* forward match: next position, length, recent misses; reverse; local */
    uint32_t *lh;
} kf_dmodel;
static void kf_dm_free(kf_dmodel *m) { for (int k = 0; k < KF_DNA_ORD; k++) kf_ctab_free(&m->t[k]); kf_ctab_free(&m->mt); free(m->mx.w); free(m->h); free(m->lh); m->mx.w = NULL; m->h = NULL; m->lh = NULL; }
static inline uint32_t kf_dm_bucket(uint64_t ctx, int k) { return kf_hsh((uint32_t)ctx ^ (uint32_t)(ctx >> 32) * 0x27D4EB2Fu, (uint32_t)k) << 4; }
static int kf_dm_new(kf_dmodel *m, size_t len) {
    int b = 12; while (b < 24 && ((size_t)1 << b) < len * 2) b++;
    memset(m, 0, sizeof *m);
    for (int k = 0; k < KF_DNA_ORD; k++) {
        int bits = k < KF_DNA_DIRECT ? 2 * kf_dna_k[k] + 2 : b;
        if (kf_ctab_new(&m->t[k], bits, 255)) { kf_dm_free(m); return -1; }
        if (k >= KF_DNA_DIRECT) m->bk[k] = kf_dm_bucket(0, k);
    }
    int hb = 12; while (hb < 26 && ((size_t)1 << hb) < len) hb++;
    m->hmask = ((uint32_t)1 << hb) - 1;
    m->h = calloc((size_t)1 << hb, sizeof *m->h);
    m->lh = calloc((size_t)1 << (2 * KF_LML), sizeof *m->lh);
    if (!m->h || !m->lh || kf_ctab_new(&m->mt, 12, 1000) || kf_mixer_new(&m->mx, KF_DIN, 16 * 3 * 32)) { kf_dm_free(m); return -1; }
    for (int i = 0; i < KF_DIN * 16 * 3 * 32; i++) m->mx.w[i] = i % KF_DIN < KF_DNA_ORD || i % KF_DIN == KF_DIN - 1 ? (1 << 16) / (KF_DNA_ORD + 1) : 1 << 14;
    return 0;
}
static inline int kf_dm_lb(uint32_t l) { return !l ? 0 : l < 16 ? 1 : l < 24 ? 2 : l < 32 ? 3 : l < 48 ? 4 : l < 64 ? 5 : l < 128 ? 6 : l < 256 ? 7 : 8; }
static inline int kf_dm_mb(uint32_t m) { int c = __builtin_popcount(m & 0xffff); return c > 3 ? 3 : c; }
/* the index bucket of a k-mer: its first KF_MML - 1 bases; the slot in it: its last base */
static inline uint32_t kf_dm_kb(uint64_t k) { return kf_hsh((uint32_t)(k >> 2), (uint32_t)(k >> 34) ^ 0x51ED27u) << 2; }
/* codes (or decodes) base i of buf (bases 0..i-1 are known); returns it */
static int kf_dm_code(kf_dmodel *m, kf_coder *c, int s, const uint8_t *buf, size_t i) {
    uint32_t base[KF_DNA_ORD]; const uint64_t h = m->hist; const uint32_t nw = (uint32_t)(h & 3) << 2;
    for (int k = 0; k < KF_DNA_DIRECT; k++) base[k] = (uint32_t)(h & (((uint64_t)1 << (2 * kf_dna_k[k])) - 1)) << 2;
    for (int k = KF_DNA_DIRECT; k < KF_DNA_ORD; k++) {
        base[k] = m->bk[k] | nw;                               /* this base's bucket, computed one base ago */
        m->bk[k] = kf_dm_bucket(h & (((uint64_t)1 << (2 * (kf_dna_k[k] - 1))) - 1), k);   /* the next base's */
        KF_PREFETCH(&m->t[k].s[m->bk[k] & m->t[k].mask]);
    }
    const uint64_t km = ((uint64_t)1 << (2 * KF_MML)) - 1;
    uint32_t nkb = kf_dm_kb((m->fk << 2) & km);               /* bucket of the k-mer that ends with this base */
    KF_PREFETCH(&m->h[nkb & m->hmask]);
    int fe = m->fl ? buf[m->fp] & 3 : -1, re = m->rl ? 3 - (buf[m->rp] & 3) : -1, le = m->ll ? buf[m->lp] & 3 : -1;
    int flb = kf_dm_lb(m->fl), rlb = kf_dm_lb(m->rl), llb = kf_dm_lb(m->ll), lb = flb > rlb ? flb : rlb;
    if (llb > lb) lb = llb;
    int node = 1, set = (int)(h & 15) * 3 * 32 + (lb << 1 | (fe >= 0 && re >= 0 && fe != re));
    for (int b = 1; b >= 0; b--) {
        for (int k = 0; k < KF_DNA_ORD; k++) m->mx.x[k] = kf_st(kf_ctab_p12(&m->t[k], base[k] + (uint32_t)node));
        /* a match model predicts while the bits so far agree with its base */
        int fb = fe >= 0 && (b == 1 || (fe >> 1) == (node & 1)) ? (fe >> b) & 1 : -1;
        int rb = re >= 0 && (b == 1 || (re >> 1) == (node & 1)) ? (re >> b) & 1 : -1;
        int lbt = le >= 0 && (b == 1 || (le >> 1) == (node & 1)) ? (le >> b) & 1 : -1;
        uint32_t fi = (uint32_t)((kf_dm_mb(m->fm) << 4 | flb) << 2 | node), ri = (uint32_t)(256 | (kf_dm_mb(m->rm) << 4 | rlb) << 2 | node);
        int fs = fb >= 0 ? kf_st(kf_ctab_p12(&m->mt, fi)) : 0, rs = rb >= 0 ? kf_st(kf_ctab_p12(&m->mt, ri)) : 0;
        m->mx.x[KF_DNA_ORD] = fb > 0 ? fs : -fs; m->mx.x[KF_DNA_ORD + 1] = rb > 0 ? rs : -rs;
        uint32_t li = (uint32_t)(512 | (kf_dm_mb(m->lm) << 4 | llb) << 2 | node); int ls = lbt >= 0 ? kf_st(kf_ctab_p12(&m->mt, li)) : 0;
        m->mx.x[KF_DNA_ORD + 2] = lbt > 0 ? ls : -ls;
        m->mx.x[KF_DNA_ORD + 3] = 256;
        int p = kf_mix(&m->mx, set + (node - 1) * 32);
        int y = kf_code(c, (s >> b) & 1, p);
        for (int k = 0; k < KF_DNA_ORD; k++) kf_ctab_upd(&m->t[k], base[k] + (uint32_t)node, y);
        if (fb >= 0) kf_ctab_upd(&m->mt, fi, y == fb);
        if (rb >= 0) kf_ctab_upd(&m->mt, ri, y == rb);
        if (lbt >= 0) kf_ctab_upd(&m->mt, li, y == lbt);
        kf_mix_upd(&m->mx, y, KF_DLR);
        node = node << 1 | y;
    }
    int v = node & 3; m->hist = h << 2 | (uint64_t)v;
    if (m->fl) {
        m->fm = (m->fm << 1 | (fe != v)) & 0xffff;
        if (m->fp + 1 <= i) { m->fp++; m->fl = fe == v ? (m->fl < 65535 ? m->fl + 1 : m->fl) : 1; } else m->fl = 0;
    }
    if (m->rl) {
        m->rm = (m->rm << 1 | (re != v)) & 0xffff;
        if (m->rp > 0) { m->rp--; m->rl = re == v ? (m->rl < 65535 ? m->rl + 1 : m->rl) : 1; } else m->rl = 0;
    }
    if (m->ll) {
        m->lm = (m->lm << 1 | (le != v)) & 0xffff;
        if (m->lp + 1 <= i && kf_dm_mb(m->lm) < 3) { m->lp++; m->ll = le == v ? (m->ll < 65535 ? m->ll + 1 : m->ll) : 1; } else m->ll = 0;
    }
    if (i + 1 >= KF_LML) {                         /* local match: the last KF_LML bases seen within KF_LW bases */
        uint32_t key = (uint32_t)(((h << 2 | (uint64_t)v)) & (((uint64_t)1 << (2 * KF_LML)) - 1)), cand = m->lh[key];
        if (m->ll < 2 * KF_LML && cand && i + 1 - cand <= KF_LW && cand != m->lp) {
            m->lp = cand; m->ll = KF_LML; m->lm = 0;      /* the index is exact: no check needed */
        }
        m->lh[key] = (uint32_t)(i + 1);
    }
    m->fk = (m->fk << 2 | (uint64_t)v) & km; m->rk = (m->rk >> 2) | (uint64_t)(3 - v) << (2 * (KF_MML - 1));
    if (i + 1 >= KF_MML) {
        uint32_t *fs = &m->h[(nkb | (uint32_t)v) & m->hmask], cand;
        if (m->fl < KF_MML && (cand = *fs) != 0 && cand != m->fp) {   /* cand: the position after an earlier k-mer */
            size_t q = cand, t = 0;
            while (t < KF_MML && (buf[q - 1 - t] & 3) == (t ? buf[i - t] & 3 : (uint8_t)v)) t++;
            if (t == KF_MML) { m->fp = q; m->fl = KF_MML; m->fm = 0; }
        }
        if (m->rl < KF_MML && (cand = m->h[(kf_dm_kb(m->rk) | (uint32_t)(m->rk & 3)) & m->hmask]) != 0 && cand > KF_MML && cand - KF_MML - 1 != m->rp) {
            size_t q = cand - KF_MML, t = 0;                   /* buf[q .. cand) is the reverse complement of the suffix */
            while (t < KF_MML && (buf[q + t] & 3) == 3 - (t ? buf[i - t] & 3 : (uint8_t)v)) t++;
            if (t == KF_MML) { m->rp = q - 1; m->rl = KF_MML; m->rm = 0; }
        }
        *fs = (uint32_t)(i + 1);
    }
    return v;
}

/* ------------------------------------------------------------------ quality scores */
/* The symbols (quality - 33, at most 128 distinct) are coded as paths in an order-preserving binary tree, split
 * where the two sides' counts are closest, so frequent symbols take few binary decisions and every decision is a
 * threshold ("below q or not"), which carries over between neighbouring contexts. Internal nodes are numbered from
 * the root in breadth-first order; ranks number the symbols by value, for the contexts. */
enum { KF_QDEPTH = 48 };
typedef struct {
    int K, nb, cqmax;                   /* symbols present; bits of an internal node number; largest context rank */
    int16_t ch[128][2];                 /* internal node -> child: >= 0 internal node, < 0 leaf -(rank + 1) */
    uint8_t len[128], bit[128][KF_QDEPTH];   /* per rank: path length and bits */
    uint8_t rank[128], val[128], cq[128];    /* value -> rank, rank -> value, rank -> context rank (6 bits) */
} kf_qtree;
/* ranks lo..hi split where the two sides' weights are closest; returns the node (leaves 0..K-1) */
static int kf_qtree_split(const uint64_t *w, int lo, int hi, int *na, int *par, int *side) {
    if (lo == hi) return lo;
    uint64_t tot = 0, left = 0, best = UINT64_MAX; int k = lo;
    for (int r = lo; r <= hi; r++) tot += w[r];
    for (int r = lo; r < hi; r++) { left += w[r]; uint64_t d = left * 2 > tot ? left * 2 - tot : tot - left * 2; if (d < best) { best = d; k = r; } }
    int a = kf_qtree_split(w, lo, k, na, par, side), b = kf_qtree_split(w, k + 1, hi, na, par, side), x = (*na)++;
    par[a] = par[b] = x; side[a] = 0; side[b] = 1;
    return x;
}
static int kf_qtree_build(kf_qtree *t, const uint64_t *cnt) {               /* cnt[128] by value; 0 = absent */
    memset(t, 0, sizeof *t);
    uint64_t w[128]; int par[256], side[256], K = 0;
    for (int v = 0; v < 128; v++) if (cnt[v]) { t->rank[v] = (uint8_t)K; t->val[K] = (uint8_t)v; K++; }
    t->K = K; if (!K) return -1;
    int shift = K > 63 ? 1 : 0; t->cqmax = (K - 1) >> shift;  /* context ranks fit in 6 bits, 63 marks a read start */
    for (int r = 0; r < K; r++) t->cq[r] = (uint8_t)(r >> shift);
    t->nb = 1; while ((1 << t->nb) < K - 1) t->nb++;
    if (K == 1) return 0;
    for (int pass = 0; pass < 64; pass++) {                    /* flatter weights until the tree is at most KF_QDEPTH deep */
        for (int r = 0; r < K; r++) { w[r] = cnt[t->val[r]] >> pass; if (!w[r]) w[r] = 1; }
        int na = K; kf_qtree_split(w, 0, K - 1, &na, par, side);
        int root = na - 1, depth_ok = 1;
        for (int r = 0; r < K && depth_ok; r++) { int d = 0; for (int x = r; x != root; x = par[x]) d++; if (d > KF_QDEPTH) depth_ok = 0; }
        if (!depth_ok) continue;
        int num[256], q[256], kid[256][2], qh = 0, qt = 0, next = 0;
        for (int i = K; i < na; i++) kid[i][0] = kid[i][1] = -1;
        for (int i = 0; i < na - 1; i++) kid[par[i]][side[i]] = i;
        q[qt++] = root;
        while (qh < qt) { int x = q[qh++]; num[x] = next++; for (int s = 0; s < 2; s++) if (kid[x][s] >= K) q[qt++] = kid[x][s]; }
        for (int i = K; i < na; i++) for (int s = 0; s < 2; s++) { int c = kid[i][s]; t->ch[num[i]][s] = (int16_t)(c >= K ? num[c] : -(c + 1)); }
        for (int r = 0; r < K; r++) {
            int d = 0, path[KF_QDEPTH];
            for (int x = r; x != root; x = par[x]) path[d++] = side[x];
            t->len[r] = (uint8_t)d;
            for (int k = 0; k < d; k++) t->bit[r][k] = (uint8_t)path[d - 1 - k];
        }
        return 0;
    }
    return -1;
}
/* parameter blob: presence mask (16 bytes), then the count of each present value as a varint */
static int kf_qcounts_put(kf_buf *b, const uint64_t *cnt) {
    uint8_t mask[16] = {0};
    for (int v = 0; v < 128; v++) if (cnt[v]) mask[v >> 3] |= (uint8_t)(1 << (v & 7));
    if (kf_put(b, mask, 16)) return -1;
    for (int v = 0; v < 128; v++) if (cnt[v] && kf_var(b, cnt[v])) return -1;
    return 0;
}
static int kf_qcounts_get(const uint8_t *p, size_t n, uint64_t *cnt) {
    if (n < 16) return -1;
    size_t i = 16;
    for (int v = 0; v < 128; v++) {
        cnt[v] = 0;
        if (p[v >> 3] >> (v & 7) & 1) { if (kf_getvar(p, n, &i, &cnt[v]) || !cnt[v]) return -1; }
    }
    return i == n ? 0 : -1;
}

/* Contexts (q1..q3: the previous context ranks in the read, 63 before the read start; pb: position bucket; db: the
 * running sum of |q[i] - q[i-1]| bucketed; ab: the mean so far, 16 levels; n: the base is not A/C/G/T):
 *   0 q1 q2            1 q1 max(q2,q3) [q2 = q3] pb/4      2 q1 pb db
 *   3 q1 q2 q3 db/2    4 pb ab n q1/2                       5 q1 db ab n
 * Six counter tables (adaptive rate 1/n, n capped at 1000), two mixers (weights selected by tree node with db, and
 * with pb), a final two-input mixer, and two adaptive probability maps (q1 with the node, pb with the node) averaged
 * with the mixer's output. */
enum { KF_QT = 6, KF_QCAP = 22 };
static const int kf_qbits[KF_QT] = {12, 17, 16, 16, 16, 15};    /* context bits before the node; hashed when larger */
typedef struct {
    const kf_qtree *qt; kf_ctab t[KF_QT]; int tb[KF_QT]; kf_mixer m1, m2, m3; kf_apm a1, a2;
    int q1, q2, q3, pos, dsum, rsum;    /* read state */
} kf_qmodel;
static void kf_qm_free(kf_qmodel *m) {
    for (int k = 0; k < KF_QT; k++) kf_ctab_free(&m->t[k]);
    free(m->m1.w); free(m->m2.w); free(m->m3.w); free(m->a1.t); free(m->a2.t);
    m->m1.w = m->m2.w = m->m3.w = NULL; m->a1.t = m->a2.t = NULL;
}
static void kf_qm_read_start(kf_qmodel *m) { m->q1 = m->q2 = m->q3 = 63; m->pos = 0; m->dsum = 0; m->rsum = 0; }
static int kf_qm_new(kf_qmodel *m, const kf_qtree *qt, size_t len) {
    memset(m, 0, sizeof *m); m->qt = qt;
    int cap = 16; while (cap < KF_QCAP && ((size_t)1 << cap) < len * 2) cap++;
    for (int k = 0; k < KF_QT; k++) {
        int want = kf_qbits[k] + qt->nb; m->tb[k] = want < cap ? want : cap;
        if (kf_ctab_new(&m->t[k], m->tb[k], 1000)) { kf_qm_free(m); return -1; }
    }
    if (kf_mixer_new(&m->m1, KF_QT + 1, 16 << qt->nb) || kf_mixer_new(&m->m2, KF_QT + 1, 64 << qt->nb) || kf_mixer_new(&m->m3, 3, 1 << qt->nb)
        || kf_apm_new(&m->a1, 64 << qt->nb) || kf_apm_new(&m->a2, 64 << qt->nb)) { kf_qm_free(m); return -1; }
    kf_qm_read_start(m);
    return 0;
}
/* codes (or decodes: r < 0) one quality rank r of a base; isn: the base is not A/C/G/T. Returns the rank. */
static int kf_qm_code(kf_qmodel *m, kf_coder *c, int r, int isn) {
    const kf_qtree *qt = m->qt; int nb = qt->nb;
    int p = m->pos, pb = p < 16 ? p : p < 64 ? 16 + ((p - 16) >> 2) : p < 320 ? 28 + ((p - 64) >> 3) : p < 832 ? 60 + ((p - 320) >> 7) : 63;
    int q1 = m->q1, q2 = m->q2, q3 = m->q3, q23 = q2 > q3 ? q2 : q3;
    int db = kf_dbk[m->dsum < 4095 ? m->dsum : 4095];
    int ab = p ? (m->rsum / p) * 16 / (qt->cqmax + 1) : 0; if (ab > 15) ab = 15;
    const uint32_t cx[KF_QT] = {
        (uint32_t)(q1 << 6 | q2),
        (uint32_t)((q1 << 6 | q23) << 5 | (q2 == q3) << 4 | pb >> 2),
        (uint32_t)((q1 << 6 | pb) << 4 | db),
        kf_hsh((uint32_t)((q1 << 6 | q2) << 6 | q3), (uint32_t)(db >> 1) | 0x100u),
        (uint32_t)(((pb << 4 | ab) << 1 | isn) << 5 | q1 >> 1),
        (uint32_t)(((q1 << 4 | db) << 4 | ab) << 1 | isn),
    };
    uint32_t base[KF_QT];
    for (int k = 0; k < KF_QT; k++) {
        int cb = m->tb[k] - nb;
        uint32_t v = k == 3 || kf_qbits[k] > cb ? kf_hsh(cx[k], (uint32_t)k + 1) >> (32 - cb) : cx[k];
        base[k] = v << nb;
    }
    int node = 0, d = 0;
    if (qt->K == 1) r = 0;
    else for (;;) {
        uint32_t ix[KF_QT];
        for (int k = 0; k < KF_QT; k++) { ix[k] = base[k] | (uint32_t)node; m->m1.x[k] = m->m2.x[k] = kf_st(kf_ctab_p12(&m->t[k], ix[k])); }
        m->m1.x[KF_QT] = m->m2.x[KF_QT] = 256;
        int s1 = kf_mix_st(&m->m1, db << nb | node), s2 = kf_mix_st(&m->m2, pb << nb | node);
        m->m3.x[0] = s1; m->m3.x[1] = s2; m->m3.x[2] = 256;
        int s3 = kf_mix_st(&m->m3, node);
        int pa = kf_apm_p(&m->a1, s3, q1 << nb | node), pc = kf_apm_p(&m->a2, s3, pb << nb | node);
        int pf = ((kf_sq(s3) << 4) * 2 + pa + pc) >> 2;
        int y = kf_code16(c, r >= 0 ? qt->bit[r][d] : 0, (uint32_t)pf);
        for (int k = 0; k < KF_QT; k++) kf_ctab_upd(&m->t[k], ix[k], y);
        kf_mix_upd14(&m->m1, y, 6); kf_mix_upd14(&m->m2, y, 6); kf_mix_upd14(&m->m3, y, 4);
        kf_apm_upd(&m->a1, y, 6); kf_apm_upd(&m->a2, y, 6);
        int nx = qt->ch[node][y]; d++;
        if (nx < 0) { r = -nx - 1; break; }
        node = nx;
    }
    int v = qt->cq[r];
    if (p) m->dsum += v > q1 ? v - q1 : q1 - v;
    m->rsum += v; m->q3 = q2; m->q2 = q1; m->q1 = v; m->pos++;
    return r;
}

/* ------------------------------------------------------------------ streams and jobs */
enum { KF_BYTES = 0, KF_DNA = 1, KF_QUAL = 2 };
enum { KF_CHUNK = 4u << 20, KF_BCHUNK = 64u << 20 };   /* default chunks: names and qualities; bases */
typedef struct { int kind; kf_buf data, ctx, par; } kf_stream;
/* a job: one slice of one stream, coded with fresh models */
typedef struct {
    int sid, kind, err;
    uint8_t *raw; size_t n;             /* the slice (encode: input; decode: output) */
    const uint8_t *ctx;                 /* encode, byte model: one context byte per byte (or NULL) */
    int (*ctxfn)(void *, size_t, const uint8_t *); void *ctxarg;    /* decode, byte model: rebuilds the context */
    const uint8_t *cin; size_t cn;      /* decode: coded bytes */
    kf_buf out;                         /* encode: coded bytes */
    /* qualities: read lengths from read0 on, exception sites from x0 on, the site of the first symbol */
    const kf_qtree *qt; const uint32_t *lens; size_t nreads, read0; const uint64_t *xs; size_t nx, x0; uint64_t site0;
} kf_job;

/* The decoder renormalises in step with the encoder, which flushes exactly 4 bytes, so a valid stream is never read
 * past its end. A damaged one (say, a raw length changed in the frame) would otherwise decode zeros up to its claimed
 * length; it is reported corrupt once it has read more than KF_OVER_MAX bytes past its end. */
#define KF_OVER_MAX 8
static void kf_job_qual(kf_job *j, kf_coder *c, int dec) {
    kf_qmodel m; if (kf_qm_new(&m, j->qt, j->n)) { j->err = 1; return; }
    size_t r = j->read0, x = j->x0, left = 0; uint64_t site = j->site0; const kf_qtree *qt = j->qt;
    for (size_t i = 0; i < j->n; i++, site++) {
        while (!left) {
            if (r >= j->nreads) { j->err = 1; kf_qm_free(&m); return; }
            left = j->lens[r++]; kf_qm_read_start(&m);
        }
        left--;
        while (x < j->nx && j->xs[x] < site) x++;
        int isn = x < j->nx && j->xs[x] == site;
        if (dec) { j->raw[i] = qt->val[kf_qm_code(&m, c, -1, isn)]; if (c->d->over > KF_OVER_MAX) { j->err = 1; break; } }
        else kf_qm_code(&m, c, qt->rank[j->raw[i] & 127], isn);
    }
    kf_qm_free(&m);
}
static void *kf_enc_job(void *arg) {
    kf_job *j = arg; kf_aenc e; memset(&e, 0, sizeof e); e.hi = 0xffffffffu; kf_coder c = {&e, NULL};
    size_t n = j->n; const uint8_t *d = j->raw;
    if (j->kind == KF_DNA) {
        kf_dmodel m; if (kf_dm_new(&m, n)) { j->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) kf_dm_code(&m, &c, d[i], d, i);
        kf_dm_free(&m);
    } else if (j->kind == KF_QUAL) {
        kf_job_qual(j, &c, 0);
    } else {
        kf_bmodel m; if (kf_bm_new(&m, n)) { j->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) kf_bm_code(&m, &c, d[i], j->ctx ? j->ctx[i] : 0);
        kf_bm_free(&m);
    }
    kf_ae_end(&e); j->out = e.out; j->err |= e.err; return NULL;
}
static void *kf_dec_job(void *arg) {
    kf_job *j = arg; kf_adec d; kf_ad_init(&d, j->cin, j->cn); kf_coder c = {NULL, &d};
    size_t n = j->n; uint8_t *o = j->raw;
    if (j->kind == KF_DNA) {
        kf_dmodel m; if (kf_dm_new(&m, n)) { j->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) { o[i] = (uint8_t)kf_dm_code(&m, &c, 0, o, i); if (d.over > KF_OVER_MAX) { j->err = 1; break; } }
        kf_dm_free(&m);
    } else if (j->kind == KF_QUAL) {
        kf_job_qual(j, &c, 1);
    } else {
        kf_bmodel m; if (kf_bm_new(&m, n)) { j->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) {
            o[i] = (uint8_t)kf_bm_code(&m, &c, 0, j->ctxfn ? (uint32_t)j->ctxfn(j->ctxarg, i, o) : 0);
            if (d.over > KF_OVER_MAX) { j->err = 1; break; }
        }
        kf_bm_free(&m);
    }
    return NULL;
}

/* ------------------------------------------------------------------ thread pool: largest jobs first */
typedef struct { kf_job **items; int n, next; pthread_mutex_t mu; void *(*fn)(void *); } kf_tpool;
static void *kf_tp_worker(void *a) {
    kf_tpool *p = a;
    for (;;) { pthread_mutex_lock(&p->mu); int i = p->next++; pthread_mutex_unlock(&p->mu); if (i >= p->n) return NULL; p->fn(p->items[i]); }
}
static void kf_tp_run(void *(*fn)(void *), kf_job *jobs, int n, int threads) {
    kf_job **v = malloc(sizeof(kf_job *) * (size_t)(n ? n : 1));
    if (!v) { for (int i = 0; i < n; i++) fn(&jobs[i]); return; }
    for (int i = 0; i < n; i++) {                                 /* insertion sort, largest first (ties: frame order) */
        kf_job *x = &jobs[i]; int k = i;
        while (k > 0 && v[k - 1]->n < x->n) { v[k] = v[k - 1]; k--; }
        v[k] = x;
    }
    if (threads > n) threads = n;
    if (threads > 64) threads = 64;
    if (threads <= 1) { for (int i = 0; i < n; i++) fn(v[i]); free(v); return; }
    kf_tpool p; p.items = v; p.n = n; p.next = 0; p.fn = fn; pthread_mutex_init(&p.mu, NULL);
    pthread_t th[64]; int started = 0;
    for (int t = 1; t < threads; t++) if (!pthread_create(&th[started], NULL, kf_tp_worker, &p)) started++;
    kf_tp_worker(&p);
    for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
    pthread_mutex_destroy(&p.mu); free(v);
}

/* ------------------------------------------------------------------ frame */
/* cut points of a stream into jobs: a list of slice sizes */
typedef struct { size_t *v; size_t n, cap; } kf_cuts;
static int kf_cut(kf_cuts *c, size_t len) {
    if (c->n == c->cap) { size_t k = c->cap ? 2 * c->cap : 16; size_t *t = realloc(c->v, k * sizeof *t); if (!t) return -1; c->v = t; c->cap = k; }
    c->v[c->n++] = len; return 0;
}
/* writes the frame for streams st[0..ns) cut as cuts[s] (NULL or empty: one job); qual fields of jobs are set by
 * the caller through setup(); returns the frame or NULL */
typedef void (*kf_setup_fn)(void *arg, kf_job *j, size_t off);
static uint8_t *kf_frame_encode(int fam, uint64_t orig, kf_stream *st, int ns, kf_cuts *cuts, kf_setup_fn setup, void *sarg, int threads, size_t *outn) {
    int nj = 0;
    for (int s = 0; s < ns; s++) nj += cuts && cuts[s].n ? (int)cuts[s].n : 1;
    kf_job *jb = calloc((size_t)nj, sizeof *jb); kf_buf h = {0}; uint8_t *res = NULL; int k = 0, err = 0;
    if (!jb) return NULL;
    for (int s = 0; s < ns; s++) {
        size_t off = 0, nc = cuts && cuts[s].n ? cuts[s].n : 1;
        for (size_t c = 0; c < nc; c++) {
            size_t len = cuts && cuts[s].n ? cuts[s].v[c] : st[s].data.n;
            kf_job *j = &jb[k++]; j->sid = s; j->kind = st[s].kind; j->raw = st[s].data.p + off; j->n = len;
            j->ctx = st[s].ctx.n ? st[s].ctx.p + off : NULL;
            if (setup) setup(sarg, j, off);
            off += len;
        }
        if (off != st[s].data.n) err = 1;
    }
    if (!err) kf_tp_run(kf_enc_job, jb, nj, threads);
    for (int i = 0; i < nj; i++) err |= jb[i].err;
    if (!err && !kf_put(&h, "KFM2", 4) && !kf_byte(&h, (uint8_t)fam) && !kf_var(&h, orig) && !kf_var(&h, (uint64_t)ns)) {
        for (int s = 0; s < ns && !err; s++) err = kf_byte(&h, (uint8_t)st[s].kind) || kf_var(&h, st[s].par.n) || kf_put(&h, st[s].par.p, st[s].par.n);
        err = err || kf_var(&h, (uint64_t)nj);
        for (int i = 0; i < nj && !err; i++) err = kf_var(&h, (uint64_t)jb[i].sid) || kf_var(&h, jb[i].n) || kf_var(&h, jb[i].out.n);
        for (int i = 0; i < nj && !err; i++) err = kf_put(&h, jb[i].out.p, jb[i].out.n);
        if (!err) { res = h.p; *outn = h.n; h.p = NULL; }
    }
    for (int i = 0; i < nj; i++) free(jb[i].out.p);
    free(jb); free(h.p);
    return res;
}
enum { KF_F_ONE = 0, KF_F_FASTQ = 1, KF_F_CSV = 2 };
/* a parsed frame: the streams with their buffers allocated, and the jobs pointing into them */
typedef struct { int fam, ns, nj; uint64_t orig; kf_stream *st; kf_job *jb; } kf_frame;
static void kf_frame_free(kf_frame *f) {
    if (f->st) for (int s = 0; s < f->ns; s++) { free(f->st[s].data.p); free(f->st[s].ctx.p); free(f->st[s].par.p); }
    free(f->st); free(f->jb); f->st = NULL; f->jb = NULL;
}
/* maxmul: the streams may hold at most maxmul * original size + 4096 bytes in all (a bound on what a corrupt frame
 * can make the decoder allocate). A native FASTQ block supplies the already admitted outer size. */
static int kf_frame_parse(const uint8_t *c, size_t cn, kf_frame *f, unsigned maxmul, const size_t *fastq_original) {
    memset(f, 0, sizeof *f);
    size_t i = 4; uint64_t v, ns, nj;
    if (cn < 6 || memcmp(c, "KFM2", 4)) return -1;
    f->fam = c[i++];
    if (kf_getvar(c, cn, &i, &f->orig) || f->orig > ((uint64_t)1 << 48) || kf_getvar(c, cn, &i, &ns) || !ns || ns > 4096) return -1;
    if (fastq_original && (f->fam != KF_F_FASTQ || ns != 5 || f->orig != *fastq_original)) return -1;
    f->ns = (int)ns; f->st = calloc((size_t)ns, sizeof *f->st);
    if (!f->st) return -1;
    for (int s = 0; s < f->ns; s++) {
        if (i >= cn || c[i] > KF_QUAL) return -1;
        f->st[s].kind = c[i++];
        if (kf_getvar(c, cn, &i, &v) || v > cn - i) return -1;
        if (v && kf_put(&f->st[s].par, c + i, (size_t)v)) return -1;
        i += (size_t)v;
    }
    if (kf_getvar(c, cn, &i, &nj) || nj > cn || nj > (1u << 24)) return -1;
    f->nj = (int)nj; f->jb = calloc((size_t)(nj ? nj : 1), sizeof *f->jb);
    if (!f->jb) return -1;
    uint64_t *tot = calloc((size_t)ns, sizeof *tot), all = 0, lim = maxmul * f->orig + 4096; int prev = 0;
    if (!tot) return -1;
    for (int k = 0; k < f->nj; k++) {
        uint64_t sid, rn, zn;
        if (kf_getvar(c, cn, &i, &sid) || kf_getvar(c, cn, &i, &rn) || kf_getvar(c, cn, &i, &zn) || sid >= ns || (int)sid < prev
            || rn > lim - all) { free(tot); return -1; }
        all += rn;
        prev = (int)sid; f->jb[k].sid = (int)sid; f->jb[k].kind = f->st[sid].kind; f->jb[k].n = (size_t)rn; f->jb[k].cn = (size_t)zn; tot[sid] += rn;
    }
    for (int k = 0; k < f->nj; k++) { if (f->jb[k].cn > cn - i) { free(tot); return -1; } f->jb[k].cin = c + i; i += f->jb[k].cn; }
    for (int s = 0; s < f->ns; s++) {
        f->st[s].data.p = malloc(tot[s] ? (size_t)tot[s] : 1); f->st[s].data.n = f->st[s].data.cap = (size_t)tot[s];
        if (!f->st[s].data.p) { free(tot); return -1; }
    }
    uint64_t *off = tot; for (int s = 0; s < f->ns; s++) off[s] = 0;
    for (int k = 0; k < f->nj; k++) { kf_job *j = &f->jb[k]; j->raw = f->st[j->sid].data.p + off[j->sid]; off[j->sid] += j->n; }
    free(tot);
    return 0;
}
/* decodes the jobs of the streams for which want[sid] is set, on up to `threads` threads */
static int kf_frame_decode(kf_frame *f, const uint8_t *want, int threads) {
    int n = 0;
    for (int k = 0; k < f->nj; k++) if (want[f->jb[k].sid]) n++;
    if (!n) return 0;
    kf_job *sel = malloc(sizeof *sel * (size_t)n); int m = 0;
    if (!sel) return -1;
    for (int k = 0; k < f->nj; k++) if (want[f->jb[k].sid]) sel[m++] = f->jb[k];
    kf_tp_run(kf_dec_job, sel, n, threads);
    int err = 0; for (int k = 0; k < n; k++) err |= sel[k].err;
    free(sel);
    return err ? -1 : 0;
}

/* ------------------------------------------------------------------ FASTQ family */
typedef struct { const uint8_t *p; size_t n; } kf_span;
static inline int kf_line(const uint8_t *s, size_t n, size_t *i, kf_span *out) {
    if (*i >= n) return -1;
    const uint8_t *e = memchr(s + *i, '\n', n - *i); if (!e) return -1;
    out->p = s + *i; out->n = (size_t)(e - (s + *i)); *i = (size_t)(e - s) + 1; return 0;
}
static const int8_t kf_b2[256] = {['A'] = 1, ['C'] = 2, ['G'] = 3, ['T'] = 4};   /* base + 1; 0: exception */
static const uint8_t kf_b2c[4] = {'A', 'C', 'G', 'T'};
static int kf_tokens(kf_span s, kf_span *t, int max) {                 /* maximal runs of digits or of non-digits */
    int k = 0; size_t i = 0;
    while (i < s.n && k < max) { size_t j = i; int d = s.p[i] >= '0' && s.p[i] <= '9'; while (j < s.n && (s.p[j] >= '0' && s.p[j] <= '9') == d) j++; t[k].p = s.p + i; t[k].n = j - i; k++; i = j; }
    return i == s.n ? k : -1;
}
static int kf_num_tok(kf_span t, uint64_t *v) {          /* a canonical decimal number of at most 18 digits */
    if (!t.n || t.n > 18 || !(t.p[0] >= '0' && t.p[0] <= '9') || (t.p[0] == '0' && t.n > 1)) return 0;
    uint64_t r = 0; for (size_t i = 0; i < t.n; i++) { if (t.p[i] < '0' || t.p[i] > '9') return 0; r = r * 10 + (uint64_t)(t.p[i] - '0'); }
    *v = r; return 1;
}
enum { KF_MAXTOK = 32, KF_MAXNAME = 4096, KF_OP_SAME = 0, KF_OP_DELTA = 1, KF_OP_LIT = 2, KF_OP_END = 3 };
typedef struct {
    kf_stream st[5];
    uint32_t *lens; size_t nreads, rcap;            /* read lengths */
    uint64_t *xs; size_t nx, xcap;                  /* exception sites */
    uint64_t *noff; size_t nocap;                   /* encoder: names offset of each read */
    kf_qtree qt;
} kf_fq;
static void kf_fq_free(kf_fq *q) {
    for (int s = 0; s < 5; s++) { free(q->st[s].data.p); free(q->st[s].ctx.p); free(q->st[s].par.p); }
    free(q->lens); free(q->xs); free(q->noff); memset(q, 0, sizeof *q);
}
static int kf_grow(void **p, size_t *cap, size_t need, size_t el) {
    if (need <= *cap) return 0;
    size_t c = *cap ? *cap : 1024; while (c < need) c *= 2;
    void *t = realloc(*p, c * el); if (!t) return -1; *p = t; *cap = c; return 0;
}
/* quick test: does s look like FASTQ records (the first record parses)? */
static int kf_fastq_like(const uint8_t *s, size_t n) {
    size_t i = 0; kf_span a, b, c, d;
    if (!n || s[0] != '@' || s[n - 1] != '\n') return 0;
    if (kf_line(s, n, &i, &a) || kf_line(s, n, &i, &b) || kf_line(s, n, &i, &c) || kf_line(s, n, &i, &d)) return 0;
    return c.n && c.p[0] == '+' && b.n == d.n;
}
static int kf_fastq_split(const uint8_t *s, size_t n, kf_fq *q) {
    kf_stream *st = q->st; kf_span pv[KF_MAXTOK]; int pk = 0; size_t i = 0, prevlen = (size_t)-1; uint64_t lastx = 0, site = 0;
    uint64_t qc[128] = {0};
    st[2].kind = KF_DNA; st[4].kind = KF_QUAL;
    /* the big streams: reserve by an estimate (half the input each for bases and qualities) */
    if (kf_grow((void **)&st[2].data.p, &st[2].data.cap, n / 2 + 16, 1) || kf_grow((void **)&st[4].data.p, &st[4].data.cap, n / 2 + 16, 1)) return -1;
    while (i < n) {
        kf_span nm, sq, pl, ql;
        if (kf_line(s, n, &i, &nm) || kf_line(s, n, &i, &sq) || kf_line(s, n, &i, &pl) || kf_line(s, n, &i, &ql)) return -1;
        if (!nm.n || nm.p[0] != '@' || !pl.n || pl.p[0] != '+' || sq.n != ql.n || nm.n > KF_MAXNAME || sq.n > 0xffffffffu) return -1;
        kf_span name = {nm.p + 1, nm.n - 1};
        int plus = pl.n == 1 ? 0 : (pl.n == nm.n && !memcmp(pl.p + 1, nm.p + 1, nm.n - 1)) ? 1 : -1;
        if (plus < 0) return -1;
        if (kf_grow((void **)&q->lens, &q->rcap, q->nreads + 1, sizeof *q->lens) || kf_grow((void **)&q->noff, &q->nocap, q->nreads + 1, sizeof *q->noff)) return -1;
        q->noff[q->nreads] = st[1].data.n; q->lens[q->nreads++] = (uint32_t)sq.n;
        if (sq.n != prevlen) { if (kf_byte(&st[0].data, (uint8_t)(2 + plus)) || kf_var(&st[0].data, sq.n)) return -1; prevlen = sq.n; }
        else if (kf_byte(&st[0].data, (uint8_t)plus)) return -1;
        kf_span tk[KF_MAXTOK]; int k = kf_tokens(name, tk, KF_MAXTOK); if (k < 0) return -1;
        kf_buf *N = &st[1].data, *C = &st[1].ctx;
        for (int t = 0; t < k; t++) {
            uint8_t ix = (uint8_t)(t < 15 ? t : 15); uint64_t a, b;
            if (t < pk && tk[t].n == pv[t].n && !memcmp(tk[t].p, pv[t].p, tk[t].n)) { if (kf_byte(N, KF_OP_SAME) || kf_byte(C, ix)) return -1; }
            else if (t < pk && kf_num_tok(tk[t], &a) && kf_num_tok(pv[t], &b)) {
                if (kf_byte(N, KF_OP_DELTA) || kf_byte(C, ix)) return -1;
                size_t before = N->n; if (kf_var(N, kf_zz((int64_t)(a - b)))) return -1;
                for (size_t z = before; z < N->n; z++) if (kf_byte(C, (uint8_t)(16 + ix))) return -1;
            } else {
                if (kf_byte(N, KF_OP_LIT) || kf_byte(C, ix)) return -1;
                size_t before = N->n; if (kf_var(N, tk[t].n)) return -1;
                for (size_t z = before; z < N->n; z++) if (kf_byte(C, (uint8_t)(32 + ix))) return -1;
                if (kf_put(N, tk[t].p, tk[t].n)) return -1;
                for (size_t z = 0; z < tk[t].n; z++) if (kf_byte(C, (uint8_t)(48 + ix))) return -1;
            }
        }
        if (kf_byte(N, KF_OP_END) || kf_byte(C, (uint8_t)(k < 15 ? k : 15))) return -1;
        memcpy(pv, tk, sizeof(kf_span) * (size_t)k); pk = k;
        if (kf_grow((void **)&st[2].data.p, &st[2].data.cap, st[2].data.n + sq.n, 1) || kf_grow((void **)&st[4].data.p, &st[4].data.cap, st[4].data.n + sq.n, 1)) return -1;
        uint8_t *B = st[2].data.p + st[2].data.n, *Q = st[4].data.p + st[4].data.n;
        for (size_t z = 0; z < sq.n; z++, site++) {
            int b = kf_b2[sq.p[z]];
            B[z] = (uint8_t)(b ? b - 1 : 0);
            if (!b) {
                if (kf_var(&st[3].data, site - lastx) || kf_byte(&st[3].data, sq.p[z])) return -1;
                if (kf_grow((void **)&q->xs, &q->xcap, q->nx + 1, sizeof *q->xs)) return -1;
                q->xs[q->nx++] = site; lastx = site;
            }
            int v = ql.p[z] - 33; if (v < 0 || v > 127) return -1;
            Q[z] = (uint8_t)v; qc[v]++;
        }
        st[2].data.n += sq.n; st[4].data.n += sq.n;
    }
    if (st[4].data.n && (kf_qtree_build(&q->qt, qc) || kf_qcounts_put(&st[4].par, qc))) return -1;
    return 0;
}
/* the name stream's context depends on the operations already decoded: rebuild it while decoding */
typedef struct { int tok, state; uint64_t left; int shift; } kf_namectx;
static int kf_name_ctx(void *arg, size_t i, const uint8_t *o) {
    kf_namectx *c = arg;
    if (i > 0) {                                   /* account for the byte just decoded (o[i - 1]) */
        uint8_t v = o[i - 1];
        if (c->state == 0) {                       /* it was an operation */
            if (v == KF_OP_SAME) c->tok++;
            else if (v == KF_OP_DELTA) c->state = 1;
            else if (v == KF_OP_LIT) { c->state = 2; c->left = 0; c->shift = 0; }
            else c->tok = 0;                       /* KF_OP_END */
        } else if (c->state == 1) { if (!(v & 128)) { c->state = 0; c->tok++; } }
        else if (c->state == 2) {
            if (c->shift < 63) c->left |= (uint64_t)(v & 127) << c->shift;
            c->shift += 7;
            if (!(v & 128)) { if (c->left) c->state = 3; else { c->state = 0; c->tok++; } }
        }
        else if (c->state == 3) { if (--c->left == 0) { c->state = 0; c->tok++; } }
    }
    int ix = c->tok < 15 ? c->tok : 15;
    return c->state == 0 ? ix : c->state == 1 ? 16 + ix : c->state == 2 ? 32 + ix : 48 + ix;
}
/* chunking: names and qualities cut at read boundaries once a slice reaches `chunk` symbols, bases once it reaches
 * `bchunk` (0: no cut). Bases are cut much more coarsely: their models learn the genome, and on a 6x coverage
 * bacterial run 16 Mi-base chunks coded 42% larger than one job, where 4 Mi-symbol quality chunks cost 0.4 to 0.8%. */
static int kf_fq_cuts(const kf_fq *q, size_t chunk, size_t bchunk, kf_cuts *cuts) {
    size_t nlen = 0, blen = 0, qlen = 0, nstart = 0;
    for (size_t r = 0; r < q->nreads; r++) {
        int last = r + 1 == q->nreads;
        uint64_t nend = last ? q->st[1].data.n : q->noff[r + 1];
        nlen = (size_t)(nend - nstart);
        if (chunk && (nlen >= chunk || last)) { if (kf_cut(&cuts[1], nlen)) return -1; nstart = (size_t)nend; nlen = 0; }
        blen += q->lens[r]; qlen += q->lens[r];
        if (bchunk && (blen >= bchunk || last) && blen) { if (kf_cut(&cuts[2], blen)) return -1; blen = 0; }
        if (chunk && (qlen >= chunk || last) && qlen) { if (kf_cut(&cuts[4], qlen)) return -1; qlen = 0; }
    }
    return 0;
}
typedef struct { kf_fq *q; } kf_fq_setup_arg;
static void kf_fq_setup(void *arg, kf_job *j, size_t off) {
    kf_fq *q = ((kf_fq_setup_arg *)arg)->q;
    if (j->sid != 4) return;
    size_t r = 0; uint64_t acc = 0;                  /* the read the slice starts with (slices start at read starts) */
    while (r < q->nreads && acc < off) acc += q->lens[r++];
    while (r < q->nreads && !q->lens[r]) r++;
    size_t x = 0; while (x < q->nx && q->xs[x] < off) x++;
    j->qt = &q->qt; j->lens = q->lens; j->nreads = q->nreads; j->read0 = r; j->xs = q->xs; j->nx = q->nx; j->x0 = x; j->site0 = off;
}
/* FASTQ text -> frame (NULL: not FASTQ, or out of memory). chunk, bchunk: see kf_fq_cuts. */
static uint8_t *kf_fastq_compress(const uint8_t *s, size_t n, int threads, size_t chunk, size_t bchunk, size_t *outn) {
    pthread_once(&kf_tab_once, kf_tab_init);
    kf_fq q; memset(&q, 0, sizeof q); kf_cuts cuts[5]; memset(cuts, 0, sizeof cuts); uint8_t *res = NULL;
    if (!kf_fastq_like(s, n) || kf_fastq_split(s, n, &q) || kf_fq_cuts(&q, chunk, bchunk, cuts)) goto done;
    { kf_fq_setup_arg a = {&q}; res = kf_frame_encode(KF_F_FASTQ, n, q.st, 5, cuts, kf_fq_setup, &a, threads, outn); }
done:
    for (int s2 = 0; s2 < 5; s2++) free(cuts[s2].v);
    kf_fq_free(&q);
    return res;
}
static int kf_fastq_join(kf_fq *q, uint8_t *out, size_t cap, size_t *outn) {
    kf_stream *st = q->st;
    const uint8_t *L = st[0].data.p, *N = st[1].data.p, *B = st[2].data.p, *X = st[3].data.p, *Q = st[4].data.p;
    size_t li = 0, ni = 0, bi = 0, xi = 0, o = 0, len = 0, r = 0; uint64_t site = 0, nextx = UINT64_MAX, dx;
    if (xi < st[3].data.n) { if (kf_getvar(X, st[3].data.n, &xi, &dx)) return -1; nextx = dx; }
    kf_span pv[KF_MAXTOK]; int pk = 0; uint8_t name[KF_MAXNAME + 24], prev[KF_MAXNAME + 24]; size_t pn = 0;
#define KF_OUT(src, k) do { if ((k) > cap - o) return -1; memcpy(out + o, (src), (k)); o += (k); } while (0)
#define KF_OUTB(c) do { if (o >= cap) return -1; out[o++] = (uint8_t)(c); } while (0)
    while (li < st[0].data.n) {
        int f = L[li++]; if (f > 3) return -1;
        if (f >= 2) { uint64_t v; if (kf_getvar(L, st[0].data.n, &li, &v) || v > 0xffffffffu) return -1; len = (size_t)v; f -= 2; }
        if (r >= q->nreads || q->lens[r] != len) return -1;
        r++;
        size_t nn = 0; int k = 0; kf_span tk[KF_MAXTOK];
        for (;;) {
            if (ni >= st[1].data.n) return -1;
            int op = N[ni++];
            if (op == KF_OP_END) break;
            if (k >= KF_MAXTOK) return -1;
            size_t start = nn;
            if (op == KF_OP_SAME) { if (k >= pk || nn + pv[k].n > KF_MAXNAME) return -1; memcpy(name + nn, prev + (size_t)(uintptr_t)pv[k].p, pv[k].n); nn += pv[k].n; }
            else if (op == KF_OP_DELTA) {
                uint64_t z, b;
                if (k >= pk || kf_getvar(N, st[1].data.n, &ni, &z)) return -1;
                kf_span pt = {prev + (size_t)(uintptr_t)pv[k].p, pv[k].n};
                if (!kf_num_tok(pt, &b)) return -1;
                uint64_t a = b + (uint64_t)kf_unzz(z); char tmp[24]; int l = 0;
                do { tmp[l++] = (char)('0' + a % 10); a /= 10; } while (a);
                if (nn + (size_t)l > KF_MAXNAME) return -1;
                for (int z2 = 0; z2 < l; z2++) name[nn++] = (uint8_t)tmp[l - 1 - z2];
            } else if (op == KF_OP_LIT) {
                uint64_t l; if (kf_getvar(N, st[1].data.n, &ni, &l) || l > st[1].data.n - ni || nn + l > KF_MAXNAME) return -1;
                memcpy(name + nn, N + ni, (size_t)l); ni += (size_t)l; nn += (size_t)l;
            } else return -1;
            tk[k].p = (const uint8_t *)(uintptr_t)start; tk[k].n = nn - start; k++;
        }
        memcpy(prev, name, nn); pn = nn; memcpy(pv, tk, sizeof(kf_span) * (size_t)k); pk = k;
        KF_OUTB('@'); KF_OUT(name, nn); KF_OUTB('\n');
        if (len > st[2].data.n - bi || len > cap - o) return -1;
        for (size_t z = 0; z < len; z++, site++, bi++) {
            uint8_t c = kf_b2c[B[bi] & 3];
            if (site == nextx) {
                if (xi >= st[3].data.n) return -1;
                c = X[xi++];
                if (kf_b2[c]) return -1;
                if (xi < st[3].data.n) { if (kf_getvar(X, st[3].data.n, &xi, &dx) || !dx) return -1; nextx = site + dx; } else nextx = UINT64_MAX;
            }
            out[o++] = c;
        }
        KF_OUTB('\n'); KF_OUTB('+'); if (f == 1) KF_OUT(name, nn); KF_OUTB('\n');
        if (len > st[4].data.n - (bi - len) || len > cap - o) return -1;
        for (size_t z = 0; z < len; z++) out[o++] = (uint8_t)(Q[bi - len + z] + 33);
        KF_OUTB('\n');
    }
#undef KF_OUT
#undef KF_OUTB
    (void)pn;
    if (r != q->nreads || ni != st[1].data.n || bi != st[2].data.n || xi != st[3].data.n || nextx != UINT64_MAX || bi != st[4].data.n) return -1;
    *outn = o; return 0;
}
/* parses the layout stream into read lengths and the exception stream into sites */
static int kf_fq_layout(kf_fq *q) {
    const uint8_t *L = q->st[0].data.p; size_t li = 0, ln = q->st[0].data.n, len = 0; uint64_t tot = 0;
    while (li < ln) {
        int f = L[li++]; if (f > 3) return -1;
        if (f >= 2) { uint64_t v; if (kf_getvar(L, ln, &li, &v) || v > 0xffffffffu) return -1; len = (size_t)v; }
        else if (!q->nreads) return -1;
        if (kf_grow((void **)&q->lens, &q->rcap, q->nreads + 1, sizeof *q->lens)) return -1;
        q->lens[q->nreads++] = (uint32_t)len; tot += len;
    }
    if (tot != q->st[4].data.n || tot != q->st[2].data.n) return -1;
    const uint8_t *X = q->st[3].data.p; size_t xi = 0, xn = q->st[3].data.n; uint64_t site = 0, d;
    while (xi < xn) {
        if (kf_getvar(X, xn, &xi, &d) || xi >= xn || (q->nx && !d)) return -1;
        xi++; site += d;
        if (site >= tot) return -1;
        if (kf_grow((void **)&q->xs, &q->xcap, q->nx + 1, sizeof *q->xs)) return -1;
        q->xs[q->nx++] = site;
    }
    return 0;
}
/* frame -> FASTQ text of exactly n bytes into out; 0 on success */
static int kf_fastq_decompress(const uint8_t *c, size_t cn, uint8_t *out, size_t n, int threads) {
    pthread_once(&kf_tab_once, kf_tab_init);
    kf_frame f; kf_fq q; memset(&q, 0, sizeof q); kf_namectx *nc = NULL; int rc = -1;
    if (kf_frame_parse(c, cn, &f, 4, &n)) goto done;
    if (f.st[0].kind != KF_BYTES || f.st[1].kind != KF_BYTES || f.st[2].kind != KF_DNA || f.st[3].kind != KF_BYTES || f.st[4].kind != KF_QUAL) goto done;
    for (int s = 0; s < 5; s++) { q.st[s] = f.st[s]; }
    { uint8_t want[5] = {1, 0, 0, 1, 0}; if (kf_frame_decode(&f, want, threads)) goto done; }
    if (kf_fq_layout(&q)) goto done;
    if (q.st[4].data.n) { uint64_t cnt[128]; if (kf_qcounts_get(q.st[4].par.p, q.st[4].par.n, cnt) || kf_qtree_build(&q.qt, cnt)) goto done; }
    nc = calloc((size_t)f.nj + 1, sizeof *nc); if (!nc) goto done;
    {
        uint64_t qoff = 0;
        for (int k = 0; k < f.nj; k++) {
            kf_job *j = &f.jb[k];
            if (j->sid == 1) { j->ctxfn = kf_name_ctx; j->ctxarg = &nc[k]; }
            if (j->sid == 4) {
                size_t r = 0; uint64_t acc = 0;
                while (r < q.nreads && acc < qoff) acc += q.lens[r++];
                if (acc != qoff) goto done;                         /* a slice must start at a read start */
                while (r < q.nreads && !q.lens[r]) r++;
                size_t x = 0; while (x < q.nx && q.xs[x] < qoff) x++;
                j->qt = &q.qt; j->lens = q.lens; j->nreads = q.nreads; j->read0 = r; j->xs = q.xs; j->nx = q.nx; j->x0 = x; j->site0 = qoff;
                qoff += j->n;
            }
        }
    }
    { uint8_t want[5] = {0, 1, 1, 0, 1}; if (kf_frame_decode(&f, want, threads)) goto done; }
    { size_t on = 0; if (kf_fastq_join(&q, out, n, &on) || on != n) goto done; }
    rc = 0;
done:
    free(q.lens); free(q.xs); free(nc);
    kf_frame_free(&f);
    return rc;
}

#ifdef KOLMO_CODEC_REGISTRY
static const kolmo_codec codec_fastq = { C_FQ, "fastq", NULL, NULL, NULL, NULL };   /* coded at block level (core/blocks.c, core/decode.c) */
#endif
