/* GR6+ blocks (codec 26, container version 11): structure transform + 6-bit units + context mixing, in one codec.
 * Ported from branch claude/dazzling-maxwell-p8y4u0 (commit 12f0c0e), where it was codec 15 / version 9 in
 * src/gr6.inc; the numbering changed for rc3 (15 is the native plane leaf, 9-10 are taken), the coded bytes did not
 * (apart from the line count, now written little-endian on every host).
 *
 * Line-structured text (tables, logs, delimited records) is split into fields at the block's separator. Each field is
 * one decision (op) followed by the values that op needs; every value is written as 6-bit units, and every unit is
 * coded as 6 binary decisions by a context-mixing model (8 hashed context models, two integer logistic mixers, an
 * APM) and a binary arithmetic coder. Integer arithmetic only, so encoder and decoder agree on every platform.
 *
 * Ops: DITTO (field equals the one above), EMPTY, NUM (residual from the column's best predictor: derived-column
 * formula, delta, delta of delta, damped trend), RANK (column move-to-front list), RANK2 (list of fields that followed
 * the previous field), CDICT (column dictionary), GID (global id), NUMNEW (number in a new format), NEW (prefix shared
 * with a reference field, then characters). Formulas are found by the encoder on a row sample and sent in the header:
 * 0 round(a/b*10^k), 1 a-lag(a), 2 100*(a/lag(a)-1), 3 lag(t)+a, 4 a, 5 a*b/10^k (exact integer arithmetic).
 *
 * Payload: version u8 (1) | separator u8 | final newline u8 | formulas u8 | lines u64 | table[62] |
 *          formula * (t u16, a u16, b u16, k u8, type u8) | arithmetic-coded bytes
 * Format: SPEC.md, "GR6+ blocks". Research notes and measurements: research/sixbit-study/FINDINGS.md sections 5-7;
 * rc3 measurements: benchmark/rc3/gr6/RESULTS.md.
 *
 * The encoder tries it in ultra on text blocks (core/blocks.c) and keeps it only when it is smaller than the block's
 * other candidates. It is decoded at block level (core/decode.c), not through the registry, so a graph leaf cannot
 * name it. */
#include <setjmp.h>

enum { G6K_NF, G6K_OP, G6K_RANK, G6K_RANK2, G6K_CDICT, G6K_GID, G6K_NUM, G6K_NUMNEW, G6K_CHAR, G6K_PFX, G6_NKIND };
enum { G6_DITTO, G6_EMPTY, G6_NUM, G6_RANK, G6_RANK2, G6_CDICT, G6_GID, G6_NUMNEW, G6_NEW, G6_NOP };
#define G6_MAXC 4096
#define G6_COLCAP 256
#define G6_PREVCAP 256
#define G6_NM 8
#define G6_MAXFORM 255
#define G6_MAXCOLS 100              /* wider tables are not tried (gr6_applies) */
#define G6_WORK_CAP ((int64_t)1 << 31)   /* rows scored by the formula search (g6_formulas); 2^29 cut owid_test short */

/* ---- tracked allocation: everything is freed at the end; failure (or corrupt input) unwinds with longjmp */
typedef struct { void **p; size_t n, cap; jmp_buf jb; } g6_heap;
static void g6_fail(g6_heap *h, int code) { longjmp(h->jb, code); }
static void *g6_alloc(g6_heap *h, size_t size) {
    if (h->n == h->cap) {
        size_t nc = h->cap * 2 + 256; void **np = realloc(h->p, nc * sizeof(void *));
        if (!np) g6_fail(h, 1);
        h->p = np; h->cap = nc;
    }
    uint8_t *b = kolmo_alloc_fail() ? NULL : malloc(size + 16);
    if (!b) g6_fail(h, 1);
    size_t idx = h->n; memcpy(b, &idx, sizeof idx); h->p[h->n++] = b;
    return b + 16;
}
static void *g6_realloc(g6_heap *h, void *old, size_t size) {
    if (!old) return g6_alloc(h, size);
    uint8_t *b = (uint8_t *)old - 16; size_t idx; memcpy(&idx, b, sizeof idx);
    uint8_t *nb = kolmo_alloc_fail() ? NULL : realloc(b, size + 16);
    if (!nb) g6_fail(h, 1);
    h->p[idx] = nb; return nb + 16;
}
static void g6_free_all(g6_heap *h) { for (size_t i = 0; i < h->n; i++) free(h->p[i]); free(h->p); h->p = NULL; h->n = h->cap = 0; }

typedef struct { uint8_t *p; size_t n, cap; } g6_buf;
static void g6_put(g6_heap *h, g6_buf *b, const void *s, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = g6_realloc(h, b->p, b->cap); }
    memcpy(b->p + b->n, s, n); b->n += n;
}

/* ---- string interning (id = order of first insertion) */
typedef struct { uint32_t *slot; uint32_t mask, count, cap; uint64_t *off; uint32_t *len, *hash; g6_buf arena; } g6_intern;
static uint32_t g6_hash_bytes(const uint8_t *s, size_t n) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ n;
    while (n >= 8) { uint64_t v; memcpy(&v, s, 8); h = (h ^ v) * 0xff51afd7ed558ccdull; h ^= h >> 32; s += 8; n -= 8; }
    uint64_t v = 0; memcpy(&v, s, n); h = (h ^ v) * 0xc4ceb9fe1a85ec53ull; h ^= h >> 29;
    return (uint32_t)h;
}
static void g6_intern_init(g6_heap *H, g6_intern *t) {
    memset(t, 0, sizeof *t); t->mask = (1u << 16) - 1; t->slot = g6_alloc(H, sizeof(uint32_t) << 16);
    memset(t->slot, 0xff, sizeof(uint32_t) << 16);
}
static uint32_t g6_intern_get(g6_heap *H, g6_intern *t, const uint8_t *s, size_t n, int insert) {
    uint32_t h = g6_hash_bytes(s, n), i = h & t->mask;
    for (;;) {
        uint32_t id = t->slot[i];
        if (id == UINT32_MAX) break;
        if (t->hash[id] == h && t->len[id] == n && memcmp(t->arena.p + t->off[id], s, n) == 0) return id;
        i = (i + 1) & t->mask;
    }
    if (!insert) return UINT32_MAX;
    if (t->count == t->cap) {
        t->cap = t->cap * 2 + 1024;
        t->off = g6_realloc(H, t->off, t->cap * sizeof *t->off); t->len = g6_realloc(H, t->len, t->cap * sizeof *t->len);
        t->hash = g6_realloc(H, t->hash, t->cap * sizeof *t->hash);
    }
    uint32_t id = t->count++;
    t->off[id] = t->arena.n; t->len[id] = (uint32_t)n; t->hash[id] = h;
    if (n) g6_put(H, &t->arena, s, n); else if (!t->arena.p) { t->arena.cap = 4096; t->arena.p = g6_alloc(H, 4096); }
    t->slot[i] = id;
    if (t->count * 2 > t->mask) {
        uint32_t nm = t->mask * 2 + 1; uint32_t *ns = g6_alloc(H, sizeof(uint32_t) * ((size_t)nm + 1));
        memset(ns, 0xff, sizeof(uint32_t) * ((size_t)nm + 1));
        for (uint32_t k = 0; k < t->count; k++) { uint32_t q = t->hash[k] & nm; while (ns[q] != UINT32_MAX) q = (q + 1) & nm; ns[q] = k; }
        t->slot = ns; t->mask = nm;   /* the old table stays tracked and is freed at the end */
    }
    return id;
}
static inline const uint8_t *g6_str(const g6_intern *t, uint32_t id) { return t->arena.p + t->off[id]; }

/* ---- 64-bit key -> u32 map */
typedef struct { uint64_t *k; uint32_t *v; uint64_t mask, count; } g6_map;
static void g6_map_init(g6_heap *H, g6_map *m, int bits) {
    m->mask = (1ull << bits) - 1; m->count = 0;
    m->k = g6_alloc(H, sizeof(uint64_t) << bits); m->v = g6_alloc(H, sizeof(uint32_t) << bits);
    memset(m->k, 0xff, sizeof(uint64_t) << bits);
}
static inline uint64_t g6_mix64(uint64_t x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdull; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ull; x ^= x >> 33; return x; }
static uint32_t *g6_map_slot(g6_heap *H, g6_map *m, uint64_t key, int insert) {
    for (;;) {
        uint64_t i = g6_mix64(key) & m->mask;
        for (;;) {
            if (m->k[i] == key) return &m->v[i];
            if (m->k[i] == UINT64_MAX) break;
            i = (i + 1) & m->mask;
        }
        if (!insert) return NULL;
        if ((m->count + 1) * 2 <= m->mask) { m->k[i] = key; m->v[i] = UINT32_MAX; m->count++; return &m->v[i]; }
        g6_map n; int bits = 0; while ((1ull << bits) <= m->mask) bits++;
        g6_map_init(H, &n, bits + 1);
        for (uint64_t j = 0; j <= m->mask; j++) if (m->k[j] != UINT64_MAX) {
            uint64_t q = g6_mix64(m->k[j]) & n.mask; while (n.k[q] != UINT64_MAX) q = (q + 1) & n.mask;
            n.k[q] = m->k[j]; n.v[q] = m->v[j]; n.count++;
        }
        *m = n;
    }
}

/* ---- move-to-front lists */
typedef struct { uint32_t *a; uint16_t n, cap; } g6_mtf;
static inline int g6_find(const g6_mtf *l, uint32_t id) { for (int i = 0; i < l->n; i++) if (l->a[i] == id) return i; return -1; }
static void g6_front(g6_heap *H, g6_mtf *l, uint32_t id, int pos, int limit) {
    if (pos < 0) {
        if (l->n < limit) {
            if (l->n == l->cap) { l->cap = l->cap ? (l->cap * 2 > limit ? limit : l->cap * 2) : 4; l->a = g6_realloc(H, l->a, (size_t)l->cap * 4); }
            l->n++;
        }
        pos = l->n - 1;
    }
    memmove(l->a + 1, l->a, (size_t)pos * 4); l->a[0] = id;
}

/* ---- numbers (wrapping arithmetic: crafted input must not overflow) */
typedef struct { uint8_t d, w; } g6_fmt;
static inline int64_t g6_add(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }
static inline int64_t g6_sub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a - (uint64_t)b); }
static inline uint64_t g6_zz(int64_t x) { return ((uint64_t)x << 1) ^ (uint64_t)(x >> 63); }
static inline int64_t g6_unzz(uint64_t z) { return (int64_t)(z >> 1) ^ -(int64_t)(z & 1); }
static inline int g6_bitlen(uint64_t x) { return x ? 64 - __builtin_clzll(x) : 0; }
static int g6_render(int64_t v, g6_fmt f, uint8_t *out) {   /* at most 1 + 18 + 20 + 1 + 18 bytes */
    char dg[48]; int nd = 0; uint64_t a = v < 0 ? -(uint64_t)v : (uint64_t)v;
    do { dg[nd++] = (char)('0' + a % 10); a /= 10; } while (a);
    while (nd < f.d + 1) dg[nd++] = '0';
    int ni = nd - f.d, o = 0;
    if (v < 0) out[o++] = '-';
    for (int i = ni; i < f.w; i++) out[o++] = '0';
    for (int i = nd - 1; i >= f.d; i--) out[o++] = (uint8_t)dg[i];
    if (f.d) { out[o++] = '.'; for (int i = f.d - 1; i >= 0; i--) out[o++] = (uint8_t)dg[i]; }
    return o;
}
static int g6_parse(const uint8_t *s, size_t n, int64_t *v, g6_fmt *f) {
    if (n == 0 || n > 40) return 0;
    size_t i = 0; int neg = 0; if (s[0] == '-') { neg = 1; i = 1; }
    size_t is = i; while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    size_t ni = i - is; if (ni == 0) return 0;
    size_t d = 0; if (i < n && s[i] == '.') { i++; size_t fs = i; while (i < n && s[i] >= '0' && s[i] <= '9') i++; d = i - fs; if (d == 0) return 0; }
    if (i != n || ni + d > 18) return 0;
    int64_t x = 0; for (size_t k = is; k < n; k++) if (s[k] != '.') x = x * 10 + (s[k] - '0');
    f->d = (uint8_t)d; f->w = (uint8_t)((ni > 1 && s[is] == '0') ? ni : 0); *v = neg ? -x : x;
    uint8_t r[64]; int rn = g6_render(*v, *f, r);
    return (size_t)rn == n && memcmp(r, s, n) == 0;
}
static int g6_ratio(int64_t va, int da, int64_t vb, int db, int ke, int dt, int64_t *out) {   /* round(a/b * 10^(ke+dt)) */
    int e = db + dt + ke; if (vb == 0 || e > 18 || da > 18 || e < 0) return 0;
    if (va > (1LL << 60) || va < -(1LL << 60) || vb > (1LL << 60) || vb < -(1LL << 60)) return 0;
    __int128 num = (__int128)va * P10[e], den = (__int128)vb * P10[da];
    int neg = (num < 0) != (den < 0); if (num < 0) num = -num; if (den < 0) den = -den;
    __int128 q = (2 * num + den) / (2 * den);
    if (q > ((__int128)1 << 62)) return 0;
    *out = neg ? -(int64_t)q : (int64_t)q; return 1;
}
static int g6_rescale(int64_t v, int from, int to, int64_t *out) {
    if (to >= from) { int e = to - from; if (e > 18) return 0; int64_t lim = INT64_MAX / P10[e] / 2; if (v > lim || v < -lim) return 0; *out = v * P10[e]; return 1; }
    int e = from - to; if (e > 18 || v == INT64_MIN) return 0; int64_t q = P10[e], a = v < 0 ? -v : v, r = (a + q / 2) / q; *out = v < 0 ? -r : r; return 1;
}
static int g6_fpred(int type, int ke, int64_t va, int da, int okl, int64_t vla, int dla, int64_t vb, int db,
                    int oklt, int64_t vlt, int dlt, int dt, int64_t *out) {
    switch (type) {
    case 0: return g6_ratio(va, da, vb, db, ke, dt, out);
    case 1: return okl && da == dla && g6_rescale(g6_sub(va, vla), da, dt, out);
    case 2: return okl && da == dla && g6_ratio(g6_sub(va, vla), da, vla, da, 2, dt, out);
    case 3: { int64_t r; if (!oklt || dlt != dt || !g6_rescale(va, da, dt, &r)) return 0; *out = g6_add(vlt, r); return 1; }
    case 4: return g6_rescale(va, da, dt, out);
    case 5: {
        if (va > (1LL << 40) || va < -(1LL << 40) || vb > (1LL << 40) || vb < -(1LL << 40)) return 0;
        __int128 m = (__int128)va * vb; int e = da + db + ke - dt; if (e < 0 || e > 18) return 0;
        __int128 q = P10[e], am = m < 0 ? -m : m, r = (am + q / 2) / q; if (r > ((__int128)1 << 62)) return 0;
        *out = m < 0 ? -(int64_t)r : (int64_t)r; return 1; }
    }
    return 0;
}

/* ---- context-mixing coder over 6-bit units (integer only) */
typedef struct { uint32_t x1, x2, x; g6_buf *out; const uint8_t *in; size_t pos, n, pad; } g6_ac;   /* pad: decoder reads past n */
typedef struct {
    uint32_t *T[G6_NM]; int32_t (*W)[G6_NM + 1], (*W2)[G6_NM + 1]; uint16_t (*apm)[33];
    uint32_t rcp[1024]; uint32_t base[G6_NM]; uint32_t *slot[G6_NM]; int st[G6_NM + 1]; int tb;
} g6_cm;
typedef struct { int kind, c, uidx, prevu, aux, sel2; uint32_t h[G6_NM]; } g6_ctx;
typedef struct { int enc; g6_ac ac; g6_cm m; g6_heap *H; } g6_io;

static inline uint32_t g6_H(uint32_t a, uint32_t b) { uint32_t h = (a ^ 0x9E3779B9u) * 0x85EBCA6Bu ^ (b + 0x632BE59Bu) * 0xC2B2AE35u; return h ^ (h >> 15); }
static inline int g6_sq(int d) { return cm_sq[(d < -2047 ? -2047 : d > 2047 ? 2047 : d) + 2048]; }

static void g6_cm_init(g6_heap *H, g6_cm *m, uint64_t n) {
    m->tb = n < (1u << 20) ? 20 : 22;
    for (int i = 0; i < G6_NM; i++) {
        m->T[i] = g6_alloc(H, sizeof(uint32_t) << m->tb);
        for (size_t j = 0; j < ((size_t)1 << m->tb); j++) m->T[i][j] = 1u << 31;
    }
    m->W = g6_alloc(H, sizeof(*m->W) * G6_NKIND * 4 * 64); m->W2 = g6_alloc(H, sizeof(*m->W2) * G6_NKIND * 16 * 64);
    for (int i = 0; i < G6_NKIND * 4 * 64; i++) for (int j = 0; j <= G6_NM; j++) m->W[i][j] = 19661;     /* 0.3 */
    for (int i = 0; i < G6_NKIND * 16 * 64; i++) for (int j = 0; j <= G6_NM; j++) m->W2[i][j] = 19661;
    m->apm = g6_alloc(H, sizeof(*m->apm) * G6_NKIND * 64 * 64);
    for (int j = 0; j < 33; j++) { int v = cm_squash_i(j * 128 - 2048) * 16; m->apm[0][j] = (uint16_t)(v > 65535 ? 65535 : v); }
    for (int i = 1; i < G6_NKIND * 64 * 64; i++) memcpy(m->apm[i], m->apm[0], sizeof m->apm[0]);
    for (int i = 0; i < 1024; i++) m->rcp[i] = (uint32_t)(655360 / (10 * i + 16));
}

static int g6_bit(g6_io *io, g6_ctx *x, int node, int bit) {
    g6_cm *m = &io->m; int64_t dot = 0, dot2 = 0;
    int32_t *w = m->W[(x->kind * 4 + (x->uidx < 3 ? x->uidx : 3)) * 64 + node], *w2 = m->W2[(x->kind * 16 + x->sel2) * 64 + node];
    for (int i = 0; i < G6_NM; i++) { m->slot[i] = &m->T[i][m->base[i] | (uint32_t)node]; m->st[i] = cm_st[*m->slot[i] >> 20]; }
    m->st[G6_NM] = 64;
    for (int i = 0; i <= G6_NM; i++) { dot += (int64_t)w[i] * m->st[i]; dot2 += (int64_t)w2[i] * m->st[i]; }
    int d1 = (int)(dot >> 16 < -2047 ? -2047 : dot >> 16 > 2047 ? 2047 : dot >> 16), d2 = (int)(dot2 >> 16 < -2047 ? -2047 : dot2 >> 16 > 2047 ? 2047 : dot2 >> 16);
    int pm1 = g6_sq(d1), pm2 = g6_sq(d2), pm = g6_sq((cm_st[pm1] + cm_st[pm2]) / 2);
    int s = cm_st[pm] + 2048, lo = s >> 7, wt = s & 127; if (lo > 31) { lo = 31; wt = 127; }
    uint16_t *a = m->apm[(x->kind * 64 + x->c) * 64 + node];
    int pa = (a[lo] * (128 - wt) + a[lo + 1] * wt) >> 11;
    int p = (pm + 3 * pa) >> 2; if (p < 1) p = 1; if (p > 4095) p = 4095;
    g6_ac *c = &io->ac;
    uint32_t xm = c->x1 + (uint32_t)(((uint64_t)(c->x2 - c->x1) * (uint32_t)p) >> 12);
    if (io->enc) {
        if (bit) c->x2 = xm; else c->x1 = xm + 1;
        while (((c->x1 ^ c->x2) & 0xff000000u) == 0) { uint8_t b = (uint8_t)(c->x2 >> 24); g6_put(io->H, c->out, &b, 1); c->x1 <<= 8; c->x2 = (c->x2 << 8) | 255; }
    } else {
        bit = c->x <= xm;
        if (bit) c->x2 = xm; else c->x1 = xm + 1;
        while (((c->x1 ^ c->x2) & 0xff000000u) == 0) { c->x1 <<= 8; c->x2 = (c->x2 << 8) | 255; c->x = (c->x << 8) | (c->pos < c->n ? c->in[c->pos++] : (c->pad++, 0)); }
    }
    for (int i = 0; i < G6_NM; i++) {
        uint32_t v = *m->slot[i], cnt = v & 1023, pr = v >> 10;
        int32_t tgt = bit ? (1 << 22) - 1 : 0;
        pr = (uint32_t)((int32_t)pr + (int32_t)(((int64_t)(tgt - (int32_t)pr) * m->rcp[cnt]) >> 16));
        if (cnt < 255) cnt++;
        *m->slot[i] = pr << 10 | cnt;
    }
    int e1 = (bit << 12) - pm1, e2 = (bit << 12) - pm2;
    for (int i = 0; i <= G6_NM; i++) { w[i] += (int32_t)(((int64_t)e1 * m->st[i] * 9) >> 13); w2[i] += (int32_t)(((int64_t)e2 * m->st[i] * 9) >> 13); }
    int g = bit ? 65535 : 0;
    a[lo] = (uint16_t)(a[lo] + ((g - a[lo]) * (128 - wt) >> 13));
    a[lo + 1] = (uint16_t)(a[lo + 1] + ((g - a[lo + 1]) * wt >> 13));
    return bit;
}
static int g6_unit(g6_io *io, g6_ctx *x, int u) {
    g6_cm *m = &io->m;
    uint32_t uh = (uint32_t)x->aux << 16 | (uint32_t)x->uidx << 8 | (uint32_t)x->prevu;
    for (int i = 0; i < G6_NM; i++) m->base[i] = (g6_H(x->h[i], uh) >> (32 - m->tb)) & ~63u;
    int node = 1;
    for (int b = 5; b >= 0; b--) node = node * 2 + g6_bit(io, x, node, io->enc ? (u >> b) & 1 : 0);
    u = node - 64; x->prevu = u; x->uidx++;
    return u;
}
/* ops: 5 data bits + continuation per unit; other values: bit length, then the bits below the top bit, 6 per unit */
static uint64_t g6_val(g6_io *io, g6_ctx *x, uint64_t v) {
    x->uidx = 0; x->prevu = 64; x->aux = 0;
    if (x->kind != G6K_OP) {
        int bl = io->enc ? g6_bitlen(v) : 0;
        int u0 = g6_unit(io, x, bl > 62 ? 63 : bl);
        if (!io->enc) bl = u0;
        if (u0 == 63) {
            uint64_t r = 0; for (int s = 60; s >= 0; s -= 6) { int u = g6_unit(io, x, io->enc ? (int)((v >> s) & 63) : 0); r |= (uint64_t)u << s; }
            return io->enc ? v : r;
        }
        if (bl <= 1) return (uint64_t)bl;
        x->aux = bl; uint64_t r = 1; int left = bl - 1;
        while (left > 0) {
            int k = left < 6 ? left : 6; left -= k;
            int u = g6_unit(io, x, io->enc ? (int)((v >> left) & ((1u << k) - 1)) : 0);
            if (u >> k) g6_fail(io->H, 2);
            r = r << k | (uint64_t)u;
        }
        return io->enc ? v : r;
    }
    if (io->enc) { uint64_t t = v; while (t >= 32) { g6_unit(io, x, (int)(32 | (t & 31))); t >>= 5; } g6_unit(io, x, (int)t); return v; }
    uint64_t r = 0; int sh = 0;
    for (;;) { int u = g6_unit(io, x, 0); if (sh < 64) r |= (uint64_t)(u & 31) << sh; sh += 5; if (!(u & 32)) return r; if (sh > 70) g6_fail(io->H, 2); }
}

/* ---- model shared by encoder and decoder */
typedef struct { int t, a, b, ke, type; } g6_formula;
typedef struct {
    g6_mtf list; uint32_t *dict; uint32_t nd, capd;
    int64_t last, last2; g6_fmt fmt; int has; uint32_t cost[4]; int sel;
    int fa, fb, fke, ftype; uint8_t ophist, lastrb[G6_NKIND], lastres;
} g6_col;
typedef struct {
    g6_heap *H; g6_intern tok; g6_map cdmap, pmap; g6_mtf *plists; size_t npl, cappl; g6_col *col;
    uint32_t *aid, *cid; uint8_t *aop, *aaop, *cop, *cres; int64_t *cv; g6_fmt *cf; uint8_t *cok;
    size_t pnf, ppnf, cap; uint8_t table[62]; int code[256];
} g6_model;

static void g6_model_init(g6_heap *H, g6_model *M) {
    memset(M, 0, sizeof *M); M->H = H; g6_intern_init(H, &M->tok); g6_map_init(H, &M->cdmap, 16); g6_map_init(H, &M->pmap, 16);
    M->col = g6_alloc(H, sizeof(g6_col) * G6_MAXC); memset(M->col, 0, sizeof(g6_col) * G6_MAXC);
    for (int i = 0; i < G6_MAXC; i++) M->col[i].fa = -1;
    g6_intern_get(H, &M->tok, (const uint8_t *)"", 0, 1);       /* id 0: the empty field */
}
static void g6_cap(g6_model *M, size_t nf) {
    if (nf <= M->cap) return;
    size_t c = nf * 2 + 64; g6_heap *H = M->H;
    M->aid = g6_realloc(H, M->aid, c * 4); M->cid = g6_realloc(H, M->cid, c * 4); M->aop = g6_realloc(H, M->aop, c);
    M->aaop = g6_realloc(H, M->aaop, c); M->cop = g6_realloc(H, M->cop, c); M->cres = g6_realloc(H, M->cres, c);
    M->cv = g6_realloc(H, M->cv, c * 8); M->cf = g6_realloc(H, M->cf, c * sizeof(g6_fmt)); M->cok = g6_realloc(H, M->cok, c); M->cap = c;
}
static g6_mtf *g6_plist(g6_model *M, uint32_t key, int insert) {
    uint32_t *v = g6_map_slot(M->H, &M->pmap, key, insert); if (!v) return NULL;
    if (*v == UINT32_MAX) {
        if (M->npl == M->cappl) { M->cappl = M->cappl * 2 + 1024; M->plists = g6_realloc(M->H, M->plists, M->cappl * sizeof(g6_mtf)); }
        memset(&M->plists[M->npl], 0, sizeof(g6_mtf)); *v = (uint32_t)M->npl++;
    }
    return &M->plists[*v];
}
static int g6_predict(g6_model *M, g6_col *C, int64_t pr[4]) {
    int avail = 0;
    if (C->has >= 1) {
        pr[1] = C->last; pr[2] = C->has >= 2 ? g6_sub(g6_add(C->last, C->last), C->last2) : C->last;
        pr[3] = C->has >= 2 ? g6_add(C->last, g6_sub(C->last, C->last2) / 2) : C->last; avail |= 14;
    }
    if (C->fa >= 0 && C->has >= 1 && M->cok[C->fa] && M->cok[C->fb]) {
        g6_col *A = &M->col[C->fa];
        if (g6_fpred(C->ftype, C->fke, M->cv[C->fa], M->cf[C->fa].d, A->has >= 2, A->last2, A->fmt.d,
                     M->cv[C->fb], M->cf[C->fb].d, 1, C->last, C->fmt.d, C->fmt.d, &pr[0])) avail |= 1;
    }
    return avail;
}
static int g6_choose(const g6_col *C, int avail) {
    int best = -1; for (int p = 0; p < 4; p++) if (avail >> p & 1) if (best < 0 || C->cost[p] < C->cost[best]) best = p;
    return best;
}
static void g6_set(g6_model *M, g6_ctx *x, int kind, int f) {
    g6_col *C = &M->col[f < G6_MAXC ? f : G6_MAXC - 1];
    int c = f < 63 ? f : 63;
    int aop = f < (int)M->pnf ? M->aop[f] : 15, aaop = f < (int)M->ppnf ? M->aaop[f] : 15, lop = f ? M->cop[f - 1] : 14;
    uint32_t aid = f < (int)M->pnf ? M->aid[f] : 0xFFFFFFFu, extra = 0, extra2 = 0;
    switch (kind) {
    case G6K_OP: extra = C->ophist; extra2 = (uint32_t)(lop | aop << 4 | (f == 0) << 8); break;
    case G6K_NUM: extra = (uint32_t)(C->sel | C->lastres << 3); extra2 = (uint32_t)((f ? M->cres[f - 1] : 0) | C->sel << 8); break;
    case G6K_NUMNEW: extra = (uint32_t)(C->fmt.d | C->fmt.w << 5); break;
    case G6K_NF: extra = (uint32_t)(M->pnf & 1023); break;
    default: extra = C->lastrb[kind]; extra2 = (uint32_t)lop; break;
    }
    uint32_t k = (uint32_t)(kind << 6 | c);
    x->kind = kind; x->c = c; x->sel2 = aop;
    x->h[0] = g6_H(k, 1);
    x->h[1] = g6_H(g6_H(k, 2), (uint32_t)(aop | lop << 4));
    x->h[2] = g6_H(g6_H(k, 3), aid);
    x->h[3] = g6_H(g6_H(k, 4), extra);
    x->h[4] = g6_H(g6_H((uint32_t)kind, 5), extra2);
    x->h[5] = g6_H(g6_H(k, 6), (uint32_t)(aop | aaop << 4 | lop << 8 | (f ? M->cop[0] == G6_DITTO : 2) << 12));
    x->h[6] = g6_H(g6_H(k, 7), f ? M->cid[f - 1] : 0xFFFFFFFu);
    x->h[7] = g6_H(g6_H(k, 12), (kind == G6K_NUM ? (uint32_t)(C->lastres | (f ? M->cres[f - 1] : 0) << 6 | C->sel << 12) : extra) | (uint32_t)aop << 16);
}

/* one field. Encoder: s/n given. Decoder: the field is appended to out (bounded by olen) */
static void g6_field(g6_model *M, g6_io *io, int f, const uint8_t *s, size_t n, uint8_t *out, size_t *opos, size_t olen) {
    g6_heap *H = M->H; g6_col *C = &M->col[f < G6_MAXC ? f : G6_MAXC - 1];
    g6_ctx x; int op = 0; uint32_t id = 0; int64_t v = 0; g6_fmt fm = {0, 0}; int isnum = 0;
    int64_t pr[4] = {0, 0, 0, 0}; int avail = 0, sel = -1; uint8_t tmp[64];
    uint32_t pkey = f ? M->cid[f - 1] : 0xFFFFFFFEu;
    if (io->enc) {
        id = g6_intern_get(H, &M->tok, s, n, 0);
        isnum = g6_parse(s, n, &v, &fm);
        int pos = -1;
        if (f < (int)M->pnf && id != UINT32_MAX && M->aid[f] == id) op = G6_DITTO;
        else if (n == 0) op = G6_EMPTY;
        else if (isnum && C->has && fm.d == C->fmt.d && fm.w == C->fmt.w && (avail = g6_predict(M, C, pr)) && (sel = g6_choose(C, avail)) >= 0 &&
                 (v == pr[sel] || id == UINT32_MAX || (pos = g6_find(&C->list, id)) < 0 || pos >= 2)) op = G6_NUM;
        else if (id != UINT32_MAX && g6_find(&C->list, id) >= 0) op = G6_RANK;
        else if (id != UINT32_MAX) {
            g6_mtf *pl = g6_plist(M, pkey, 0);
            if (pl && g6_find(pl, id) >= 0) op = G6_RANK2;
            else if (g6_map_slot(H, &M->cdmap, (uint64_t)(f < G6_MAXC ? f : G6_MAXC - 1) << 32 | id, 0)) op = G6_CDICT;
            else op = G6_GID;
        } else op = isnum ? G6_NUMNEW : G6_NEW;
    }
    g6_set(M, &x, G6K_OP, f);
    uint64_t ov = g6_val(io, &x, (uint64_t)op);
    if (ov >= G6_NOP) g6_fail(H, 2);
    op = (int)ov;
    switch (op) {
    case G6_DITTO: if (f >= (int)M->pnf) g6_fail(H, 2); id = M->aid[f]; break;
    case G6_EMPTY: id = 0; break;
    case G6_NUM: {
        if (!io->enc) { if (!C->has) g6_fail(H, 2); avail = g6_predict(M, C, pr); sel = g6_choose(C, avail); if (sel < 0) g6_fail(H, 2); }
        g6_set(M, &x, G6K_NUM, f);
        uint64_t r = g6_val(io, &x, g6_zz(g6_sub(v, pr[sel])));
        if (!io->enc) { v = g6_add(pr[sel], g6_unzz(r)); fm = C->fmt; }
        isnum = 1; break;
    }
    case G6_RANK: case G6_RANK2: {
        g6_mtf *l = op == G6_RANK ? &C->list : g6_plist(M, pkey, 0);
        if (!l) g6_fail(H, 2);
        g6_set(M, &x, op == G6_RANK ? G6K_RANK : G6K_RANK2, f);
        uint64_t r = g6_val(io, &x, io->enc ? (uint64_t)g6_find(l, id) : 0);
        if (r >= l->n) g6_fail(H, 2);
        id = l->a[r]; C->lastrb[x.kind] = (uint8_t)(r < 63 ? r : 63); break;
    }
    case G6_CDICT: {
        g6_set(M, &x, G6K_CDICT, f);
        uint64_t i = io->enc ? *g6_map_slot(H, &M->cdmap, (uint64_t)(f < G6_MAXC ? f : G6_MAXC - 1) << 32 | id, 0) : 0;
        uint64_t r = g6_val(io, &x, io->enc ? C->nd - 1 - i : 0);
        if (r >= C->nd) g6_fail(H, 2);
        id = C->dict[C->nd - 1 - r]; break;
    }
    case G6_GID: {
        g6_set(M, &x, G6K_GID, f);
        uint64_t r = g6_val(io, &x, io->enc ? M->tok.count - 1 - (uint64_t)id : 0);
        if (r >= M->tok.count) g6_fail(H, 2);
        id = M->tok.count - 1 - (uint32_t)r; break;
    }
    case G6_NUMNEW: {
        g6_set(M, &x, G6K_NUMNEW, f);
        uint64_t d = g6_val(io, &x, fm.d), w = g6_val(io, &x, fm.w);
        if (d > 18 || w > 18) g6_fail(H, 2);
        fm.d = (uint8_t)d; fm.w = (uint8_t)w;
        uint64_t z = g6_val(io, &x, g6_zz(v)); if (!io->enc) v = g6_unzz(z);
        isnum = 1; break;
    }
    case G6_NEW: {
        g6_mtf *gl = g6_plist(M, pkey, 0);
        uint32_t ref = gl && gl->n ? gl->a[0] : C->list.n ? C->list.a[0] : 0;
        const uint8_t *rs = g6_str(&M->tok, ref); size_t rn = M->tok.len[ref];
        g6_set(M, &x, G6K_PFX, f);
        size_t L = 0; if (io->enc) while (L < n && L < rn && s[L] == rs[L]) L++;
        uint64_t lv = g6_val(io, &x, L); if (lv > rn) g6_fail(H, 2); L = (size_t)lv;
        g6_set(M, &x, G6K_CHAR, f);
        uint32_t hist = 0; size_t start = io->enc ? 0 : *opos, k = L;
        if (!io->enc) { if (L > olen - *opos) g6_fail(H, 2); memcpy(out + *opos, rs, L); }
        for (size_t q = 0; q < L; q++) hist = hist << 8 | rs[q];
        for (;;) {
            size_t j = k - L;
            x.uidx = j < 3 ? (int)j : 3;
            x.h[3] = g6_H(g6_H((uint32_t)(G6K_CHAR << 6 | x.c), 8), hist & 0xFFFF);
            x.h[4] = g6_H(g6_H(G6K_CHAR, 9), hist & 0xFFFFFF);
            x.h[6] = g6_H(g6_H(G6K_CHAR, 10), hist & 0xFF);
            x.h[2] = g6_H(g6_H((uint32_t)(G6K_CHAR << 6 | x.c), 11), (k < rn ? rs[k] : 256u) | (hist & 0xFF) << 9 | (k + 1 < rn ? rs[k + 1] : 256u) << 17);
            x.aux = 0; x.prevu = 64;
            int u = g6_unit(io, &x, io->enc ? (k == n ? 62 : M->code[s[k]] >= 0 ? M->code[s[k]] : 63) : 0);
            x.uidx = 0;
            if (u == 62) break;
            int ch;
            if (u == 63) {
                int hi = g6_unit(io, &x, io->enc ? s[k] >> 6 : 0), lo = g6_unit(io, &x, io->enc ? s[k] & 63 : 0);
                if (hi > 3) g6_fail(H, 2);
                ch = hi << 6 | lo;
            } else ch = M->table[u];
            if (!io->enc) { if (*opos + k >= olen) g6_fail(H, 2); out[start + k] = (uint8_t)ch; }
            hist = hist << 8 | (uint32_t)ch; k++;
        }
        if (!io->enc) id = g6_intern_get(H, &M->tok, out + start, k, 1);
        break;
    }
    }
    /* materialise the field (decoder) and its id */
    const uint8_t *fs = s; size_t fn = n;
    if (!io->enc) {
        if (op == G6_NUM || op == G6_NUMNEW) { fn = (size_t)g6_render(v, fm, tmp); fs = tmp; id = g6_intern_get(H, &M->tok, tmp, fn, 1); }
        else { fs = g6_str(&M->tok, id); fn = M->tok.len[id]; }
        if (op != G6_NEW) { if (fn > olen - *opos) g6_fail(H, 2); memcpy(out + *opos, fs, fn); }
        else fs = out + *opos;
        *opos += fn;
    } else if (id == UINT32_MAX || op == G6_NUM || op == G6_NUMNEW) id = g6_intern_get(H, &M->tok, s, n, 1);
    /* numeric state */
    if (op != G6_NUM && op != G6_NUMNEW) isnum = g6_parse(fs, fn, &v, &fm);
    M->cv[f] = v; M->cf[f] = fm; M->cok[f] = (uint8_t)isnum; M->cres[f] = 0;
    if (isnum) {
        if (C->has && fm.d == C->fmt.d && fm.w == C->fmt.w) {
            if (op != G6_NUM) avail = g6_predict(M, C, pr);
            for (int p = 0; p < 4; p++) if (avail >> p & 1) C->cost[p] = C->cost[p] - (C->cost[p] >> 4) + 16u * (uint32_t)g6_bitlen(g6_zz(g6_sub(v, pr[p])));
            int s2 = g6_choose(C, avail); C->sel = s2 < 0 ? 0 : s2;
            int bl = avail ? g6_bitlen(g6_zz(g6_sub(v, pr[C->sel]))) : 0; C->lastres = (uint8_t)bl; M->cres[f] = (uint8_t)bl;
        } else if (C->has) { C->cost[0] = C->cost[1] = C->cost[2] = C->cost[3] = 0; }
        if (C->has && (fm.d != C->fmt.d || fm.w != C->fmt.w)) C->has = 0;
        C->last2 = C->last; C->last = v; C->fmt = fm; if (C->has < 2) C->has++;
    }
    if (id != 0) {
        g6_front(H, &C->list, id, g6_find(&C->list, id), G6_COLCAP);
        g6_mtf *pl = g6_plist(M, pkey, 1);
        g6_front(H, pl, id, g6_find(pl, id), G6_PREVCAP);
        uint32_t *cd = g6_map_slot(H, &M->cdmap, (uint64_t)(f < G6_MAXC ? f : G6_MAXC - 1) << 32 | id, 1);
        if (*cd == UINT32_MAX) {
            if (C->nd == C->capd) { C->capd = C->capd * 2 + 16; C->dict = g6_realloc(H, C->dict, (size_t)C->capd * 4); }
            *cd = C->nd; C->dict[C->nd++] = id;
        }
    }
    M->cid[f] = id; M->cop[f] = (uint8_t)op; C->ophist = (uint8_t)(C->ophist << 4 | op);
}
static void g6_line_end(g6_model *M, size_t nf) {
    uint8_t *t = M->aaop; M->aaop = M->aop; M->aop = M->cop; M->cop = t;
    uint32_t *u = M->aid; M->aid = M->cid; M->cid = u;
    M->ppnf = M->pnf; M->pnf = nf;
}

/* ---- formula discovery (encoder): the search for one target column t. Every target is independent of the others:
 * its best formula depends only on its own candidates. *pw counts the rows scored (screening included) and is
 * compared with G6_WORK_CAP: the search is O(columns^2) per target column, so a table that reaches the cap keeps the
 * formulas found so far. Counted in rows, not time, so the output stays the same on every machine. OWID CO2
 * (79 columns) needs 0.34 G, but owid_test.csv (79 columns) needs between 2^29 and 2^30: the first cap (2^29) cost it
 * +4.6% at -x and a 5x slower decode (CM chosen instead of GR6+). Tables wider than G6_MAXCOLS are not tried at all
 * (energy_test.csv, 130 columns, needed 3.9 G), so 2^31 only bounds a pathological table of at most 100 columns.
 * Returns 1 with *f when a formula is kept. */
typedef struct { const int64_t *V; const uint8_t *D, *OK; int R, rows; } g6_samp;
static int g6_target(const g6_samp *S, int t, int64_t *pw, g6_formula *f) {
    static const int KE[5] = {0, 2, 3, 6, 9};
    const int64_t *V = S->V; const uint8_t *D = S->D, *OK = S->OK; const int R = S->R, rows = S->rows;
    int64_t work = *pw;
#define G6_V(c, r) V[(size_t)(c) * R + (r)]
#define G6_D(c, r) D[(size_t)(c) * R + (r)]
#define G6_OK(c, r) OK[(size_t)(c) * R + (r)]
#define G6_PRED(ty, a, b, k, r, p) g6_fpred(ty, k, G6_V(a, r), G6_D(a, r), G6_OK(a, (r) - 1), G6_V(a, (r) - 1), G6_D(a, (r) - 1), \
        G6_V(b, r), G6_D(b, r), G6_OK(t, (r) - 1), G6_V(t, (r) - 1), G6_D(t, (r) - 1), G6_D(t, r), p)
#define G6_NEAR(p, r) ((p) - G6_V(t, r) <= 2 && G6_V(t, r) - (p) <= 2)
    int cnt = 0; for (int r = 0; r < rows; r++) cnt += G6_OK(t, r);
    if (cnt < rows / 10 || cnt < 50) return 0;
    int bh = 0, bs = 1, ba = -1, bb = -1, bk = 0, bt = 0;   /* best rate bh/bs, at least one half */
    for (int ty = 0; ty < 6; ty++) {
        int two = ty == 0 || ty == 5;
        for (int a = 0; a < t; a++) for (int b = 0; b < (two ? t : 1); b++) for (int q = 0; q < (two ? 5 : 1); q++) {
            if (two && a == b) continue;
            int bc = two ? b : a, hit = 0, miss = 0, seen = 0;
            for (int r = 1; r < rows && r < 3000 && seen < 64; r++) {
                if (!G6_OK(t, r) || !G6_OK(a, r) || !G6_OK(bc, r)) continue;
                int64_t p; seen++;
                if (G6_PRED(ty, a, bc, KE[q], r, &p) && G6_NEAR(p, r)) hit++;
                else if (++miss > 24 || (miss >= 6 && hit * 2 < miss)) break;
            }
            work += 64;
            if (seen < 16 || miss > 24 || hit * 2 < miss || hit < seen / 2 || work >= G6_WORK_CAP) continue;
            hit = 0; seen = 0;
            const int lim = rows < 8000 ? rows : 8000;
            for (int r = 1; r < lim; r++, work++) {
                if ((r & 63) == 0) {
                    /* stop when the formula could not be kept even if every remaining row were a hit: the best
                     * reachable rate is (hit + rem) / (seen + rem). Exact, the formulas found are the same
                     * (energy_test.csv, 130 columns: discovery 10.7 s -> see benchmark/rc3/gr6/RESULTS.md) */
                    int64_t rem = lim - r;
                    if (2 * hit + rem <= seen || (hit + rem) * bs <= (int64_t)bh * (seen + rem)) break;
                }
                if (!G6_OK(t, r) || !G6_OK(a, r) || !G6_OK(bc, r)) continue;
                int64_t p; seen++;
                hit += G6_PRED(ty, a, bc, KE[q], r, &p) && G6_NEAR(p, r);
            }
            if (seen >= 32 && 2 * hit > seen && (int64_t)hit * bs > (int64_t)bh * seen) { bh = hit; bs = seen; ba = a; bb = bc; bk = KE[q]; bt = ty; }
        }
    }
#undef G6_V
#undef G6_D
#undef G6_OK
#undef G6_PRED
#undef G6_NEAR
    *pw = work;
    if (ba < 0) return 0;
    *f = (g6_formula){t, ba, bb, bk, bt}; return 1;
}
/* Threads (same formulas for every thread count): each target is searched on its own with its work counted from 0;
 * targets are searched in batches of 4 per thread and merged in target order. While the work so far plus a target's own work stays under the cap, that
 * target ran exactly as it would have in order (the cap is only compared, never otherwise used); the first target
 * where the cap could bite, and every one after it, is searched again in order by the one-thread path. */
typedef struct { g6_formula f; int64_t work; int found; } g6_tres;
typedef struct { const g6_samp *S; g6_tres *res; int t0, nb; } g6_par;
static void g6_target_task(void *ctx, int task, int worker) {
    g6_par *P = ctx; (void)worker; int t = P->t0 + P->nb - 1 - task;  /* widest targets of the batch first */
    P->res[t].work = 0; P->res[t].found = g6_target(P->S, t, &P->res[t].work, &P->res[t].f);
}

/* ---- formula discovery (encoder): a sample of up to 20000 rows with the first data row's field count */
static int g6_formulas(g6_heap *H, const uint8_t *d, size_t n, uint8_t sep, g6_formula *out, int threads) {
    size_t i = 0, line = 0; int K = -1; const int R = 20000;
    int64_t *V = NULL; uint8_t *D = NULL, *OK = NULL; int rows = 0;
    while (i < n && rows < R) {
        const uint8_t *nl = memchr(d + i, '\n', n - i); size_t e = nl ? (size_t)(nl - d) : n;
        if (line > 0) {
            int nf = 1; for (size_t k = i; k < e && nf <= 512; k++) nf += d[k] == sep;
            if (K < 0) { if (nf > 512 || nf <= 2) return 0; K = nf; V = g6_alloc(H, sizeof(int64_t) * K * R); D = g6_alloc(H, (size_t)K * R); OK = g6_alloc(H, (size_t)K * R); }
            if (nf == K) {
                size_t fs = i; int f = 0;
                for (size_t k = i; k <= e; k++) if (k == e || d[k] == sep) {
                    g6_fmt fm = {0, 0}; int64_t v = 0; int ok = g6_parse(d + fs, k - fs, &v, &fm);
                    V[(size_t)f * R + rows] = v; D[(size_t)f * R + rows] = fm.d; OK[(size_t)f * R + rows] = (uint8_t)ok; f++; fs = k + 1;
                }
                rows++;
            }
        }
        line++; i = e + 1;
    }
    if (K <= 2 || rows < 50) return 0;
    g6_samp S = {V, D, OK, R, rows};
    int nout = 0; int64_t work = 0; int t = 1;
    g6_tres *res = threads > 1 && K > 8 ? calloc((size_t)K, sizeof *res) : NULL;
    if (res) {                                       /* batches of targets on threads, merged in order */
        int stop = 0;
        while (!stop && t < K && nout < G6_MAXFORM && work < G6_WORK_CAP) {
            int nb = K - t < 4 * threads ? K - t : 4 * threads;
            g6_par P = {&S, res, t, nb}; kolmo_par_for(threads, nb, g6_target_task, &P);
            for (int e = t + nb; t < e && nout < G6_MAXFORM && work < G6_WORK_CAP; t++) {
                if (work + res[t].work >= G6_WORK_CAP) { stop = 1; break; }   /* the cap may bite here: in order from t */
                work += res[t].work; if (res[t].found) out[nout++] = res[t].f;
            }
        }
        free(res);
    }
    for (; t < K && nout < G6_MAXFORM && work < G6_WORK_CAP; t++) { g6_formula f; if (g6_target(&S, t, &work, &f)) out[nout++] = f; }
    return nout;
}

static void g6_setup(g6_model *M, const uint8_t *table) {
    memcpy(M->table, table, 62);
    for (int i = 0; i < 256; i++) M->code[i] = -1;
    for (int i = 61; i >= 0; i--) M->code[table[i]] = i;
}

/* the field separator: the most frequent of , TAB ; | and space in the first MiB */
static uint8_t g6_sep(const uint8_t *d, size_t n) {
    static const uint8_t SEPS[5] = {',', '\t', ';', '|', ' '};
    size_t c[5] = {0}, m = n < (1u << 20) ? n : (1u << 20);
    for (size_t i = 0; i < m; i++) for (int k = 0; k < 5; k++) c[k] += d[i] == SEPS[k];
    int bsep = 0; for (int k = 1; k < 5; k++) if (c[k] > c[bsep]) bsep = k;
    return SEPS[bsep];
}

/* worth trying: a table. Text with at least 64 lines, where at least 90% of the first 256 lines have the same number
 * of fields, 3 to G6_MAXCOLS. GR6+ was measured to win only on such tables (CSV with derived columns); prose, logs and
 * JSON never chose it and cost 3-11% more `-x` time, and 130 columns cost +126% for nothing
 * (benchmark/rc3/gr6/RESULTS.md, benchmark/rc3w2/regress/RESULTS.md) */
static int gr6_applies(const uint8_t *s, size_t n) {
    if (n < 4096 || !looks_text(s, n)) return 0;
    const uint8_t sep = g6_sep(s, n); uint32_t cnt[G6_MAXCOLS + 2] = {0}; size_t lines = 0;
    for (const uint8_t *p = s, *e = s + n; p < e && lines < 256;) {
        const uint8_t *q = memchr(p, '\n', (size_t)(e - p)); if (!q) break;
        size_t nf = 1; for (const uint8_t *k = p; k < q && nf <= G6_MAXCOLS; k++) nf += *k == sep;
        cnt[nf <= G6_MAXCOLS ? nf : G6_MAXCOLS + 1]++; lines++; p = q + 1;
    }
    if (lines < 64) return 0;
    uint32_t best = 0; for (int k = 3; k <= G6_MAXCOLS; k++) if (cnt[k] > best) best = cnt[k];
    return (size_t)best * 10 >= lines * 9;
}

/* returns the coded size, 0 when it does not fit in cap or memory runs out */
static size_t gr6_compress(const uint8_t *d, size_t n, uint8_t *out, size_t cap, int threads) {
    pthread_once(&cm_once, cm_init_tables);
    g6_heap *H = calloc(1, sizeof *H); if (!H) return 0;
    g6_model *M = calloc(1, sizeof *M); g6_io *io = calloc(1, sizeof *io); g6_buf *ob = calloc(1, sizeof *ob);
    g6_formula *F = malloc(sizeof(g6_formula) * G6_MAXFORM);
    size_t ret = 0;
    if (!M || !io || !ob || !F) goto done;
    if (setjmp(H->jb)) { ret = 0; goto done; }
    {
        uint8_t sep = g6_sep(d, n);
        size_t cnt[256] = {0}; for (size_t i = 0; i < n; i++) cnt[d[i]]++;
        int ord[256]; for (int i = 0; i < 256; i++) ord[i] = i;
        for (int i = 1; i < 256; i++) { int x = ord[i], j = i; while (j > 0 && cnt[ord[j - 1]] < cnt[x]) { ord[j] = ord[j - 1]; j--; } ord[j] = x; }
        uint8_t table[62]; for (int i = 0; i < 62; i++) table[i] = (uint8_t)ord[i];
        int nform = g6_formulas(H, d, n, sep, F, threads);
        g6_model_init(H, M); g6_setup(M, table);
        for (int i = 0; i < nform; i++) { g6_col *cc = &M->col[F[i].t]; cc->fa = F[i].a; cc->fb = F[i].b; cc->fke = F[i].ke; cc->ftype = F[i].type; }
        uint64_t lines = n && d[n - 1] != '\n';                  /* newlines, plus an unterminated last line */
        for (const uint8_t *p = d, *e = d + n; (p = memchr(p, '\n', (size_t)(e - p))) != NULL; p++) lines++;
        uint8_t hdr[12] = {1, sep, (uint8_t)(n && d[n - 1] == '\n'), (uint8_t)nform};
        for (int k = 0; k < 8; k++) hdr[4 + k] = (uint8_t)(lines >> (8 * k));
        g6_put(H, ob, hdr, 12); g6_put(H, ob, table, 62);
        for (int k = 0; k < nform; k++) {
            uint8_t e[8] = {(uint8_t)F[k].t, (uint8_t)(F[k].t >> 8), (uint8_t)F[k].a, (uint8_t)(F[k].a >> 8), (uint8_t)F[k].b, (uint8_t)(F[k].b >> 8), (uint8_t)F[k].ke, (uint8_t)F[k].type};
            g6_put(H, ob, e, 8);
        }
        io->enc = 1; io->H = H; io->ac = (g6_ac){0, 0xffffffffu, 0, ob, NULL, 0, 0};
        g6_cm_init(H, &io->m, n);
        for (size_t i = 0; i < n;) {
            const uint8_t *q = memchr(d + i, '\n', n - i); size_t e = q ? (size_t)(q - d) : n;
            size_t nf = 1; for (size_t k = i; k < e; k++) nf += d[k] == sep;
            g6_cap(M, nf);
            g6_ctx x; g6_set(M, &x, G6K_NF, 0); g6_val(io, &x, nf == M->pnf ? 0 : nf + 1);
            size_t fs = i; int f = 0;
            for (size_t k = i; k <= e; k++) if (k == e || d[k] == sep) { g6_field(M, io, f++, d + fs, k - fs, NULL, NULL, 0); fs = k + 1; }
            g6_line_end(M, nf);
            if (ob->n > cap) goto done;
            i = e + 1;
        }
        uint8_t tail[4] = {(uint8_t)(io->ac.x1 >> 24), 255, 255, 255}; g6_put(H, ob, tail, 4);
        if (ob->n <= cap) { memcpy(out, ob->p, ob->n); ret = ob->n; }
    }
done:
    g6_free_all(H); free(H); free(M); free(io); free(ob); free(F);
    return ret;
}

/* 0 ok, KOLMO_ERR_CORRUPT or KOLMO_ERR_ALLOC */
static int gr6_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    pthread_once(&cm_once, cm_init_tables);
    if (cn < 74 || in[0] != 1) return KOLMO_ERR_CORRUPT;
    uint8_t sep = in[1]; int final_nl = in[2], nform = in[3]; uint64_t lines = 0;
    for (int k = 0; k < 8; k++) lines |= (uint64_t)in[4 + k] << (8 * k);
    if ((sep != ',' && sep != '\t' && sep != ';' && sep != '|' && sep != ' ') || final_nl > 1 || lines > (uint64_t)n + 1) return KOLMO_ERR_CORRUPT;
    size_t q = 74 + (size_t)nform * 8; if (q > cn) return KOLMO_ERR_CORRUPT;
    g6_heap *H = calloc(1, sizeof *H); g6_model *M = calloc(1, sizeof *M); g6_io *io = calloc(1, sizeof *io);
    volatile int ret = KOLMO_ERR_ALLOC;
    if (!H || !M || !io) goto done;
    int jr = setjmp(H->jb);
    if (jr) { ret = jr == 1 ? KOLMO_ERR_ALLOC : KOLMO_ERR_CORRUPT; goto done; }
    {
        g6_model_init(H, M); g6_setup(M, in + 12);
        for (int k = 0; k < nform; k++) {
            const uint8_t *e = in + 74 + (size_t)k * 8;
            int t = e[0] | e[1] << 8, a = e[2] | e[3] << 8, b = e[4] | e[5] << 8;
            if (t >= G6_MAXC || a >= t || b >= t || e[6] > 18 || e[7] > 5) g6_fail(H, 2);
            g6_col *cc = &M->col[t]; cc->fa = a; cc->fb = b; cc->fke = e[6]; cc->ftype = e[7];
        }
        io->enc = 0; io->H = H; io->ac = (g6_ac){0, 0xffffffffu, 0, NULL, in + q, 0, cn - q};
        for (int i = 0; i < 4; i++) io->ac.x = (io->ac.x << 8) | (io->ac.pos < io->ac.n ? io->ac.in[io->ac.pos++] : 0);
        g6_cm_init(H, &io->m, n);
        size_t o = 0;
        for (uint64_t l = 0; l < lines; l++) {
            /* the encoder's 4 tail bytes are all the decoder reads: past them the stream is truncated or forged, and
             * decoding stops instead of running to the claimed size on padding */
            if (io->ac.pad > 8) g6_fail(H, 2);
            g6_ctx x; g6_set(M, &x, G6K_NF, 0); uint64_t v = g6_val(io, &x, 0);
            uint64_t nf = v == 0 ? M->pnf : v - 1;
            if (nf == 0 || nf - 1 > n - o || nf > INT32_MAX) g6_fail(H, 2);
            g6_cap(M, (size_t)nf);
            for (size_t f = 0; f < nf; f++) {
                if (f) { if (o >= n) g6_fail(H, 2); out[o++] = sep; }
                g6_field(M, io, (int)(f < INT32_MAX ? f : INT32_MAX), NULL, 0, out, &o, n);
            }
            g6_line_end(M, (size_t)nf);
            if (l + 1 < lines || final_nl) { if (o >= n) g6_fail(H, 2); out[o++] = '\n'; }
        }
        ret = o == n ? 0 : KOLMO_ERR_CORRUPT;
    }
done:
    if (H) g6_free_all(H);
    free(H); free(M); free(io);
    return ret;
}

static size_t gr6_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)level; return gr6_compress(in, n, out, cap, x ? x->threads : 1);
}
static const kolmo_codec codec_gr6 = { C_GR6, "gr6", NULL, gr6_codec_compress, NULL, NULL };   /* decoded at block level (core/decode.c) */

/* test hooks */
size_t gr6_compress_pub(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { return gr6_compress(in, n, out, cap, 1); }
size_t gr6_compress_pub_t(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int threads) { return gr6_compress(in, n, out, cap, threads); }
int gr6_decompress_pub(const uint8_t *in, size_t cn, uint8_t *out, size_t n) { return gr6_decompress(in, cn, out, n); }
int gr6_applies_pub(const uint8_t *in, size_t n) { return gr6_applies(in, n); }
