/* GR6+: structure transform -> 6-bit units -> adaptive final coder (research prototype, lossless).
 *
 * 1. Transform. Lines are split into fields at the file's separator (the most frequent of , \t ; | space).
 *    Each field is one decision (an op), followed by the units that op needs:
 *      DITTO   same field as the line above                        EMPTY   empty field
 *      NUM     number, residual from the column predictor: the best (by recent cost) of
 *              delta, delta-of-delta and a derived-column formula round(a / b * 10^k) found by the encoder
 *      RANK    rank in the column's move-to-front list of recent values
 *      RANK2   rank in the graph list of fields that followed the previous field (word graph for text)
 *      CDICT   index in the column's dictionary                    GID     global id
 *      NUMNEW  number with a new format (decimals, zero padding) and raw value
 *      NEW     characters (62-entry table, END, ESC + 2 units)
 * 2. All values are written as 6-bit units (5 data bits + continuation bit), so the alphabet is always 64.
 * 3. Final coder (mode): 0 fixed 6-bit packing, 1 static Huffman (huff0) per stream (ops split per column),
 *    2 context mixing: every unit is 6 binary decisions predicted by 7 context models that see the unit's role,
 *    column, the ops above / to the left, the field above, the previous characters; a small neural network
 *    (logistic mixer) combines them, an APM refines, a binary arithmetic coder codes.
 *
 * usage: gr6plus c MODE in out | gr6plus d in out | gr6plus b MODE in   (b: round trip + timing + stats)
 */
#define HUF_STATIC_LINKING_ONLY
#include "common.h"
#include <math.h>
#include "common/huf.h"
#include "common/error_private.h"

enum { K_NF, K_OP, K_RANK, K_RANK2, K_CDICT, K_GID, K_NUM, K_NUMNEW, K_CHAR, K_PFX, NKIND };
enum { OP_DITTO, OP_EMPTY, OP_NUM, OP_RANK, OP_RANK2, OP_CDICT, OP_GID, OP_NUMNEW, OP_NEW, NOP };
static const char *opname[NOP] = {"ditto", "empty", "num", "rank", "graph", "cdict", "gid", "numnew", "new"};
#define MAXC 4096
#define COLCAP 256
#define PREVCAP 256
#define NSTREAM (NKIND + 64)   /* huffman mode: op units get one stream per column (first 64) */

/* ------------------------------------------------------------------ numbers */
typedef struct { uint8_t d, w; } nfmt;
static int render_num(int64_t v, nfmt f, uint8_t *out) {
    char dg[48]; int nd = 0; uint64_t a = v < 0 ? -(uint64_t)v : (uint64_t)v;
    do { dg[nd++] = '0' + a % 10; a /= 10; } while (a);
    while (nd < f.d + 1) dg[nd++] = '0';
    int ni = nd - f.d, o = 0;
    if (v < 0) out[o++] = '-';
    for (int i = ni; i < f.w; i++) out[o++] = '0';
    for (int i = nd - 1; i >= f.d; i--) out[o++] = dg[i];
    if (f.d) { out[o++] = '.'; for (int i = f.d - 1; i >= 0; i--) out[o++] = dg[i]; }
    return o;
}
static int parse_num(const uint8_t *s, int n, int64_t *v, nfmt *f) {
    if (n == 0 || n > 40) return 0;
    int i = 0, neg = 0; if (s[0] == '-') { neg = 1; i = 1; }
    int is = i; while (i < n && s[i] >= '0' && s[i] <= '9') i++;
    int ni = i - is; if (ni == 0) return 0;
    int d = 0; if (i < n && s[i] == '.') { i++; int fs = i; while (i < n && s[i] >= '0' && s[i] <= '9') i++; d = i - fs; if (d == 0) return 0; }
    if (i != n || ni + d > 18) return 0;
    int64_t x = 0; for (int k = is; k < n; k++) if (s[k] != '.') x = x * 10 + (s[k] - '0');
    f->d = d; f->w = (ni > 1 && s[is] == '0') ? ni : 0; *v = neg ? -x : x;
    uint8_t r[48]; int rn = render_num(*v, *f, r);
    return rn == n && memcmp(r, s, n) == 0;
}
static inline uint64_t zz(int64_t x) { return ((uint64_t)x << 1) ^ (uint64_t)(x >> 63); }
static inline int64_t unzz(uint64_t z) { return (int64_t)(z >> 1) ^ -(int64_t)(z & 1); }
static inline int bitlen(uint64_t x) { return x ? 64 - __builtin_clzll(x) : 0; }
static const int64_t P10[19] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 10000000000LL,
    100000000000LL, 1000000000000LL, 10000000000000LL, 100000000000000LL, 1000000000000000LL, 10000000000000000LL,
    100000000000000000LL, 1000000000000000000LL};
/* round(a/10^da / (b/10^db) * 10^ke * 10^dt), half away from zero; 0 if not computable */
static int formula(int64_t va, int da, int64_t vb, int db, int ke, int dt, int64_t *out) {
    int e = db + dt + ke; if (vb == 0 || e > 18 || da > 18) return 0;
    if (va > (1LL << 60) || va < -(1LL << 60) || vb > (1LL << 60) || vb < -(1LL << 60)) return 0;
    __int128 num = (__int128)va * P10[e], den = (__int128)vb * P10[da];
    int neg = (num < 0) != (den < 0); if (num < 0) num = -num; if (den < 0) den = -den;
    __int128 q = (2 * num + den) / (2 * den);
    if (q > ((__int128)1 << 62)) return 0;
    *out = neg ? -(int64_t)q : (int64_t)q; return 1;
}

/* v at `from` decimals -> `to` decimals (rounded half away from zero) */
static int rescale(int64_t v, int from, int to, int64_t *out) {
    if (to >= from) { int e = to - from; if (e > 18) return 0; int64_t lim = INT64_MAX / P10[e] / 2; if (v > lim || v < -lim) return 0; *out = v * P10[e]; return 1; }
    int e = from - to; if (e > 18) return 0; int64_t q = P10[e], a = v < 0 ? -v : v, r = (a + q / 2) / q; *out = v < 0 ? -r : r; return 1;
}
/* derived-column predictions. type 0: round(a / b * 10^ke); 1: a - lag(a); 2: 100 * (a / lag(a) - 1);
 * 3: last(t) + a (running total). All at the target's dt decimals, exact integer arithmetic. */
static int fpred(int type, int ke, int64_t va, int da, int okl, int64_t vla, int dla, int64_t vb, int db,
                 int oklt, int64_t vlt, int dlt, int dt, int64_t *out) {
    switch (type) {
    case 0: return formula(va, da, vb, db, ke, dt, out);
    case 1: return okl && da == dla && va - vla == va - vla && rescale(va - vla, da, dt, out);
    case 2: return okl && da == dla && formula(va - vla, da, vla, da, 2, dt, out);
    case 3: { int64_t r; if (!oklt || dlt != dt || !rescale(va, da, dt, &r)) return 0; *out = vlt + r; return 1; }
    case 4: return rescale(va, da, dt, out);                                   /* same as column a */
    case 5: {                                                                   /* a * b / 10^ke */
        if (va > (1LL << 40) || va < -(1LL << 40) || vb > (1LL << 40) || vb < -(1LL << 40)) return 0;
        __int128 m = (__int128)va * vb; int e = da + db + ke - dt; if (e < 0 || e > 18) return 0;
        __int128 q = P10[e], am = m < 0 ? -m : m, r = (am + q / 2) / q; if (r > ((__int128)1 << 62)) return 0;
        *out = m < 0 ? -(int64_t)r : (int64_t)r; return 1; }
    }
    return 0;
}

/* ------------------------------------------------------------------ CM coder */
#ifndef TB
#define TB 22
#endif
#ifndef NM
#define NM 8
#endif
typedef struct { uint32_t x1, x2, x; buf *out; const uint8_t *in; size_t pos, n; } acoder;
static inline void ac_enc(acoder *a, int bit, int p12) {
    uint32_t xm = a->x1 + (uint32_t)(((uint64_t)(a->x2 - a->x1) * p12) >> 12);
    if (bit) a->x2 = xm; else a->x1 = xm + 1;
    while (((a->x1 ^ a->x2) & 0xff000000u) == 0) { buf_put(a->out, a->x2 >> 24); a->x1 <<= 8; a->x2 = (a->x2 << 8) | 255; }
}
static inline int ac_dec(acoder *a, int p12) {
    uint32_t xm = a->x1 + (uint32_t)(((uint64_t)(a->x2 - a->x1) * p12) >> 12);
    int bit = a->x <= xm;
    if (bit) a->x2 = xm; else a->x1 = xm + 1;
    while (((a->x1 ^ a->x2) & 0xff000000u) == 0) { a->x1 <<= 8; a->x2 = (a->x2 << 8) | 255; a->x = (a->x << 8) | (a->pos < a->n ? a->in[a->pos++] : 0); }
    return bit;
}
typedef struct {
    uint32_t *T[NM]; float W[NKIND * 4 * 64][NM + 1]; float W2[NKIND * 16 * 64][NM + 1]; int pm2; uint16_t (*apm)[33];
    float st[4096]; uint32_t rcp[1024];
    uint32_t base[NM]; uint32_t *slot[NM]; float s[NM + 1]; int pm, api; float apw; int tb;
} cm;
/* table size follows the input size (encoder and decoder both know n) */
static int tb_for(uint64_t n) {
#ifdef TBFIX
    (void)n; return TB;
#else
    return n < (1u << 20) ? 20 : n < (8u << 20) ? 22 : 23;
#endif
}
static void cm_init(cm *m, uint64_t n) {
    m->tb = tb_for(n);
    for (int i = 0; i < NM; i++) { m->T[i] = xmalloc(sizeof(uint32_t) << m->tb); for (size_t j = 0; j < (1u << m->tb); j++) m->T[i][j] = (1u << 31); }
    for (int i = 0; i < NKIND * 4 * 64; i++) for (int j = 0; j <= NM; j++) m->W[i][j] = 0.3f;
    for (int i = 0; i < NKIND * 16 * 64; i++) for (int j = 0; j <= NM; j++) m->W2[i][j] = 0.3f;
    m->apm = xmalloc(sizeof(*m->apm) * NKIND * 64 * 64);
    for (int i = 0; i < NKIND * 64 * 64; i++) for (int j = 0; j < 33; j++) m->apm[i][j] = (uint16_t)(65535.0 / (1 + exp(-((j - 16) / 2.0))));
    for (int i = 0; i < 4096; i++) m->st[i] = logf((i + 0.5f) / (4096 - i - 0.5f));
    for (int i = 0; i < 1024; i++) m->rcp[i] = (uint32_t)(65536.0 / (i + 1.6));
}
static inline int squash(float x) { if (x > 15) x = 15; if (x < -15) x = -15; int p = (int)(4096.0f / (1.0f + expf(-x))); return p < 1 ? 1 : p > 4095 ? 4095 : p; }
#ifndef CLIMIT
#define CLIMIT 255
#endif
#ifndef MLR
#define MLR 0.018f
#endif

/* ------------------------------------------------------------------ unit I/O */
typedef struct { int kind, c, uidx, prevu, aux, sel2; uint32_t h[8]; } uctx;
typedef struct {
    int mode, enc; uint64_t units, kunits[NKIND];
    buf st[NSTREAM]; size_t rp[NSTREAM];   /* raw6 uses st[0]; huffman uses all */
    acoder ac; cm *m; double bits[NKIND]; double colbits[MAXC]; int curf;
} uio;

static inline uint32_t H(uint32_t a, uint32_t b) { uint32_t h = (a ^ 0x9E3779B9u) * 0x85EBCA6Bu ^ (b + 0x632BE59Bu) * 0xC2B2AE35u; return h ^ (h >> 15); }

static int cm_bit(uio *io, uctx *x, int node, int bit) {
    cm *m = io->m; float dot = 0;
    float *w = m->W[(x->kind * 4 + (x->uidx < 3 ? x->uidx : 3)) * 64 + node];
    for (int i = 0; i < NM; i++) {
        m->slot[i] = &m->T[i][m->base[i] | node];
        int p = *m->slot[i] >> 20; m->s[i] = m->st[p]; dot += w[i] * m->s[i];
    }
    m->s[NM] = 0.25f; dot += w[NM] * m->s[NM];
    float *w2 = m->W2[(x->kind * 16 + x->sel2) * 64 + node]; float dot2 = 0;
    for (int i = 0; i <= NM; i++) dot2 += w2[i] * m->s[i];
    int pm1 = squash(dot), pm2 = squash(dot2), pm = squash((m->st[pm1] + m->st[pm2]) * 0.5f);
    float sx = (m->st[pm] + 8.0f) * 2.0f; if (sx < 0) sx = 0; if (sx > 31.999f) sx = 31.999f;
    int lo = (int)sx; float aw = sx - lo; int ai = (x->kind * 64 + x->c) * 64 + node;
    int pa = (int)(m->apm[ai][lo] * (1 - aw) + m->apm[ai][lo + 1] * aw) >> 4;
    int p = (pm + 3 * pa) >> 2; if (p < 1) p = 1; if (p > 4095) p = 4095;
    if (io->enc) ac_enc(&io->ac, bit, p); else bit = ac_dec(&io->ac, p);
    { double b_ = -log2((bit ? p : 4096 - p) / 4096.0); io->bits[x->kind] += b_; if (x->kind == K_NUM) io->colbits[io->curf] += b_; }
    /* update */
    for (int i = 0; i < NM; i++) {
        uint32_t v = *m->slot[i], n = v & 1023, pr = v >> 10;
        int32_t tgt = bit ? (1 << 22) - 1 : 0;
        pr += (int32_t)(((int64_t)(tgt - (int32_t)pr) * m->rcp[n]) >> 16);
        if (n < CLIMIT) n++;
        *m->slot[i] = pr << 10 | n;
    }
    float err = ((bit << 12) - pm1) * (MLR / 4096.0f), err2 = ((bit << 12) - pm2) * (MLR / 4096.0f);
    for (int i = 0; i <= NM; i++) { w[i] += err * m->s[i]; w2[i] += err2 * m->s[i]; }
    uint16_t tg = bit ? 65535 : 0; uint16_t *a = &m->apm[ai][lo];
    a[0] += ((int)tg - a[0]) * (1 - aw) * 0.02f; a[1] += ((int)tg - a[1]) * aw * 0.02f;
    return bit;
}

static int unit_io(uio *io, uctx *x, int u) {
    io->units++; io->kunits[x->kind]++;
    if (io->mode == 0) { if (io->enc) buf_put(&io->st[0], u); else u = io->st[0].p[io->rp[0]++]; }
    else if (io->mode == 1) {
        int s = x->kind == K_OP ? NKIND + x->c : x->kind;
        if (io->enc) buf_put(&io->st[s], u); else u = io->st[s].p[io->rp[s]++];
    } else {
        cm *m = io->m;
        uint32_t uh = (uint32_t)x->aux << 16 | (uint32_t)x->uidx << 8 | x->prevu;
        for (int i = 0; i < NM; i++) m->base[i] = (H(x->h[i], uh) >> (32 - m->tb)) & ~63u;
        int node = 1;
        for (int b = 5; b >= 0; b--) { int bit = cm_bit(io, x, node, io->enc ? (u >> b) & 1 : 0); node = node * 2 + bit; }
        u = node - 64;
    }
    x->prevu = u; x->uidx++;
    return u;
}
/* a value in units: 5 data bits per unit + continuation bit (bit 5) */
static uint64_t val_io(uio *io, uctx *x, uint64_t v) {
    x->uidx = 0; x->prevu = 64; x->aux = 0;
    if (x->kind != K_OP) {   /* magnitude first */
        int bl = io->enc ? bitlen(v) : 0;
        bl = io->enc ? (unit_io(io, x, bl > 62 ? 63 : bl), bl) : unit_io(io, x, 0);
        if (bl == 63 || bl > 62) {   /* escape: 64 raw bits, 6 per unit */
            uint64_t r = 0; for (int s = 60; s >= 0; s -= 6) { int u = io->enc ? (v >> s) & 63 : 0; u = unit_io(io, x, u); r |= (uint64_t)u << s; }
            return r;
        }
        if (bl <= 1) return bl;
        x->aux = bl; uint64_t r = 1; int left = bl - 1;
        while (left > 0) {
            int k = left < 6 ? left : 6; left -= k;
            int u = io->enc ? (v >> left) & ((1 << k) - 1) : 0;
            u = unit_io(io, x, u); r = r << k | u;
        }
        return r;
    }
    if (io->enc) { while (v >= 32) { unit_io(io, x, 32 | (v & 31)); v >>= 5; } unit_io(io, x, v); return 0; }
    uint64_t r = 0; int sh = 0;
    for (;;) { int u = unit_io(io, x, 0); r |= (uint64_t)(u & 31) << sh; sh += 5; if (!(u & 32) || sh > 63) return r; }
}

/* ------------------------------------------------------------------ model state shared by encoder and decoder */
typedef struct { int t, a, b, ke, type; } formula_t;
typedef struct {
    mtf list; uint32_t *dict; uint32_t nd, capd;
    int64_t last, last2; nfmt fmt; int has; uint32_t cost[4]; int sel;
    int fa, fb, fke, ftype;   /* formula (fa < 0: none) */
    uint8_t ophist, lastrb[NKIND]; uint8_t lastres;
} colst;
typedef struct {
    intern tok; map64 cdmap, pmap; mtf *plists; size_t npl, cappl; colst *col;
    uint32_t *aid, *cid; uint8_t *aop, *aaop, *cop, *cres; int64_t *cv; nfmt *cf; uint8_t *cok;
    size_t pnf, ppnf, cap;
    uint8_t table[62]; int code[256];
    uint64_t opcount[NOP];
} model;

static void model_init(model *M) {
    memset(M, 0, sizeof *M); intern_init(&M->tok); map_init(&M->cdmap, 16); map_init(&M->pmap, 16);
    M->col = xmalloc(sizeof(colst) * MAXC); memset(M->col, 0, sizeof(colst) * MAXC);
    for (int i = 0; i < MAXC; i++) M->col[i].fa = -1;
    intern_get(&M->tok, (const uint8_t *)"", 0, 1, NULL);   /* id 0 = empty field */
}
static void ensure_cap(model *M, size_t nf) {
    if (nf <= M->cap) return;
    size_t c = nf * 2 + 64;
    M->aid = xrealloc(M->aid, c * 4); M->cid = xrealloc(M->cid, c * 4); M->aop = xrealloc(M->aop, c); M->aaop = xrealloc(M->aaop, c);
    M->cop = xrealloc(M->cop, c); M->cres = xrealloc(M->cres, c); M->cv = xrealloc(M->cv, c * 8); M->cf = xrealloc(M->cf, c * sizeof(nfmt));
    M->cok = xrealloc(M->cok, c); M->cap = c;
}
static mtf *plist(model *M, uint32_t key, int insert) {
    uint32_t *v = map_slot(&M->pmap, key, insert); if (!v) return NULL;
    if (*v == UINT32_MAX) {
        if (M->npl == M->cappl) { M->cappl = M->cappl * 2 + 1024; M->plists = xrealloc(M->plists, M->cappl * sizeof(mtf)); }
        memset(&M->plists[M->npl], 0, sizeof(mtf)); *v = M->npl++;
    }
    return &M->plists[*v];
}
/* predictions for a numeric column at field f; avail bit p set if prediction p exists */
static int predict(model *M, colst *C, int f, int64_t pr[4]) {
    int avail = 0;
    if (C->has >= 1) { pr[1] = C->last; avail |= 2; pr[2] = C->has >= 2 ? 2 * C->last - C->last2 : C->last; avail |= 4; pr[3] = C->has >= 2 ? C->last + (C->last - C->last2) / 2 : C->last; avail |= 8; }
    if (C->fa >= 0 && C->has >= 1 && M->cok[C->fa] && M->cok[C->fb]) {
        colst *A = &M->col[C->fa];
        if (fpred(C->ftype, C->fke, M->cv[C->fa], M->cf[C->fa].d, A->has >= 2, A->last2, A->fmt.d,
                  M->cv[C->fb], M->cf[C->fb].d, 1, C->last, C->fmt.d, C->fmt.d, &pr[0])) avail |= 1;
    }
    (void)f; return avail;
}
static int choose(colst *C, int avail) {
    int best = -1; for (int p = 0; p < 4; p++) if (avail >> p & 1) if (best < 0 || C->cost[p] < C->cost[best]) best = p;
    return best;
}

/* context hashes for the next value */
static void set_ctx(model *M, uctx *x, int kind, int f, int nf) {
    (void)nf;
    colst *C = &M->col[f < MAXC ? f : MAXC - 1];
    int c = f < 63 ? f : 63;
    int aop = f < (int)M->pnf ? M->aop[f] : 15, aaop = f < (int)M->ppnf ? M->aaop[f] : 15, lop = f ? M->cop[f - 1] : 14;
    uint32_t aid = f < (int)M->pnf ? M->aid[f] : 0xFFFFFFF;
    uint32_t extra = 0, extra2 = 0;
    switch (kind) {
    case K_OP: extra = C->ophist; extra2 = lop | aop << 4 | (f == 0) << 8; break;
    case K_NUM: extra = C->sel | C->lastres << 3; extra2 = (f ? M->cres[f - 1] : 0) | C->sel << 8; break;
    case K_NUMNEW: extra = C->fmt.d | C->fmt.w << 5; break;
    case K_NF: extra = M->pnf & 1023; break;
    default: extra = C->lastrb[kind]; extra2 = lop; break;
    }
    uint32_t k = kind << 6 | c;
    x->kind = kind; x->c = c; x->sel2 = aop;
    x->h[0] = H(k, 1);
    x->h[1] = H(H(k, 2), aop | lop << 4);
    x->h[2] = H(H(k, 3), aid);
    x->h[3] = H(H(k, 4), extra);
    x->h[4] = H(H(kind, 5), extra2);
    x->h[5] = H(H(k, 6), aop | aaop << 4 | lop << 8 | (f ? M->cop[0] == OP_DITTO : 2) << 12);
    x->h[6] = H(H(k, 7), f ? M->cid[f - 1] : 0xFFFFFFF);
    x->h[7] = H(H(k, 12), (kind == K_NUM ? C->lastres | (f ? M->cres[f - 1] : 0) << 6 | C->sel << 12 : extra) | aop << 16);
}
static void set_char_ctx(uctx *x, uint32_t hist) {
    x->h[3] = H(H(K_CHAR << 6 | x->c, 8), hist & 0xFFFF);
    x->h[4] = H(H(K_CHAR, 9), hist & 0xFFFFFF);
    x->h[6] = H(H(K_CHAR, 10), hist & 0xFF);
}

/* code one field: encoder passes s/n and gets nothing back; decoder gets the field into out */
static void field_io(model *M, uio *io, int f, int nf, const uint8_t *s, int n, buf *out) {
    colst *C = &M->col[f < MAXC ? f : MAXC - 1];
    io->curf = f < MAXC ? f : MAXC - 1;
    uctx x; int op = 0; uint32_t id = 0; int64_t v = 0; nfmt fm = {0, 0}; int isnum = 0;
    int64_t pr[4] = {0, 0, 0, 0}; int avail = 0, sel = -1;
    uint8_t tmp[64];
    if (io->enc) {
        id = intern_get(&M->tok, s, n, 0, NULL);
        isnum = parse_num(s, n, &v, &fm);
        int pos;
        if (f < (int)M->pnf && id != UINT32_MAX && M->aid[f] == id) op = OP_DITTO;
        else if (n == 0) op = OP_EMPTY;
        else if (isnum && C->has && fm.d == C->fmt.d && fm.w == C->fmt.w &&
                 (avail = predict(M, C, f, pr)) && (sel = choose(C, avail)) >= 0 &&
                 (v == pr[sel] || id == UINT32_MAX || (pos = mtf_find(&C->list, id)) < 0 || pos >= 2)) op = OP_NUM;
        else if (id != UINT32_MAX && mtf_find(&C->list, id) >= 0) op = OP_RANK;
        else if (id != UINT32_MAX) {
            mtf *pl = plist(M, f ? M->cid[f - 1] : 0xFFFFFFFEu, 0);
            uint32_t *cd = map_slot(&M->cdmap, (uint64_t)(f < MAXC ? f : MAXC - 1) << 32 | id, 0);
            if (pl && mtf_find(pl, id) >= 0) op = OP_RANK2;
            else if (cd) op = OP_CDICT;
            else op = OP_GID;
        } else op = isnum ? OP_NUMNEW : OP_NEW;
    }
    set_ctx(M, &x, K_OP, f, nf);
    op = io->enc ? (val_io(io, &x, op), op) : (int)val_io(io, &x, 0);
    M->opcount[op]++;
    switch (op) {
    case OP_DITTO: id = M->aid[f]; break;
    case OP_EMPTY: id = 0; break;
    case OP_NUM: {
        if (!io->enc) { avail = predict(M, C, f, pr); sel = choose(C, avail); }
        set_ctx(M, &x, K_NUM, f, nf);
        uint64_t r = val_io(io, &x, zz(v - pr[sel]));
        if (!io->enc) { v = pr[sel] + unzz(r); fm = C->fmt; }
        isnum = 1; break;
    }
    case OP_RANK: case OP_RANK2: {
        mtf *l = op == OP_RANK ? &C->list : plist(M, f ? M->cid[f - 1] : 0xFFFFFFFEu, 0);
        set_ctx(M, &x, op == OP_RANK ? K_RANK : K_RANK2, f, nf);
        int r = io->enc ? mtf_find(l, id) : 0;
        r = io->enc ? (val_io(io, &x, r), r) : (int)val_io(io, &x, 0);
        id = l->a[r]; C->lastrb[x.kind] = r < 63 ? r : 63; break;
    }
    case OP_CDICT: {
        set_ctx(M, &x, K_CDICT, f, nf);
        uint32_t i = io->enc ? *map_slot(&M->cdmap, (uint64_t)(f < MAXC ? f : MAXC - 1) << 32 | id, 0) : 0;
        i = io->enc ? (val_io(io, &x, C->nd - 1 - i), i) : C->nd - 1 - (uint32_t)val_io(io, &x, 0);
        id = C->dict[i]; break;
    }
    case OP_GID: {
        set_ctx(M, &x, K_GID, f, nf);
        id = io->enc ? (val_io(io, &x, M->tok.count - 1 - id), id) : M->tok.count - 1 - (uint32_t)val_io(io, &x, 0);
        break;
    }
    case OP_NUMNEW: {
        set_ctx(M, &x, K_NUMNEW, f, nf);
        fm.d = io->enc ? (val_io(io, &x, fm.d), fm.d) : val_io(io, &x, 0);
        fm.w = io->enc ? (val_io(io, &x, fm.w), fm.w) : val_io(io, &x, 0);
        uint64_t z = val_io(io, &x, zz(v)); if (!io->enc) v = unzz(z);
        isnum = 1; break;
    }
    case OP_NEW: {
        mtf *gl = plist(M, f ? M->cid[f - 1] : 0xFFFFFFFEu, 0);
        uint32_t ref = gl && gl->n ? gl->a[0] : C->list.n ? C->list.a[0] : 0;
        const uint8_t *rs = intern_str(&M->tok, ref); int rn = M->tok.len[ref];
        set_ctx(M, &x, K_PFX, f, nf);
        int L = 0; if (io->enc) while (L < n && L < rn && s[L] == rs[L]) L++;
        L = io->enc ? (val_io(io, &x, L), L) : (int)val_io(io, &x, 0);
        set_ctx(M, &x, K_CHAR, f, nf);
        uint32_t hist = 0; buf lit = {0};
        for (int q = 0; q < L; q++) { if (!io->enc) buf_put(&lit, rs[q]); hist = hist << 8 | rs[q]; }
        int k = L;
        for (;;) {
            x.uidx = k - L < 3 ? k - L : 3; set_char_ctx(&x, hist);
            x.h[2] = H(H(K_CHAR << 6 | x.c, 11), (k < rn ? rs[k] : 256) | (hist & 0xFF) << 9 | (k + 1 < rn ? rs[k + 1] : 256) << 17);
            x.aux = 0; x.prevu = 64;
            int u;
            if (io->enc) { u = k == n ? 62 : M->code[s[k]] >= 0 ? M->code[s[k]] : 63; unit_io(io, &x, u); }
            else u = unit_io(io, &x, 0);
            x.uidx = 0;
            if (u == 62) break;
            int ch;
            if (u == 63) {
                int hi = io->enc ? (unit_io(io, &x, s[k] >> 6), s[k] >> 6) : unit_io(io, &x, 0);
                int lo = io->enc ? (unit_io(io, &x, s[k] & 63), s[k] & 63) : unit_io(io, &x, 0);
                ch = hi << 6 | lo;
            } else ch = M->table[u];
            if (!io->enc) buf_put(&lit, ch);
            hist = hist << 8 | ch; k++;
        }
        if (!io->enc) { id = intern_get(&M->tok, lit.p, lit.n, 1, NULL); free(lit.p); }
        break;
    }
    }
    /* materialise the field (decoder) and its id */
    const uint8_t *fs = s; int fn = n;
    if (!io->enc) {
        if (op == OP_NUM || op == OP_NUMNEW) { fn = render_num(v, fm, tmp); fs = tmp; id = intern_get(&M->tok, tmp, fn, 1, NULL); }
        else { fs = intern_str(&M->tok, id); fn = M->tok.len[id]; }
        buf_add(out, fs, fn);
    } else if (id == UINT32_MAX) id = intern_get(&M->tok, s, n, 1, NULL);
    else if (op == OP_NUM || op == OP_NUMNEW) id = intern_get(&M->tok, s, n, 1, NULL);
    /* numeric state */
    if (op != OP_NUM && op != OP_NUMNEW) isnum = parse_num(fs, fn, &v, &fm);
    M->cv[f] = v; M->cf[f] = fm; M->cok[f] = isnum; M->cres[f] = 0;
    if (isnum) {
        if (C->has && fm.d == C->fmt.d && fm.w == C->fmt.w) {
            if (op != OP_NUM) { avail = predict(M, C, f, pr); }
            for (int p = 0; p < 4; p++) if (avail >> p & 1) C->cost[p] = C->cost[p] - (C->cost[p] >> 4) + 16 * bitlen(zz(v - pr[p]));
            int s2 = choose(C, avail); C->sel = s2 < 0 ? 0 : s2;
            int bl = avail ? bitlen(zz(v - pr[C->sel])) : 0; C->lastres = bl; M->cres[f] = bl;
        } else if (C->has) { C->cost[0] = C->cost[1] = C->cost[2] = C->cost[3] = 0; }
        if (C->has && (fm.d != C->fmt.d || fm.w != C->fmt.w)) C->has = 0;
        C->last2 = C->last; C->last = v; C->fmt = fm; if (C->has < 2) C->has++;
    }
    /* lists */
    if (id != 0) {
        int p = mtf_find(&C->list, id); mtf_front(&C->list, id, p, COLCAP);
        mtf *pl = plist(M, f ? M->cid[f - 1] : 0xFFFFFFFEu, 1);
        p = mtf_find(pl, id); mtf_front(pl, id, p, PREVCAP);
        uint32_t *cd = map_slot(&M->cdmap, (uint64_t)(f < MAXC ? f : MAXC - 1) << 32 | id, 1);
        if (*cd == UINT32_MAX) {
            if (C->nd == C->capd) { C->capd = C->capd * 2 + 16; C->dict = xrealloc(C->dict, C->capd * 4); }
            *cd = C->nd; C->dict[C->nd++] = id;
        }
    }
    M->cid[f] = id; M->cop[f] = op; C->ophist = C->ophist << 4 | op;
}

static void line_end(model *M, size_t nf) {
    uint8_t *t = M->aaop; M->aaop = M->aop; M->aop = M->cop; M->cop = t;
    uint32_t *u = M->aid; M->aid = M->cid; M->cid = u;
    M->ppnf = M->pnf; M->pnf = nf;
}

/* ------------------------------------------------------------------ formula discovery (encoder) */
static int find_formulas(const uint8_t *d, size_t n, uint8_t sep, formula_t *out, int maxout) {
    size_t i = 0, line = 0; int K = -1; const int R = 20000;
    int64_t *V = NULL; uint8_t *D = NULL, *OK = NULL; int rows = 0;
    while (i < n && rows < R) {
        size_t e = i; while (e < n && d[e] != '\n') e++;
        if (line > 0) {
            int nf = 1; for (size_t k = i; k < e; k++) nf += d[k] == sep;
            if (K < 0) { K = nf; if (K > 512) return 0; V = xmalloc(sizeof(int64_t) * K * R); D = xmalloc(K * R); OK = xmalloc(K * R); }
            if (nf == K) {
                size_t fs = i; int f = 0;
                for (size_t k = i; k <= e; k++) if (k == e || d[k] == sep) {
                    nfmt fm; int64_t v; int ok = parse_num(d + fs, k - fs, &v, &fm);
                    V[f * R + rows] = v; D[f * R + rows] = fm.d; OK[f * R + rows] = ok; f++; fs = k + 1;
                }
                rows++;
            }
        }
        line++; i = e + 1;
    }
    if (K <= 2 || rows < 50) { free(V); free(D); free(OK); return 0; }
    static const int KE[5] = {0, 2, 3, 6, 9};
    int nout = 0;
    #define OKr(c, r) ((r) >= 0 && OK[(c) * R + (r)])
    #define PRED(ty, a, b, k, r, p) fpred(ty, k, V[(a) * R + (r)], D[(a) * R + (r)], OKr(a, (r) - 1), (r) ? V[(a) * R + (r) - 1] : 0, \
        (r) ? D[(a) * R + (r) - 1] : 0, V[(b) * R + (r)], D[(b) * R + (r)], OKr(t, (r) - 1), (r) ? V[t * R + (r) - 1] : 0, \
        (r) ? D[t * R + (r) - 1] : 0, D[t * R + (r)], p)
    for (int t = 1; t < K && nout < maxout; t++) {
        int cnt = 0; for (int r = 0; r < rows; r++) cnt += OK[t * R + r];
        if (cnt < rows / 10 || cnt < 50) continue;
        double best = 0.5; int ba = -1, bb = -1, bk = 0, bt = 0;
        for (int ty = 0; ty < 6; ty++) for (int a = 0; a < t; a++) for (int b = 0; b < ((ty == 0 || ty == 5) ? t : 1); b++) for (int q = 0; q < ((ty == 0 || ty == 5) ? 5 : 1); q++) {
            if ((ty == 0 || ty == 5) && a == b) continue;
            int bcol = (ty == 0 || ty == 5) ? b : a;
            int hit = 0, miss = 0, seen = 0;
            for (int r = 1; r < rows && r < 3000 && seen < 64; r++) {
                if (!OK[t * R + r] || !OK[a * R + r] || !OK[bcol * R + r]) continue;
                int64_t p; seen++;
                if (PRED(ty, a, bcol, KE[q], r, &p) && p - V[t * R + r] <= 2 && V[t * R + r] - p <= 2) hit++;
                else if (++miss > 24 || (miss >= 6 && hit * 2 < miss)) break;
            }
            if (seen < 16 || miss > 24 || hit * 2 < miss || hit < seen / 2) continue;
            hit = 0; seen = 0;
            for (int r = 1; r < rows && r < 8000; r++) {
                if (!OK[t * R + r] || !OK[a * R + r] || !OK[bcol * R + r]) continue;
                int64_t p; seen++;
                hit += PRED(ty, a, bcol, KE[q], r, &p) && p - V[t * R + r] <= 2 && V[t * R + r] - p <= 2;
            }
            if (seen >= 32 && (double)hit / seen > best) { best = (double)hit / seen; ba = a; bb = bcol; bk = KE[q]; bt = ty; }
        }
        if (ba >= 0) out[nout++] = (formula_t){t, ba, bb, bk, bt};
    }
    free(V); free(D); free(OK);
    return nout;
}

/* ------------------------------------------------------------------ container */
static uint8_t pick_sep(const uint8_t *d, size_t n) {
    size_t c[5] = {0}; const uint8_t S[5] = {',', '\t', ';', '|', ' '}; size_t m = n < (1 << 20) ? n : (1 << 20);
    for (size_t i = 0; i < m; i++) for (int k = 0; k < 5; k++) c[k] += d[i] == S[k];
    int b = 0; for (int k = 1; k < 5; k++) if (c[k] > c[b]) b = k;
    return S[b];
}
static void build_table(const uint8_t *d, size_t n, uint8_t *table) {
    size_t cnt[256] = {0}; for (size_t i = 0; i < n; i++) cnt[d[i]]++;
    int ord[256]; for (int i = 0; i < 256; i++) ord[i] = i;
    for (int i = 1; i < 256; i++) { int x = ord[i], j = i; while (j > 0 && cnt[ord[j - 1]] < cnt[x]) { ord[j] = ord[j - 1]; j--; } ord[j] = x; }
    for (int i = 0; i < 62; i++) table[i] = ord[i];
}
static void huf_pack(buf *out, const buf *s) {
    static unsigned wk[HUF_WORKSPACE_SIZE / 4 + 64]; static HUF_CElt ct[HUF_CTABLE_SIZE_ST(255)];
    uint64_t n = s->n; buf_add(out, &n, 8);
    for (size_t o = 0; o < s->n; o += 128 * 1024) {
        size_t len = s->n - o < 128 * 1024 ? s->n - o : 128 * 1024; HUF_repeat rep = HUF_repeat_none;
        buf_reserve(out, len + 1024);
        size_t c = HUF_compress4X_repeat(out->p + out->n + 5, len + 512, s->p + o, len, 255, 11, wk, sizeof wk, ct, &rep, 0);
        uint8_t type; uint32_t cs;
        if (HUF_isError(c) || c == 0) { type = 0; cs = len; memcpy(out->p + out->n + 5, s->p + o, len); }
        else if (c == 1) { type = 1; cs = 1; } else { type = 2; cs = c; }
        out->p[out->n] = type; memcpy(out->p + out->n + 1, &cs, 4); out->n += 5 + cs;
    }
}
static size_t huf_unpack(const uint8_t *p, buf *s) {
    HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX); static unsigned dwk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32 * 4];
    uint64_t n; memcpy(&n, p, 8); size_t q = 8; s->p = xmalloc(n + 8); s->n = n; s->cap = n + 8;
    for (size_t o = 0; o < n; o += 128 * 1024) {
        size_t len = n - o < 128 * 1024 ? n - o : 128 * 1024; uint8_t type = p[q]; uint32_t cs; memcpy(&cs, p + q + 1, 4); q += 5;
        if (type == 0) memcpy(s->p + o, p + q, len);
        else if (type == 1) memset(s->p + o, p[q], len);
        else if (HUF_isError(HUF_decompress4X_hufOnly_wksp(dt, s->p + o, len, p + q, cs, dwk, sizeof dwk, 0))) { fprintf(stderr, "huf error\n"); exit(1); }
        q += cs;
    }
    return q;
}

typedef struct { uint8_t magic[4], mode, sep, final_nl, nform; uint64_t n, lines; uint8_t table[62]; } header;

static buf encode(const uint8_t *d, size_t n, int mode, model *M, uio *io) {
    header h = {{'G', '6', 'P', '1'}, mode, pick_sep(d, n), n && d[n - 1] == '\n', 0, n, 0, {0}};
    build_table(d, n, h.table);
    double tf0 = now();
    formula_t F[256]; int nf_ = find_formulas(d, n, h.sep, F, 256); h.nform = nf_;
    if (getenv("TIMING")) fprintf(stderr, "formula search %.2f s\n", now() - tf0);
    model_init(M); memcpy(M->table, h.table, 62);
    for (int i = 0; i < 256; i++) M->code[i] = -1; for (int i = 61; i >= 0; i--) M->code[h.table[i]] = i;
    for (int i = 0; i < nf_; i++) { colst *c = &M->col[F[i].t]; c->fa = F[i].a; c->fb = F[i].b; c->fke = F[i].ke; c->ftype = F[i].type; }
    if (getenv("FORMULAS")) for (int i = 0; i < nf_; i++) fprintf(stderr, "formula t=%d type=%d a=%d b=%d ke=%d\n", F[i].t, F[i].type, F[i].a, F[i].b, F[i].ke);
    memset(io, 0, sizeof *io); io->mode = mode; io->enc = 1;
    buf cmout = {0};
    if (mode == 2) { io->m = xmalloc(sizeof(cm)); cm_init(io->m, n); io->ac = (acoder){0, 0xffffffffu, 0, &cmout, NULL, 0, 0}; }
    size_t i = 0; uint64_t lines = 0;
    while (i < n) {
        size_t e = i; while (e < n && d[e] != '\n') e++;
        size_t nf = 1; for (size_t k = i; k < e; k++) nf += d[k] == h.sep;
        ensure_cap(M, nf);
        uctx x; set_ctx(M, &x, K_NF, 0, nf); val_io(io, &x, nf == M->pnf ? 0 : nf + 1);
        size_t fs = i; int f = 0;
        for (size_t k = i; k <= e; k++) if (k == e || d[k] == h.sep) { field_io(M, io, f++, nf, d + fs, k - fs, NULL); fs = k + 1; }
        line_end(M, nf); lines++; i = e + 1;
    }
    h.lines = lines;
    buf out = {0}; buf_add(&out, &h, sizeof h);
    for (int k = 0; k < nf_; k++) { uint16_t t[3] = {F[k].t, F[k].a, F[k].b}; buf_add(&out, t, 6); buf_put(&out, F[k].ke); buf_put(&out, F[k].type); }
    if (mode == 0) { uint64_t k = io->st[0].n; buf_add(&out, &k, 8); buf_reserve(&out, k); out.n += pack6(io->st[0].p, k, out.p + out.n); }
    else if (mode == 1) for (int s = 0; s < NSTREAM; s++) huf_pack(&out, &io->st[s]);
    else { acoder *a = &io->ac; buf_put(&cmout, a->x1 >> 24); buf_put(&cmout, 255); buf_put(&cmout, 255); buf_put(&cmout, 255); buf_add(&out, cmout.p, cmout.n); }
    return out;
}

static buf decode(const uint8_t *p, size_t pn, model *M, uio *io) {
    header h; memcpy(&h, p, sizeof h); size_t q = sizeof h;
    model_init(M); memcpy(M->table, h.table, 62);
    for (int k = 0; k < h.nform; k++) { uint16_t t[3]; memcpy(t, p + q, 6); colst *c = &M->col[t[0]]; c->fa = t[1]; c->fb = t[2]; c->fke = p[q + 6]; c->ftype = p[q + 7]; q += 8; }
    memset(io, 0, sizeof *io); io->mode = h.mode; io->enc = 0;
    if (h.mode == 0) { uint64_t k; memcpy(&k, p + q, 8); q += 8; io->st[0].p = xmalloc(k + 8); unpack6(p + q, k, io->st[0].p); }
    else if (h.mode == 1) for (int s = 0; s < NSTREAM; s++) q += huf_unpack(p + q, &io->st[s]);
    else {
        io->m = xmalloc(sizeof(cm)); cm_init(io->m, h.n);
        io->ac = (acoder){0, 0xffffffffu, 0, NULL, p + q, 0, pn - q};
        for (int i = 0; i < 4; i++) io->ac.x = (io->ac.x << 8) | (io->ac.pos < io->ac.n ? io->ac.in[io->ac.pos++] : 0);
    }
    buf out = {0}; buf_reserve(&out, h.n + 64);
    for (uint64_t l = 0; l < h.lines; l++) {
        uctx x; set_ctx(M, &x, K_NF, 0, 0); uint64_t v = val_io(io, &x, 0);
        size_t nf = v == 0 ? M->pnf : v - 1; ensure_cap(M, nf);
        for (size_t f = 0; f < nf; f++) { if (f) buf_put(&out, h.sep); field_io(M, io, f, nf, NULL, 0, &out); }
        line_end(M, nf);
        if (l + 1 < h.lines || h.final_nl) buf_put(&out, '\n');
    }
    return out;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: gr6plus c MODE in out | d in out | b MODE in\n"); return 2; }
    model M; uio io;
    if (argv[1][0] == 'c') { size_t n; uint8_t *in = read_file(argv[3], &n); buf o = encode(in, n, atoi(argv[2]), &M, &io); write_file(argv[4], o.p, o.n); return 0; }
    if (argv[1][0] == 'd') { size_t n; uint8_t *in = read_file(argv[2], &n); buf o = decode(in, n, &M, &io); write_file(argv[3], o.p, o.n); return 0; }
    int mode = atoi(argv[2]); size_t n; uint8_t *in = read_file(argv[3], &n);
    int reps = mode == 2 ? 1 : 3; double te = 1e9, td = 1e9; buf o = {0}, back = {0}; uint64_t ops[NOP], units = 0, ku[NKIND]; int nform = 0; double bits_[NKIND];
    for (int r = 0; r < reps; r++) {
        double a = now(); o = encode(in, n, mode, &M, &io); double b = now();
        memcpy(ops, M.opcount, sizeof ops); units = io.units; memcpy(ku, io.kunits, sizeof ku); memcpy(bits_, io.bits, sizeof bits_); nform = ((header *)o.p)->nform;
        back = decode(o.p, o.n, &M, &io); double e = now();
        if (b - a < te) te = b - a; if (e - b < td) td = e - b;
    }
    int ok = back.n == n && memcmp(back.p, in, n) == 0;
    uint64_t tot = 0; for (int k = 0; k < NOP; k++) tot += ops[k];
    printf("%s mode %d: %zu -> %zu (%.2f%%, ratio %.3f)  enc %.2f MB/s  dec %.2f MB/s  %s  | units %lu, formulas %d | ops:",
           strrchr(argv[3], '/') ? strrchr(argv[3], '/') + 1 : argv[3], mode, n, o.n, 100.0 * o.n / n, (double)n / o.n,
           n / te / 1e6, n / td / 1e6, ok ? "OK" : "FAIL", (unsigned long)units, nform);
    for (int k = 0; k < NOP; k++) if (ops[k]) printf(" %s %.0f%%", opname[k], 100.0 * ops[k] / tot);
    printf("\n");
    if (mode == 2) { static const char *kn[NKIND] = {"nf", "op", "rank", "graph", "cdict", "gid", "num", "numnew", "char", "pfx"};
        if (getenv("COLS")) { FILE *cf = fopen(getenv("COLS"), "w"); for (int c = 0; c < MAXC; c++) if (io.colbits[c] > 0) fprintf(cf, "%d %.0f %d %d\n", c, io.colbits[c] / 8, M.col[c].sel, M.col[c].fa); fclose(cf); }
        printf("   bytes by role:"); for (int k = 0; k < NKIND; k++) if (bits_[k] > 8) printf(" %s %.0f", kn[k], bits_[k] / 8); printf("\n"); }
    return !ok;
}
