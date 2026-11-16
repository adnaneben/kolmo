/* kfam: research prototype of a family-stream coder. Apache-2.0, Kolmo contributors.
 *
 * The input is split into streams by family (FASTQ: read names, bases, qualities, lengths; CSV: one stream per
 * column; anything else: one stream). Each stream is coded independently by a small model dedicated to its family
 * and a binary arithmetic coder with 16-bit probabilities, all written for Kolmo (no third-party coder). Streams
 * are independent, so they are coded on separate threads; the frame records each stream's size so a decoder can
 * schedule them on any mix of cores.
 *
 *   kfam [-T threads] [-f auto|fastq|csv|one] in out      compress
 *   kfam -d [-T threads] in out                            decompress
 *   kfam -b [-T threads] [-f ...] in                       in-memory timing, JSON line (fastest of at least 3 runs)
 *
 * Frame: "KFM1", family u8, stream count u8, original size u64, then per stream: raw size u64, coded size u64;
 * then the coded streams. Family-specific layout data travels in stream 0. Not a Kolmo frame.
 *
 * Determinism: integer arithmetic only, except the logistic tables, which are built from IEEE double +, *, /
 * only (no libm), so every platform gets the same tables. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ logistic tables (12-bit domain) */
static int16_t ksq[4096];        /* squash: stretch domain [-2048, 2047] -> probability in [1, 4095] */
static int16_t kst[4096];        /* stretch: probability -> smallest x with squash(x) >= p */
static uint32_t krate[256];      /* adaptation rate of a counter after n updates: 65536 / (n + 1.6) */
static pthread_once_t ktab_once = PTHREAD_ONCE_INIT;
static double kexp(double x) {   /* e^x from basic operations: exp(x) = exp(x / 1024)^1024, Taylor series inside */
    double y = x / 1024.0, t = 1.0, s = 1.0;
    for (int k = 1; k <= 12; k++) { t = t * y / k; s = s + t; }
    for (int k = 0; k < 10; k++) s = s * s;
    return s;
}
static void ktab_init(void) {
    for (int i = 0; i < 4096; i++) {
        double v = 4096.0 / (1.0 + kexp(-(i - 2048) / 256.0));
        int q = (int)(v + 0.5); ksq[i] = (int16_t)(q < 1 ? 1 : q > 4095 ? 4095 : q);
    }
    for (int p = 0; p < 4096; p++) {
        int lo = -2048, hi = 2048;
        while (lo < hi) { int mid = lo + ((hi - lo) >> 1); if (ksq[mid + 2048] >= p) hi = mid; else lo = mid + 1; }
        kst[p] = (int16_t)(lo > 2047 ? 2047 : lo);
    }
    for (int n = 0; n < 256; n++) krate[n] = (uint32_t)(65536.0 / (n + 1.6));
}
static inline int sq(int x) { return ksq[(x < -2048 ? -2048 : x > 2047 ? 2047 : x) + 2048]; }
static inline int st(int p) { return kst[p]; }

/* ------------------------------------------------------------------ binary arithmetic coder, 16-bit probabilities */
typedef struct { uint32_t lo, hi; uint8_t *buf; size_t n, cap; int err; } aenc;
typedef struct { uint32_t lo, hi, x; const uint8_t *in; size_t n, pos, over; } adec;   /* over: reads past the end */
static void ae_put(aenc *e, uint8_t b) {
    if (e->n == e->cap) { size_t c = e->cap ? 2 * e->cap : 4096; uint8_t *t = realloc(e->buf, c); if (!t) { e->err = 1; return; } e->buf = t; e->cap = c; }
    e->buf[e->n++] = b;
}
static inline void ae_bit(aenc *e, int y, uint32_t p) {          /* p = P(y = 1) in [1, 65535] */
    uint32_t mid = e->lo + (uint32_t)(((uint64_t)(e->hi - e->lo) * p) >> 16);
    if (y) e->hi = mid; else e->lo = mid + 1;
    while ((e->lo ^ e->hi) < (1u << 24)) { ae_put(e, (uint8_t)(e->hi >> 24)); e->lo <<= 8; e->hi = (e->hi << 8) | 0xffu; }
}
static void ae_end(aenc *e) { for (int k = 0; k < 4; k++) { ae_put(e, (uint8_t)(e->lo >> 24)); e->lo <<= 8; } }
static inline uint8_t ad_next(adec *d) { if (d->pos < d->n) return d->in[d->pos++]; d->over++; return 0; }
static void ad_init(adec *d, const uint8_t *in, size_t n) {
    d->lo = 0; d->hi = 0xffffffffu; d->x = 0; d->in = in; d->n = n; d->pos = 0; d->over = 0;
    for (int k = 0; k < 4; k++) d->x = (d->x << 8) | ad_next(d);
}
static inline int ad_bit(adec *d, uint32_t p) {
    uint32_t mid = d->lo + (uint32_t)(((uint64_t)(d->hi - d->lo) * p) >> 16);
    int y = d->x <= mid;
    if (y) d->hi = mid; else d->lo = mid + 1;
    while ((d->lo ^ d->hi) < (1u << 24)) { d->lo <<= 8; d->hi = (d->hi << 8) | 0xffu; d->x = (d->x << 8) | ad_next(d); }
    return y;
}
/* one coder object for both directions, so a model's code is written once */
typedef struct { aenc *e; adec *d; } coder;
static inline int code_bit(coder *c, int y, int p12) {           /* p12 = P(1) in 12 bits, refined to 16 */
    uint32_t p = (uint32_t)p12 << 4; if (p < 32) p = 32; if (p > 65503) p = 65503;
    if (c->e) { ae_bit(c->e, y, p); return y; }
    return ad_bit(c->d, p);
}

/* ------------------------------------------------------------------ counters and mixer */
typedef struct { uint16_t *p; uint8_t *n; uint32_t mask; int limit; } ctab;    /* adaptive probabilities, 16-bit */
static int ctab_new(ctab *t, int bits, int limit) {
    size_t sz = (size_t)1 << bits; t->mask = (uint32_t)sz - 1; t->limit = limit;
    t->p = malloc(sz * 2); t->n = calloc(sz, 1);
    if (!t->p || !t->n) return -1;
    for (size_t i = 0; i < sz; i++) t->p[i] = 32768;
    return 0;
}
static void ctab_free(ctab *t) { free(t->p); free(t->n); }
static inline int ctab_p12(const ctab *t, uint32_t i) { return t->p[i & t->mask] >> 4; }
static inline void ctab_upd(ctab *t, uint32_t i, int y) {
    i &= t->mask; int n = t->n[i], p = t->p[i];
    p += (int)(((int64_t)((y ? 65535 : 0) - p) * krate[n]) >> 16);
    t->p[i] = (uint16_t)(p < 1 ? 1 : p > 65535 ? 65535 : p);
    if (n < t->limit) t->n[i] = (uint8_t)(n + 1);
}
enum { MAXIN = 8 };
typedef struct { int32_t *w; int nin, nsets, x[MAXIN], pr, set; } mixer;   /* logistic mixing, weights selected by a context */
static int mixer_new(mixer *m, int nin, int nsets) {
    m->nin = nin; m->nsets = nsets; m->w = malloc(sizeof(int32_t) * (size_t)nin * nsets);
    if (!m->w) return -1;
    for (int i = 0; i < nin * nsets; i++) m->w[i] = (1 << 16) / nin;
    return 0;
}
static inline int mix(mixer *m, int set) {
    m->set = set; const int32_t *w = m->w + (size_t)set * m->nin; int64_t dot = 0;
    for (int i = 0; i < m->nin; i++) dot += (int64_t)m->x[i] * w[i];
    return m->pr = sq((int)(dot >> 16));
}
static inline void mix_upd(mixer *m, int y, int lr) {
    int32_t *w = m->w + (size_t)m->set * m->nin; int err = ((y << 12) - m->pr) * lr;
    for (int i = 0; i < m->nin; i++) w[i] += (m->x[i] * err) >> 10;
}

/* ------------------------------------------------------------------ the byte model (generic family) */
/* Orders 0 to 3 of the stream's own bytes, plus one context the family supplies (column, token index, byte index
 * of a number); five counters mixed by a mixer chosen by the partial byte. Table sizes follow the stream size. */
typedef struct { ctab t[5]; mixer mx; uint32_t c4, h[5]; } bmodel;
static int bm_new(bmodel *m, size_t len) {
    int b = 10; while (b < 22 && ((size_t)1 << b) < len * 2) b++;
    memset(m, 0, sizeof *m);
    if (ctab_new(&m->t[0], 16, 60) || ctab_new(&m->t[1], 16, 255) || ctab_new(&m->t[2], b, 255) || ctab_new(&m->t[3], b, 255)
        || ctab_new(&m->t[4], b, 255) || mixer_new(&m->mx, 6, 256)) return -1;
    return 0;
}
static void bm_free(bmodel *m) { for (int i = 0; i < 5; i++) ctab_free(&m->t[i]); free(m->mx.w); }
static inline uint32_t hsh(uint32_t a, uint32_t b) { uint32_t h = a * 0x9E3779B1u ^ b * 0x85EBCA77u; h ^= h >> 15; h *= 0x2C1B3C6Du; return h ^ (h >> 13); }
static int bm_code(bmodel *m, coder *c, int byte, uint32_t xc) {   /* codes (or decodes) one byte; returns it */
    uint32_t c4 = m->c4;
    m->h[0] = (xc & 0xff) << 8; m->h[1] = (c4 & 0xff) << 8;
    m->h[2] = hsh(c4 & 0xffff, 2); m->h[3] = hsh(c4 & 0xffffff, 3); m->h[4] = hsh(xc, (c4 & 0xff) | 4u << 8);
    int c0 = 1;
    for (int b = 7; b >= 0; b--) {
        uint32_t i[5] = {m->h[0] | c0, m->h[1] | c0, m->h[2] + c0 * 0x3C6EF372u, m->h[3] + c0 * 0x3C6EF372u, m->h[4] + c0 * 0x3C6EF372u};
        for (int k = 0; k < 5; k++) m->mx.x[k] = st(ctab_p12(&m->t[k], i[k]));
        m->mx.x[5] = 256;
        int p = mix(&m->mx, c0);
        int y = code_bit(c, (byte >> b) & 1, p);
        for (int k = 0; k < 5; k++) ctab_upd(&m->t[k], i[k], y);
        mix_upd(&m->mx, y, 6);
        c0 = c0 << 1 | y;
    }
    int v = c0 & 0xff; m->c4 = c4 << 8 | (uint32_t)v; return v;
}

/* ------------------------------------------------------------------ DNA bases: 2-bit symbols, orders up to 22 */
enum { DNA_ORD = 6 };
static const int dna_k[DNA_ORD] = {2, 4, 8, 12, 16, 22};
typedef struct { ctab t[DNA_ORD]; mixer mx; uint64_t hist; } dmodel;
static int dm_new(dmodel *m, size_t len) {
    int b = 12; while (b < 24 && ((size_t)1 << b) < len * 2) b++;
    memset(m, 0, sizeof *m);
    for (int k = 0; k < DNA_ORD; k++) { int bits = 2 * dna_k[k] + 2 < b ? 2 * dna_k[k] + 2 : b; if (ctab_new(&m->t[k], bits, 255)) return -1; }
    return mixer_new(&m->mx, DNA_ORD + 1, 16 * 3);
}
static void dm_free(dmodel *m) { for (int k = 0; k < DNA_ORD; k++) ctab_free(&m->t[k]); free(m->mx.w); }
static int dm_code(dmodel *m, coder *c, int s) {                  /* s in 0..3 */
    uint32_t base[DNA_ORD];
    for (int k = 0; k < DNA_ORD; k++) {
        uint64_t ctx = dna_k[k] >= 32 ? m->hist : m->hist & (((uint64_t)1 << (2 * dna_k[k])) - 1);
        base[k] = 2 * dna_k[k] + 2 <= 24 ? (uint32_t)(ctx << 2) : hsh((uint32_t)ctx ^ (uint32_t)(ctx >> 32) * 0x27D4EB2Fu, (uint32_t)k) << 2;
    }
    int node = 1, set = (int)(m->hist & 15) * 3;
    for (int b = 1; b >= 0; b--) {
        for (int k = 0; k < DNA_ORD; k++) m->mx.x[k] = st(ctab_p12(&m->t[k], base[k] + (uint32_t)node));
        m->mx.x[DNA_ORD] = 256;
        int p = mix(&m->mx, set + node - 1);
        int y = code_bit(c, (s >> b) & 1, p);
        for (int k = 0; k < DNA_ORD; k++) ctab_upd(&m->t[k], base[k] + (uint32_t)node, y);
        mix_upd(&m->mx, y, 4);
        node = node << 1 | y;
    }
    int v = node & 3; m->hist = m->hist << 2 | (uint64_t)v; return v;
}

/* ------------------------------------------------------------------ quality scores: position and recent qualities */
typedef struct { ctab t[4]; mixer mx; int q1, q2, q3, pos, dsum; } qmodel;
static int qm_new(qmodel *m) {
    memset(m, 0, sizeof *m);
    if (ctab_new(&m->t[0], 21, 255) || ctab_new(&m->t[1], 22, 255) || ctab_new(&m->t[2], 22, 255) || ctab_new(&m->t[3], 16, 255)
        || mixer_new(&m->mx, 5, 128)) return -1;
    return 0;
}
static void qm_free(qmodel *m) { for (int k = 0; k < 4; k++) ctab_free(&m->t[k]); free(m->mx.w); }
static void qm_read_start(qmodel *m) { m->q1 = m->q2 = m->q3 = 0; m->pos = 0; m->dsum = 0; }
static int qm_code(qmodel *m, coder *c, int q) {                  /* q in 0..127 (quality byte - 33, clamped by caller) */
    int pb = m->pos < 64 ? m->pos / 2 : 32 + (m->pos < 320 ? (m->pos - 64) / 16 : 16);
    int q23 = m->q2 > m->q3 ? m->q2 : m->q3, ds = m->dsum < 63 ? m->dsum : 63;
    uint32_t b0 = (uint32_t)(m->q1 << 7 | m->q2) << 7, b1 = hsh((uint32_t)(m->q1 << 7 | q23), (uint32_t)pb) << 7,
             b2 = hsh((uint32_t)(m->q1 << 6 | ds), (uint32_t)pb + 77u) << 7, b3 = (uint32_t)pb << 7;
    int node = 1;
    for (int b = 6; b >= 0; b--) {
        uint32_t i[4] = {b0 + (uint32_t)node, b1 + (uint32_t)node, b2 + (uint32_t)node, b3 + (uint32_t)node};
        for (int k = 0; k < 4; k++) m->mx.x[k] = st(ctab_p12(&m->t[k], i[k]));
        m->mx.x[4] = 256;
        int p = mix(&m->mx, node);
        int y = code_bit(c, (q >> b) & 1, p);
        for (int k = 0; k < 4; k++) ctab_upd(&m->t[k], i[k], y);
        mix_upd(&m->mx, y, 4);
        node = node << 1 | y;
    }
    int v = node & 127;
    m->dsum += v > m->q1 ? v - m->q1 : m->q1 - v;
    m->q3 = m->q2; m->q2 = m->q1; m->q1 = v; m->pos++;
    return v;
}

/* ------------------------------------------------------------------ growable byte buffer */
typedef struct { uint8_t *p; size_t n, cap; } buf;
static int bput(buf *b, const void *s, size_t n) {
    if (b->n + n > b->cap) { size_t c = b->cap ? b->cap : 4096; while (c < b->n + n) c *= 2; uint8_t *t = realloc(b->p, c); if (!t) return -1; b->p = t; b->cap = c; }
    memcpy(b->p + b->n, s, n); b->n += n; return 0;
}
static int bbyte(buf *b, uint8_t v) { return bput(b, &v, 1); }
static int bvar(buf *b, uint64_t v) { while (v >= 128) { if (bbyte(b, (uint8_t)(v | 128))) return -1; v >>= 7; } return bbyte(b, (uint8_t)v); }
static int getvar(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int s = 0;
    while (*i < n && s < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << s; if (!(c & 128)) { *v = r; return 0; } s += 7; }
    return -1;
}
static inline uint64_t zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
static inline int64_t unzz(uint64_t v) { return (int64_t)(v >> 1) ^ -(int64_t)(v & 1); }

/* ------------------------------------------------------------------ streams and their coders */
/* Every stream is a byte string plus a model kind. The byte model takes an extra context per byte from a parallel
 * context string (one byte per byte), which the family builds; NULL means none. */
enum { K_BYTES = 0, K_DNA = 1, K_QUAL = 2 };
typedef struct { int kind; buf data, ctx; buf out; int err; } stream;

static void *enc_stream(void *arg) {
    stream *s = arg; aenc e = {0, 0xffffffffu, NULL, 0, 0, 0}; coder c = {&e, NULL};
    size_t n = s->data.n; const uint8_t *d = s->data.p;
    if (s->kind == K_DNA) {
        dmodel m; if (dm_new(&m, n)) { s->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) dm_code(&m, &c, d[i]);
        dm_free(&m);
    } else if (s->kind == K_QUAL) {
        qmodel m; if (qm_new(&m)) { s->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) { qm_code(&m, &c, d[i]); if (d[i] == 127) qm_read_start(&m); }   /* 127 ends the previous read */
        qm_free(&m);
    } else {
        bmodel m; if (bm_new(&m, n)) { s->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) bm_code(&m, &c, d[i], s->ctx.n ? s->ctx.p[i] : 0);
        bm_free(&m);
    }
    ae_end(&e); s->out.p = e.buf; s->out.n = e.n; s->err |= e.err; return NULL;
}
/* decoding needs the context string, which some families can only rebuild while decoding: they pass a callback */
typedef struct { stream *s; const uint8_t *in; size_t inn; size_t rawn; int (*ctxfn)(void *, size_t, const uint8_t *); void *ctxarg; } djob;
/* The decoder renormalises in step with the encoder, which flushes exactly 4 bytes at the end, so a valid stream is
 * never read past its end. A damaged stream (e.g. a raw length changed in the frame header) would otherwise decode
 * zeros for up to 2^40 symbols; it is reported corrupt once it has read more than AD_OVER_MAX bytes past its end. */
#define AD_OVER_MAX 8
static void *dec_stream(void *arg) {
    djob *j = arg; stream *s = j->s; adec d; ad_init(&d, j->in, j->inn); coder c = {NULL, &d};
    size_t n = j->rawn; s->data.p = malloc(n ? n : 1); s->data.n = n;
    if (!s->data.p) { s->err = 1; return NULL; }
    uint8_t *o = s->data.p;
    if (s->kind == K_DNA) {
        dmodel m; if (dm_new(&m, n)) { s->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) { o[i] = (uint8_t)dm_code(&m, &c, 0); if (d.over > AD_OVER_MAX) { s->err = 1; break; } }
        dm_free(&m);
    } else if (s->kind == K_QUAL) {
        qmodel m; if (qm_new(&m)) { s->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) {
            int v = qm_code(&m, &c, 0);
            if (v == 127) qm_read_start(&m);
            o[i] = (uint8_t)v;
            if (d.over > AD_OVER_MAX) { s->err = 1; break; }
        }
        qm_free(&m);
    } else {
        bmodel m; if (bm_new(&m, n)) { s->err = 1; return NULL; }
        for (size_t i = 0; i < n; i++) {
            o[i] = (uint8_t)bm_code(&m, &c, 0, j->ctxfn ? (uint32_t)j->ctxfn(j->ctxarg, i, o) : 0);
            if (d.over > AD_OVER_MAX) { s->err = 1; break; }
        }
        bm_free(&m);
    }
    return NULL;
}

/* ------------------------------------------------------------------ families */
enum { F_ONE = 0, F_FASTQ = 1, F_CSV = 2 };

/* FASTQ (complete four-line records, "\n" endings, the plus line bare or repeating the name):
 *   stream 0  layout: per read the length (varint, only when it changes) and a flag byte (plus-line form)
 *   stream 1  name tokens: per token an operation (same / numeric delta / literal), with the token index as context
 *   stream 2  bases as 2-bit symbols (A C G T); stream 3 the exceptions: (position delta, byte) for anything else
 *   stream 4  qualities - 33, a 127 marking each read's start */
typedef struct { const uint8_t *p; size_t n; } span;
static int line(const uint8_t *s, size_t n, size_t *i, span *out) {
    if (*i >= n) return -1;
    const uint8_t *e = memchr(s + *i, '\n', n - *i); if (!e) return -1;
    out->p = s + *i; out->n = (size_t)(e - (s + *i)); *i = (size_t)(e - s) + 1; return 0;
}
static int base2(uint8_t c) { return c == 'A' ? 0 : c == 'C' ? 1 : c == 'G' ? 2 : c == 'T' ? 3 : -1; }
static const uint8_t b2c[4] = {'A', 'C', 'G', 'T'};
/* tokens: maximal runs of digits or of non-digits */
static int tokens(span s, span *t, int max) {
    int k = 0; size_t i = 0;
    while (i < s.n && k < max) { size_t j = i; int d = s.p[i] >= '0' && s.p[i] <= '9'; while (j < s.n && (s.p[j] >= '0' && s.p[j] <= '9') == d) j++; t[k].p = s.p + i; t[k].n = j - i; k++; i = j; }
    return i == s.n ? k : -1;
}
static int num_tok(span t, uint64_t *v) {          /* a canonical decimal number of at most 18 digits */
    if (!t.n || t.n > 18 || !(t.p[0] >= '0' && t.p[0] <= '9') || (t.p[0] == '0' && t.n > 1)) return 0;
    uint64_t r = 0; for (size_t i = 0; i < t.n; i++) { if (t.p[i] < '0' || t.p[i] > '9') return 0; r = r * 10 + (uint64_t)(t.p[i] - '0'); }
    *v = r; return 1;
}
enum { MAXTOK = 32, OP_SAME = 0, OP_DELTA = 1, OP_LIT = 2, OP_END = 3 };
static int fastq_split(const uint8_t *s, size_t n, stream *st) {
    span pv[MAXTOK]; int pk = 0; size_t i = 0, prevlen = (size_t)-1, lastx = 0, site = 0;
    while (i < n) {
        span nm, sq, pl, ql;
        if (line(s, n, &i, &nm) || line(s, n, &i, &sq) || line(s, n, &i, &pl) || line(s, n, &i, &ql)) return -1;
        if (!nm.n || nm.p[0] != '@' || !pl.n || pl.p[0] != '+' || sq.n != ql.n) return -1;
        span name = {nm.p + 1, nm.n - 1};
        int plus = pl.n == 1 ? 0 : (pl.n == nm.n && !memcmp(pl.p + 1, nm.p + 1, nm.n - 1)) ? 1 : -1;
        if (plus < 0 || memchr(name.p, '\r', name.n)) return -1;
        if (sq.n != prevlen) { if (bbyte(&st[0].data, 2 + plus) || bvar(&st[0].data, sq.n)) return -1; prevlen = sq.n; }
        else if (bbyte(&st[0].data, (uint8_t)plus)) return -1;
        span tk[MAXTOK]; int k = tokens(name, tk, MAXTOK); if (k < 0) return -1;
        for (int t = 0; t < k; t++) {
            uint8_t ix = (uint8_t)(t < 15 ? t : 15); uint64_t a, b;
            if (t < pk && tk[t].n == pv[t].n && !memcmp(tk[t].p, pv[t].p, tk[t].n)) { if (bbyte(&st[1].data, OP_SAME) || bbyte(&st[1].ctx, ix)) return -1; }
            else if (t < pk && num_tok(tk[t], &a) && num_tok(pv[t], &b)) {
                buf tmp = {0}; if (bvar(&tmp, zz((int64_t)(a - b)))) return -1;
                if (bbyte(&st[1].data, OP_DELTA) || bbyte(&st[1].ctx, ix)) { free(tmp.p); return -1; }
                for (size_t q = 0; q < tmp.n; q++) if (bbyte(&st[1].data, tmp.p[q]) || bbyte(&st[1].ctx, (uint8_t)(16 + ix))) { free(tmp.p); return -1; }
                free(tmp.p);
            } else {
                if (bbyte(&st[1].data, OP_LIT) || bbyte(&st[1].ctx, ix)) return -1;
                size_t before = st[1].data.n; if (bvar(&st[1].data, tk[t].n)) return -1;
                for (size_t q = before; q < st[1].data.n; q++) if (bbyte(&st[1].ctx, (uint8_t)(32 + ix))) return -1;
                for (size_t q = 0; q < tk[t].n; q++) if (bbyte(&st[1].data, tk[t].p[q]) || bbyte(&st[1].ctx, (uint8_t)(48 + ix))) return -1;
            }
        }
        if (bbyte(&st[1].data, OP_END) || bbyte(&st[1].ctx, (uint8_t)(k < 15 ? k : 15))) return -1;
        memcpy(pv, tk, sizeof(span) * (size_t)k); pk = k;
        for (size_t q = 0; q < sq.n; q++, site++) {
            int b = base2(sq.p[q]);
            if (bbyte(&st[2].data, (uint8_t)(b < 0 ? 0 : b))) return -1;
            if (b < 0) { if (bvar(&st[3].data, site - lastx) || bbyte(&st[3].data, sq.p[q])) return -1; lastx = site; }
        }
        if (bbyte(&st[4].data, 127)) return -1;
        for (size_t q = 0; q < ql.n; q++) { if (ql.p[q] < 33 || ql.p[q] - 33 >= 127) return -1; if (bbyte(&st[4].data, (uint8_t)(ql.p[q] - 33))) return -1; }
    }
    return 0;
}
/* the name stream's context depends on the operations already decoded: rebuild it while decoding */
typedef struct { int tok, state; uint64_t left; int shift; } namectx;
static int name_ctx(void *arg, size_t i, const uint8_t *o) {
    namectx *c = arg;
    if (i > 0) {                                   /* account for the byte just decoded (o[i - 1]) */
        uint8_t v = o[i - 1];
        if (c->state == 0) {                       /* it was an operation */
            if (v == OP_SAME) c->tok++;
            else if (v == OP_DELTA) c->state = 1;
            else if (v == OP_LIT) { c->state = 2; c->left = 0; c->shift = 0; }
            else c->tok = 0;                       /* OP_END */
        } else if (c->state == 1) { if (!(v & 128)) { c->state = 0; c->tok++; } }
        else if (c->state == 2) { c->left |= (uint64_t)(v & 127) << c->shift; c->shift += 7; if (!(v & 128)) c->state = c->left ? 3 : 0, c->tok += c->left ? 0 : 1; }
        else if (c->state == 3) { if (--c->left == 0) { c->state = 0; c->tok++; } }
    }
    int ix = c->tok < 15 ? c->tok : 15;
    return c->state == 0 ? ix : c->state == 1 ? 16 + ix : c->state == 2 ? 32 + ix : 48 + ix;
}
static int fastq_join(stream *st, buf *out) {
    const uint8_t *L = st[0].data.p, *N = st[1].data.p, *B = st[2].data.p, *X = st[3].data.p, *Q = st[4].data.p;
    size_t li = 0, ni = 0, bi = 0, xi = 0, qi = 0, len = 0, site = 0, nextx = (size_t)-1;
    uint64_t dx; if (xi < st[3].data.n) { if (getvar(X, st[3].data.n, &xi, &dx)) goto fail; nextx = dx; }
    span pv[MAXTOK]; int pk = 0; buf names = {0};          /* previous name's tokens live in `names` */
    uint8_t name[4096];
    while (li < st[0].data.n) {
        int f = L[li++]; if (f >= 2) { uint64_t v; if (getvar(L, st[0].data.n, &li, &v)) goto fail; len = v; f -= 2; }
        size_t nn = 0; int k = 0; span tk[MAXTOK];
        for (;;) {
            if (ni >= st[1].data.n) goto fail;
            int op = N[ni++];
            if (op == OP_END) break;
            if (k >= MAXTOK) goto fail;
            size_t start = nn;
            if (op == OP_SAME) { if (k >= pk || nn + pv[k].n > sizeof name) goto fail; memcpy(name + nn, pv[k].p, pv[k].n); nn += pv[k].n; }
            else if (op == OP_DELTA) {
                uint64_t z, b; if (getvar(N, st[1].data.n, &ni, &z) || k >= pk || !num_tok(pv[k], &b)) goto fail;
                uint64_t a = b + (uint64_t)unzz(z); char tmp[24]; int l = snprintf(tmp, sizeof tmp, "%llu", (unsigned long long)a);
                if (nn + (size_t)l > sizeof name) goto fail; memcpy(name + nn, tmp, (size_t)l); nn += (size_t)l;
            } else if (op == OP_LIT) {
                uint64_t l; if (getvar(N, st[1].data.n, &ni, &l) || l > st[1].data.n - ni || nn + l > sizeof name) goto fail;
                memcpy(name + nn, N + ni, l); ni += l; nn += l;
            } else goto fail;
            tk[k].p = (const uint8_t *)(uintptr_t)start; tk[k].n = nn - start; k++;
        }
        names.n = 0; if (bput(&names, name, nn)) goto fail;
        for (int t = 0; t < k; t++) { pv[t].p = names.p + (size_t)(uintptr_t)tk[t].p; pv[t].n = tk[t].n; } pk = k;
        if (bbyte(out, '@') || bput(out, name, nn) || bbyte(out, '\n')) goto fail;
        if (bi + len > st[2].data.n) goto fail;
        for (size_t q = 0; q < len; q++, site++, bi++) {
            uint8_t c = b2c[B[bi] & 3];
            if (site == nextx) { if (xi >= st[3].data.n) goto fail; c = X[xi++]; if (xi < st[3].data.n) { if (getvar(X, st[3].data.n, &xi, &dx)) goto fail; nextx = site + dx; } else nextx = (size_t)-1; }
            if (bbyte(out, c)) goto fail;
        }
        if (bbyte(out, '\n') || bbyte(out, '+') || (f == 1 && bput(out, name, nn)) || bbyte(out, '\n')) goto fail;
        if (qi >= st[4].data.n || Q[qi++] != 127 || qi + len > st[4].data.n) goto fail;
        for (size_t q = 0; q < len; q++) if (bbyte(out, (uint8_t)(Q[qi++] + 33))) goto fail;
        if (bbyte(out, '\n')) goto fail;
    }
    free(names.p);
    return 0;
fail:
    free(names.p);
    return -1;
}

/* CSV / TSV (one delimiter, the same field count on every line, no quotes, "\n" endings):
 *   stream 0  layout: delimiter, column count, row count, per column its type (0 text, 1 number), then the header line
 *   stream 1+c column c.
 *     number columns (every non-empty cell is -?(0|[1-9]d*)(.d{1,9})? with at most 17 digits): per cell a header
 *       byte h (0 empty; else 1 + fraction digits, plus 16 when the value is coded as a delta from the column's
 *       previous number with the same fraction digits), then the zigzag mantissa (or delta) as a varint. Contexts:
 *       the previous header for a header, the byte index and the delta flag for a varint byte.
 *     text columns: the cells separated by '\n'. */
static int parse_num(span t, int64_t *mant, int *frac) {
    size_t i = 0; int neg = 0; if (i < t.n && t.p[i] == '-') { neg = 1; i++; }
    size_t a = i; uint64_t r = 0; int nd = 0, fr = 0;
    while (i < t.n && t.p[i] >= '0' && t.p[i] <= '9') { r = r * 10 + (uint64_t)(t.p[i] - '0'); i++; nd++; }
    if (i == a || nd > 17 || (t.p[a] == '0' && i - a > 1)) return 0;
    if (i < t.n && t.p[i] == '.') {
        i++;
        while (i < t.n && t.p[i] >= '0' && t.p[i] <= '9') { r = r * 10 + (uint64_t)(t.p[i] - '0'); i++; nd++; fr++; }
        if (!fr || fr > 9 || nd > 17) return 0;
    }
    if (i != t.n || (neg && r == 0)) return 0;
    *mant = neg ? -(int64_t)r : (int64_t)r; *frac = fr; return 1;
}
static int csv_fields(span l, uint8_t d, span *f, int max) {
    int k = 0; size_t i = 0, s = 0;
    for (;; i++) { if (i == l.n || l.p[i] == d) { if (k >= max) return -1; f[k].p = l.p + s; f[k].n = i - s; k++; s = i + 1; if (i == l.n) break; } }
    return k;
}
enum { MAXCOL = 1024 };
static int csv_split(const uint8_t *s, size_t n, stream **pst, int *pns) {
    if (!n || s[n - 1] != '\n' || memchr(s, '"', n) || memchr(s, '\r', n)) return -1;
    size_t i = 0; span hdr; if (line(s, n, &i, &hdr)) return -1;
    static const uint8_t cand[3] = {',', '\t', ';'}; uint8_t d = 0; size_t best = 0;
    for (int c = 0; c < 3; c++) { size_t k = 0; for (size_t q = 0; q < hdr.n; q++) k += hdr.p[q] == cand[c]; if (k > best) { best = k; d = cand[c]; } }
    if (!best) return -1;
    span *f = malloc(sizeof(span) * MAXCOL); int *type = malloc(sizeof(int) * MAXCOL); int64_t *pm = calloc(MAXCOL, sizeof(int64_t)); int *pf = malloc(sizeof(int) * MAXCOL);
    int nc, rc = -1; stream *st = NULL;
    if (!f || !type || !pm || !pf || (nc = csv_fields(hdr, d, f, MAXCOL)) < 2) goto out;
    size_t body = i, rows = 0;
    for (int c = 0; c < nc; c++) type[c] = 1;
    for (size_t j = body; j < n; ) {                         /* pass 1: field counts and column types */
        span l; if (line(s, n, &j, &l) || csv_fields(l, d, f, MAXCOL) != nc) goto out;
        for (int c = 0; c < nc; c++) { int64_t m; int fr; if (type[c] && f[c].n && !parse_num(f[c], &m, &fr)) type[c] = 0; }
        rows++;
    }
    st = calloc((size_t)nc + 1, sizeof *st); if (!st) goto out;
    *pst = st; *pns = nc + 1;
    buf *L = &st[0].data;
    if (bbyte(L, d) || bvar(L, (uint64_t)nc) || bvar(L, rows)) goto out;
    for (int c = 0; c < nc; c++) if (bbyte(L, (uint8_t)type[c])) goto out;
    if (bput(L, hdr.p, hdr.n)) goto out;
    for (int c = 0; c < nc; c++) pf[c] = -1;
    int *ph = calloc((size_t)nc, sizeof(int)); if (!ph) goto out;
    for (size_t j = body; j < n; ) {
        span l; line(s, n, &j, &l); csv_fields(l, d, f, MAXCOL);
        for (int c = 0; c < nc; c++) {
            buf *D = &st[1 + c].data, *C = &st[1 + c].ctx;
            if (!type[c]) { if (bput(D, f[c].p, f[c].n) || bbyte(D, '\n')) goto out2; continue; }
            int h = 0; int64_t m = 0; int fr = 0;
            if (f[c].n) { parse_num(f[c], &m, &fr); h = 1 + fr; if (pf[c] == fr) h += 16; }
            if (bbyte(D, (uint8_t)h) || bbyte(C, (uint8_t)(128 + (ph[c] & 31)))) goto out2;
            ph[c] = h;
            if (!h) continue;
            uint64_t z = h >= 17 ? zz(m - pm[c]) : zz(m); pm[c] = m; pf[c] = fr;
            buf tmp = {0}; if (bvar(&tmp, z)) goto out2;
            for (size_t q = 0; q < tmp.n; q++) if (bbyte(D, tmp.p[q]) || bbyte(C, (uint8_t)((q < 9 ? q : 9) + (h >= 17 ? 10 : 0)))) { free(tmp.p); goto out2; }
            free(tmp.p);
        }
    }
    rc = 0;
out2:
    free(ph);
out:
    free(f); free(type); free(pm); free(pf);
    return rc;
}
/* decoder side of the number columns' contexts: a header, then (unless empty) a varint */
typedef struct { int state, byte, delta, ph; } numctx;
static int num_ctx(void *arg, size_t i, const uint8_t *o) {
    numctx *c = arg;
    if (i > 0) {
        uint8_t v = o[i - 1];
        if (c->state == 0) { c->ph = v; if (v) { c->state = 1; c->byte = 0; c->delta = v >= 17; } }
        else if (v & 128) c->byte++;
        else c->state = 0;
    }
    return c->state == 0 ? 128 + (c->ph & 31) : (c->byte < 9 ? c->byte : 9) + (c->delta ? 10 : 0);
}
static int csv_join(stream *st, int ns, buf *out) {
    const uint8_t *L = st[0].data.p; size_t li = 0, ln = st[0].data.n; uint64_t nc, rows;
    if (ln < 1) return -1; uint8_t d = L[li++];
    if (getvar(L, ln, &li, &nc) || getvar(L, ln, &li, &rows) || nc + 1 != (uint64_t)ns || nc > MAXCOL || li + nc > ln) return -1;
    const uint8_t *type = L + li; li += nc;
    if (bput(out, L + li, ln - li) || bbyte(out, '\n')) return -1;
    size_t *pos = calloc(nc, sizeof(size_t)); int64_t *pm = calloc(nc, sizeof(int64_t)); int rc = -1;
    if (!pos || !pm) goto done;
    for (uint64_t r = 0; r < rows; r++) {
        for (uint64_t c = 0; c < nc; c++) {
            const uint8_t *D = st[1 + c].data.p; size_t dn = st[1 + c].data.n;
            if (c && bbyte(out, d)) goto done;
            if (!type[c]) {
                const uint8_t *e = pos[c] < dn ? memchr(D + pos[c], '\n', dn - pos[c]) : NULL; if (!e) goto done;
                if (bput(out, D + pos[c], (size_t)(e - (D + pos[c])))) goto done; pos[c] = (size_t)(e - D) + 1; continue;
            }
            if (pos[c] >= dn) goto done;
            int h = D[pos[c]++]; if (!h) continue;
            if (h > 26 || (h > 10 && h < 17)) goto done;
            uint64_t z; if (getvar(D, dn, &pos[c], &z)) goto done;
            int64_t m = h >= 17 ? pm[c] + unzz(z) : unzz(z); pm[c] = m;
            int fr = (h >= 17 ? h - 16 : h) - 1; uint64_t a = m < 0 ? (uint64_t)0 - (uint64_t)m : (uint64_t)m;
            char tmp[48]; int l;
            if (!fr) l = snprintf(tmp, sizeof tmp, "%s%llu", m < 0 ? "-" : "", (unsigned long long)a);
            else { uint64_t p10 = 1; for (int k = 0; k < fr; k++) p10 *= 10;
                   l = snprintf(tmp, sizeof tmp, "%s%llu.%0*llu", m < 0 ? "-" : "", (unsigned long long)(a / p10), fr, (unsigned long long)(a % p10)); }
            if (bput(out, tmp, (size_t)l)) goto done;
        }
        if (bbyte(out, '\n')) goto done;
    }
    rc = 0;
done:
    free(pos); free(pm); return rc;
}

/* ------------------------------------------------------------------ thread pool: largest items first */
typedef struct { void **items; int n, next; pthread_mutex_t mu; void *(*fn)(void *); } tpool;
static void *tp_worker(void *a) {
    tpool *p = a;
    for (;;) { pthread_mutex_lock(&p->mu); int i = p->next++; pthread_mutex_unlock(&p->mu); if (i >= p->n) return NULL; p->fn(p->items[i]); }
}
static void tp_run(void *(*fn)(void *), void **items, int n, int threads) {
    if (threads > n) threads = n;
    if (threads <= 1) { for (int i = 0; i < n; i++) fn(items[i]); return; }
    tpool p = {items, n, 0, PTHREAD_MUTEX_INITIALIZER, fn}; pthread_t th[64]; int k = threads < 64 ? threads : 64, started = 0;
    for (int t = 0; t < k; t++) if (!pthread_create(&th[t], NULL, tp_worker, &p)) started++;
    if (!started) { for (int i = 0; i < n; i++) fn(items[i]); return; }
    for (int t = 0; t < k; t++) pthread_join(th[t], NULL);   /* (k == started unless creation failed) */
}
static size_t rawsize_of(const void *x, int dec) { return dec ? ((const djob *)x)->rawn : ((const stream *)x)->data.n; }
static void sort_by_size(void **v, int n, int dec) {          /* insertion sort, descending */
    for (int i = 1; i < n; i++) { void *x = v[i]; int j = i; while (j > 0 && rawsize_of(v[j - 1], dec) < rawsize_of(x, dec)) { v[j] = v[j - 1]; j--; } v[j] = x; }
}

/* ------------------------------------------------------------------ frame */
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint64_t get64(const uint8_t *p) { uint64_t v = 0; for (int i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }
enum { HDR = 4 + 1 + 2 + 8, PER = 8 + 8 + 1 };
static void free_streams(stream *st, int ns) { for (int i = 0; i < ns; i++) { free(st[i].data.p); free(st[i].ctx.p); free(st[i].out.p); } free(st); }

static uint8_t *kfam_compress(const uint8_t *s, size_t n, int fam, int threads, size_t *outn, int *famout, int *nsout) {
    pthread_once(&ktab_once, ktab_init);
    stream *st = NULL; int ns = 0;
    if (fam == F_FASTQ || (fam < 0 && n && s[0] == '@')) {
        st = calloc(5, sizeof *st); ns = 5;
        if (st) { st[2].kind = K_DNA; st[4].kind = K_QUAL; }
        if (!st || fastq_split(s, n, st)) { if (st) free_streams(st, ns); st = NULL; if (fam == F_FASTQ) return NULL; } else fam = F_FASTQ;
    }
    if (!st && (fam == F_CSV || fam < 0)) {
        if (csv_split(s, n, &st, &ns)) { if (st) free_streams(st, ns); st = NULL; if (fam == F_CSV) return NULL; } else fam = F_CSV;
    }
    if (!st) { fam = F_ONE; st = calloc(1, sizeof *st); ns = 1; if (!st || bput(&st[0].data, s, n)) return NULL; }
    void **items = malloc(sizeof(void *) * (size_t)ns);
    for (int i = 0; i < ns; i++) items[i] = &st[i];
    sort_by_size(items, ns, 0);
    tp_run(enc_stream, items, ns, threads);
    free(items);
    size_t tot = HDR + (size_t)ns * PER; int err = 0;
    for (int i = 0; i < ns; i++) { tot += st[i].out.n; err |= st[i].err; }
    uint8_t *o = err ? NULL : malloc(tot);
    if (o) {
        memcpy(o, "KFM1", 4); o[4] = (uint8_t)fam; o[5] = (uint8_t)ns; o[6] = (uint8_t)(ns >> 8); put64(o + 7, n);
        size_t q = HDR;
        for (int i = 0; i < ns; i++) { put64(o + q, st[i].data.n); put64(o + q + 8, st[i].out.n); o[q + 16] = (uint8_t)st[i].kind; q += PER; }
        for (int i = 0; i < ns; i++) { memcpy(o + q, st[i].out.p, st[i].out.n); q += st[i].out.n; }
        *outn = tot; *famout = fam; *nsout = ns;
    }
    free_streams(st, ns);
    return o;
}
static uint8_t *kfam_decompress(const uint8_t *c, size_t cn, int threads, size_t *outn) {
    pthread_once(&ktab_once, ktab_init);
    if (cn < HDR || memcmp(c, "KFM1", 4)) return NULL;
    int fam = c[4], ns = c[5] | c[6] << 8; uint64_t n = get64(c + 7);
    if (!ns || cn < HDR + (size_t)ns * PER || fam > F_CSV) return NULL;
    stream *st = calloc((size_t)ns, sizeof *st); djob *jb = calloc((size_t)ns, sizeof *jb);
    namectx *nc = calloc(1, sizeof *nc); numctx *uc = calloc((size_t)ns, sizeof *uc);
    uint8_t *res = NULL; buf out = {0};
    if (!st || !jb || !nc || !uc) goto done;
    size_t q = HDR + (size_t)ns * PER;
    for (int i = 0; i < ns; i++) {
        const uint8_t *e = c + HDR + (size_t)i * PER; uint64_t rn = get64(e), zn = get64(e + 8);
        /* Every symbol costs at least 2 binary decisions of at least -log2(65503/65536) bits each (probabilities are
         * clamped), so a valid stream holds fewer than 5,500 symbols per compressed byte: a larger raw length is damage,
         * refused before its buffer is allocated. */
        if (zn > cn - q || rn > (1ull << 40) || rn / 8192 > zn + 16 || e[16] > K_QUAL) goto done;
        st[i].kind = e[16]; jb[i] = (djob){&st[i], c + q, (size_t)zn, (size_t)rn, NULL, NULL}; q += zn;
    }
    dec_stream(&jb[0]);                                   /* the layout stream first: it says how to read the rest */
    if (st[0].err) goto done;
    if (fam == F_FASTQ) { if (ns != 5) goto done; jb[1].ctxfn = name_ctx; jb[1].ctxarg = nc; }
    if (fam == F_CSV) {
        const uint8_t *L = st[0].data.p; size_t li = 1, ln = st[0].data.n; uint64_t ncol, rows;
        if (ln < 1 || getvar(L, ln, &li, &ncol) || getvar(L, ln, &li, &rows) || ncol + 1 != (uint64_t)ns || li + ncol > ln) goto done;
        for (uint64_t k = 0; k < ncol; k++) if (L[li + k] != 0) { jb[1 + k].ctxfn = num_ctx; jb[1 + k].ctxarg = &uc[1 + k]; }
    }
    void **items = malloc(sizeof(void *) * (size_t)ns); if (!items) goto done;
    for (int i = 1; i < ns; i++) items[i - 1] = &jb[i];
    sort_by_size(items, ns - 1, 1);
    tp_run(dec_stream, items, ns - 1, threads);
    free(items);
    for (int i = 0; i < ns; i++) if (st[i].err) goto done;
    if (fam == F_ONE) { if (ns != 1) goto done; out.p = st[0].data.p; out.n = st[0].data.n; st[0].data.p = NULL; }
    else if ((fam == F_FASTQ ? fastq_join(st, &out) : csv_join(st, ns, &out))) goto done;
    if (out.n != n) goto done;
    res = out.p; out.p = NULL; *outn = (size_t)n;
done:
    free(out.p); if (st) for (int i = 0; i < ns; i++) { free(st[i].data.p); } free(st); free(jb); free(nc); free(uc);
    return res;
}

/* ------------------------------------------------------------------ command line */
static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); long l = ftell(f); rewind(f);
    uint8_t *b = malloc(l > 0 ? (size_t)l : 1);
    if (!b || fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)l; return b;
}
static int spit(const char *path, const uint8_t *b, size_t n) {
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return -1; }
    int bad = fwrite(b, 1, n, f) != n; if (fclose(f)) bad = 1; if (bad) perror(path); return bad ? -1 : 0;
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static const char *famname[] = {"one", "fastq", "csv"};

int main(int argc, char **argv) {
    int dec = 0, bench = 0, threads = 1, fam = -1, a = 1;
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        if (!strcmp(argv[a], "-d")) dec = 1; else if (!strcmp(argv[a], "-b")) bench = 1;
        else if (!strcmp(argv[a], "-T") && a + 1 < argc) threads = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-f") && a + 1 < argc) { a++; fam = !strcmp(argv[a], "fastq") ? F_FASTQ : !strcmp(argv[a], "csv") ? F_CSV : !strcmp(argv[a], "one") ? F_ONE : -1; }
        else { fprintf(stderr, "kfam: unknown option %s\n", argv[a]); return 2; }
    }
    if (argc - a != (bench ? 1 : 2)) { fprintf(stderr, "usage: kfam [-T threads] [-f auto|fastq|csv|one] in out | -d in out | -b in\n"); return 2; }
    size_t n; uint8_t *s = slurp(argv[a], &n); if (!s) return 1;
    if (dec) {
        size_t m; uint8_t *o = kfam_decompress(s, n, threads, &m); free(s);
        if (!o) { fprintf(stderr, "kfam: corrupt input\n"); return 1; }
        int r = spit(argv[a + 1], o, m) ? 1 : 0; free(o); return r;
    }
    size_t z = 0; int ff = 0, ns = 0; uint8_t *c = NULL; double ce = 1e30, de = 1e30, t0 = now(); int k = 0;
    do { free(c); double t = now(); c = kfam_compress(s, n, fam, threads, &z, &ff, &ns); double e = now() - t; if (e < ce) ce = e; k++; }
    while (bench && c && (k < 3 || now() - t0 < 1.0));
    if (!c) { fprintf(stderr, "kfam: the input does not fit family %s\n", fam >= 0 ? famname[fam] : "auto"); free(s); return 1; }
    size_t m = 0; uint8_t *back = NULL; t0 = now(); k = 0;
    do { free(back); double t = now(); back = kfam_decompress(c, z, threads, &m); double e = now() - t; if (e < de) de = e; k++; }
    while (bench && back && (k < 3 || now() - t0 < 1.0));
    int ok = back && m == n && !memcmp(back, s, n);
    free(back); free(s);
    if (!ok) { fprintf(stderr, "kfam: round trip failed\n"); free(c); return 1; }
    if (bench) {
        printf("{\"family\": \"%s\", \"streams\": %d, \"threads\": %d, \"input\": %zu, \"compressed\": %zu, \"compress_mbs\": %.3f, \"decompress_mbs\": %.3f, \"stream_sizes\": [",
               famname[ff], ns, threads, n, z, n / ce / 1e6, n / de / 1e6);
        for (int i = 0; i < ns && i < 12; i++) printf("%s[%llu, %llu]", i ? ", " : "", (unsigned long long)get64(c + HDR + (size_t)i * PER), (unsigned long long)get64(c + HDR + (size_t)i * PER + 8));
        printf("%s]}\n", ns > 12 ? ", ..." : "");
        free(c); return 0;
    }
    int r = spit(argv[a + 1], c, z) ? 1 : 0; free(c); return r;
}
