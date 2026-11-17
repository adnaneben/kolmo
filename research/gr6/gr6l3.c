/* GR6-L3: three-level 6-bit encoding (research prototype, lossless).
 *   level 3  phrases: repeated runs of 2..8 fields (words, values) found by map-reduce mining, chosen by a decision
 *            policy, sent as a dictionary; a match costs one PHRASE op + a rank in the phrase graph (phrases that
 *            followed the same previous field) or a dictionary id
 *   level 2  whole fields/words: GR6+ ops (ditto, column and graph ranks, dictionaries, numbers with predictors)
 *   level 1  letters: prefix + characters in 6-bit codes
 * every value is 6-bit units coded by the GR6+ context-mixing model. Blocks (one per thread) are coded in parallel
 * and share the phrase dictionary, the formulas and the character table.
 * usage: gr6l3 b in [threads] [policy]      policy: none | all | rule[:tau] | sel:FILE
 *        env: MINN (2) MAXN (8) SAMPLE (bytes mined, 4 MiB) FIELDBITS_OUT=f  CAND_OUT=f  FIELDBITS_IN=f
 */
#include "common.h"
#include <math.h>
#include <pthread.h>
#include "zstd.h"

enum { K_NF, K_OP, K_RANK, K_RANK2, K_CDICT, K_GID, K_NUM, K_NUMNEW, K_CHAR, K_PFX, K_PHR, NKIND };
enum { OP_DITTO, OP_EMPTY, OP_NUM, OP_RANK, OP_RANK2, OP_CDICT, OP_GID, OP_NUMNEW, OP_NEW, OP_PHR_R, OP_PHR_G, OP_PHR_M, NOP, OP_COVERED = 12 };
static const char *opname[NOP] = {"ditto", "empty", "num", "rank", "graph", "cdict", "gid", "numnew", "new", "phrase-rank", "phrase-id", "phrase-match"};
#define MAXC 4096
#define COLCAP 256
#define PREVCAP 256
#define PHRCAP 64

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
    acoder ac; cm *m; double bits[NKIND]; double fbits; int curf;
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
    { double b_ = -log2((bit ? p : 4096 - p) / 4096.0); io->bits[x->kind] += b_; io->fbits += b_; }
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
    {
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

/* ------------------------------------------------------------------ phrase dictionary (level 3), shared read-only */
typedef struct {
    uint32_t n;                    /* phrases */
    uint8_t *len;                  /* fields per phrase */
    uint32_t *first;               /* index of the phrase's first field in foff/flen */
    uint32_t *foff, *flen;         /* field strings in blob */
    buf blob;
    map64 look;                    /* key: hash of the field hashes and the length -> phrase index (encoder) */
    int maxn;
} phdict;
static inline uint64_t fhash(const uint8_t *s, size_t n) { return mix64(hash_bytes(s, n) ^ (uint64_t)n << 32 ^ 0x51ED270B9C1D2A57ull); }
static inline uint64_t seqkey(const uint64_t *fh, int n) {
    uint64_t h = 0x9E3779B97F4A7C15ull * (uint64_t)n;
    for (int i = 0; i < n; i++) h = mix64(h ^ fh[i]) + (uint64_t)i;
    return h == UINT64_MAX ? 0 : h;
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
    uint64_t opcount[NOP], covered;
    const phdict *ph; map64 phmap; mtf *phl; size_t nphl, capphl;   /* phrase graph: previous field -> recent phrases */
    int lzp, minn, level1; const uint8_t *colL1; uint32_t *hist; size_t nh, caph; map64 hmap;      /* match model: field history, (prev 2 fields) -> next position */
} model;

static void model_init(model *M) {
    memset(M, 0, sizeof *M); intern_init(&M->tok); map_init(&M->cdmap, 16); map_init(&M->pmap, 16); map_init(&M->phmap, 12); map_init(&M->hmap, 16);
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
static mtf *phlist(model *M, uint32_t key, int insert) {
    uint32_t *v = map_slot(&M->phmap, key, insert); if (!v) return NULL;
    if (*v == UINT32_MAX) {
        if (M->nphl == M->capphl) { M->capphl = M->capphl * 2 + 256; M->phl = xrealloc(M->phl, M->capphl * sizeof(mtf)); }
        memset(&M->phl[M->nphl], 0, sizeof(mtf)); *v = M->nphl++;
    }
    return &M->phl[*v];
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

static void commit(model *M, int f, uint32_t id, int op, const uint8_t *fs, int fn, int isnum, int64_t v, nfmt fm, int avail, int64_t *pr);
#define HLINE 0xFFFFFFFFu
static inline uint64_t hkey(model *M, int f) {
    uint32_t p1 = f ? M->cid[f - 1] : HLINE, p2 = f > 1 ? M->cid[f - 2] : f == 1 ? HLINE : HLINE - 1;
    return ((uint64_t)p2 << 32 | p1) ^ 0x7A3B9C1D00000000ull;
}
/* match model: fields that followed the same two previous fields last time; returns the history position or -1 */
static long hpred(model *M, int f) { uint32_t *v = map_slot(&M->hmap, hkey(M, f), 0); return v ? (long)*v : -1; }

/* code one field (or, at level 3, a whole phrase). Encoder: s/n is the field, pidx the phrase chosen here (-1 none).
 * Decoder: fields are appended to out. Returns the number of fields consumed. */
static int field_io(model *M, uio *io, int f, int nf, const uint8_t *s, int n, buf *out, int pidx, uint8_t sep) {
    colst *C = &M->col[f < MAXC ? f : MAXC - 1];
    io->curf = f < MAXC ? f : MAXC - 1;
    uctx x; int op = 0; uint32_t id = 0; int64_t v = 0; nfmt fm = {0, 0}; int isnum = 0;
    int64_t pr[4] = {0, 0, 0, 0}; int avail = 0, sel = -1;
    uint8_t tmp[64];
    uint32_t pkey = f ? M->cid[f - 1] : 0xFFFFFFFEu;
    if (io->enc && pidx <= -2) op = OP_PHR_M;
    else if (io->enc && pidx >= 0) { mtf *pl = phlist(M, pkey, 0); op = pl && mtf_find(pl, pidx) >= 0 ? OP_PHR_R : OP_PHR_G; }
    else if (io->enc) {
        id = intern_get(&M->tok, s, n, 0, NULL);
        isnum = parse_num(s, n, &v, &fm);
        int pos;
        if (M->level1) op = n == 0 ? OP_EMPTY : OP_NEW;      /* ablation: letters only */
        else if (M->colL1 && M->colL1[f < MAXC ? f : MAXC - 1])   /* level decision for this column: letters */
            op = f < (int)M->pnf && id != UINT32_MAX && M->aid[f] == id ? OP_DITTO : n == 0 ? OP_EMPTY : OP_NEW;
        else if (f < (int)M->pnf && id != UINT32_MAX && M->aid[f] == id) op = OP_DITTO;
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
    if (op >= NOP) { fprintf(stderr, "corrupt op\n"); exit(1); }
    M->opcount[op]++;
    if (op == OP_PHR_M) {                                   /* level 3 without a dictionary: copy fields from history */
        long p = hpred(M, f); if (p < 0) { fprintf(stderr, "corrupt match\n"); exit(1); }
        set_ctx(M, &x, K_PHR, f, nf); x.h[0] = H(x.h[0], op); x.h[6] = H(x.h[6], op);
        uint64_t m = io->enc ? (uint64_t)(-2 - pidx) : 0;
        m = io->enc ? (val_io(io, &x, m - M->minn), m) : val_io(io, &x, 0) + M->minn;
        size_t lim = M->nh;
        if (m > (uint64_t)(nf - f) || (size_t)p + m > lim) { fprintf(stderr, "corrupt match length\n"); exit(1); }
        for (uint64_t q = 0; q < m; q++) {
            uint32_t pid = M->hist[p + q]; if (pid == HLINE) { fprintf(stderr, "corrupt match\n"); exit(1); }
            const uint8_t *fs = intern_str(&M->tok, pid); int fn = M->tok.len[pid];
            if (!io->enc) { if (q) buf_put(out, sep); buf_add(out, fs, fn); }
            int64_t pv = 0; nfmt pfm = {0, 0}; int pn = parse_num(fs, fn, &pv, &pfm);
            int64_t ppr[4]; commit(M, f + (int)q, pid, q ? OP_COVERED : op, fs, fn, pn, pv, pfm, 0, ppr);
        }
        M->covered += m;
        return (int)m;
    }
    if (op == OP_PHR_R || op == OP_PHR_G) {                 /* level 3: a whole phrase */
        const phdict *P = M->ph; mtf *pl = phlist(M, pkey, 1);
        set_ctx(M, &x, K_PHR, f, nf); x.h[0] = H(x.h[0], op); x.h[6] = H(x.h[6], op);
        uint64_t r = op == OP_PHR_R ? (io->enc ? (uint64_t)mtf_find(pl, pidx) : 0) : (io->enc ? (uint64_t)pidx : 0);
        r = io->enc ? (val_io(io, &x, r), r) : val_io(io, &x, 0);
        int pi = op == OP_PHR_R ? (int)pl->a[r] : (int)r;
        if (!P || pi < 0 || (uint32_t)pi >= P->n || f + P->len[pi] > nf) { fprintf(stderr, "corrupt phrase\n"); exit(1); }
        mtf_front(pl, pi, mtf_find(pl, pi), PHRCAP);
        int L = P->len[pi];
        for (int q = 0; q < L; q++) {
            const uint8_t *fs = P->blob.p + P->foff[P->first[pi] + q]; int fn = P->flen[P->first[pi] + q];
            if (!io->enc) { if (q) buf_put(out, sep); buf_add(out, fs, fn); }
            int64_t pv = 0; nfmt pfm = {0, 0}; int pn = parse_num(fs, fn, &pv, &pfm);
            uint32_t pid = intern_get(&M->tok, fs, fn, 1, NULL);
            int64_t ppr[4]; commit(M, f + q, pid, q ? OP_COVERED : op, fs, fn, pn, pv, pfm, 0, ppr);
        }
        M->covered += L;
        return L;
    }
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
        uint32_t ref = M->level1 ? 0 : gl && gl->n ? gl->a[0] : C->list.n ? C->list.a[0] : 0;
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
            if (!io->enc) { buf_put(&lit, ch); if (lit.n > (1u << 24)) { fprintf(stderr, "corrupt literal\n"); exit(1); } }
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
    if (op != OP_NUM && op != OP_NUMNEW) isnum = parse_num(fs, fn, &v, &fm);
    commit(M, f, id, op, fs, fn, isnum, v, fm, op == OP_NUM ? avail : -1, pr);
    return 1;
}

/* state updates after a field is known: numeric predictors, column and graph lists, dictionaries, ops */
static void commit(model *M, int f, uint32_t id, int op, const uint8_t *fs, int fn, int isnum, int64_t v, nfmt fm, int avail, int64_t *pr) {
    colst *C = &M->col[f < MAXC ? f : MAXC - 1]; (void)fs; (void)fn;
    M->cv[f] = v; M->cf[f] = fm; M->cok[f] = isnum; M->cres[f] = 0;
    if (isnum) {
        if (C->has && fm.d == C->fmt.d && fm.w == C->fmt.w) {
            if (avail < 0 || op != OP_NUM) avail = predict(M, C, f, pr);
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
    if (op == OP_COVERED && f < (int)M->pnf && M->aid[f] == id) op = OP_DITTO;
    if (M->lzp) {
        *map_slot(&M->hmap, hkey(M, f), 1) = (uint32_t)M->nh;
        if (M->nh + 2 > M->caph) { M->caph = M->caph * 2 + 65536; M->hist = xrealloc(M->hist, M->caph * 4); }
        M->hist[M->nh++] = id;
    }
    M->cid[f] = id; M->cop[f] = op; C->ophist = C->ophist << 4 | op;
}

static void line_end(model *M, size_t nf) {
    if (M->lzp) { if (M->nh + 2 > M->caph) { M->caph = M->caph * 2 + 65536; M->hist = xrealloc(M->hist, M->caph * 4); } M->hist[M->nh++] = HLINE; }
    uint8_t *t = M->aaop; M->aaop = M->aop; M->aop = M->cop; M->cop = t;
    uint32_t *u = M->aid; M->aid = M->cid; M->cid = u;
    M->ppnf = M->pnf; M->pnf = nf;
}

/* ------------------------------------------------------------------ formula discovery (encoder) */
typedef struct { const int64_t *V; const uint8_t *D, *OK; int R, rows, t0, t1; formula_t *best; int step; } fjob;
static void *formula_worker(void *arg);
static int formula_threads = 1;
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
    /* map: each thread searches a range of target columns; reduce: the targets' best formulas, in column order */
    formula_t *best = xmalloc(sizeof(formula_t) * K); for (int t = 0; t < K; t++) best[t].a = -1;
    int T = formula_threads < 1 ? 1 : formula_threads; if (T > K) T = K;
    fjob *jobs = xmalloc(sizeof(fjob) * T); pthread_t *th = xmalloc(sizeof(pthread_t) * T);
    for (int k = 0; k < T; k++) jobs[k] = (fjob){V, D, OK, R, rows, 1 + k, K, best};   /* interleaved: work grows with t */
    for (int k = 0; k < T; k++) jobs[k].step = T;
    for (int k = 1; k < T; k++) pthread_create(&th[k], NULL, formula_worker, &jobs[k]);
    formula_worker(&jobs[0]);
    for (int k = 1; k < T; k++) pthread_join(th[k], NULL);
    int nout = 0;
    for (int t = 1; t < K && nout < maxout; t++) if (best[t].a >= 0) out[nout++] = best[t];
    free(best); free(jobs); free(th); free(V); free(D); free(OK);
    return nout;
}
static void *formula_worker(void *arg) {
    fjob *J = arg; const int64_t *V = J->V; const uint8_t *D = J->D, *OK = J->OK; const int R = J->R, rows = J->rows;
    static const int KE[5] = {0, 2, 3, 6, 9};
    #define OKr(c, r) ((r) >= 0 && OK[(c) * R + (r)])
    #define PRED(ty, a, b, k, r, p) fpred(ty, k, V[(a) * R + (r)], D[(a) * R + (r)], OKr(a, (r) - 1), (r) ? V[(a) * R + (r) - 1] : 0, \
        (r) ? D[(a) * R + (r) - 1] : 0, V[(b) * R + (r)], D[(b) * R + (r)], OKr(t, (r) - 1), (r) ? V[t * R + (r) - 1] : 0, \
        (r) ? D[t * R + (r) - 1] : 0, D[t * R + (r)], p)
    for (int t = J->t0; t < J->t1; t += J->step) {
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
        if (ba >= 0) J->best[t] = (formula_t){t, ba, bb, bk, bt};
    }
    return NULL;
}


/* ------------------------------------------------------------------ level 3: phrase mining (map-reduce over threads) */
typedef struct { uint32_t cnt, n, bytes; uint64_t pos; } ngram;
typedef struct { map64 m; ngram *g; size_t ng, cap; map64 uni; uint32_t *ucnt; size_t nu, capu; uint64_t fields; } mine_part;
typedef struct { const uint8_t *d; size_t a, b; uint8_t sep; int minn, maxn; mine_part *out; } mine_job;

static void part_add(mine_part *P, uint64_t key, uint32_t n, uint32_t bytes, uint64_t pos, uint32_t c) {
    uint32_t *v = map_slot(&P->m, key, 1);
    if (*v == UINT32_MAX) {
        if (P->ng == P->cap) { P->cap = P->cap * 2 + 4096; P->g = xrealloc(P->g, P->cap * sizeof(ngram)); }
        P->g[P->ng] = (ngram){0, n, bytes, pos}; *v = P->ng++;
    }
    ngram *g = &P->g[*v]; g->cnt += c; if (pos < g->pos) g->pos = pos;
}
static void uni_add(mine_part *P, uint64_t key, uint32_t c) {
    uint32_t *v = map_slot(&P->uni, key, 1);
    if (*v == UINT32_MAX) { if (P->nu == P->capu) { P->capu = P->capu * 2 + 4096; P->ucnt = xrealloc(P->ucnt, P->capu * 4); } P->ucnt[P->nu] = 0; *v = P->nu++; }
    P->ucnt[*v] += c;
}
/* fields of one line: hashes and start offsets (fo[nf] = end + 1) */
static size_t line_fields(const uint8_t *d, size_t i, size_t e, uint8_t sep, uint64_t **fh, size_t **fo, size_t *cap) {
    size_t nf = 1; for (size_t k = i; k < e; k++) nf += d[k] == sep;
    if (nf + 1 > *cap) { *cap = (nf + 1) * 2; *fh = xrealloc(*fh, *cap * 8); *fo = xrealloc(*fo, *cap * sizeof(size_t)); }
    size_t f = 0, s0 = i;
    for (size_t k = i; k <= e; k++) if (k == e || d[k] == sep) { (*fo)[f] = s0; (*fh)[f] = fhash(d + s0, k - s0); f++; s0 = k + 1; }
    (*fo)[nf] = e + 1;
    return nf;
}
static void *mine_worker(void *arg) {   /* map: count unigrams and 2..maxn-field n-grams in one slice of lines */
    mine_job *J = arg; mine_part *P = J->out; map_init(&P->m, 16); map_init(&P->uni, 14);
    uint64_t *fh = NULL; size_t *fo = NULL, cap = 0;
    for (size_t i = J->a; i < J->b;) {
        const uint8_t *q = memchr(J->d + i, '\n', J->b - i); size_t e = q ? (size_t)(q - J->d) : J->b;
        size_t nf = line_fields(J->d, i, e, J->sep, &fh, &fo, &cap);
        P->fields += nf;
        for (size_t f = 0; f < nf; f++) {
            uni_add(P, fh[f], 1);
            for (int n = J->minn > 2 ? 2 : J->minn; n <= J->maxn && f + n <= nf; n++) {
                size_t bytes = fo[f + n] - 1 - fo[f]; if (bytes > 255) break;
                part_add(P, seqkey(fh + f, n), n, (uint32_t)bytes, fo[f], 1);
            }
        }
        i = e + 1;
    }
    free(fh); free(fo); return NULL;
}

typedef struct { uint64_t key, pos; uint32_t n, cnt, bytes, nnum, nempty; double sumsi, maxsi, score; int keep; } cand;
typedef struct { cand *c; size_t n; double t_map, t_reduce; uint64_t fields; } candset;

static int cand_cmp(const void *a, const void *b) {
    const cand *x = a, *y = b; uint64_t vx = (uint64_t)x->cnt * x->n, vy = (uint64_t)y->cnt * y->n;
    if (vx != vy) return vx > vy ? -1 : 1;
    return x->key < y->key ? -1 : x->key > y->key;
}
/* mine the first `sample` bytes with T threads; reduce; keep closed candidates (count >= 2, n >= minn) with features */
static candset mine(const uint8_t *d, size_t n, size_t sample, uint8_t sep, int minn, int maxn, int T) {
    candset R = {0};
    size_t S = n < sample ? n : sample; if (S < n) { const uint8_t *q = memchr(d + S, '\n', n - S); S = q ? (size_t)(q - d) + 1 : n; }
    double t0 = now();
    mine_part *parts = xmalloc(sizeof(mine_part) * T); memset(parts, 0, sizeof(mine_part) * T);
    mine_job *jobs = xmalloc(sizeof(mine_job) * T); pthread_t *th = xmalloc(sizeof(pthread_t) * T);
    size_t a = 0;
    for (int t = 0; t < T; t++) {
        size_t b = t + 1 == T ? S : S * (t + 1) / T; if (b < S) { const uint8_t *q = memchr(d + b, '\n', S - b); b = q ? (size_t)(q - d) + 1 : S; } if (b < a) b = a;
        jobs[t] = (mine_job){d, a, b, sep, minn, maxn, &parts[t]}; a = b;
    }
    for (int t = 1; t < T; t++) pthread_create(&th[t], NULL, mine_worker, &jobs[t]);
    mine_worker(&jobs[0]);
    for (int t = 1; t < T; t++) pthread_join(th[t], NULL);
    double t1 = now();
    /* reduce: merge every part into part 0, in order */
    mine_part *G = &parts[0];
    for (int t = 1; t < T; t++) {
        mine_part *P = &parts[t];
        for (uint64_t j = 0; j <= P->m.mask; j++) if (P->m.k[j] != UINT64_MAX) { ngram *g = &P->g[P->m.v[j]]; part_add(G, P->m.k[j], g->n, g->bytes, g->pos, g->cnt); }
        for (uint64_t j = 0; j <= P->uni.mask; j++) if (P->uni.k[j] != UINT64_MAX) uni_add(G, P->uni.k[j], P->ucnt[P->uni.v[j]]);
        G->fields += P->fields;
        free(P->m.k); free(P->m.v); free(P->g); free(P->uni.k); free(P->uni.v); free(P->ucnt);
    }
    /* closed patterns: drop an n-gram when an (n+1)-gram that extends it occurs as often */
    uint8_t *drop = xmalloc(G->ng + 1); memset(drop, 0, G->ng + 1);
    uint64_t *fh = NULL; size_t *fo = NULL, cap = 0;
    for (size_t k = 0; k < G->ng; k++) {
        ngram *g = &G->g[k]; if (g->cnt < 2 || g->n < 3) continue;
        const uint8_t *q = memchr(d + g->pos, '\n', S - g->pos); size_t e = q ? (size_t)(q - d) : S;
        size_t nf = line_fields(d, g->pos, e, sep, &fh, &fo, &cap); (void)nf;
        for (int side = 0; side < 2; side++) {
            uint32_t *v = map_slot(&G->m, seqkey(fh + side, g->n - 1), 0);
            if (v && G->g[*v].cnt == g->cnt) drop[*v] = 1;
        }
    }
    double N1 = (double)G->fields;
    for (size_t k = 0; k < G->ng; k++) {
        ngram *g = &G->g[k];
        if (g->cnt < 2 || (int)g->n < minn || drop[k]) continue;
        const uint8_t *q = memchr(d + g->pos, '\n', S - g->pos); size_t e = q ? (size_t)(q - d) : S;
        line_fields(d, g->pos, e, sep, &fh, &fo, &cap);
        cand c = {seqkey(fh, g->n), g->pos, g->n, g->cnt, g->bytes, 0, 0, 0, 0, 0, 0};
        for (uint32_t f = 0; f < g->n; f++) {
            uint32_t *u = map_slot(&G->uni, fh[f], 0); double si = log2(N1 / (u ? G->ucnt[*u] : 1));
            c.sumsi += si; if (si > c.maxsi) c.maxsi = si;
            int64_t v; nfmt fm; size_t fl = fo[f + 1] - 1 - fo[f];
            c.nnum += parse_num(d + fo[f], fl, &v, &fm); c.nempty += fl == 0;
        }
        if (R.n % 4096 == 0) R.c = xrealloc(R.c, (R.n + 4096) * sizeof(cand));
        R.c[R.n++] = c;
    }
    qsort(R.c, R.n, sizeof(cand), cand_cmp);
    R.t_map = t1 - t0; R.t_reduce = now() - t1; R.fields = G->fields;
    free(drop); free(fh); free(fo); free(G->m.k); free(G->m.v); free(G->g); free(G->uni.k); free(G->uni.v); free(G->ucnt);
    free(parts); free(jobs); free(th);
    return R;
}

/* ------------------------------------------------------------------ level 3: decision policies */
/* rule: estimated saving in bits = (count - 1) * alpha * (sum of field self-information)
 *                                  - count * (log2(dictionary size) + 4)  - 8 * beta * phrase bytes        > tau
 * alpha: how much of the unigram self-information the level-2 model still pays (it has contexts, so < 1);
 * beta: dictionary bytes after compression, per raw byte. */
static size_t apply_policy(candset *C, const char *policy, size_t limit) {
    size_t kept = 0;
    if (!strcmp(policy, "none")) { for (size_t i = 0; i < C->n; i++) C->c[i].keep = 0; return 0; }
    if (!strcmp(policy, "all")) { for (size_t i = 0; i < C->n; i++) kept += C->c[i].keep = kept < limit; return kept; }
    if (!strncmp(policy, "rule", 4)) {
        double tau = policy[4] == ':' ? atof(policy + 5) : 0;
        double alpha = getenv("ALPHA") ? atof(getenv("ALPHA")) : 0.25, beta = getenv("BETA") ? atof(getenv("BETA")) : 0.4;
        double lk = log2((double)(C->n < limit ? C->n : limit) + 2);
        for (size_t i = 0; i < C->n; i++) {
            cand *c = &C->c[i];
            c->score = (c->cnt - 1.0) * alpha * c->sumsi - c->cnt * (lk + 4) - 8 * beta * c->bytes;
            kept += c->keep = c->score > tau && kept < limit;
        }
        return kept;
    }
    if (!strncmp(policy, "sel:", 4)) {          /* a selection made elsewhere (the learned models): one index per line */
        for (size_t i = 0; i < C->n; i++) C->c[i].keep = 0;
        FILE *f = fopen(policy + 4, "r"); if (!f) { perror(policy + 4); exit(1); }
        long ix; while (fscanf(f, "%ld", &ix) == 1) if (ix >= 0 && (size_t)ix < C->n && !C->c[ix].keep && kept < limit) { C->c[ix].keep = 1; kept++; }
        fclose(f); return kept;
    }
    fprintf(stderr, "unknown policy %s\n", policy); exit(2);
}

static void dict_build(phdict *P, const uint8_t *d, size_t n, uint8_t sep, const candset *C) {
    memset(P, 0, sizeof *P); map_init(&P->look, 12);
    uint64_t *fh = NULL; size_t *fo = NULL, cap = 0; size_t nfld = 0;
    for (size_t i = 0; i < C->n; i++) {
        const cand *c = &C->c[i]; if (!c->keep) continue;
        const uint8_t *q = memchr(d + c->pos, '\n', n - c->pos); size_t e = q ? (size_t)(q - d) : n;
        line_fields(d, c->pos, e, sep, &fh, &fo, &cap);
        if (P->n % 1024 == 0) { P->len = xrealloc(P->len, P->n + 1024); P->first = xrealloc(P->first, (P->n + 1024) * 4); }
        P->len[P->n] = c->n; P->first[P->n] = nfld;
        for (uint32_t f = 0; f < c->n; f++) {
            if (nfld % 4096 == 0) { P->foff = xrealloc(P->foff, (nfld + 4096) * 4); P->flen = xrealloc(P->flen, (nfld + 4096) * 4); }
            size_t fl = fo[f + 1] - 1 - fo[f]; P->foff[nfld] = P->blob.n; P->flen[nfld] = fl; buf_add(&P->blob, d + fo[f], fl); nfld++;
        }
        *map_slot(&P->look, seqkey(fh, c->n), 1) = P->n;
        if ((int)c->n > P->maxn) P->maxn = c->n;
        P->n++;
    }
    free(fh); free(fo);
}
static void dict_write(const phdict *P, buf *out) {   /* count u32 | zstd(n u8, field len u8 + bytes ...) */
    buf raw = {0};
    for (uint32_t p = 0; p < P->n; p++) {
        buf_put(&raw, P->len[p]);
        for (int q = 0; q < P->len[p]; q++) { uint32_t k = P->first[p] + q; buf_put(&raw, P->flen[k]); buf_add(&raw, P->blob.p + P->foff[k], P->flen[k]); }
    }
    uint32_t cnt = P->n; buf_add(out, &cnt, 4);
    size_t cb = ZSTD_compressBound(raw.n); uint64_t rn = raw.n; buf_add(out, &rn, 8); buf_reserve(out, cb + 8);
    uint64_t cz = raw.n ? ZSTD_compress(out->p + out->n + 8, cb, raw.p, raw.n, 19) : 0; memcpy(out->p + out->n, &cz, 8); out->n += 8 + cz;
    free(raw.p);
}
static size_t dict_read(phdict *P, const uint8_t *p) {
    memset(P, 0, sizeof *P); uint32_t cnt; uint64_t rn, cz; memcpy(&cnt, p, 4); memcpy(&rn, p + 4, 8); memcpy(&cz, p + 12, 8);
    uint8_t *raw = xmalloc(rn + 1); if (rn) ZSTD_decompress(raw, rn, p + 20, cz);
    P->len = xmalloc(cnt + 1); P->first = xmalloc((cnt + 1) * 4); size_t nfld = 0, q = 0, capf = 0;
    for (uint32_t i = 0; i < cnt; i++) {
        P->len[i] = raw[q++]; P->first[i] = nfld;
        for (int f = 0; f < P->len[i]; f++) {
            if (nfld == capf) { capf = capf * 2 + 1024; P->foff = xrealloc(P->foff, capf * 4); P->flen = xrealloc(P->flen, capf * 4); }
            uint32_t fl = raw[q++]; P->foff[nfld] = P->blob.n; P->flen[nfld] = fl; buf_add(&P->blob, raw + q, fl); q += fl; nfld++;
        }
    }
    P->n = cnt; free(raw);
    return 20 + cz;
}

/* ------------------------------------------------------------------ blocks (one model each), coded in parallel */
typedef struct { uint8_t sep; uint8_t table[62]; int nform; formula_t F[256]; phdict ph; int phrases; int minn; int lzp; uint8_t *colL1; int level1; } shared;
typedef struct {
    const shared *S; const uint8_t *d; size_t n;             /* encoder input / decoder output length */
    buf out; const uint8_t *in; size_t inlen;               /* encoder output / decoder input */
    float *fbits; size_t nfb;                               /* per-field bits (encoder, research) */
    uint64_t ops[NOP], covered, fields, phr_used;
} blockjob;

static void model_setup(model *M, const shared *S) {
    model_init(M); memcpy(M->table, S->table, 62);
    for (int i = 0; i < 256; i++) M->code[i] = -1; for (int i = 61; i >= 0; i--) M->code[S->table[i]] = i;
    for (int i = 0; i < S->nform; i++) { colst *c = &M->col[S->F[i].t]; c->fa = S->F[i].a; c->fb = S->F[i].b; c->fke = S->F[i].ke; c->ftype = S->F[i].type; }
    M->ph = S->phrases ? &S->ph : NULL; M->lzp = S->lzp; M->minn = S->minn; M->level1 = S->level1 || getenv("LEVEL1") != NULL; M->colL1 = S->colL1;
}
static void *enc_block(void *arg) {
    blockjob *J = arg; const shared *S = J->S; const uint8_t *d = J->d; size_t n = J->n;
    model *M = xmalloc(sizeof(model)); model_setup(M, S);
    uio *io = xmalloc(sizeof(uio)); memset(io, 0, sizeof *io); io->mode = 2; io->enc = 1;
    buf cm_out = {0}; io->m = xmalloc(sizeof(cm)); cm_init(io->m, n); io->ac = (acoder){0, 0xffffffffu, 0, &cm_out, NULL, 0, 0};
    uint64_t lines = n && d[n - 1] != '\n'; for (const uint8_t *p = d, *e = d + n; (p = memchr(p, '\n', (size_t)(e - p))) != NULL; p++) lines++;
    uint64_t *fh = NULL; size_t *fo = NULL, cap = 0;
    for (size_t i = 0; i < n;) {
        const uint8_t *q = memchr(d + i, '\n', n - i); size_t e = q ? (size_t)(q - d) : n;
        size_t nf = line_fields(d, i, e, S->sep, &fh, &fo, &cap);
        ensure_cap(M, nf);
        uctx x; set_ctx(M, &x, K_NF, 0, nf); val_io(io, &x, nf == M->pnf ? 0 : nf + 1);
        for (size_t f = 0; f < nf;) {
            int pidx = -1;
            if (S->lzp) {
                long p = hpred(M, (int)f);
                if (p >= 0) {
                    size_t m = 0; int allditto = 1;
                    while (f + m < nf && (size_t)p + m < M->nh && M->hist[p + m] != HLINE) {
                        size_t fl = fo[f + m + 1] - 1 - fo[f + m]; uint32_t id = intern_get(&M->tok, d + fo[f + m], fl, 0, NULL);
                        if (id != M->hist[p + m]) break;
                        if (!(f + m < M->pnf && M->aid[f + m] == id)) allditto = 0;
                        m++;
                    }
                    if (m >= (size_t)S->minn && !allditto) pidx = -2 - (int)m;
                }
            }
            if (pidx == -1 && S->phrases && S->ph.n) {
                for (int L = (int)(nf - f < (size_t)S->ph.maxn ? nf - f : (size_t)S->ph.maxn); L >= S->minn && pidx < 0; L--) {
                    uint32_t *v = map_slot((map64 *)&S->ph.look, seqkey(fh + f, L), 0); if (!v) continue;
                    uint32_t p = *v; if (S->ph.len[p] != L) continue;
                    int ok = 1, allditto = 1;
                    for (int t = 0; t < L && ok; t++) {
                        size_t fl = fo[f + t + 1] - 1 - fo[f + t]; uint32_t k = S->ph.first[p] + t;
                        ok = fl == S->ph.flen[k] && !memcmp(d + fo[f + t], S->ph.blob.p + S->ph.foff[k], fl);
                        if (allditto) { uint32_t id = intern_get(&M->tok, d + fo[f + t], fl, 0, NULL); allditto = id != UINT32_MAX && f + t < M->pnf && M->aid[f + t] == id; }
                    }
                    if (ok && !allditto) pidx = (int)p;   /* occurrence-level decision: a run the line above repeats is already cheap */
                }
            }
            io->fbits = 0;
            int used = field_io(M, io, (int)f, (int)nf, d + fo[f], (int)(fo[f + 1] - 1 - fo[f]), NULL, pidx, S->sep);
            if (J->fbits) { for (int t = 0; t < used; t++) { if (J->nfb % 65536 == 0) J->fbits = xrealloc(J->fbits, (J->nfb + 65536) * 4); J->fbits[J->nfb++] = (float)(io->fbits / used); } }
            J->phr_used += used > 1; f += used;
        }
        J->fields += nf;
        line_end(M, nf); i = e + 1;
    }
    acoder *a = &io->ac; buf_put(&cm_out, a->x1 >> 24); buf_put(&cm_out, 255); buf_put(&cm_out, 255); buf_put(&cm_out, 255);
    uint8_t fl = n && d[n - 1] == '\n'; buf_add(&J->out, &lines, 8); buf_put(&J->out, fl); buf_add(&J->out, cm_out.p, cm_out.n);
    memcpy(J->ops, M->opcount, sizeof J->ops); J->covered = M->covered;
    free(fh); free(fo); free(cm_out.p);
    for (int i = 0; i < NM; i++) free(io->m->T[i]); free(io->m->apm); free(io->m); free(io);
    return NULL;
}
static void *dec_block(void *arg) {
    blockjob *J = arg; const shared *S = J->S;
    model *M = xmalloc(sizeof(model)); model_setup(M, S);
    uio *io = xmalloc(sizeof(uio)); memset(io, 0, sizeof *io); io->mode = 2; io->enc = 0;
    uint64_t lines; memcpy(&lines, J->in, 8); int final_nl = J->in[8];
    io->m = xmalloc(sizeof(cm)); cm_init(io->m, J->n);
    io->ac = (acoder){0, 0xffffffffu, 0, NULL, J->in + 9, 0, J->inlen - 9};
    for (int i = 0; i < 4; i++) io->ac.x = (io->ac.x << 8) | (io->ac.pos < io->ac.n ? io->ac.in[io->ac.pos++] : 0);
    buf_reserve(&J->out, J->n + 64);
    for (uint64_t l = 0; l < lines; l++) {
        uctx x; set_ctx(M, &x, K_NF, 0, 0); uint64_t v = val_io(io, &x, 0);
        size_t nf = v == 0 ? M->pnf : v - 1; ensure_cap(M, nf);
        for (size_t f = 0; f < nf;) { if (f) buf_put(&J->out, S->sep); f += field_io(M, io, (int)f, (int)nf, NULL, 0, &J->out, -1, S->sep); }
        line_end(M, nf);
        if (l + 1 < lines || final_nl) buf_put(&J->out, '\n');
    }
    for (int i = 0; i < NM; i++) free(io->m->T[i]); free(io->m->apm); free(io->m); free(io);
    return NULL;
}
static void run_jobs(void *(*fn)(void *), blockjob *J, int nb) {
    pthread_t *th = xmalloc(sizeof(pthread_t) * nb);
    for (int b = 1; b < nb; b++) pthread_create(&th[b], NULL, fn, &J[b]);
    fn(&J[0]);
    for (int b = 1; b < nb; b++) pthread_join(th[b], NULL);
    free(th);
}

/* ------------------------------------------------------------------ level decision per column (encoder only)
 * letters (level 1) for a column whose sampled values are mostly distinct and not numbers: word-level decisions
 * (ranks, dictionary ids) cost more there than spelling the value with the column's character contexts */
static uint8_t *column_levels(const uint8_t *d, size_t n, uint8_t sep, double tau, int *ncols) {
    uint8_t *L = xmalloc(MAXC); memset(L, 0, MAXC);
    uint32_t *ne = calloc(MAXC, 4), *num = calloc(MAXC, 4), *dist = calloc(MAXC, 4);
    map64 seen; map_init(&seen, 16); uint64_t *fh = NULL; size_t *fo = NULL, cap = 0; size_t lines = 0;
    for (size_t i = 0; i < n && lines < 20000; lines++) {
        const uint8_t *q = memchr(d + i, '\n', n - i); size_t e = q ? (size_t)(q - d) : n;
        size_t nf = line_fields(d, i, e, sep, &fh, &fo, &cap);
        for (size_t f = 0; f < nf && f < MAXC; f++) {
            size_t fl = fo[f + 1] - 1 - fo[f]; if (!fl) continue;
            ne[f]++; int64_t v; nfmt fm; num[f] += parse_num(d + fo[f], fl, &v, &fm);
            uint32_t *x = map_slot(&seen, mix64(fh[f] ^ (uint64_t)f * 0x9E3779B97F4A7C15ull), 1); if (*x == UINT32_MAX) { *x = 1; dist[f]++; }
        }
        i = e + 1;
    }
    int k = 0;
    for (int f = 0; f < MAXC; f++) if (ne[f] >= 20 && num[f] * 2 < ne[f] && dist[f] >= tau * ne[f]) { L[f] = 1; k++; }
    *ncols = k; free(ne); free(num); free(dist); free(seen.k); free(seen.v); free(fh); free(fo);
    return L;
}

/* ------------------------------------------------------------------ level decision by racing (encoder only)
 * the configurations (letters only; letters + words; letters + words + phrases) code the same sample in parallel
 * threads (map); the smallest wins (reduce), ties going to the cheaper configuration */
typedef struct { shared S; const uint8_t *d; size_t n; size_t size; } race_job;
static void *race_worker(void *arg) {
    race_job *R = arg; blockjob J; memset(&J, 0, sizeof J); J.S = &R->S; J.d = R->d; J.n = R->n;
    enc_block(&J); R->size = J.out.n; free(J.out.p); return NULL;
}
static int race(const shared *base, const uint8_t *d, size_t n, size_t sample, size_t sizes[3]) {
    size_t S = n < sample ? n : sample; if (S < n) { const uint8_t *q = memchr(d + S, '\n', n - S); S = q ? (size_t)(q - d) + 1 : n; }
    race_job R[3]; pthread_t th[3];
    for (int k = 0; k < 3; k++) { R[k].S = *base; R[k].S.phrases = 0; R[k].S.level1 = k == 0; R[k].S.lzp = k == 2; R[k].d = d; R[k].n = S; }
    for (int k = 1; k < 3; k++) pthread_create(&th[k], NULL, race_worker, &R[k]);
    race_worker(&R[0]);
    for (int k = 1; k < 3; k++) pthread_join(th[k], NULL);
    int best = 1;                                   /* order of preference on ties: words, letters, phrases */
    if (R[0].size < R[best].size) best = 0;
    if (R[2].size * 1000 < R[best].size * 997) best = 2;   /* phrases must win by 0.3% to pay for their time */
    for (int k = 0; k < 3; k++) sizes[k] = R[k].size;
    return best;
}

/* ------------------------------------------------------------------ driver: build, encode, decode, verify, time */
static uint8_t pick_sep(const uint8_t *d, size_t n) {
    size_t c[5] = {0}; const uint8_t SP[5] = {',', '\t', ';', '|', ' '}; size_t m = n < (1 << 20) ? n : (1 << 20);
    for (size_t i = 0; i < m; i++) for (int k = 0; k < 5; k++) c[k] += d[i] == SP[k];
    int b = 0; for (int k = 1; k < 5; k++) if (c[k] > c[b]) b = k;
    return SP[b];
}

int main(int argc, char **argv) {
    if (argc < 3 || argv[1][0] != 'b') { fprintf(stderr, "usage: gr6l3 b in [threads] [policy]\n"); return 2; }
    size_t n; uint8_t *d = read_file(argv[2], &n);
    int T = argc > 3 ? atoi(argv[3]) : 1; const char *policy = argc > 4 ? argv[4] : "rule";
    /* block-count decision: every block relearns its model, so a block must be large enough to pay for that:
     * at most one block per MiB (measured: 4 blocks cost 11-28% on 0.06-0.3 MB files, 2-4% on 1-15 MB files) */
    int blocks = getenv("BLOCKS") ? atoi(getenv("BLOCKS")) : (int)(n >> 20 < (size_t)T ? (n >> 20 ? n >> 20 : 1) : (size_t)T);
    int minn = getenv("MINN") ? atoi(getenv("MINN")) : 2, maxn = getenv("MAXN") ? atoi(getenv("MAXN")) : 8;
    size_t sample = getenv("SAMPLE") ? (size_t)atoll(getenv("SAMPLE")) : (4u << 20);
    shared *S = xmalloc(sizeof(shared)); memset(S, 0, sizeof *S); S->sep = pick_sep(d, n); S->minn = minn;
    double t0 = now();
    { size_t cnt[256] = {0}; for (size_t i = 0; i < n; i++) cnt[d[i]]++; int ord[256]; for (int i = 0; i < 256; i++) ord[i] = i;
      for (int i = 1; i < 256; i++) { int x = ord[i], j = i; while (j > 0 && cnt[ord[j - 1]] < cnt[x]) { ord[j] = ord[j - 1]; j--; } ord[j] = x; }
      for (int i = 0; i < 62; i++) S->table[i] = ord[i]; }
    formula_threads = T; S->nform = find_formulas(d, n, S->sep, S->F, 255);
    int ncolL1 = 0; if (getenv("COLTAU")) S->colL1 = column_levels(d, n, S->sep, atof(getenv("COLTAU")), &ncolL1);
    double t1 = now();
    candset C = {0}; size_t kept = 0;
    const char *dpol = policy; if (!strncmp(policy, "lzp", 3)) { S->lzp = 1; dpol = policy[3] == '+' ? policy + 4 : "none"; }
    double tr0 = now(), trace = 0;
    if (!strncmp(policy, "auto", 4)) {
        size_t smp = policy[4] == ':' ? (size_t)atoll(policy + 5) : (n / 8 > (256u << 10) ? n / 8 : (256u << 10));
        size_t sz[3]; int b = race(S, d, n, smp, sz); dpol = "none"; S->level1 = b == 0; S->lzp = b == 2;
        trace = now() - tr0;
        printf("   race on %zu B: letters %zu, words %zu, phrases %zu -> %s (%.2fs)\n", smp < n ? smp : n, sz[0], sz[1], sz[2],
               b == 0 ? "letters" : b == 1 ? "words" : "phrases", trace);
    }
    if (strcmp(dpol, "none")) { C = mine(d, n, sample, S->sep, minn, maxn, T); kept = apply_policy(&C, dpol, 65535); }
    double t2 = now();
    if (kept) { dict_build(&S->ph, d, n, S->sep, &C); S->phrases = 1; }
    buf hdr = {0}; buf_add(&hdr, &n, 8); buf_put(&hdr, S->sep); buf_add(&hdr, S->table, 62); buf_put(&hdr, S->nform);
    buf_add(&hdr, S->F, sizeof(formula_t) * S->nform); buf_put(&hdr, S->lzp); buf_put(&hdr, S->level1 || getenv("LEVEL1") != NULL); buf_put(&hdr, S->phrases); size_t dict0 = hdr.n; if (S->phrases) dict_write(&S->ph, &hdr);
    size_t dictbytes = hdr.n - dict0;
    /* blocks at line boundaries */
    blockjob *J = xmalloc(sizeof(blockjob) * blocks); memset(J, 0, sizeof(blockjob) * blocks);
    size_t a = 0;
    for (int b = 0; b < blocks; b++) {
        size_t e = b + 1 == blocks ? n : n * (b + 1) / blocks; if (e < n) { const uint8_t *q = memchr(d + e, '\n', n - e); e = q ? (size_t)(q - d) + 1 : n; } if (e < a) e = a;
        J[b].S = S; J[b].d = d + a; J[b].n = e - a; a = e;
        if (getenv("FIELDBITS_OUT") && blocks == 1) J[b].fbits = xmalloc(4 * 65536);
    }
    double t3 = now(); run_jobs(enc_block, J, blocks); double t4 = now();
    size_t total = hdr.n; for (int b = 0; b < blocks; b++) total += 8 + J[b].out.n;
    /* decode with fresh jobs (the decoder sees only the header, the dictionary and the coded blocks) */
    shared *S2 = xmalloc(sizeof(shared)); memset(S2, 0, sizeof *S2);
    { size_t q = 8; S2->sep = hdr.p[q++]; memcpy(S2->table, hdr.p + q, 62); q += 62; S2->nform = hdr.p[q++]; memcpy(S2->F, hdr.p + q, sizeof(formula_t) * S2->nform); q += sizeof(formula_t) * S2->nform;
      S2->lzp = hdr.p[q++]; S2->level1 = hdr.p[q++]; S2->phrases = hdr.p[q++]; S2->minn = minn; if (S2->phrases) q += dict_read(&S2->ph, hdr.p + q); }
    blockjob *D = xmalloc(sizeof(blockjob) * blocks); memset(D, 0, sizeof(blockjob) * blocks);
    for (int b = 0; b < blocks; b++) { D[b].S = S2; D[b].n = J[b].n; D[b].in = J[b].out.p; D[b].inlen = J[b].out.n; }
    double t5 = now(); run_jobs(dec_block, D, blocks); double t6 = now();
    int ok = 1; size_t off = 0;
    for (int b = 0; b < blocks; b++) { ok &= D[b].out.n == J[b].n && !memcmp(D[b].out.p, d + off, J[b].n); off += J[b].n; }
    uint64_t ops[NOP] = {0}, cov = 0, fields = 0, phr = 0;
    for (int b = 0; b < blocks; b++) { for (int k = 0; k < NOP; k++) ops[k] += J[b].ops[k]; cov += J[b].covered; fields += J[b].fields; phr += J[b].phr_used; }
    const char *nm = strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2];
    printf("%s threads %d blocks %d policy %s minn %d: %zu -> %zu ratio %.3f | dict %zu B, %zu phrases (%zu candidates), %lu uses, %.1f%% fields covered"
           " | prep %.2fs mine %.2fs (map %.2f reduce %.2f) enc %.2fs dec %.2fs | enc %.2f MB/s dec %.2f MB/s | %s\n",
           nm, T, blocks, policy, minn, n, total, (double)n / total, dictbytes, kept, C.n, (unsigned long)phr, 100.0 * cov / (fields ? fields : 1),
           t1 - t0, t2 - t1, C.t_map, C.t_reduce, t4 - t3, t6 - t5, n / (t4 - t0) / 1e6, n / (t6 - t5) / 1e6, ok ? "OK" : "FAIL");
    if (getenv("FIELDBITS_OUT") && blocks == 1) write_file(getenv("FIELDBITS_OUT"), J[0].fbits, J[0].nfb * 4);
    if (getenv("CAND_OUT")) {   /* research: candidate features + measured saving from a phrase-free run's per-field bits */
        size_t nb = 0; float *fb = getenv("FIELDBITS_IN") ? (float *)read_file(getenv("FIELDBITS_IN"), &nb) : NULL; nb /= 4;
        map64 cm_; map_init(&cm_, 16); for (size_t i = 0; i < C.n; i++) *map_slot(&cm_, C.c[i].key, 1) = (uint32_t)i;
        double *truth = calloc(C.n, sizeof(double)); uint32_t *occ = calloc(C.n, 4); uint64_t *lastend = calloc(C.n, 8);
        uint64_t *fh = NULL; size_t *fo = NULL, cap = 0; uint64_t g = 0;
        for (size_t i = 0; i < n;) {
            const uint8_t *q = memchr(d + i, '\n', n - i); size_t e = q ? (size_t)(q - d) : n;
            size_t nf = line_fields(d, i, e, S->sep, &fh, &fo, &cap);
            for (size_t f = 0; f < nf; f++) for (int L = minn; L <= maxn && f + L <= nf; L++) {
                uint32_t *v = map_slot(&cm_, seqkey(fh + f, L), 0); if (!v || C.c[*v].n != (uint32_t)L) continue;
                if (g + f < lastend[*v]) continue;
                if (occ[*v]++ && fb) for (int t = 0; t < L && g + f + t < nb; t++) truth[*v] += fb[g + f + t];
                lastend[*v] = g + f + L;
            }
            g += nf; i = e + 1;
        }
        FILE *o = fopen(getenv("CAND_OUT"), "w");
        fprintf(o, "idx,n,cnt,bytes,sumsi,maxsi,nnum,nempty,occ,truthbits\n");
        for (size_t i = 0; i < C.n; i++) { cand *c = &C.c[i]; fprintf(o, "%zu,%u,%u,%u,%.3f,%.3f,%u,%u,%u,%.1f\n", i, c->n, c->cnt, c->bytes, c->sumsi, c->maxsi, c->nnum, c->nempty, occ[i], truth[i]); }
        fclose(o);
    }
    if (S->colL1) printf("   letter columns: %d\n", ncolL1);
    printf("   ops:"); uint64_t tot = 0; for (int k = 0; k < NOP; k++) tot += ops[k];
    for (int k = 0; k < NOP; k++) if (ops[k]) printf(" %s %.1f%%", opname[k], 100.0 * ops[k] / tot); printf("\n");
    return !ok;
}
