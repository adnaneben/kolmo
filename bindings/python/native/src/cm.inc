/* ---------------------------------------------------------------- czcm: integer context-mixing coder (codec 7)
 * lpaq-style: bitwise prediction from hashed contexts (orders 1-4, 6, a word model, a sparse model) stored in
 * nibble buckets, an order-1 direct model and a match model; inputs mixed by two small neural networks with integer
 * weights selected by context; two adaptive probability maps refine the result; binary arithmetic coding.
 * Integer arithmetic only, so encoder and decoder agree bit-for-bit on every compiler and CPU.
 * Model 2 (streams flagged 8 in the size word) adds two sparse contexts (bytes 3-4, byte 2), two record contexts (the
 * bytes one and two records back, once a fixed record length is detected, and the column within the record), two more mixers (weights
 * selected by byte 2 and by order 1), a learned final mixer in place of the plain average, and an order-2 hashed APM.
 * Stream: size u32 (low 28 bits n, top 4 bits f: 0 = model 1 with 2^CM_BB buckets, else log2 buckets - 15 in f & 7,
 * model 2 when f & 8) | coded bytes.  Memory: 2^bits * 64 B per hashed model. */
#ifndef CM_NM
#define CM_NM 16                /* models: 0 = order-1 direct, 1..9 hashed */
#endif
#define CM_NI (2 * CM_NM + 3)   /* two inputs per model, two from the match model, bias */
#ifndef CM_SMALL
#define CM_SMALL 1               /* model-2 contexts (16+) use tables 2^CM_SMALL times smaller */
#endif
#define CM_NMX 21               /* models in model 2 */
#define CM_NIX (2 * CM_NMX + 3)
#ifndef CM_BB
#define CM_BB 20                /* log2 buckets per hashed model in legacy streams (no size field) */
#endif
#ifndef CM_LIM
#define CM_LIM 2
#endif
_Static_assert(CM_LIM * 15 < 64, "CM_LIM too large for the rate table");
#define CM_W1 (256 * 3)
#define CM_W2 (4 * 8 * 4)
#define CM_W3 (256 * 8)         /* model 2: byte 2 x bit position */
#define CM_W4 65536             /* model 2: order 1 (partial byte, previous byte) */
#define CM_WF 1024              /* model 2: final mixer, by partial byte x match length */
#define CM_A3 (1 << 16)         /* model 2: order-2 hashed APM */
#define CM_LRK 20               /* model 2: mixer learning rate decays from 64/16 to CM_LRK/16 of 1/2^CM_LR ... */
#define CM_LRD 13               /* ... by 1/16 every 2^CM_LRD bytes */
#ifndef CM_LRF
#define CM_LRF 12
#endif
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

#ifdef KOLMO_CM_COMPACT
enum { CM_BYTE_VALUES = 257, CM_NIBBLES = 16, CM_PAIR_BITS = 13 };
typedef struct {
    uint32_t keys[1u << CM_PAIR_BITS];
    uint16_t pairs[1u << CM_PAIR_BITS];
} cm_pair_scratch;
#endif
typedef struct { uint32_t s[16]; } cm_bucket;          /* s[0] = check << 16 | priority; s[1..15] nibble-tree counters */
typedef struct {
    cm_bucket *t[CM_NMX];
#ifdef KOLMO_CM_COMPACT
    /* High 16 bits hold the compact slot, low 16 hold the original tag. */
    uint32_t one_byte[2][CM_BYTE_VALUES][CM_NIBBLES];
#endif
    int sh, v2; size_t mem, tb; uint8_t shi[CM_NMX]; size_t tbi[CM_NMX];                                  /* 32 - log2 buckets; model 2?; bytes held */
    uint32_t o1[256 * 256];
    uint32_t h[CM_NMX]; uint32_t *slot[CM_NMX]; cm_bucket *cur[CM_NMX];
    int32_t w1[CM_W1][CM_NIX], w2[CM_W2][CM_NIX];
    int32_t (*w3)[CM_NIX], (*w4)[CM_NIX], wf[CM_WF][5];        /* model 2 */
    int xf[5], pr3, pr4, prf, w3i, w4i, wfi, a3i; uint16_t (*a3)[33];
    uint32_t sm[CM_NMX][256 * 8]; uint16_t sst[CM_NMX];           /* state -> probability (22 bits) | count (10 bits) */
    int x[CM_NIX];
    uint32_t rl_last[256], rl_d[256], rlen, rcnt, rcand;
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
#ifdef KOLMO_CM_COMPACT
    while (cm_used && (b > cm_budget || cm_used > cm_budget - b)) pthread_cond_wait(&cm_cv, &cm_mu);
#else
    while (cm_used && cm_used + b > cm_budget) pthread_cond_wait(&cm_cv, &cm_mu);
#endif
    cm_used += b; pthread_mutex_unlock(&cm_mu);
}
static void cm_release(size_t b) { pthread_mutex_lock(&cm_mu); cm_used -= b; pthread_cond_broadcast(&cm_cv); pthread_mutex_unlock(&cm_mu); }
/* Hash-table size for an n-byte stream: 1 bucket per byte, between 2^16 and 2^22 per model (3.8 GB at most). */
#ifndef CM_BB_MAX
#define CM_BB_MAX 22
#endif
static int cm_bits_for(size_t n) { int b = 16; while (b < CM_BB_MAX && ((size_t)1 << b) < n) b++; return b; }
static size_t cm_mem_for(int bb, int v2) {
    size_t t = (size_t)sizeof(cm_bucket) << bb, ts = bb > 18 ? t >> CM_SMALL : t;
#ifdef KOLMO_CM_COMPACT
    /* A distinct (byte, nibble) can introduce at most one two-bucket pair.
     * Account for both maps and the temporary hash table before allocation. */
    size_t max_bank = 2u * CM_BYTE_VALUES * CM_NIBBLES * sizeof(cm_bucket);
    uint64_t bytes = ((uint64_t)(v2 ? 48 : 16) << 20) + sizeof(((cm_t *)0)->one_byte) + sizeof(cm_pair_scratch);
    int nm = v2 ? CM_NMX : CM_NM;
    for (int bank = 1; bank < nm; ++bank) {
        size_t bank_bytes = bank >= CM_NM ? ts : t;
        if ((bank == 1 || bank == 17) && bank_bytes > max_bank) bank_bytes = max_bank;
        bytes += bank_bytes;
    }
    return bytes > SIZE_MAX ? SIZE_MAX : (size_t)bytes;
#else
    return (size_t)(CM_NM - 1) * t + (v2 ? (size_t)(CM_NMX - CM_NM) * ts : 0) + ((size_t)(v2 ? 48 : 16) << 20);
#endif
}

/* Model tables: zeroed, and on Linux backed by transparent huge pages where available (far fewer TLB misses on the
 * random lookups that dominate CM time). Contents never depend on how the memory was obtained. */
#if defined(__linux__)
#include <sys/mman.h>
static void *cm_big(size_t n) {
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return NULL;
#ifdef MADV_HUGEPAGE
    madvise(p, n, MADV_HUGEPAGE);
#endif
    return p;
}
static void cm_big_free(void *p, size_t n) { if (p) munmap(p, n); }
#else
static void *cm_big(size_t n) { return calloc(1, n); }
static void cm_big_free(void *p, size_t n) { (void)n; free(p); }
#endif
static void cm_free_parts(cm_t *m) {
    for (int i = 1; i < CM_NMX; i++) cm_big_free(m->t[i], m->tbi[i]);
    cm_big_free(m->w3, sizeof(*m->w3) * CM_W3); cm_big_free(m->w4, sizeof(*m->w4) * CM_W4);
    cm_big_free(m->a3, sizeof(*m->a3) * CM_A3); cm_big_free(m->mt, ((size_t)m->mmask + 1) * 4); free(m);
}
#ifdef KOLMO_CM_COMPACT
/* Only these two context banks depend on a single previous byte. Map
 * neighboring buckets together to preserve collisions and tie-breaking. */
static size_t cm_build_byte_table(uint32_t map[CM_BYTE_VALUES][CM_NIBBLES], int bank, unsigned shift) {
    cm_pair_scratch *scratch = calloc(1, sizeof(*scratch));
    if (!scratch) return 0;
    unsigned count = 0;
    for (unsigned c = 0; c < CM_BYTE_VALUES; ++c) {
        /* Before the first byte, both context hashes are exactly zero. */
        uint32_t context = 0;
        if (c != 256) {
            context = bank == 1 ? cm_hmix(c + 0x1000000u)
                                : cm_hmix((c << 8) * 0x85EBCA6Bu + 17);
        }
        for (unsigned nib = 0; nib < CM_NIBBLES; ++nib) {
            uint32_t h = cm_hmix(context + nib * 0x5BD1E995u);
            uint32_t original = h >> shift;
            uint32_t key = (original >> 1) + 1;
            uint32_t slot = (key * 0x9E3779B1u) >> (32 - CM_PAIR_BITS);
            while (scratch->keys[slot] && scratch->keys[slot] != key)
                slot = (slot + 1) & ((1u << CM_PAIR_BITS) - 1);
            if (!scratch->keys[slot]) {
                scratch->keys[slot] = key;
                scratch->pairs[slot] = (uint16_t)count++;
            }
            uint32_t compact = 2u * scratch->pairs[slot] + (original & 1u);
            map[c][nib] = (compact << 16) | (h & 65535);
        }
    }
    free(scratch);
    return 2 * (size_t)count * sizeof(cm_bucket);
}
#endif

static cm_t *cm_new(uint8_t *buf, int bb, int v2) {
    pthread_once(&cm_once, cm_init_tables);
    size_t mem = cm_mem_for(bb, v2); cm_acquire(mem);
    cm_t *m = calloc(1, sizeof(cm_t)); if (!m) { cm_release(mem); return NULL; }
    int nm = v2 ? CM_NMX : CM_NM;
    m->sh = 32 - bb; m->mem = mem; m->v2 = v2;
    m->tb = sizeof(cm_bucket) << bb;
    for (int i = 1; i < nm; i++) {                            /* the model-2 contexts (16+) have fewer distinct values */
        int b = i >= CM_NM && bb > 18 ? bb - CM_SMALL : bb;
        m->shi[i] = (uint8_t)(32 - b); m->tbi[i] = sizeof(cm_bucket) << b;
#ifdef KOLMO_CM_COMPACT
        if (i == 1 || i == 17) {
            m->tbi[i] = cm_build_byte_table(m->one_byte[i == 17], i, m->shi[i]);
            if (!m->tbi[i]) goto fail;
        }
#endif
        m->t[i] = cm_big(m->tbi[i]); if (!m->t[i]) goto fail;
    }
    if (v2) {
        m->w3 = cm_big(sizeof(*m->w3) * CM_W3); m->w4 = cm_big(sizeof(*m->w4) * CM_W4); m->a3 = cm_big(sizeof(*m->a3) * CM_A3);
        if (!m->w3 || !m->w4 || !m->a3) goto fail;
        for (int i = 0; i < CM_W3; i++) for (int j = 0; j < CM_NIX; j++) m->w3[i][j] = 1 << 14;
        for (int i = 0; i < CM_W4; i++) for (int j = 0; j < CM_NIX; j++) m->w4[i][j] = 1 << 14;
        for (int i = 0; i < CM_WF; i++) { for (int j = 0; j < 4; j++) m->wf[i][j] = 1 << 14; m->wf[i][4] = 0; }
        for (int c = 0; c < CM_A3; c++) for (int j = 0; j < 33; j++) m->a3[c][j] = (uint16_t)(cm_SQ((j - 16) * 128) * 16);
    }
    for (int i = 0; i < 65536; i++) m->o1[i] = 0x80000000u;
    for (int i = 0; i < nm; i++) for (int j = 0; j < 256 * 8; j++) {
        int st = j & 255; uint32_t n0 = st < bh_ns ? bh_n0[st] : 0, n1 = st < bh_ns ? bh_n1[st] : 0;
        m->sm[i][j] = (uint32_t)(((2ull * n1 + 1) << 22) / (2ull * (n0 + n1) + 2)) << 10;
    }
    for (int i = 0; i < CM_W1; i++) for (int j = 0; j < CM_NIX; j++) m->w1[i][j] = 1 << 14;
    for (int i = 0; i < CM_W2; i++) for (int j = 0; j < CM_NIX; j++) m->w2[i][j] = 1 << 14;
    for (int c = 0; c < 256; c++) for (int j = 0; j < 33; j++) m->a1[c][j] = (uint16_t)(cm_SQ((j - 16) * 128) * 16);
    for (int c = 0; c < 65536; c++) for (int j = 0; j < 33; j++) m->a2[c][j] = (uint16_t)(cm_SQ((j - 16) * 128) * 16);
    m->buf = buf; m->c0 = 1;
    m->mmask = (1u << 22) - 1; m->mt = cm_big(((size_t)m->mmask + 1) * 4); if (!m->mt) goto fail;
    for (int i = 0; i < 64; i++) m->mc[i] = 1u << 15;
    return m;
fail:
    cm_free_parts(m); cm_release(mem); return NULL;
}
static void cm_free(cm_t *m) {
    if (!m) return;
    size_t mem = m->mem;
    cm_free_parts(m); cm_release(mem);
}

#ifdef KOLMO_CM_COMPACT
static inline cm_bucket *cm_find_at(cm_bucket *t, uint32_t idx, uint32_t chk) {
#else
static inline cm_bucket *cm_find(cm_bucket *t, uint32_t h, int sh) {
    uint32_t idx = h >> sh, chk = h & 0xffff;
#endif
    cm_bucket *a = &t[idx], *b = &t[idx ^ 1];
    if ((a->s[0] >> 16) == chk) return a;
    if ((b->s[0] >> 16) == chk) return b;
    cm_bucket *v = ((a->s[0] & 0xffff) <= (b->s[0] & 0xffff)) ? a : b;
    v->s[0] = chk << 16;
    for (int k = 1; k < 16; k++) v->s[k] = 0x80000000u;
    return v;
}
#ifdef KOLMO_CM_COMPACT
static inline uint32_t cm_byte_entry(const cm_t *m, int i, unsigned nib) {
    unsigned c = i == 1 ? m->c4 & 255 : (m->c4 >> 8) & 255;
    return m->one_byte[i == 17][m->pos ? c : 256][nib];
}
static inline cm_bucket *cm_find_model(cm_t *m, int i, unsigned nib, uint32_t salt) {
    if (i == 1 || i == 17) {
        uint32_t entry = cm_byte_entry(m, i, nib);
        return cm_find_at(m->t[i], entry >> 16, entry & 65535);
    }
    uint32_t h = cm_hmix(m->h[i] + salt);
    return cm_find_at(m->t[i], h >> m->shi[i], h & 65535);
}
static inline uint32_t cm_prefetch_slot(const cm_t *m, int i, unsigned nib, uint32_t salt) {
    if (i == 1 || i == 17) return cm_byte_entry(m, i, nib) >> 16;
    return cm_hmix(m->h[i] + salt) >> m->shi[i];
}
#endif
static inline int cm_expected(cm_t *m) {
    if (!m->mlen) return -1;
    int b = m->buf[m->mptr];
    if (((b + 256) >> (8 - m->bitpos)) != (int)m->c0) return -1;
    return (b >> (7 - m->bitpos)) & 1;
}
/* slot: high 16 bits adaptive probability, bits 8..15 bit-history state, low 8 bits count */
static inline int cm_stc(uint32_t v) { return cm_st[v >> 20]; }

/* v2 is a compile-time constant at each call site, so model 1 and model 2 each get their own specialized loop. */
static inline __attribute__((always_inline)) int cm_predict(cm_t *m, const int v2) {
    const int nm = v2 ? CM_NMX : CM_NM, ni = 2 * nm + 3;
    uint32_t c0 = m->c0; int bp = (int)m->bitpos;
    if (bp == 0 || bp == 4) {
        uint32_t nib = bp ? (c0 & 15) * 0x5BD1E995u : 0;
#ifdef KOLMO_CM_COMPACT
        for (int i = 1; i < nm; i++) m->cur[i] = cm_find_model(m, i, bp ? (c0 & 15) : 0, nib);
#else
        for (int i = 1; i < nm; i++) m->cur[i] = cm_find(m->t[i], cm_hmix(m->h[i] + nib), m->shi[i]);
#endif
    }
    int node = bp < 4 ? (int)((c0 & ((1u << bp) - 1)) | (1u << bp)) : (int)((c0 & ((1u << (bp - 4)) - 1)) | (1u << (bp - 4)));
    m->slot[0] = &m->o1[c0 | (m->c4 & 0xff) << 8];
    for (int i = 1; i < nm; i++) m->slot[i] = &m->cur[i]->s[node];
    for (int i = 0; i < nm; i++) {
        uint32_t v = *m->slot[i]; uint16_t st = (uint16_t)((v >> 8 & 255) | bp << 8); m->sst[i] = st;
        m->x[i] = cm_stc(v); m->x[nm + 3 + i] = cm_st[m->sm[i][st] >> 20];
    }
    int eb = cm_expected(m);
    if (eb >= 0) {
        int l = m->mlen > 31 ? 31 : m->mlen;
        m->mci = l * 2 + eb;
        int p = (int)(m->mc[m->mci] >> 4); if (p < 1) p = 1; if (p > 4095) p = 4095;
        m->x[nm] = cm_st[p];
        m->x[nm + 1] = eb ? l * 16 : -l * 16;
    } else { m->mci = -1; m->x[nm] = 0; m->x[nm + 1] = 0; }
    m->x[nm + 2] = 64;
    int ml = m->mlen == 0 ? 0 : m->mlen < 16 ? 1 : m->mlen < 32 ? 2 : 3;
    uint32_t c1 = m->c4 & 0xff;
    int cls = ((c1 | 32) >= 'a' && (c1 | 32) <= 'z') ? 0 : (c1 >= '0' && c1 <= '9') ? 1 : 2;
    m->w1i = (int)c0 + 256 * cls;
    m->w2i = (ml * 8 + bp) * 4 + (int)(c1 >> 6);
    const int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    int st;
    if (!v2) {
        int64_t s1 = 0, s2 = 0;
        for (int i = 0; i < ni; i++) { s1 += (int64_t)w1[i] * m->x[i]; s2 += (int64_t)w2[i] * m->x[i]; }
        int d1 = (int)(s1 >> 16), d2 = (int)(s2 >> 16);
        m->pr1 = cm_SQ(d1); m->pr2 = cm_SQ(d2);
        st = (d1 + d2) >> 1;
    } else {                                                  /* four mixers, then a small learned mixer of their outputs */
        m->w3i = (int)((m->c4 >> 8 & 0xff) << 3 | (uint32_t)bp); m->w4i = (int)(c0 | c1 << 8); m->wfi = (int)c0 | ml << 8;
        const int32_t *w3 = m->w3[m->w3i], *w4 = m->w4[m->w4i], *wf = m->wf[m->wfi];
        int64_t s1 = 0, s2 = 0, s3 = 0, s4 = 0;
        for (int i = 0; i < ni; i++) {
            s1 += (int64_t)w1[i] * m->x[i]; s2 += (int64_t)w2[i] * m->x[i]; s3 += (int64_t)w3[i] * m->x[i]; s4 += (int64_t)w4[i] * m->x[i];
        }
        int d[4] = {(int)(s1 >> 16), (int)(s2 >> 16), (int)(s3 >> 16), (int)(s4 >> 16)};
        int64_t sf = (int64_t)wf[4] * 256;
        for (int k = 0; k < 4; k++) { if (d[k] > 2047) d[k] = 2047; if (d[k] < -2047) d[k] = -2047; m->xf[k] = d[k]; sf += (int64_t)wf[k] * d[k]; }
        m->xf[4] = 256;
        m->pr1 = cm_SQ(d[0]); m->pr2 = cm_SQ(d[1]); m->pr3 = cm_SQ(d[2]); m->pr4 = cm_SQ(d[3]);
        st = (int)(sf >> 16); if (st > 2047) st = 2047; if (st < -2047) st = -2047;
        m->prf = cm_SQ(st);
    }
    if (st > 2047) st = 2047; if (st < -2047) st = -2047;
    int mixed = cm_SQ(st);
    int s = st + 2048;
    m->ai = s >> 7; m->aw = s & 127;
    m->a2i = (int)((c0 | c1 << 8) & 0xffff);
    int lo = m->ai, wt = m->aw;
    int p1 = (m->a1[c0][lo] * (128 - wt) + m->a1[c0][lo + 1] * wt) >> 11;
    int p2 = (m->a2[m->a2i][lo] * (128 - wt) + m->a2[m->a2i][lo + 1] * wt) >> 11;
    int p;
    if (!v2) p = (mixed + p1 + p2) / 3;
    else {
        m->a3i = (int)(cm_hmix((m->c4 & 0xffff) * 0x9E3779B1u + c0 * 0x85EBCA6Bu) >> 16);
        int p3 = (m->a3[m->a3i][lo] * (128 - wt) + m->a3[m->a3i][lo + 1] * wt) >> 11;
        p = (mixed + p1 + p2 + p3 + 2) >> 2;
    }
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
static inline __attribute__((always_inline)) void cm_update(cm_t *m, int y, const int v2) {
    const int nm = v2 ? CM_NMX : CM_NM, ni = 2 * nm + 3;
    for (int i = 0; i < nm; i++) { cm_upd_sm(&m->sm[i][m->sst[i]], y); cm_upd_ctr(m->slot[i], y); }
    for (int i = 1; i < nm; i++) if ((m->cur[i]->s[0] & 0xffff) < 0xffff) m->cur[i]->s[0]++;
    if (m->mci >= 0) { uint32_t *c = &m->mc[m->mci]; *c = (uint32_t)((int64_t)*c + ((((int64_t)y << 16) - (int64_t)*c) >> 6)); }
    int e1 = (y << 12) - m->pr1, e2 = (y << 12) - m->pr2;                  /* learning rate 1/2048 in 16.16 weights */
    int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    if (!v2) for (int i = 0; i < ni; i++) { w1[i] += (m->x[i] * e1) >> CM_LR; w2[i] += (m->x[i] * e2) >> CM_LR; }
    else {
        int k = 64 - (int)(m->pos >> CM_LRD); if (k < CM_LRK) k = CM_LRK;
        e1 = e1 * k >> 4; e2 = e2 * k >> 4;
        int e3 = ((y << 12) - m->pr3) * k >> 4, e4 = ((y << 12) - m->pr4) * k >> 4, ef = (y << 12) - m->prf;
        int32_t *w3 = m->w3[m->w3i], *w4 = m->w4[m->w4i], *wf = m->wf[m->wfi];
        for (int i = 0; i < ni; i++) {
            w1[i] += (m->x[i] * e1) >> CM_LR; w2[i] += (m->x[i] * e2) >> CM_LR; w3[i] += (m->x[i] * e3) >> CM_LR; w4[i] += (m->x[i] * e4) >> CM_LR;
        }
        for (int k = 0; k < 5; k++) wf[k] += (m->xf[k] * ef) >> CM_LRF;
        cm_upd_apm(m->a3[m->a3i], m->ai, m->aw, y);
    }
    cm_upd_apm(m->a1[m->c0], m->ai, m->aw, y);
    cm_upd_apm(m->a2[m->a2i], m->ai, m->aw, y);
    int eb = cm_expected(m);
    if (eb >= 0 && eb != y) m->mlen = 0;
    m->c0 = (m->c0 << 1) | (uint32_t)y;
    if (m->bitpos == 2) {                                     /* prefetch both candidate buckets for the second nibble */
        for (int b = 0; b < 2; b++) {
            uint32_t nib = (((m->c0 << 1) | (uint32_t)b) & 15) * 0x5BD1E995u;
#ifdef KOLMO_CM_COMPACT
            for (int i = 1; i < nm; i++) { uint32_t idx = cm_prefetch_slot(m, i, ((m->c0 << 1) | (uint32_t)b) & 15, nib); __builtin_prefetch(&m->t[i][idx]); }
#else
            for (int i = 1; i < nm; i++) { uint32_t idx = cm_hmix(m->h[i] + nib) >> m->shi[i]; __builtin_prefetch(&m->t[i][idx]); }
#endif
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
        m->h[16] = cm_hmix((c4 & 0xffff0000u) * 0x9E3779B1u + 16);                                     /* model 2: bytes 3-4 */
        m->h[17] = cm_hmix((c4 & 0x0000ff00u) * 0x85EBCA6Bu + 17);                                     /* model 2: byte 2 */
        {   uint32_t pos = (uint32_t)m->pos, d = pos - m->rl_last[c];                 /* record length: a repeated byte-to-byte distance */
            if (m->rl_last[c] && d == m->rl_d[c] && d >= 2 && d < 65536) {
                if (d == m->rcand) { if (m->rcnt < 255) m->rcnt++; if (m->rcnt >= 8 && m->rlen != d) m->rlen = d; }
                else if (m->rcnt) m->rcnt--; else { m->rcand = d; m->rcnt = 1; }
            }
            m->rl_d[c] = d; m->rl_last[c] = pos;
            uint32_t r = m->rlen, ab = r && pos >= r ? m->buf[pos - r] : 0, ab2 = r && pos >= 2 * r ? m->buf[pos - 2 * r] : 0;
            m->h[18] = cm_hmix((ab << 16 | ab2 << 8 | (r ? 1u : 0u)) * 0x9E3779B1u + r * 0x85EBCA6Bu + 18);
            m->h[19] = cm_hmix((ab << 8 | (c4 & 0xff)) * 0xC2B2AE35u + r * 0x9E3779B1u + 19);
            uint32_t col = r ? pos % r : 0; if (col > 4095) col = 4095;
            m->h[20] = cm_hmix((col << 8 | (c4 & 0xff)) * 0x2545F491u + r * 0x85EBCA6Bu + 20);
        }
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
        for (int i = 1; i < nm; i++) {                        /* prefetch the buckets for the first nibble */
#ifdef KOLMO_CM_COMPACT
            uint32_t idx = cm_prefetch_slot(m, i, 0, 0);
#else
            uint32_t idx = cm_hmix(m->h[i]) >> m->shi[i];
#endif
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
/* One coding loop per model, each with the model inlined and specialized. */
static inline __attribute__((always_inline)) int cm_enc_loop(cm_t *m, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *po, const int v2) {
    uint32_t x1 = 0, x2 = 0xffffffff; size_t o = *po;
    for (size_t i = 0; i < n; i++) {
        int c = in[i];
        for (int b = 7; b >= 0; b--) {
            int y = (c >> b) & 1, p = cm_predict(m, v2);
            uint32_t xm = x1 + (uint32_t)(((uint64_t)(x2 - x1) * (uint32_t)p) >> 12);
            if (y) x2 = xm; else x1 = xm + 1;
            cm_update(m, y, v2);
            while (((x1 ^ x2) & 0xff000000u) == 0) { if (o >= cap) return -1; out[o++] = (uint8_t)(x2 >> 24); x1 <<= 8; x2 = (x2 << 8) | 255; }
        }
    }
    if (o + 4 > cap) return -1;
    out[o++] = (uint8_t)(x1 >> 24); out[o++] = 255; out[o++] = 255; out[o++] = 255;
    *po = o; return 0;
}
static inline __attribute__((always_inline)) void cm_dec_loop(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, const int v2) {
    (void)out;
    uint32_t x1 = 0, x2 = 0xffffffff, x = 0; size_t ip = 4;
    for (int k = 0; k < 4; k++) x = (x << 8) | (ip < cn ? in[ip++] : 255);
    for (size_t i = 0; i < n; i++)
        for (int b = 7; b >= 0; b--) {
            int p = cm_predict(m, v2);
            uint32_t xm = x1 + (uint32_t)(((uint64_t)(x2 - x1) * (uint32_t)p) >> 12);
            int y = x <= xm;
            if (y) x2 = xm; else x1 = xm + 1;
            cm_update(m, y, v2);
            while (((x1 ^ x2) & 0xff000000u) == 0) { x1 <<= 8; x2 = (x2 << 8) | 255; x = (x << 8) | (ip < cn ? in[ip++] : 255); }
        }
}
/* model 2 by default; model 1 (fewer contexts, about 30% faster, what kolmo 1.0.0 wrote) when model1 is set, and always
 * for n >= 2^28, whose size word has no room for flags */
static size_t cm_compress_m(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model1) {
    if (cap < 8 || n > 0xFFFFFFF0u) return 0;
    int small = n < ((size_t)1 << 28), v2 = small && !model1;
    int bb = small ? cm_bits_for(n) : CM_BB;
    uint8_t *hist = malloc(n + 8); cm_t *m = hist ? cm_new(hist, bb, v2) : NULL;
    if (!m) { free(hist); return 0; }
    uint32_t n32 = (uint32_t)n | (small ? (uint32_t)((bb - 15) | (v2 ? 8 : 0)) << 28 : 0); memcpy(out, &n32, 4);
    size_t o = 4;
    int r = v2 ? cm_enc_loop(m, in, n, out, cap, &o, 1) : cm_enc_loop(m, in, n, out, cap, &o, 0);
    cm_free(m); free(hist);
    return r ? 0 : o;
}
static size_t cm_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { return cm_compress_m(in, n, out, cap, 0); }
static int cm_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 8) return -1;
    uint32_t n32; memcpy(&n32, in, 4);
    int bb = CM_BB, v2 = 0;
    if (n < ((size_t)1 << 28)) { uint32_t f = n32 >> 28; n32 &= 0x0FFFFFFFu; if (f) { bb = (int)(f & 7) + 15; v2 = (int)(f >> 3); } }
    if (n32 != n || bb < 16 || bb > 22) return -1;
    cm_t *m = cm_new(out, bb, v2); if (!m) return -1;
    if (v2) cm_dec_loop(m, in, cn, out, n, 1); else cm_dec_loop(m, in, cn, out, n, 0);
    cm_free(m);
    return 0;
}
