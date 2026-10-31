/* ---------------------------------------------------------------- czcm: integer context-mixing coder (codec 7)
 * lpaq-style: bitwise prediction from hashed contexts (orders 1-4, 6, a word model, a sparse model) stored in
 * nibble buckets, an order-1 direct model and a match model; inputs mixed by two small neural networks with integer
 * weights selected by context; two adaptive probability maps refine the result; binary arithmetic coding.
 * Integer arithmetic only, so encoder and decoder agree bit-for-bit on every compiler and CPU.
 * Stream: size u32 (low 28 bits n, top 4 bits log2 hash buckets - 15; 0 = 2^CM_BB) | coded bytes.  Memory: 2^bits * 64 B per hashed model. */
#ifndef CM_NM
#define CM_NM 16                /* models: 0 = order-1 direct, 1..9 hashed */
#endif
#define CM_NI (2 * CM_NM + 3)   /* two inputs per model, two from the match model, bias */
#ifndef CM_BB
#define CM_BB 20                /* log2 buckets per hashed model in legacy streams (no size field) */
#endif
#ifndef CM_LIM
#define CM_LIM 2
#endif
_Static_assert(CM_LIM * 15 < 64, "CM_LIM too large for the rate table");
#define CM_W1 (256 * 3)
#define CM_W2 (4 * 8 * 4)
#ifndef CM_LR
#define CM_LR 12
#endif

static int16_t cm_sq[4096], cm_st[4096];
static uint32_t cm_rcp[64], cm_srcp[1024];
#define SM_LIM 255
static pthread_once_t cm_once = PTHREAD_ONCE_INIT;
static int cm_squash_i(int d) {                     /* 4096 / (1 + e^(-d/256)), integer interpolation (lpaq1) */
    static const int t[33] = {1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101, 1546, 2047, 2549, 2994, 3348,
                              3607, 3785, 3901, 3975, 4022, 4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094};
    if (d > 2047) return 4095;
    if (d < -2047) return 1;
    int w = d & 127; d = (d >> 7) + 16;
    return (t[d] * (128 - w) + t[d + 1] * w + 64) >> 7;
}
/* bit histories: state = (zeros, ones, last bit) with counts capped at BH_CAP; on a bit, the opposite count above 2
 * is halved. 215 states, numbered by breadth-first search from the empty history (identical everywhere). */
#define BH_CAP 24
static uint8_t bh_next[256][2]; static uint8_t bh_n0[256], bh_n1[256]; static int bh_ns;
static void bh_init(void) {
    static int16_t id[BH_CAP + 1][BH_CAP + 1][2]; int q[256], qh = 0, qt = 0;
    memset(id, -1, sizeof id);
    id[0][0][0] = 0; bh_n0[0] = bh_n1[0] = 0; bh_ns = 1; q[qt++] = 0;
    int qs[256][3]; qs[0][0] = qs[0][1] = qs[0][2] = 0;
    while (qh < qt) {
        int st = q[qh++], n0 = qs[st][0], n1 = qs[st][1];
        for (int y = 0; y < 2; y++) {
            int a = n0, b = n1;
            if (y) { b = b + 1 > BH_CAP ? BH_CAP : b + 1; if (a > 2) a = (a + 1) / 2; }
            else   { a = a + 1 > BH_CAP ? BH_CAP : a + 1; if (b > 2) b = (b + 1) / 2; }
            if (id[a][b][y] < 0) { id[a][b][y] = (int16_t)bh_ns; qs[bh_ns][0] = a; qs[bh_ns][1] = b; qs[bh_ns][2] = y; bh_n0[bh_ns] = (uint8_t)a; bh_n1[bh_ns] = (uint8_t)b; q[qt++] = bh_ns; bh_ns++; }
            bh_next[st][y] = (uint8_t)id[a][b][y];
        }
    }
}
static void cm_init_tables(void) {
    bh_init();
    for (int i = 0; i < 4096; i++) { int v = cm_squash_i(i - 2048); cm_sq[i] = (int16_t)(v < 1 ? 1 : v > 4095 ? 4095 : v); }
    int pi = 0;
    for (int x = -2047; x <= 2047; x++) { int v = cm_squash_i(x); for (int j = pi; j <= v && j < 4096; j++) cm_st[j] = (int16_t)x; pi = v + 1; }
    for (int j = pi; j < 4096; j++) cm_st[j] = 2047;
    for (int i = 0; i < 64; i++) cm_rcp[i] = (uint32_t)(655360 / (10 * i + 16));
    for (int i = 0; i < 1024; i++) cm_srcp[i] = (uint32_t)(655360 / (10 * i + 15));
}
static inline int cm_SQ(int x) { if (x > 2047) x = 2047; if (x < -2047) x = -2047; return cm_sq[x + 2048]; }

typedef struct { uint32_t s[16]; } cm_bucket;          /* s[0] = check << 16 | priority; s[1..15] nibble-tree counters */
typedef struct {
    cm_bucket *t[CM_NM];
    int sh; size_t mem;                                          /* 32 - log2 buckets; bytes held */
    uint32_t o1[256 * 256];
    uint32_t h[CM_NM]; uint32_t *slot[CM_NM]; cm_bucket *cur[CM_NM];
    int32_t w1[CM_W1][CM_NI], w2[CM_W2][CM_NI];
    uint32_t sm[CM_NM][256 * 8]; uint16_t sst[CM_NM];             /* state -> probability (22 bits) | count (10 bits) */
    int x[CM_NI];
    int pr, pr1, pr2, w1i, w2i;
    uint16_t a1[256][33], a2[65536][33];
    int ai, aw, a2i;
    uint8_t *buf; int64_t pos;
    uint32_t c0, c4, c8, word, word1, word2, bitpos; uint16_t ind1[256]; uint16_t ind2[65536]; int64_t line0, line1;   /* current and previous line starts */
    uint32_t *mt, mmask; int64_t mptr; int mlen;
    uint32_t mc[64];
    int mci;
} cm_t;

static inline uint32_t cm_hmix(uint32_t a) { a ^= a >> 15; a *= 0x2C1B3C6Du; a ^= a >> 12; a *= 0x297A2D39u; a ^= a >> 15; return a; }

/* Concurrent coders share a memory budget (60% of physical memory), so parallel blocks wait instead of swapping.
 * A coder larger than the whole budget still runs when it is alone. */
static pthread_mutex_t cm_mu = PTHREAD_MUTEX_INITIALIZER; static pthread_cond_t cm_cv = PTHREAD_COND_INITIALIZER;
static size_t cm_used, cm_budget;
static void cm_acquire(size_t b) {
    pthread_mutex_lock(&cm_mu);
    if (!cm_budget) {
        long pg = sysconf(_SC_PHYS_PAGES), ps = sysconf(_SC_PAGE_SIZE);
        cm_budget = (pg > 0 && ps > 0) ? (size_t)((double)pg * (double)ps * 0.6) : ((size_t)4 << 30);
    }
    while (cm_used && cm_used + b > cm_budget) pthread_cond_wait(&cm_cv, &cm_mu);
    cm_used += b; pthread_mutex_unlock(&cm_mu);
}
static void cm_release(size_t b) { pthread_mutex_lock(&cm_mu); cm_used -= b; pthread_cond_broadcast(&cm_cv); pthread_mutex_unlock(&cm_mu); }
/* Hash-table size for an n-byte stream: 1 bucket per byte, between 2^16 and 2^22 per model (3.8 GB at most). */
#ifndef CM_BB_MAX
#define CM_BB_MAX 22
#endif
static int cm_bits_for(size_t n) { int b = 16; while (b < CM_BB_MAX && ((size_t)1 << b) < n) b++; return b; }
static size_t cm_mem_for(int bb) { return (size_t)(CM_NM - 1) * ((size_t)sizeof(cm_bucket) << bb) + ((size_t)16 << 20); }

static cm_t *cm_new(uint8_t *buf, int bb) {
    pthread_once(&cm_once, cm_init_tables);
    size_t mem = cm_mem_for(bb); cm_acquire(mem);
    cm_t *m = calloc(1, sizeof(cm_t)); if (!m) { cm_release(mem); return NULL; }
    m->sh = 32 - bb; m->mem = mem;
    for (int i = 1; i < CM_NM; i++) { m->t[i] = calloc((size_t)1 << bb, sizeof(cm_bucket)); if (!m->t[i]) goto fail; }
    for (int i = 0; i < 65536; i++) m->o1[i] = 0x80000000u;
    for (int i = 0; i < CM_NM; i++) for (int j = 0; j < 256 * 8; j++) {
        int st = j & 255; uint32_t n0 = st < bh_ns ? bh_n0[st] : 0, n1 = st < bh_ns ? bh_n1[st] : 0;
        m->sm[i][j] = (uint32_t)(((2ull * n1 + 1) << 22) / (2ull * (n0 + n1) + 2)) << 10;
    }
    for (int i = 0; i < CM_W1; i++) for (int j = 0; j < CM_NI; j++) m->w1[i][j] = 1 << 14;
    for (int i = 0; i < CM_W2; i++) for (int j = 0; j < CM_NI; j++) m->w2[i][j] = 1 << 14;
    for (int c = 0; c < 256; c++) for (int j = 0; j < 33; j++) m->a1[c][j] = (uint16_t)(cm_SQ((j - 16) * 128) * 16);
    for (int c = 0; c < 65536; c++) for (int j = 0; j < 33; j++) m->a2[c][j] = (uint16_t)(cm_SQ((j - 16) * 128) * 16);
    m->buf = buf; m->c0 = 1;
    m->mmask = (1u << 22) - 1; m->mt = calloc((size_t)m->mmask + 1, 4); if (!m->mt) goto fail;
    for (int i = 0; i < 64; i++) m->mc[i] = 1u << 15;
    return m;
fail:
    for (int i = 1; i < CM_NM; i++) free(m->t[i]);
    free(m->mt); free(m); cm_release(mem); return NULL;
}
static void cm_free(cm_t *m) { if (!m) return; size_t mem = m->mem; for (int i = 1; i < CM_NM; i++) free(m->t[i]); free(m->mt); free(m); cm_release(mem); }

static inline cm_bucket *cm_find(cm_bucket *t, uint32_t h, int sh) {
    uint32_t idx = h >> sh, chk = h & 0xffff;
    cm_bucket *a = &t[idx], *b = &t[idx ^ 1];
    if ((a->s[0] >> 16) == chk) return a;
    if ((b->s[0] >> 16) == chk) return b;
    cm_bucket *v = ((a->s[0] & 0xffff) <= (b->s[0] & 0xffff)) ? a : b;
    v->s[0] = chk << 16;
    for (int k = 1; k < 16; k++) v->s[k] = 0x80000000u;
    return v;
}
static inline int cm_expected(cm_t *m) {
    if (!m->mlen) return -1;
    int b = m->buf[m->mptr];
    if (((b + 256) >> (8 - m->bitpos)) != (int)m->c0) return -1;
    return (b >> (7 - m->bitpos)) & 1;
}
/* slot: high 16 bits adaptive probability, bits 8..15 bit-history state, low 8 bits count */
static inline int cm_stc(uint32_t v) { return cm_st[v >> 20]; }

static int cm_predict(cm_t *m) {
    uint32_t c0 = m->c0; int bp = (int)m->bitpos;
    if (bp == 0 || bp == 4) {
        uint32_t nib = bp ? (c0 & 15) * 0x5BD1E995u : 0;
        for (int i = 1; i < CM_NM; i++) m->cur[i] = cm_find(m->t[i], cm_hmix(m->h[i] + nib), m->sh);
    }
    int node = bp < 4 ? (int)((c0 & ((1u << bp) - 1)) | (1u << bp)) : (int)((c0 & ((1u << (bp - 4)) - 1)) | (1u << (bp - 4)));
    m->slot[0] = &m->o1[c0 | (m->c4 & 0xff) << 8];
    for (int i = 1; i < CM_NM; i++) m->slot[i] = &m->cur[i]->s[node];
    for (int i = 0; i < CM_NM; i++) {
        uint32_t v = *m->slot[i]; uint16_t st = (uint16_t)((v >> 8 & 255) | bp << 8); m->sst[i] = st;
        m->x[i] = cm_stc(v); m->x[CM_NM + 3 + i] = cm_st[m->sm[i][st] >> 20];
    }
    int eb = cm_expected(m);
    if (eb >= 0) {
        int l = m->mlen > 31 ? 31 : m->mlen;
        m->mci = l * 2 + eb;
        int p = (int)(m->mc[m->mci] >> 4); if (p < 1) p = 1; if (p > 4095) p = 4095;
        m->x[CM_NM] = cm_st[p];
        m->x[CM_NM + 1] = eb ? l * 16 : -l * 16;
    } else { m->mci = -1; m->x[CM_NM] = 0; m->x[CM_NM + 1] = 0; }
    m->x[CM_NM + 2] = 64;
    int ml = m->mlen == 0 ? 0 : m->mlen < 16 ? 1 : m->mlen < 32 ? 2 : 3;
    uint32_t c1 = m->c4 & 0xff;
    int cls = ((c1 | 32) >= 'a' && (c1 | 32) <= 'z') ? 0 : (c1 >= '0' && c1 <= '9') ? 1 : 2;
    m->w1i = (int)c0 + 256 * cls;
    m->w2i = (ml * 8 + bp) * 4 + (int)(c1 >> 6);
    const int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    int64_t s1 = 0, s2 = 0;
    for (int i = 0; i < CM_NI; i++) { s1 += (int64_t)w1[i] * m->x[i]; s2 += (int64_t)w2[i] * m->x[i]; }
    int d1 = (int)(s1 >> 16), d2 = (int)(s2 >> 16);
    m->pr1 = cm_SQ(d1); m->pr2 = cm_SQ(d2);
    int st = (d1 + d2) >> 1; if (st > 2047) st = 2047; if (st < -2047) st = -2047;
    int mixed = cm_SQ(st);
    int s = st + 2048;
    m->ai = s >> 7; m->aw = s & 127;
    m->a2i = (int)((c0 | c1 << 8) & 0xffff);
    int lo = m->ai, wt = m->aw;
    int p1 = (m->a1[c0][lo] * (128 - wt) + m->a1[c0][lo + 1] * wt) >> 11;
    int p2 = (m->a2[m->a2i][lo] * (128 - wt) + m->a2[m->a2i][lo + 1] * wt) >> 11;
    int p = (mixed + p1 + p2) / 3;
    if (p < 1) p = 1; if (p > 4095) p = 4095;
    return m->pr = p;
}
static inline void cm_upd_apm(uint16_t *a, int lo, int wt, int y) {
    int g = y ? 65535 : 0;
    a[lo] = (uint16_t)(a[lo] + ((g - a[lo]) * (128 - wt) >> 13));
    a[lo + 1] = (uint16_t)(a[lo + 1] + ((g - a[lo + 1]) * wt >> 13));
}
static inline void cm_upd_ctr(uint32_t *s, int y) {
    uint32_t v = *s; int p = (int)(v >> 16), n = (int)(v & 15); uint8_t st = (uint8_t)(v >> 8);
    p += (int)(((int64_t)((y ? 65535 : 0) - p) * cm_rcp[n * CM_LIM]) >> 16);
    if (n < 15) n++;
    if (p < 0) p = 0; if (p > 65535) p = 65535;
    *s = (uint32_t)p << 16 | (uint32_t)bh_next[st][y] << 8 | (uint32_t)n;
}
static inline void cm_upd_sm(uint32_t *e, int y) {
    uint32_t v = *e, n = v & 1023, p = v >> 10;
    int64_t d = ((int64_t)((uint32_t)y << 22) - (int64_t)p) * cm_srcp[n] >> 16;
    p = (uint32_t)((int64_t)p + d); if (p > (1u << 22) - 1) p = (1u << 22) - 1;
    *e = p << 10 | (n < SM_LIM ? n + 1 : n);
}
static void cm_update(cm_t *m, int y) {
    for (int i = 0; i < CM_NM; i++) { cm_upd_sm(&m->sm[i][m->sst[i]], y); cm_upd_ctr(m->slot[i], y); }
    for (int i = 1; i < CM_NM; i++) if ((m->cur[i]->s[0] & 0xffff) < 0xffff) m->cur[i]->s[0]++;
    if (m->mci >= 0) { uint32_t *c = &m->mc[m->mci]; *c = (uint32_t)((int64_t)*c + ((((int64_t)y << 16) - (int64_t)*c) >> 6)); }
    int e1 = (y << 12) - m->pr1, e2 = (y << 12) - m->pr2;                  /* learning rate 1/2048 in 16.16 weights */
    int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    for (int i = 0; i < CM_NI; i++) { w1[i] += (m->x[i] * e1) >> CM_LR; w2[i] += (m->x[i] * e2) >> CM_LR; }
    cm_upd_apm(m->a1[m->c0], m->ai, m->aw, y);
    cm_upd_apm(m->a2[m->a2i], m->ai, m->aw, y);
    int eb = cm_expected(m);
    if (eb >= 0 && eb != y) m->mlen = 0;
    m->c0 = (m->c0 << 1) | (uint32_t)y;
    if (m->bitpos == 2) {                                     /* prefetch both candidate buckets for the second nibble */
        for (int b = 0; b < 2; b++) {
            uint32_t nib = (((m->c0 << 1) | (uint32_t)b) & 15) * 0x5BD1E995u;
            for (int i = 1; i < CM_NM; i++) { uint32_t idx = cm_hmix(m->h[i] + nib) >> m->sh; __builtin_prefetch(&m->t[i][idx]); }
        }
    }
    if (++m->bitpos == 8) {
        uint8_t c = (uint8_t)m->c0;
        m->buf[m->pos++] = c;
        m->c8 = (m->c8 << 8) | (m->c4 >> 24); m->c4 = (m->c4 << 8) | c;
        m->c0 = 1; m->bitpos = 0;
        if (((c | 32) >= 'a' && (c | 32) <= 'z') || c >= 128) m->word = (m->word + (c | 32) + 1) * 0x3D4D51CBu;
        else if (m->word) { m->word2 = m->word1; m->word1 = m->word; m->word = 0; }
        uint32_t c4 = m->c4;
        m->h[1] = cm_hmix((c4 & 0xff) + 0x1000000u);
        m->h[2] = cm_hmix((c4 & 0xffff) + 0x2000000u);
        m->h[3] = cm_hmix((c4 & 0xffffff) * 0x9E3779B1u + 3);
        m->h[4] = cm_hmix(c4 * 0x9E3779B1u + 4);
        m->h[5] = cm_hmix(cm_hmix(c4) + (m->c8 & 0xffff) * 0x85EBCA6Bu + 5);
        m->h[6] = cm_hmix(m->word * 0x27D4EB2Fu + m->word1 * 0x165667B1u + 6);
        m->h[7] = cm_hmix((c4 & 0xff00ff00u) * 0x9E3779B1u + 7);
#if CM_NM > 14
        {   uint32_t c1 = c4 >> 8 & 0xff, c2 = c4 >> 8 & 0xffff;                           /* what followed the previous context last time */
            m->ind1[c1] = (uint16_t)(m->ind1[c1] << 8 | c); m->ind2[c2] = (uint16_t)(m->ind2[c2] << 8 | c);
            m->h[14] = cm_hmix(((uint32_t)m->ind1[c] << 8 | c) * 0x9E3779B1u + 14);
            m->h[15] = cm_hmix(((uint32_t)m->ind2[c4 & 0xffff] << 16 | (c4 & 0xffff)) * 0x85EBCA6Bu + 15);
        }
#endif
#if CM_NM > 12
        m->h[12] = cm_hmix(m->word * 0x2545F491u + 12);                                              /* current word */
        m->h[13] = cm_hmix(m->word * 0x9E3779B1u + m->word1 * 0x85EBCA6Bu + m->word2 * 0xC2B2AE35u + 13);  /* word trigram */
#endif
#if CM_NM > 10
        if (c == '\n') { m->line1 = m->line0; m->line0 = m->pos; }
        {   int64_t col = m->pos - m->line0, plen = m->line0 - m->line1;
            uint32_t above = (col < plen) ? m->buf[m->line1 + col] : 0, above2 = (col + 1 < plen) ? m->buf[m->line1 + col + 1] : 0;
            uint32_t cc = (uint32_t)(col < 255 ? col : 255);
            m->h[10] = cm_hmix((above << 8 | above2) * 0x9E3779B1u + (cc << 20) + 10);          /* column, bytes above */
            m->h[11] = cm_hmix((above << 16 | (c4 & 0xffff)) * 0x85EBCA6Bu + 11);               /* byte above + order 2 */
        }
#endif
#if CM_NM > 8
        m->h[8] = cm_hmix(cm_hmix(c4 * 0x9E3779B1u) + (m->c8 & 0xff) * 0x85EBCA6Bu + 8);          /* order 5 */
        m->h[9] = cm_hmix(cm_hmix(c4 + 0x51ED27u) + (m->c8 & 0xffffff) * 0xC2B2AE35u + 9);     /* order 7 */
#endif
        for (int i = 1; i < CM_NM; i++) {                     /* prefetch the buckets for the first nibble */
            uint32_t idx = cm_hmix(m->h[i]) >> m->sh;
            __builtin_prefetch(&m->t[i][idx]); __builtin_prefetch(&m->t[i][idx ^ 1]);
        }
        if (m->pos >= 6) {
            uint32_t hh = cm_hmix(c4 ^ ((m->c8 & 0xffff) * 0x9E3779B1u)) & m->mmask;
            if (m->mlen) { m->mptr++; if (m->mlen < 65535) m->mlen++; }
            else {
                int64_t cand = m->mt[hh];
                if (cand > 0) {
                    int l = 0;
                    while (l < 400 && cand - 1 - l >= 0 && m->buf[cand - 1 - l] == m->buf[m->pos - 1 - l]) l++;
                    if (l >= 6) { m->mlen = l; m->mptr = cand; }
                }
            }
            m->mt[hh] = (uint32_t)m->pos;
            if (m->mlen && m->mptr >= m->pos) m->mlen = 0;
        }
    }
}
static size_t cm_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (cap < 8 || n > 0xFFFFFFF0u) return 0;
    int bb = n < ((size_t)1 << 28) ? cm_bits_for(n) : CM_BB;   /* top 4 bits of the size word: log2 buckets - 15 (0 = legacy) */
    uint8_t *hist = malloc(n + 8); cm_t *m = hist ? cm_new(hist, bb) : NULL;
    if (!m) { free(hist); return 0; }
    uint32_t n32 = (uint32_t)n | (n < ((size_t)1 << 28) ? (uint32_t)(bb - 15) << 28 : 0); memcpy(out, &n32, 4);
    uint32_t x1 = 0, x2 = 0xffffffff; size_t o = 4;
    for (size_t i = 0; i < n; i++) {
        int c = in[i];
        for (int b = 7; b >= 0; b--) {
            int y = (c >> b) & 1, p = cm_predict(m);
            uint32_t xm = x1 + (uint32_t)(((uint64_t)(x2 - x1) * (uint32_t)p) >> 12);
            if (y) x2 = xm; else x1 = xm + 1;
            cm_update(m, y);
            while (((x1 ^ x2) & 0xff000000u) == 0) { if (o >= cap) { cm_free(m); free(hist); return 0; } out[o++] = (uint8_t)(x2 >> 24); x1 <<= 8; x2 = (x2 << 8) | 255; }
        }
    }
    if (o + 4 > cap) { cm_free(m); free(hist); return 0; }
    out[o++] = (uint8_t)(x1 >> 24); out[o++] = 255; out[o++] = 255; out[o++] = 255;
    cm_free(m); free(hist);
    return o;
}
static int cm_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 8) return -1;
    uint32_t n32; memcpy(&n32, in, 4);
    int bb = CM_BB;
    if (n < ((size_t)1 << 28)) { uint32_t f = n32 >> 28; n32 &= 0x0FFFFFFFu; if (f) bb = (int)f + 15; }
    if (n32 != n || bb < 16 || bb > 22) return -1;
    cm_t *m = cm_new(out, bb); if (!m) return -1;
    uint32_t x1 = 0, x2 = 0xffffffff, x = 0; size_t ip = 4;
    for (int k = 0; k < 4; k++) x = (x << 8) | (ip < cn ? in[ip++] : 255);
    for (size_t i = 0; i < n; i++)
        for (int b = 7; b >= 0; b--) {
            int p = cm_predict(m);
            uint32_t xm = x1 + (uint32_t)(((uint64_t)(x2 - x1) * (uint32_t)p) >> 12);
            int y = x <= xm;
            if (y) x2 = xm; else x1 = xm + 1;
            cm_update(m, y);
            while (((x1 ^ x2) & 0xff000000u) == 0) { x1 <<= 8; x2 = (x2 << 8) | 255; x = (x << 8) | (ip < cn ? in[ip++] : 255); }
        }
    cm_free(m);
    return 0;
}
