/* ---------------------------------------------------------------- czcm: integer context-mixing coder (codec 7)
 * lpaq-style: bitwise prediction from hashed contexts (orders 1-4, 6, a word model, a sparse model) stored in
 * nibble buckets, an order-1 direct model and a match model; inputs mixed by two small neural networks with integer
 * weights selected by context; two adaptive probability maps refine the result; binary arithmetic coding.
 * Integer arithmetic only, so encoder and decoder agree bit-for-bit on every compiler and CPU.
 * Model 2 (streams flagged 8 in the size word) adds two sparse contexts (bytes 3-4, byte 2), two record contexts (the
 * bytes one and two records back, once a fixed record length is detected, and the column within the record), two more mixers (weights
 * selected by byte 2 and by order 1), a learned final mixer in place of the plain average, and an order-2 hashed APM.
 * Model 3 (extended header) adds orders 12 and 20, the byte the match model expects (with orders 1 and 2) as
 * contexts, finer length classes for long matches, and a match that carries on past a one-byte difference.
 * Stream: size u32 (low 28 bits n, top 4 bits f: 0 = model 1 with 2^CM_BB buckets, else log2 buckets - 15 in f & 7,
 * model 2 when f & 8; f = 8 exactly: one more byte, nibble << 4 | log2 buckets - 15, nibble 3 = model 3, 5/6/7 = model
 * 1/2/3 with the exact logistic tables, which every encoder since 1.2.0-rc2 writes) | coded bytes.  Image model (codec 17):
 * bit 7 of the channel byte selects the exact tables.  Memory: 2^bits * 64 B per hashed model. */
#include <math.h>              /* exp, floor: the squash knots (cm_knots_init) */
#include "../arch/kernels.h"   /* mixer dot products and updates: kolmo_cm_dot_c / kolmo_cm_train_c (src/algo/mixer.c) */
#ifndef CM_NM
#define CM_NM 16                /* models: 0 = order-1 direct, 1..9 hashed */
#endif
#define CM_NI (2 * CM_NM + 3)   /* two inputs per model, two from the match model, bias */
#ifndef CM_SMALL
#define CM_SMALL 1               /* model-2 contexts (16+) use tables 2^CM_SMALL times smaller */
#endif
#define CM_NMX 21               /* models in model 2 */
#define CM_NIX (2 * CM_NMX + 3)
#define CM_NM3 30               /* models in model 3: model 2 + orders 12 and 20, the match model's expected byte with orders 1 and 2,
                                 * and five structure contexts (key, field) */
#define CM_NIX3 (2 * CM_NM3 + 3)
#ifndef CM_MCAND
#define CM_MCAND 16              /* model 3: match candidates examined */
#endif
#define CM_NM4 32               /* models in model 4: model 3 + two token-shape contexts (a hand-written lexer: digits, letters,
                                 * spaces and each punctuation byte as classes; the classes of the last tokens, and the same
                                 * token of the line before) */
#define CM_NIX4 (2 * CM_NM4 + 3)
#define CM_NMOD(v) ((v) == 4 ? CM_NM4 : (v) == 3 ? CM_NM3 : (v) ? CM_NMX : CM_NM)
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

static int16_t cm_sq[4096], cm_st[4096];      /* the logistic function sampled at 33 knots and interpolated (legacy) */
static int16_t cm_sqx[4096], cm_stx[4096];    /* the logistic function itself at every point (model nibbles 5-7) */
static uint32_t cm_rcp[64], cm_srcp[1024];
#define SM_LIM 255
static pthread_once_t cm_once = PTHREAD_ONCE_INIT;
/* The logistic function in 12-bit fixed point: squash(x) ~ 4096 / (1 + e^(-x/256)), x a stretched probability in
 * [-2047, 2047]. It is sampled at the 33 knots x = 128 (k - 16), k = 0..32, and linearly interpolated between them
 * with rounding to nearest. Knots below the midpoint are floor(4096 / (1 + e^((16 - k) / 2))); the curve is
 * symmetric, squash(-x) = 4095 - squash(x), so the knots above it mirror those below and the midpoint is 2047.
 * (Every knot value lies at least 0.0025 away from an integer, so floor() of the double result is exact on any
 * conforming libm.) The values are those every PAQ-family coder uses, so frames stay compatible. */
static int cm_knot[33];
static void cm_knots_init(void) {
    for (int k = 0; k < 16; k++) cm_knot[k] = (int)floor(4096.0 / (1.0 + exp((16 - k) * 0.5)));
    cm_knot[16] = 2047;
    for (int k = 0; k < 16; k++) cm_knot[32 - k] = 4095 - cm_knot[k];
}
static int cm_squash_i(int x) {
    if (x >= 2048) return 4095;
    if (x <= -2048) return 1;
    int seg = (x + 2048) >> 7, frac = (x + 2048) & 127;          /* knot index below x, distance past it (of 128) */
    return (cm_knot[seg] * (128 - frac) + cm_knot[seg + 1] * frac + 64) >> 7;   /* weighted mean of the two knots */
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
    cm_knots_init();
    for (int i = 0; i < 4096; i++) { int v = cm_squash_i(i - 2048); cm_sq[i] = (int16_t)(v < 1 ? 1 : v > 4095 ? 4095 : v); }
    /* exact: round(4096 / (1 + e^(-x/256))) at every x; no value lies within 0.00017 of a rounding boundary, so the
     * result is the same with any conforming libm */
    for (int i = 0; i < 4096; i++) { int v = (int)floor(4096.0 / (1.0 + exp(-(i - 2048) / 256.0)) + 0.5); cm_sqx[i] = (int16_t)(v < 1 ? 1 : v > 4095 ? 4095 : v); }
    /* stretch = the inverse of squash: for a 12-bit probability p, the smallest x in [-2047, 2047] whose squash
     * reaches p (2047 when none does). squash is non-decreasing, so a binary search over x finds it. */
    for (int p = 0; p < 4096; p++) {
        int lo = -2047, hi = 2048, lx = -2047, hx = 2048;  /* answers in [lo, hi] and [lx, hx]; 2048 stands for "none" */
        while (lo < hi) { int mid = lo + ((hi - lo) >> 1); if (cm_squash_i(mid) >= p) hi = mid; else lo = mid + 1; }
        while (lx < hx) { int mid = lx + ((hx - lx) >> 1); if (cm_sqx[mid + 2048] >= p) hx = mid; else lx = mid + 1; }
        cm_st[p] = (int16_t)(lo > 2047 ? 2047 : lo); cm_stx[p] = (int16_t)(lx > 2047 ? 2047 : lx);
    }
    for (int i = 0; i < 64; i++) cm_rcp[i] = (uint32_t)(655360 / (10 * i + 16));
    for (int i = 0; i < 1024; i++) cm_srcp[i] = (uint32_t)(655360 / (10 * i + 15));
}
static inline int cm_SQ(const int16_t *sq, int x) { if (x > 2047) x = 2047; if (x < -2047) x = -2047; return sq[x + 2048]; }

#ifdef KOLMO_CM_COMPACT
enum { CM_BYTE_VALUES = 257, CM_NIBBLES = 16, CM_PAIR_BITS = 13 };
typedef struct {
    uint32_t keys[1u << CM_PAIR_BITS];
    uint16_t pairs[1u << CM_PAIR_BITS];
} cm_pair_scratch;
#endif
/* Per-stream learning settings of models 2 and 3 (docs/TWOPASS.md). The defaults (code 0) are the constants every
 * stream before format 2 used; an encoder that searched the file for better ones writes them as two bytes after the
 * model byte (model nibbles 13-15), so the decoder learns at exactly the same rates. Each field takes 2 bits:
 *   byte 0: mixer rate shift CM_LR + {0,-1,+1,+2} | counter limit {2,1,3,4} << 2 | APM rate shift {13,12,14,15} << 4
 *           | match-model rate shift {6,5,7,8} << 6
 *   byte 2: initial mixer weights: 0 = every weight 1/4 (as before), 1 = the shared prior of the model: one row per
 *           mixer, trained once on other data and compiled in (src/codecs/cm_prior.inc), copied to every row
 *   byte 1: mixer rate floor {20,12,28,40} | rate decay period 2^{13,12,14,16} << 2 | state-map count limit
 *           {255,127,511,1023} << 4 | final weights of mixer and APMs (of 8) {2222,4112,1223,3122} << 6 */
typedef struct { uint8_t b0, b1, b2; } cm_tune_t;   /* b2: initial mixer weights, 0 = 1/4 per input, 1 = the shared prior of the model (cm_prior.inc) */
static const int8_t cm_tv_lr[4] = {0, -1, 1, 2}, cm_tv_lim[4] = {2, 1, 3, 4}, cm_tv_apm[4] = {13, 12, 14, 15}, cm_tv_mc[4] = {6, 5, 7, 8};
static const int8_t cm_tv_lrk[4] = {20, 12, 28, 40}, cm_tv_lrd[4] = {13, 12, 14, 16};
static const uint16_t cm_tv_sm[4] = {255, 127, 511, 1023};
static const uint8_t cm_tv_fin[4][4] = {{2, 2, 2, 2}, {4, 1, 1, 2}, {1, 2, 2, 3}, {3, 1, 2, 2}};
typedef struct { uint32_t s[16]; } cm_bucket;          /* s[0] = check << 16 | priority; s[1..15] nibble-tree counters */
typedef struct {
    cm_bucket *t[CM_NM4];
    const int16_t *sq, *st;                  /* logistic tables: cm_sq/cm_st (legacy) or cm_sqx/cm_stx (exact) */
#ifdef KOLMO_CM_COMPACT
    /* High 16 bits hold the compact slot, low 16 hold the original tag. */
    uint32_t one_byte[2][CM_BYTE_VALUES][CM_NIBBLES];
#endif
    int sh, v2; size_t mem, tb; uint8_t shi[CM_NM4]; size_t tbi[CM_NM4];                                  /* 32 - log2 buckets; model 2?; bytes held */
    uint32_t o1[256 * 256];
    uint32_t h[CM_NM4]; uint32_t *slot[CM_NM4]; cm_bucket *cur[CM_NM4];
    int32_t w1[CM_W1][CM_NIX4], w2[CM_W2][CM_NIX4];
    int32_t (*w3)[CM_NIX4], (*w4)[CM_NIX4], wf[CM_WF][5];        /* model 2 */
    int xf[5], pr3, pr4, prf, w3i, w4i, wfi, a3i; uint16_t (*a3)[33];
    uint32_t sm[CM_NM4][256 * 8]; uint16_t sst[CM_NM4];           /* state -> probability (22 bits) | count (10 bits) */
    int x[CM_NIX4];
    uint32_t rl_last[256], rl_d[256], rlen, rcnt, rcand;
    int pr, pr1, pr2, w1i, w2i;
    uint16_t a1[256][33], a2[65536][33];
    int ai, aw, a2i;
    uint8_t *buf; int64_t pos;
    uint32_t c0, c4, c8, word, word1, word2, bitpos; uint16_t ind1[256]; uint16_t ind2[65536]; int64_t line0, line1;   /* current and previous line starts */
    uint32_t *mt, *mprev, mmask; int64_t mptr; int mlen; size_t mprev_n;   /* mprev (model 3): the earlier position with the same hash, a chain */
    int mrec;                                             /* model 3: attempts left to pick a broken match up again */
    uint32_t s_tok, s_ltok, s_key, s_kpos, s_lt, s_fidx, s_fst[32], s_pfst[32], s_pfn;   /* model 3: token, last token, key, bytes since the key, after '<', field index and starts */
    int64_t s_ls, s_pls;                                  /* this line's and the previous line's start */
    uint32_t k_cls, k_tok, k_kp, k_pn; uint64_t k_sr;        /* model 4: class of the last byte, token index in the line, bytes into the token,
                                                           * tokens of the previous line, classes of the last 8 tokens */
    uint32_t k_st[128], k_pst[128];                       /* model 4: token starts (offsets in the line) of this line and the line before */
    uint32_t iw, ich, ih, i_pred, i_cls, i_act, i_n, i_med, i_wn;   /* image model: row bytes, channels, head; mixer selectors */
    uint32_t mc[64];
    int mci;
    const kolmo_kernels *kk;                              /* dispatched kernels (src/arch) */
    /* learning settings (cm_tune_t; the defaults are the compile-time constants, a tuned stream carries its own) */
    int lr, lrk, lrd, apm_sh, mc_sh; uint32_t smlim; int fw[4]; uint32_t rcp[16];
    size_t ck_at, ck_out;                                 /* the race: output bytes written once ck_at input bytes are coded */
} cm_t;

static inline uint32_t cm_hmix(uint32_t a) { a ^= a >> 15; a *= 0x2C1B3C6Du; a ^= a >> 12; a *= 0x297A2D39u; a ^= a >> 15; return a; }

/* Memory: a model reserves its tables with the governor (core/memory.c) unless the thread is inside a unit of work
 * whose reservation already covers it (a block job, a trial, a part). Concurrent coders then wait instead of swapping;
 * a coder larger than the whole budget still runs when it is alone. When decoding, a model larger than the frame
 * declares (header byte 11) is refused before anything is allocated. */
#ifndef KMEM_SAMPLE_NS   /* built on its own (tests/test_cm_compact.c): no governor */
#include <stdatomic.h>
#define KMEM_UNLIMITED UINT64_MAX
static _Thread_local int kmem_tls_job; static _Thread_local uint64_t kmem_tls_cap, kmem_tls_dec_limit; static _Thread_local size_t kmem_tls_raw;
static size_t cm_used;   /* bytes the models hold (tests/test_cm_compact.c checks it returns to 0) */
static void kmem_reserve(uint64_t b, uint64_t cap) { (void)cap; cm_used += b; }
static void kmem_release(uint64_t b) { cm_used -= b; }
static uint64_t kmem_buffers(uint64_t raw) { return 2 * raw + ((uint64_t)1 << 20); }
static int kmem_try_reserve(uint64_t b, uint64_t cap) { (void)cap; cm_used += b; return 1; }
typedef struct { uint64_t cap, dec_limit; size_t raw; } kmem_ctx;
static kmem_ctx kmem_ctx_get(void) { kmem_ctx c = {kmem_tls_cap, kmem_tls_dec_limit, kmem_tls_raw}; return c; }
static void kmem_ctx_set(kmem_ctx c) { kmem_tls_cap = c.cap; kmem_tls_dec_limit = c.dec_limit; kmem_tls_raw = c.raw; }
#endif
static atomic_long cm_models_made;   /* models allocated (tests: a refused model is never allocated) */
static size_t cm_acquire(size_t b) {
    if (kmem_tls_job) return 0;
    kmem_reserve(b, kmem_tls_cap); return b;
}
static void cm_release(size_t b) { if (b) kmem_release(b); }
/* Hash-table size for an n-byte stream: 1 bucket per byte, between 2^16 and 2^22 per model (3.8 GB at most). */
#ifndef CM_BB_MAX
#define CM_BB_MAX 22
#endif
static int cm_bits_for(size_t n);
static size_t cm_mem_for(int bb, int v2) {
    size_t t = (size_t)sizeof(cm_bucket) << bb, ts = bb > 18 ? t >> CM_SMALL : t;
#ifdef KOLMO_CM_COMPACT
    /* A distinct (byte, nibble) can introduce at most one two-bucket pair.
     * Account for both maps and the temporary hash table before allocation. */
    size_t max_bank = 2u * CM_BYTE_VALUES * CM_NIBBLES * sizeof(cm_bucket);
    uint64_t bytes = ((uint64_t)(v2 ? 48 : 16) << 20) + sizeof(((cm_t *)0)->one_byte) + sizeof(cm_pair_scratch);
    int nm = CM_NMOD(v2);
    for (int bank = 1; bank < nm; ++bank) {
        size_t bank_bytes = bank >= CM_NM3 - 5 ? (bb > 18 ? ts >> 1 : t) : bank >= CM_NM ? ts : t;
        if ((bank == 1 || bank == 17) && bank_bytes > max_bank) bank_bytes = max_bank;
        bytes += bank_bytes;
    }
    return bytes > SIZE_MAX ? SIZE_MAX : (size_t)bytes;
#else
    return (size_t)(CM_NM - 1) * t + (v2 ? (size_t)(CM_NMX - CM_NM) * ts : 0) + (v2 >= 3 ? (size_t)(CM_NM3 - 5 - CM_NMX) * ts + (v2 == 4 ? 7 : 5) * (bb > 18 ? ts >> 1 : t) : 0) + ((size_t)(v2 ? 48 : 16) << 20);
#endif
}

/* Table sizing (encoder; the stream header carries the bits, so the decoder needs exactly what the encoder chose).
 * A format decision: it depends on the stream's length, this setting and the call's explicit memory cap only, never on
 * the memory free at the time (docs/MEMORY-MODEL.md, "memory changes speed, never bytes").
 * -1 ("scaled", the default from rc3): the largest size whose tables stay within 16 times the stream (2^16 at least);
 * 0 ("full", rc2's sizing): one bucket per input byte, 2^16 to 2^CM_BB_MAX per model; 16..22: full, capped at 2^bits.
 * Then, with an explicit cap (kmem_tls_cap), the largest size whose decode working set fits the cap (2^16 at least). */
static int cm_tables_mode = -1;
int kolmo_set_x_tables(int mode) {
    if (mode != 0 && mode != -1 && (mode < 16 || mode > CM_BB_MAX)) return -1;
    __atomic_store_n(&cm_tables_mode, mode, __ATOMIC_RELAXED); return 0;
}
static int cm_bits_for(size_t n) {
    const int mode = __atomic_load_n(&cm_tables_mode, __ATOMIC_RELAXED), cap = mode >= 16 ? mode : CM_BB_MAX;
    int b = 16; while (b < cap && ((size_t)1 << b) < n) b++;
    if (mode == -1) while (b > 16 && cm_mem_for(b, 3) > 16 * (n + ((size_t)1 << 20))) b--;
    if (kmem_tls_cap && kmem_tls_cap != KMEM_UNLIMITED) {
        size_t raw = kmem_tls_raw > n ? kmem_tls_raw : n;
        uint64_t fixed = kmem_buffers(raw) + 4 * ((uint64_t)n + 8);   /* the block's buffers and model 3's match chain */
        while (b > 16 && cm_mem_for(b, 3) + fixed > kmem_tls_cap) b--;
    }
    return b;
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
    for (int i = 1; i < CM_NM4; i++) cm_big_free(m->t[i], m->tbi[i]);
    cm_big_free(m->w3, sizeof(*m->w3) * CM_W3); cm_big_free(m->w4, sizeof(*m->w4) * CM_W4);
    cm_big_free(m->a3, sizeof(*m->a3) * CM_A3); cm_big_free(m->mt, ((size_t)m->mmask + 1) * 4); cm_big_free(m->mprev, m->mprev_n); free(m);
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

#include "cm_prior.inc"   /* cm_prior[model 2 / 3][mixer][input]: weights / 4 */
#ifdef KOLMO_CODEC_REGISTRY
enum { CM_NKPRIOR = TT_SQL - 1, CM_NPRIOR = 2 + CM_NKPRIOR };   /* tune byte 2 & 0x7f: 0 uniform weights, 1 the shared prior, 2 + k - 1 the
                                                                * prior of kind k (1-8; SQL dumps, which -x codes as tables, have none) */
#else
enum { CM_NPRIOR = 2 };                  /* built on its own (tests/test_cm_compact.c): no per-kind priors */
#endif
/* every row of the five mixers starts from the prior's row for that mixer (weights in 16.16, stored / 4). The prior's
 * rows are laid out for model 3 (30 models); model 4's two extra models start at 1/4 and the inputs after them move. */
static void cm_apply_prior(cm_t *m, const int16_t p[5][CM_NIX3]) {
    const int nm = CM_NMOD(m->v2), ni = 2 * nm + 3;
    for (int j = 0; j < ni; j++) {
        const int jp = m->v2 != 4 || j < CM_NM3 ? j : j < nm ? -1 : j - nm + CM_NM3 < CM_NIX3 ? j - nm + CM_NM3 : -1;
        if (jp < 0) continue;                          /* stays 1/4 (cm_new_x) */
        const int32_t a = p[0][jp] * 4, b = p[1][jp] * 4, c = p[2][jp] * 4, d = p[3][jp] * 4;
        for (int i = 0; i < CM_W1; i++) m->w1[i][j] = a;
        for (int i = 0; i < CM_W2; i++) m->w2[i][j] = b;
        if (m->v2) { for (int i = 0; i < CM_W3; i++) m->w3[i][j] = c; for (int i = 0; i < CM_W4; i++) m->w4[i][j] = d; }
    }
    if (m->v2) for (int i = 0; i < CM_WF; i++) for (int j = 0; j < 5; j++) m->wf[i][j] = p[4][j] * 4;
}
static const int16_t (*cm_prior_override)[CM_NIX3];   /* research harnesses only (benchmark/rc3w2/twopass): replaces prior 1 */
#ifdef KOLMO_CODEC_REGISTRY
/* Per-kind priors (docs/PRIORS.md): for each kind of text (algo/texttype.c), the tables of a model-3 coder trained on
 * public data of that kind that do not depend on context hashes, quantised: every row of mixers w1, w2, w3 and the final
 * mixer, the mean row of w4 (weights of 4 bits, -7..7 in steps of 1/8), the state maps and the order-1 table (stretched
 * probabilities in 17 levels with a count class), the match-model counters. Stored as raw LZMA2 (cm_kprior.inc),
 * unpacked once per process on first use. */
#include "cm_kprior.inc"
typedef struct {
    int8_t w1[CM_W1][CM_NIX3], w2[CM_W2][CM_NIX3], w3[CM_W3][CM_NIX3], w4m[CM_NIX3], wf[CM_WF][5];
    uint8_t sm[CM_NM3][8][256], o1q[65536], o1s[65536], mc[64];
} cm_kprior_t;
static cm_kprior_t *cm_kp_cache[CM_NKPRIOR];
static pthread_mutex_t cm_kp_mu = PTHREAD_MUTEX_INITIALIZER;
static const cm_kprior_t *cm_kprior_get(int k) {   /* k: kind - 1 */
    pthread_mutex_lock(&cm_kp_mu);
    cm_kprior_t *p = cm_kp_cache[k];
    if (!p) {
        uint8_t *raw = malloc(CM_KP_RAW); p = calloc(1, sizeof *p);
        lzma_options_lzma opt; lzma_lzma_preset(&opt, 6); opt.dict_size = 1u << 20; opt.lc = 3; opt.lp = 0; opt.pb = 2;
        lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
        size_t ip = 0, op = 0;
        if (!raw || !p || lzma_raw_buffer_decode(f, NULL, cm_kp_blob[k].z, &ip, cm_kp_blob[k].zn, raw, &op, CM_KP_RAW) != LZMA_OK || op != CM_KP_RAW) { free(p); p = NULL; }
        else {
            const uint8_t *q = raw;
            memcpy(p->w1, q, sizeof p->w1); q += sizeof p->w1; memcpy(p->w2, q, sizeof p->w2); q += sizeof p->w2;
            memcpy(p->w3, q, sizeof p->w3); q += sizeof p->w3; memcpy(p->w4m, q, sizeof p->w4m); q += sizeof p->w4m;
            memcpy(p->wf, q, sizeof p->wf); q += sizeof p->wf;
            for (int i = 0; i < CM_NM3; i++) for (int b = 0; b < 8; b++) for (int st = 0; st < bh_ns; st++) p->sm[i][b][st] = *q++;
            memcpy(p->o1q, q, 65536); q += 65536; memcpy(p->o1s, q, 65536); q += 65536; memcpy(p->mc, q, 64);
            cm_kp_cache[k] = p;
        }
        free(raw);
    }
    pthread_mutex_unlock(&cm_kp_mu);
    return p;
}
/* model 3's input j of the prior for model 4's input j (-1: one of model 4's own models, which keeps its start value) */
static inline int cm_kp_col(int v2, int j) { const int nm = CM_NMOD(v2); return v2 != 4 || j < CM_NM3 ? j : j < nm || j - nm + CM_NM3 >= CM_NIX3 ? -1 : j - nm + CM_NM3; }
static int cm_apply_kprior(cm_t *m, int k) {
    const cm_kprior_t *p = cm_kprior_get(k); if (!p) return -1;
    const int ni = 2 * CM_NMOD(m->v2) + 3;
#define KPW(row, j) (cm_kp_col(m->v2, j) < 0 ? 1 << 14 : (int32_t)(row)[cm_kp_col(m->v2, j)] * 8192)
    for (int i = 0; i < CM_W1; i++) for (int j = 0; j < ni; j++) m->w1[i][j] = KPW(p->w1[i], j);
    for (int i = 0; i < CM_W2; i++) for (int j = 0; j < ni; j++) m->w2[i][j] = KPW(p->w2[i], j);
    for (int i = 0; i < CM_W3; i++) for (int j = 0; j < ni; j++) m->w3[i][j] = KPW(p->w3[i], j);
    { int32_t r[CM_NIX4]; for (int j = 0; j < ni; j++) r[j] = KPW(p->w4m, j); for (int i = 0; i < CM_W4; i++) memcpy(m->w4[i], r, sizeof(int32_t) * (size_t)ni); }
#undef KPW
    for (int i = 0; i < CM_WF; i++) for (int j = 0; j < 5; j++) m->wf[i][j] = (int32_t)p->wf[i][j] * 8192;
    static const uint16_t smn[4] = {0, 4, 12, 30}, o1n[4] = {0, 2, 6, 15};
    for (int i = 0; i < CM_NM3; i++) for (int b = 0; b < 8; b++) for (int st = 0; st < bh_ns; st++) {
        const uint8_t v = p->sm[i][b][st]; const uint32_t p12 = (uint32_t)cm_SQ(m->sq, ((v & 31) - 8) * 256);   /* level 16 = 2047 */
        m->sm[i][b << 8 | st] = p12 << 20 | smn[v >> 5];
    }
    for (int i = 0; i < 65536; i++) { const uint8_t v = p->o1q[i]; const uint32_t p12 = (uint32_t)cm_SQ(m->sq, ((v & 31) - 8) * 256); m->o1[i] = p12 << 20 | (uint32_t)p->o1s[i] << 8 | o1n[v >> 5]; }
    for (int i = 0; i < 64; i++) m->mc[i] = (uint32_t)cm_SQ(m->sq, (p->mc[i] - 32) * 64) << 4;
    return 0;
}
#endif
static int cm_set_tune(cm_t *m, cm_tune_t t) {
    const int pid = t.b2 & 0x7f;
#ifdef KOLMO_CODEC_REGISTRY
    if (pid >= 2) { if (m->v2 < 3 || pid >= CM_NPRIOR || cm_apply_kprior(m, pid - 2)) return -1; }
#endif
    if (pid == 1 && m->v2) cm_apply_prior(m, cm_prior_override ? cm_prior_override : cm_prior[m->v2 >= 3]);
    m->lr = CM_LR + cm_tv_lr[t.b0 & 3]; const int lim = cm_tv_lim[t.b0 >> 2 & 3]; m->apm_sh = cm_tv_apm[t.b0 >> 4 & 3]; m->mc_sh = cm_tv_mc[t.b0 >> 6];
    m->lrk = cm_tv_lrk[t.b1 & 3]; m->lrd = cm_tv_lrd[t.b1 >> 2 & 3]; m->smlim = cm_tv_sm[t.b1 >> 4 & 3];
    for (int k = 0; k < 4; k++) m->fw[k] = cm_tv_fin[t.b1 >> 6][k];
    for (int n = 0; n < 16; n++) m->rcp[n] = cm_rcp[n * lim];
    return 0;
}
static cm_t *cm_new_x(uint8_t *buf, int bb, int v2, size_t chain, int lx) {   /* lx: exact logistic tables */   /* chain: bytes for model 3's match chain (4 per input byte) */
    pthread_once(&cm_once, cm_init_tables);
    size_t mem = cm_mem_for(bb, v2) + chain;
    if (kmem_tls_dec_limit && mem > kmem_tls_dec_limit) return NULL;   /* decoding: more than the frame declares */
    size_t resv = cm_acquire(mem);
    atomic_fetch_add_explicit(&cm_models_made, 1, memory_order_relaxed);
    cm_t *m = calloc(1, sizeof(cm_t)); if (!m) { cm_release(resv); return NULL; }
    int nm = CM_NMOD(v2);
    m->kk = kolmo_kernels_get();
    m->sh = 32 - bb; m->mem = resv; m->v2 = v2; m->sq = lx ? cm_sqx : cm_sq; m->st = lx ? cm_stx : cm_st;
    m->tb = sizeof(cm_bucket) << bb;
    for (int i = 1; i < nm; i++) {                            /* the model-2 contexts (16+) have fewer distinct values */
        int b = i >= CM_NM && bb > 18 ? bb - CM_SMALL - (i >= CM_NM3 - 5) : bb;   /* model 2 and 3 contexts: half; structure contexts: a quarter */
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
        for (int i = 0; i < CM_W3; i++) for (int j = 0; j < CM_NIX4; j++) m->w3[i][j] = 1 << 14;
        for (int i = 0; i < CM_W4; i++) for (int j = 0; j < CM_NIX4; j++) m->w4[i][j] = 1 << 14;
        for (int i = 0; i < CM_WF; i++) { for (int j = 0; j < 4; j++) m->wf[i][j] = 1 << 14; m->wf[i][4] = 0; }
        for (int c = 0; c < CM_A3; c++) for (int j = 0; j < 33; j++) m->a3[c][j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    }
    for (int i = 0; i < 65536; i++) m->o1[i] = 0x80000000u;
    for (int i = 0; i < nm; i++) for (int j = 0; j < 256 * 8; j++) {
        int st = j & 255; uint32_t n0 = st < bh_ns ? bh_n0[st] : 0, n1 = st < bh_ns ? bh_n1[st] : 0;
        m->sm[i][j] = (uint32_t)(((2ull * n1 + 1) << 22) / (2ull * (n0 + n1) + 2)) << 10;
    }
    for (int i = 0; i < CM_W1; i++) for (int j = 0; j < CM_NIX4; j++) m->w1[i][j] = 1 << 14;
    for (int i = 0; i < CM_W2; i++) for (int j = 0; j < CM_NIX4; j++) m->w2[i][j] = 1 << 14;
    for (int c = 0; c < 256; c++) for (int j = 0; j < 33; j++) m->a1[c][j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    for (int c = 0; c < 65536; c++) for (int j = 0; j < 33; j++) m->a2[c][j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    m->buf = buf; m->c0 = 1; m->ck_at = SIZE_MAX;
    m->mmask = (1u << 22) - 1; m->mt = cm_big(((size_t)m->mmask + 1) * 4); if (!m->mt) goto fail;
    for (int i = 0; i < 64; i++) m->mc[i] = 1u << 15;
    if (chain) { m->mprev_n = chain; m->mprev = cm_big(chain); if (!m->mprev) goto fail; }
    cm_set_tune(m, (cm_tune_t){0, 0, 0});
    return m;
fail:
    cm_free_parts(m); cm_release(resv); return NULL;
}
static cm_t *cm_new(uint8_t *buf, int bb, int v2) { return cm_new_x(buf, bb, v2, 0, 0); }
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
static inline int cm_stc(const int16_t *st, uint32_t v) { return st[v >> 20]; }

/* v2 is a compile-time constant at each call site, so model 1 and model 2 each get their own specialized loop. */
static inline __attribute__((always_inline)) int cm_predict(cm_t *m, const int v2) {
    const int nm = CM_NMOD(v2), ni = 2 * nm + 3;
    uint32_t c0 = m->c0; int bp = (int)m->bitpos;
    if (bp == 0 || bp == 4) {
        uint32_t nib = bp ? (c0 & 15) * 0x5BD1E995u : 0;
#ifdef KOLMO_CM_COMPACT
        for (int i = 1; i < nm; i++) m->cur[i] = cm_find_model(m, i, bp ? (c0 & 15) : 0, nib);
#else
        uint32_t hv[CM_NM4]; m->kk->cm_hash(m->h + 1, nm - 1, nib, hv + 1);       /* the context hashes, 8 at a time */
        for (int i = 1; i < nm; i++) m->cur[i] = cm_find(m->t[i], hv[i], m->shi[i]);
#endif
    }
    int node = bp < 4 ? (int)((c0 & ((1u << bp) - 1)) | (1u << bp)) : (int)((c0 & ((1u << (bp - 4)) - 1)) | (1u << (bp - 4)));
    const int img = v2 == 2;
    m->slot[0] = &m->o1[c0 | (img ? m->i_pred : (m->c4 & 0xff)) << 8];
    for (int i = 1; i < nm; i++) m->slot[i] = &m->cur[i]->s[node];
    for (int i = 0; i < nm; i++) {
        uint32_t v = *m->slot[i]; uint16_t st = (uint16_t)((v >> 8 & 255) | bp << 8); m->sst[i] = st;
        m->x[i] = cm_stc(m->st, v); m->x[nm + 3 + i] = m->st[m->sm[i][st] >> 20];
    }
    int eb = cm_expected(m);
    if (eb >= 0) {
        int l = m->mlen > 31 ? 31 : m->mlen;
        if (v2 >= 3 && m->mlen >= 16) {                       /* model 3: long matches keep telling their length apart, up to 4096 */
            int ml3 = m->mlen; l = ml3 < 24 ? 16 : ml3 < 32 ? 17 : ml3 < 48 ? 18 : ml3 < 64 ? 19 : ml3 < 96 ? 20 : ml3 < 128 ? 21 : ml3 < 192 ? 22 : ml3 < 256 ? 23
                             : ml3 < 384 ? 24 : ml3 < 512 ? 25 : ml3 < 768 ? 26 : ml3 < 1024 ? 27 : ml3 < 1536 ? 28 : ml3 < 2048 ? 29 : ml3 < 4096 ? 30 : 31;
        }
        m->mci = l * 2 + eb;
        int p = (int)(m->mc[m->mci] >> 4); if (p < 1) p = 1; if (p > 4095) p = 4095;
        m->x[nm] = m->st[p];
        m->x[nm + 1] = eb ? l * 16 : -l * 16;
    } else { m->mci = -1; m->x[nm] = 0; m->x[nm + 1] = 0; }
    m->x[nm + 2] = 64;
    int ml = m->mlen == 0 ? 0 : m->mlen < 16 ? 1 : m->mlen < 32 ? 2 : 3;
    uint32_t c1 = m->c4 & 0xff;
    int cls = ((c1 | 32) >= 'a' && (c1 | 32) <= 'z') ? 0 : (c1 >= '0' && c1 <= '9') ? 1 : 2;
    if (img) { cls = (int)m->i_cls; c1 = m->i_pred; }
    m->w1i = (int)c0 + 256 * cls;
    m->w2i = (ml * 8 + bp) * 4 + (int)(img ? m->i_act : c1 >> 6);
    const int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    int st;
    if (!v2) {
        int64_t sd[4]; m->kk->cm_dot(m->x, ni, w1, w2, NULL, NULL, sd);
        int d1 = (int)(sd[0] >> 16), d2 = (int)(sd[1] >> 16);
        m->pr1 = cm_SQ(m->sq, d1); m->pr2 = cm_SQ(m->sq, d2);
        st = (d1 + d2) >> 1;
    } else {                                                  /* four mixers, then a small learned mixer of their outputs */
        m->w3i = (int)((img ? m->i_n : (m->c4 >> 8 & 0xff)) << 3 | (uint32_t)bp); m->w4i = (int)(c0 | (img ? m->i_med : c1) << 8); m->wfi = (int)c0 | ml << 8;
        const int32_t *w3 = m->w3[m->w3i], *w4 = m->w4[m->w4i], *wf = m->wf[m->wfi];
        int64_t sd[4]; m->kk->cm_dot(m->x, ni, w1, w2, w3, w4, sd);
        int d[4] = {(int)(sd[0] >> 16), (int)(sd[1] >> 16), (int)(sd[2] >> 16), (int)(sd[3] >> 16)};
        int64_t sf = (int64_t)wf[4] * 256;
        for (int k = 0; k < 4; k++) { if (d[k] > 2047) d[k] = 2047; if (d[k] < -2047) d[k] = -2047; m->xf[k] = d[k]; sf += (int64_t)wf[k] * d[k]; }
        m->xf[4] = 256;
        m->pr1 = cm_SQ(m->sq, d[0]); m->pr2 = cm_SQ(m->sq, d[1]); m->pr3 = cm_SQ(m->sq, d[2]); m->pr4 = cm_SQ(m->sq, d[3]);
        st = (int)(sf >> 16); if (st > 2047) st = 2047; if (st < -2047) st = -2047;
        m->prf = cm_SQ(m->sq, st);
    }
    if (st > 2047) st = 2047; if (st < -2047) st = -2047;
    int mixed = cm_SQ(m->sq, st);
    int s = st + 2048;
    m->ai = s >> 7; m->aw = s & 127;
    m->a2i = (int)((c0 | c1 << 8) & 0xffff);
    int lo = m->ai, wt = m->aw;
    int p1 = (m->a1[c0][lo] * (128 - wt) + m->a1[c0][lo + 1] * wt) >> 11;
    int p2 = (m->a2[m->a2i][lo] * (128 - wt) + m->a2[m->a2i][lo + 1] * wt) >> 11;
    int p;
    if (!v2) p = (mixed + p1 + p2) / 3;
    else {
        m->a3i = (int)(cm_hmix((img ? m->i_wn : (m->c4 & 0xffff)) * 0x9E3779B1u + c0 * 0x85EBCA6Bu) >> 16);
        int p3 = (m->a3[m->a3i][lo] * (128 - wt) + m->a3[m->a3i][lo + 1] * wt) >> 11;
        p = (m->fw[0] * mixed + m->fw[1] * p1 + m->fw[2] * p2 + m->fw[3] * p3 + 4) >> 3;   /* default: the plain average */
    }
    if (p < 1) p = 1; if (p > 4095) p = 4095;
    return m->pr = p;
}
static inline void cm_upd_apm(uint16_t *a, int lo, int wt, int y, int sh) {
    int g = y ? 65535 : 0;
    a[lo] = (uint16_t)(a[lo] + ((g - a[lo]) * (128 - wt) >> sh));
    a[lo + 1] = (uint16_t)(a[lo + 1] + ((g - a[lo + 1]) * wt >> sh));
}
static inline void cm_upd_ctr(uint32_t *s, int y, const uint32_t *rcp) {
    uint32_t v = *s; int p = (int)(v >> 16), n = (int)(v & 15); uint8_t st = (uint8_t)(v >> 8);
    p += (int)(((int64_t)((y ? 65535 : 0) - p) * rcp[n]) >> 16);
    if (n < 15) n++;
    if (p < 0) p = 0; if (p > 65535) p = 65535;
    *s = (uint32_t)p << 16 | (uint32_t)bh_next[st][y] << 8 | (uint32_t)n;
}
static inline void cm_upd_sm(uint32_t *e, int y, uint32_t lim) {
    uint32_t v = *e, n = v & 1023, p = v >> 10;
    int64_t d = ((int64_t)((uint32_t)y << 22) - (int64_t)p) * cm_srcp[n] >> 16;
    p = (uint32_t)((int64_t)p + d); if (p > (1u << 22) - 1) p = (1u << 22) - 1;
    *e = p << 10 | (n < lim ? n + 1 : n);
}
static inline __attribute__((always_inline)) void cm_update(cm_t *m, int y, const int v2) {
    const int nm = CM_NMOD(v2), ni = 2 * nm + 3;
    {   /* the next bit's rows of the large selector tables (order-1 mixer, order-1 and order-2 APMs) are known now:
         * prefetch them so their cache misses overlap this update (memory access only; output unchanged). The image
         * model's selectors change at the byte's end, after this point, so its last bit is not prefetched. */
        const int img = v2 == 2, last = m->bitpos == 7;
        if (!(img && last)) {
            const uint32_t nc0 = last ? 1 : m->c0 << 1 | (uint32_t)y, c4n = last ? (m->c4 << 8) | ((m->c0 << 1 | (uint32_t)y) & 255) : m->c4;
            const uint32_t c1 = img ? m->i_pred : c4n & 0xff;
            const char *a2 = (const char *)m->a2[(nc0 | c1 << 8) & 0xffff]; __builtin_prefetch(a2); __builtin_prefetch(a2 + 64);
            if (v2) {
                const char *w4 = (const char *)m->w4[nc0 | (img ? m->i_med : c1) << 8];
                __builtin_prefetch(w4); __builtin_prefetch(w4 + 64); __builtin_prefetch(w4 + 128); __builtin_prefetch(w4 + 192);
                const char *a3 = (const char *)m->a3[cm_hmix((img ? m->i_wn : (c4n & 0xffff)) * 0x9E3779B1u + nc0 * 0x85EBCA6Bu) >> 16];
                __builtin_prefetch(a3); __builtin_prefetch(a3 + 64);
            }
        }
    }
    for (int i = 0; i < nm; i++) { cm_upd_sm(&m->sm[i][m->sst[i]], y, m->smlim); cm_upd_ctr(m->slot[i], y, m->rcp); }
    for (int i = 1; i < nm; i++) if ((m->cur[i]->s[0] & 0xffff) < 0xffff) m->cur[i]->s[0]++;
    if (m->mci >= 0) { uint32_t *c = &m->mc[m->mci]; *c = (uint32_t)((int64_t)*c + ((((int64_t)y << 16) - (int64_t)*c) >> m->mc_sh)); }
    int e1 = (y << 12) - m->pr1, e2 = (y << 12) - m->pr2;                  /* learning rate 1/2048 in 16.16 weights */
    int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    if (!v2) { const int e[4] = {e1, e2, 0, 0}; m->kk->cm_train(m->x, ni, w1, w2, NULL, NULL, e, m->lr); }
    else {
        int k = 64 - (int)(m->pos >> m->lrd); if (k < m->lrk) k = m->lrk;
        e1 = e1 * k >> 4; e2 = e2 * k >> 4;
        int e3 = ((y << 12) - m->pr3) * k >> 4, e4 = ((y << 12) - m->pr4) * k >> 4, ef = (y << 12) - m->prf;
        int32_t *w3 = m->w3[m->w3i], *w4 = m->w4[m->w4i], *wf = m->wf[m->wfi];
        { const int e[4] = {e1, e2, e3, e4}; m->kk->cm_train(m->x, ni, w1, w2, w3, w4, e, m->lr); }
        for (int k = 0; k < 5; k++) wf[k] += (m->xf[k] * ef) >> CM_LRF;
        cm_upd_apm(m->a3[m->a3i], m->ai, m->aw, y, m->apm_sh);
    }
    cm_upd_apm(m->a1[m->c0], m->ai, m->aw, y, m->apm_sh);
    cm_upd_apm(m->a2[m->a2i], m->ai, m->aw, y, m->apm_sh);
    int eb = cm_expected(m);
    if (eb >= 0 && eb != y) {
        /* model 3: a match of 16 or more that breaks is given 8 bytes to pick up again past the difference (an
         * edited copy); a match still recovering loses one attempt per miss */
        if (v2 >= 3) m->mrec = m->mlen >= 16 ? 8 : m->mrec > 0 ? m->mrec - 1 : 0;
        m->mlen = 0;
    }
    m->c0 = (m->c0 << 1) | (uint32_t)y;
    if (m->bitpos == 2) {                                     /* prefetch both candidate buckets for the second nibble */
        for (int b = 0; b < 2; b++) {
            uint32_t nib = (((m->c0 << 1) | (uint32_t)b) & 15) * 0x5BD1E995u;
#ifdef KOLMO_CM_COMPACT
            for (int i = 1; i < nm; i++) { uint32_t idx = cm_prefetch_slot(m, i, ((m->c0 << 1) | (uint32_t)b) & 15, nib); __builtin_prefetch(&m->t[i][idx]); }
#else
            uint32_t hv[CM_NM4]; m->kk->cm_hash(m->h + 1, nm - 1, nib, hv + 1);
            for (int i = 1; i < nm; i++) { uint32_t idx = hv[i] >> m->shi[i]; __builtin_prefetch(&m->t[i][idx]); }
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
        if (v2 >= 3) {
            /* model 3, structure: the key a value belongs to ("key": in JSON, key= in logs and attributes, the tag
             * after '<'), and the field of a delimited line, with the byte above it in the same field of the line before */
            const int tk = (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'z') || c == '_' || c == '-' || c == '.' || c >= 128;
            if (tk) m->s_tok = (m->s_tok + c + 1) * 0x2F0B3A55u;
            else {
                if (m->s_tok) { m->s_ltok = m->s_tok; if (m->s_lt) { m->s_key = m->s_tok; m->s_kpos = 0; } m->s_tok = 0; }
                if ((c == ':' || c == '=') && m->s_ltok) { m->s_key = m->s_ltok; m->s_kpos = 0; }
                m->s_lt = c == '<' || (c == '/' && m->s_lt);
            }
            if (m->s_kpos < 255) m->s_kpos++;
            if (c == '\n') {
                m->s_pfn = m->s_fidx + 1; memcpy(m->s_pfst, m->s_fst, sizeof m->s_fst); m->s_pls = m->s_ls; m->s_ls = m->pos;
                m->s_fidx = 0; m->s_fst[0] = 0; m->s_key = 0; m->s_ltok = 0;
            } else if (c == ' ' || c == ',' || c == '\t' || c == ';' || c == '|') {
                if (m->s_fidx < 31) { m->s_fidx++; m->s_fst[m->s_fidx] = (uint32_t)(m->pos - m->s_ls); }
            }
            uint32_t ab = 0, ab1 = 0;
            if (m->s_fidx < m->s_pfn) {
                int64_t off = (m->pos - m->s_ls) - m->s_fst[m->s_fidx], a = m->s_pls + m->s_pfst[m->s_fidx] + off;
                if (a >= m->s_pls && a < m->s_ls) { ab = m->buf[a]; ab1 = a + 1 < m->s_ls ? m->buf[a + 1] : 0; }
            }
            const uint32_t c1 = c4 & 0xff, kq = m->s_kpos < 16 ? m->s_kpos : 16 + (m->s_kpos >> 4);
            m->h[25] = cm_hmix(m->s_key * 0x9E3779B1u + c1 * 0x85EBCA6Bu + 25);
            m->h[26] = cm_hmix(m->s_key * 0xC2B2AE35u + m->s_tok * 0x9E3779B1u + 26);
            m->h[27] = cm_hmix((m->s_fidx << 16 | (c4 & 0xffff)) * 0x9E3779B1u + 27);
            m->h[28] = cm_hmix((m->s_fidx + 1) * 0x85EBCA6Bu + m->s_tok * 0xC2B2AE35u + 28);
            m->h[29] = cm_hmix((ab << 16 | ab1 << 8 | m->s_fidx) * 0x9E3779B1u + c1 * 0x2545F491u + 29);
            (void)kq;
        }
        if (v2 >= 3) {                                        /* model 3: orders 12 and 20 */
            const uint8_t *bf = m->buf + m->pos; uint32_t hh = 0; const int lim = m->pos < 20 ? (int)m->pos : 20;
            m->h[21] = 21; m->h[22] = 22;
            for (int k = 1; k <= lim; k++) { hh = (hh + bf[-k] + 1) * 0x2F0B3A55u; if (k == 12) m->h[21] = cm_hmix(hh + 21); if (k == 20) m->h[22] = cm_hmix(hh + 22); }
        }
        if (v2 == 4) {
            /* model 4, token shapes (what a regular expression would call character classes): a byte is a digit, a
             * letter (or '_', or a byte >= 128), a space or tab, or a punctuation byte of its own; a token is a run of
             * one class (each punctuation byte is a token). Contexts: the classes of the last 8 tokens with the
             * position in the current one; and the token index in the line with the bytes at the same place of the
             * same token of the line before (timestamps, counters and identifiers line up there even when the fields
             * have different widths) */
            const uint32_t k = (c >= '0' && c <= '9') ? 1u : (((c | 32) >= 'a' && (c | 32) <= 'z') || c == '_' || c >= 128) ? 2u : (c == ' ' || c == '\t') ? 3u : 4u + c;
            if (c == '\n') {
                memcpy(m->k_pst, m->k_st, sizeof m->k_st); m->k_pn = m->k_tok + 1; m->k_tok = 0; m->k_st[0] = 0; m->k_kp = 0; m->k_cls = 0;
                m->k_sr = m->k_sr << 8 | 0xffu;
            } else if (k != m->k_cls || k >= 4) {
                if (m->k_cls) { if (m->k_tok < 127) m->k_tok++; m->k_st[m->k_tok] = (uint32_t)(m->pos - 1 - m->s_ls); }
                m->k_kp = 0; m->k_cls = k; m->k_sr = m->k_sr << 8 | (k < 4 ? k : (uint8_t)c);
            } else if (m->k_kp < 255) m->k_kp++;
            const uint32_t kq = m->k_kp < 12 ? m->k_kp : 12 + (m->k_kp >= 24);
            uint32_t a0 = 0, a1 = 0;
            if (m->k_tok < m->k_pn) {               /* the next byte's place in the same token of the line before */
                const int64_t a = m->s_pls + m->k_pst[m->k_tok] + (m->k_cls ? m->k_kp + 1 : 0);
                if (a >= m->s_pls && a < m->s_ls) { a0 = m->buf[a]; a1 = a + 1 < m->s_ls ? m->buf[a + 1] : 0; }
            }
            m->h[30] = cm_hmix((uint32_t)(m->k_sr & 0xffffffffffffull) * 0x9E3779B1u + (uint32_t)(m->k_sr >> 32 & 0xffff) * 0x85EBCA6Bu + kq * 0xC2B2AE35u + 30);
            m->h[31] = cm_hmix((a0 << 16 | a1 << 8 | kq) * 0x9E3779B1u + (m->k_tok << 8 | (c4 & 0xff)) * 0x2545F491u + 31);
        }
        if (v2 == 2) {
            /* image model: the hashed contexts are the pixel's neighbours instead of the preceding bytes. W, N, NW, NE
             * are the same channel of the pixels to the left, above and diagonal; P1, P2 the earlier channels of this
             * pixel. Predictions (gradient, median, inter-channel) are contexts too, with the local activity. */
            const uint8_t *bf = m->buf; const int64_t q = m->pos; const int64_t w = m->iw, ch = m->ich;
#define IB(d) ((uint32_t)(q >= (int64_t)(d) + (int64_t)m->ih ? bf[q - (d)] : 0))
#define ICL(x) ((uint32_t)((x) < 0 ? 0 : (x) > 255 ? 255 : (x)))
            const uint32_t W = IB(ch), N = IB(w), NW = IB(w + ch), NE = w > ch ? IB(w - ch) : 0, WW = IB(2 * ch), NN = IB(2 * w);
            const uint32_t NNE = w > ch ? IB(2 * w - ch) : 0, P1 = IB(1), P2 = IB(2), W1 = IB(ch + 1), N1 = IB(w + 1);
            const uint32_t ci = q >= (int64_t)m->ih ? (uint32_t)((q - m->ih) % ch) : 0;
            const uint32_t grad = ICL((int)W + (int)N - (int)NW), mn = W < N ? W : N, mx = W < N ? N : W;
            const uint32_t med = NW >= mx ? mn : NW <= mn ? mx : grad, avg = (W + N + 1) >> 1;
            const int a0 = abs((int)W - (int)NW) + abs((int)N - (int)NW) + abs((int)N - (int)NE);
            const uint32_t act = a0 < 3 ? 0 : a0 < 8 ? 1 : a0 < 16 ? 2 : a0 < 32 ? 3 : a0 < 64 ? 4 : a0 < 128 ? 5 : a0 < 256 ? 6 : 7;
            const int col = ci > 0;                                   /* an earlier channel of this pixel is known */
            const uint32_t icp = col ? ICL((int)W + (int)P1 - (int)W1) : grad, icn = col ? ICL((int)N + (int)P1 - (int)N1) : avg;
            const uint32_t k = ci * 0x9E3779B1u;
#define IH(i, v) m->h[i] = cm_hmix((uint32_t)(v) * 0x9E3779B1u + k + (uint32_t)(i) * 0x85EBCA6Bu)
            IH(1, W << 8 | N); IH(2, N << 8 | NE); IH(3, W << 8 | NW); IH(4, grad << 3 | act); IH(5, med << 3 | act);
            IH(6, W << 16 | N << 8 | NW); IH(7, W << 16 | N << 8 | NE); IH(8, N << 8 | NN); IH(9, W << 8 | WW);
            IH(10, (ICL(2 * (int)W - (int)WW) >> 2) << 6 | ICL(2 * (int)N - (int)NN) >> 2); IH(11, avg << 6 | ((W - N + 256) & 511) >> 3);
            if (col) { IH(12, icp << 3 | act); IH(13, icp << 8 | P1); IH(14, P1 << 8 | W); IH(15, P1 << 8 | N); IH(16, P1 << 8 | P2); IH(17, icn << 8 | W); }
            else { IH(12, ICL((int)N + (int)NE - (int)NNE) << 3 | act); IH(13, ICL((int)W + (int)NE - (int)N)); IH(14, NW << 8 | NE); IH(15, ((W + NE) >> 1) << 6 | N >> 2);
                   IH(16, (W >> 2) << 18 | (N >> 2) << 12 | (NW >> 2) << 6 | NE >> 2); IH(17, WW << 8 | NN); }
            IH(18, (grad >> 1) << 7 | med >> 1); IH(19, (W >> 3) << 20 | (N >> 3) << 15 | (NE >> 3) << 10 | (NW >> 3) << 5 | P1 >> 3);
            IH(20, (icp + icn + 1) >> 1);
#undef IH
#undef IB
#undef ICL
            m->i_pred = icp; m->i_cls = ci > 2 ? 2 : ci; m->i_act = act >> 1; m->i_n = N; m->i_med = med; m->i_wn = W << 8 | N;
        }
#ifndef KOLMO_CM_COMPACT
        uint32_t hv[CM_NM4]; m->kk->cm_hash(m->h + 1, nm - 1, 0, hv + 1);
#endif
        for (int i = 1; i < nm; i++) {                        /* prefetch the buckets for the first nibble */
#ifdef KOLMO_CM_COMPACT
            uint32_t idx = cm_prefetch_slot(m, i, 0, 0);
#else
            uint32_t idx = hv[i] >> m->shi[i];
#endif
            __builtin_prefetch(&m->t[i][idx]); __builtin_prefetch(&m->t[i][idx ^ 1]);
        }
        if (m->pos >= 6) {
            uint32_t hh = cm_hmix(c4 ^ ((m->c8 & 0xffff) * 0x9E3779B1u)) & m->mmask;
            if (m->mlen) { m->mptr++; if (m->mlen < 65535) m->mlen++; }
            else {
                int64_t cand = m->mt[hh];
                int found = 0;
                if (v2 >= 3) {
                    /* model 3: of the last CM_MCAND places this hash was seen, the one whose preceding bytes agree
                     * for longest. The most recent one predicts the next byte 88% of the time on a JSON file of
                     * repeated documentation, the longest one 94%: half the misses, and misses are where the bits go. */
                    int best = 0; int64_t bc = 0;
                    for (int k = 0; cand > 0 && k < CM_MCAND; k++) {
                        int l = 0;
                        while (l < 400 && cand - 1 - l >= 0 && m->buf[cand - 1 - l] == m->buf[m->pos - 1 - l]) l++;
                        if (l > best) { best = l; bc = cand; if (l >= 400) break; }
                        cand = m->mprev[cand];
                    }
                    if (best >= 6) { m->mlen = best; m->mptr = bc; found = 1; }
                } else if (cand > 0) {
                    int l = 0;
                    while (l < 400 && cand - 1 - l >= 0 && m->buf[cand - 1 - l] == m->buf[m->pos - 1 - l]) l++;
                    if (l >= 6) { m->mlen = l; m->mptr = cand; found = 1; }
                }
                if (v2 >= 3 && !found && m->mrec > 0 && m->mptr + 1 < m->pos) { m->mptr++; m->mlen = 1; }   /* carry on past the difference */
            }
            if (v2 >= 3) m->mprev[m->pos] = m->mt[hh];
            m->mt[hh] = (uint32_t)m->pos;
            if (m->mlen && m->mptr >= m->pos) m->mlen = 0;
        }
        if (v2 >= 3) {                                        /* model 3: the byte the match model expects, with orders 1 and 2 */
            uint32_t eb = m->mlen ? m->buf[m->mptr] : 256, lq = m->mlen == 0 ? 0 : m->mlen < 8 ? 1 : m->mlen < 12 ? 2 : m->mlen < 16 ? 3 : m->mlen < 24 ? 4 : m->mlen < 32 ? 5 : m->mlen < 64 ? 6 : 7;
            m->h[23] = cm_hmix((eb << 3 | lq) * 0x9E3779B1u + (m->c4 & 0xff) * 0x85EBCA6Bu + 23);
            m->h[24] = cm_hmix((eb << 3 | lq) * 0xC2B2AE35u + (m->c4 & 0xffff) * 0x9E3779B1u + 24);
        }
    }
}
/* Binary arithmetic coder: interval coding on 32 bits without carries. The current interval is [low, high]. A bit
 * whose probability of being 1 is p / 4096 splits it at mid = low + (high - low) * p / 4096 (rounded down): a 1
 * keeps [low, mid], a 0 keeps [mid + 1, high]. Once low and high share their top byte, that byte can no longer
 * change: the encoder emits it, the decoder consumes one more input byte, and both ends move up by 8 bits (high
 * refilled with ones). The encoder ends with the top byte of low and three 0xff bytes. */
typedef struct { uint32_t low, high; } cm_ac;
static inline uint32_t cm_ac_mid(const cm_ac *a, int p) { return a->low + (uint32_t)(((uint64_t)(a->high - a->low) * (uint32_t)p) >> 12); }
static inline int cm_ac_settled(const cm_ac *a) { return (a->low ^ a->high) < (1u << 24); }
static inline void cm_ac_shift(cm_ac *a) { a->low <<= 8; a->high = (a->high << 8) | 0xffu; }

/* One coding loop per model, each with the model inlined and specialized. */
static inline __attribute__((always_inline)) int cm_enc_loop(cm_t *m, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *po, const int v2) {
    cm_ac ac = {0, 0xffffffffu}; size_t o = *po;
    for (size_t i = 0; i < n; i++) {
        int c = in[i];
        if (i == m->ck_at) m->ck_out = o;
        for (int b = 7; b >= 0; b--) {
            int y = (c >> b) & 1, p = cm_predict(m, v2);
#ifdef CM_STAT
            extern int getenv_miss;
            { static int cls; if (b == 7) { cls = m->mlen == 0 ? 0 : m->mlen < 16 ? 1 : m->mlen < 32 ? 2 : m->mlen < 400 ? 3 : 4; if (getenv_miss && m->mlen) cls = (m->mlen < 16 ? 0 : 2) + (m->buf[m->mptr] != c); else if (getenv_miss) cls = 4; }
              extern double cm_stat_bits[5]; extern long cm_stat_n[5]; extern int getenv_miss; cm_stat_bits[cls] -= log2((y ? p : 4096 - p) / 4096.0); if (b == 7) cm_stat_n[cls]++; }
#endif
            uint32_t mid = cm_ac_mid(&ac, p);
            if (y) ac.high = mid; else ac.low = mid + 1;
            cm_update(m, y, v2);
            for (; cm_ac_settled(&ac); cm_ac_shift(&ac)) { if (o >= cap) return -1; out[o++] = (uint8_t)(ac.high >> 24); }
        }
    }
    if (o + 4 > cap) return -1;
    out[o++] = (uint8_t)(ac.low >> 24); out[o++] = 0xff; out[o++] = 0xff; out[o++] = 0xff;
    *po = o; return 0;
}
static inline __attribute__((always_inline)) void cm_dec_loop(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, const int v2) {
    (void)out;
    cm_ac ac = {0, 0xffffffffu}; uint32_t code = 0; size_t ip = 4;      /* code: the next 32 bits of the stream */
    for (int k = 0; k < 4; k++) code = (code << 8) | (ip < cn ? in[ip++] : 0xffu);
    for (size_t i = 0; i < n; i++)
        for (int b = 7; b >= 0; b--) {
            int p = cm_predict(m, v2);
            uint32_t mid = cm_ac_mid(&ac, p);
            int y = code <= mid;
            if (y) ac.high = mid; else ac.low = mid + 1;
            cm_update(m, y, v2);
            for (; cm_ac_settled(&ac); cm_ac_shift(&ac)) code = (code << 8) | (ip < cn ? in[ip++] : 0xffu);
        }
}
/* The coding loops are instantiated once per model and, on x86-64 builds for the baseline ISA, a second time for AVX2
 * (the mixer dot products and weight updates vectorize; integer arithmetic, so both write the same bytes), chosen at
 * run time by kolmo_have_avx2(). */
#define CM_LOOPS(SUF, ATTR) \
    ATTR __attribute__((noinline)) static int cm_enc##SUF(cm_t *m, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *po, int v2) { \
        switch (v2) { case 0: return cm_enc_loop(m, in, n, out, cap, po, 0); case 1: return cm_enc_loop(m, in, n, out, cap, po, 1); \
                      case 2: return cm_enc_loop(m, in, n, out, cap, po, 2); case 3: return cm_enc_loop(m, in, n, out, cap, po, 3); \
                      default: return cm_enc_loop(m, in, n, out, cap, po, 4); } } \
    ATTR __attribute__((noinline)) static void cm_dec##SUF(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, int v2) { \
        switch (v2) { case 0: cm_dec_loop(m, in, cn, out, n, 0); break; case 1: cm_dec_loop(m, in, cn, out, n, 1); break; \
                      case 2: cm_dec_loop(m, in, cn, out, n, 2); break; case 3: cm_dec_loop(m, in, cn, out, n, 3); break; \
                      default: cm_dec_loop(m, in, cn, out, n, 4); } }
CM_LOOPS(_gen, )
#if defined(KOLMO_X86_DISPATCH) && !defined(__AVX2__) && !defined(KOLMO_CM_NO_DISPATCH)
CM_LOOPS(_avx2, KOLMO_TARGET_AVX2)
static int cm_enc_run(cm_t *m, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *po, int v2) {
    return kolmo_have_avx2() ? cm_enc_avx2(m, in, n, out, cap, po, v2) : cm_enc_gen(m, in, n, out, cap, po, v2);
}
static void cm_dec_run(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, int v2) {
    if (kolmo_have_avx2()) cm_dec_avx2(m, in, cn, out, n, v2); else cm_dec_gen(m, in, cn, out, n, v2);
}
#else
#define cm_enc_run cm_enc_gen
#define cm_dec_run cm_dec_gen
#endif
#undef CM_LOOPS

/* model 1 (fewer contexts, about 30% faster, what kolmo 1.0.0 wrote; always for n >= 2^28, whose size word has no room
 * for flags), model 2, or model 3 (about 20% slower than model 2; 1 to 4% smaller on text, no gain on binary data) */
/* model: 1, 2 or 3 */
static size_t cm_compress_t(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, cm_tune_t tune);
static size_t cm_compress_mx(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap) { return cm_compress_t(in, n, out, cap, model, bbcap, (cm_tune_t){0, 0, 0}); }
static size_t cm_compress_m(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model) { return cm_compress_mx(in, n, out, cap, model, 0); }
/* bbcap: at most 2^bbcap buckets per model (0: no cap beyond cm_bits_for); tune: learning settings (models 2 and 3;
 * {0, 0, 0} writes the stream every earlier encoder wrote) */
static _Thread_local size_t cm_tls_ck_at = SIZE_MAX, cm_tls_ck_out;   /* the race's checkpoint (cm_compress_text) */
static size_t cm_compress_t(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, cm_tune_t tune) {
    if (cap < 8 || n > 0xFFFFFFF0u) return 0;
    int small = n < ((size_t)1 << 28), v2 = small && model > 1 ? (model >= 3 ? model : 1) : 0;
    if (v2 == 4) tune.b2 |= 0x80;                    /* model 4 is written as model 3 with bit 7 of tune byte 2 */
    const int tuned = v2 && (tune.b0 | tune.b1 | tune.b2);
    /* exact logistic tables: model 3 always (its header has the model byte anyway); models 1 and 2 from 64 KiB, below
     * which the extra header byte can cost more than the better tables gain */
    int lx = small && (v2 >= 3 || n >= ((size_t)1 << 16) || tuned);   /* a tuned stream's header always says exact tables */
    int bb = small ? cm_bits_for(n) : CM_BB;
    if (small && bbcap >= 16 && bb > bbcap) bb = bbcap;
    uint8_t *hist = malloc(n + 8); cm_t *m = hist ? cm_new_x(hist, bb, v2, v2 >= 3 ? (n + 8) * 4 : 0, lx) : NULL;
    if (!m) { free(hist); return 0; }
    size_t o = 4; int r;
    m->ck_at = cm_tls_ck_at;
    if (tuned) {              /* extended header with learning settings: nibble 8 | 4 | model, then the two tune bytes */
        if (cm_set_tune(m, tune)) { cm_free(m); free(hist); return 0; }
        uint32_t n32 = (uint32_t)n | (uint32_t)8 << 28; memcpy(out, &n32, 4); out[4] = (uint8_t)((v2 >= 3 ? 15 : 14) << 4 | (bb - 15));
        out[5] = tune.b0; out[6] = tune.b1; out[7] = tune.b2;
        r = cm_enc_run(m, in, n, out + 4, cap - 4, &o, v2);
        o += 4;
    } else if (lx || v2 == 3) {      /* extended header: flags 8, then model nibble << 4 | log2 buckets - 15; nibble = model | 4 when exact */
        int base = v2 == 3 ? 3 : v2 ? 2 : 1;
        uint32_t n32 = (uint32_t)n | (uint32_t)8 << 28; memcpy(out, &n32, 4); out[4] = (uint8_t)((base | (lx ? 4 : 0)) << 4 | (bb - 15));
        r = cm_enc_run(m, in, n, out + 1, cap - 1, &o, v2);
        o++;
    } else {
        uint32_t n32 = (uint32_t)n | (small ? (uint32_t)((bb - 15) | (v2 ? 8 : 0)) << 28 : 0); memcpy(out, &n32, 4);
        r = cm_enc_run(m, in, n, out, cap, &o, v2 ? 1 : 0);
    }
    cm_tls_ck_out = m->ck_out;
    cm_free(m); free(hist);
    return r ? 0 : o;
}
static size_t cm_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { return cm_compress_m(in, n, out, cap, 2); }

/* -x: learning settings chosen per stream by a race (docs/TWOPASS.md). Four presets, each with the shared prior
 * (byte 2 = 1), code the stream's first CM_RACE_SAMPLE bytes; the smallest wins (the first on a tie) and codes the
 * whole stream. A stream no longer than the sample is coded whole by every preset and the smallest is kept. Presets:
 * the prior with the default rates; and three settings that the full search (49 trials per file) found most often on
 * text, logs, tables, numbers and images: faster APMs, a slower match-model counter, more weight on the APMs, a
 * faster-settling or slower mixer. The choice depends on the stream's bytes only; threads only run the trials side
 * by side. Decoding costs the same as before. */
#ifndef CM_RACE_SAMPLE
#define CM_RACE_SAMPLE (256u << 10)
#endif
enum { CM_NPRESET = 4 };
static const cm_tune_t cm_presets[CM_NPRESET] = {{0x00, 0x00, 1}, {0x1c, 0xa5, 1}, {0x52, 0x04, 1}, {0xdc, 0xb4, 1}};
typedef struct { const uint8_t *in; size_t n, cap, z; uint8_t *out; int model, bbcap; cm_tune_t t; size_t ck_at, score; kmem_ctx ctx; uint64_t unit; } cm_race_job;
static void *cm_race_run(void *a) {
    cm_race_job *j = a;
    cm_tls_ck_at = j->ck_at; cm_tls_ck_out = 0;
    j->z = j->out ? cm_compress_t(j->in, j->n, j->out, j->cap, j->model, j->bbcap, j->t) : 0;
    j->score = j->z && j->ck_at < j->n ? j->z - cm_tls_ck_out : j->z;   /* the bytes coded for the input after ck_at */
    cm_tls_ck_at = SIZE_MAX; return NULL;
}
/* a trial on a thread of its own: optional work, started only when the governor grants its working set at once (as
 * the parts of codec 25); it holds that reservation (its tables are covered by it) and releases it when done. Waiting
 * for memory here could deadlock: the caller may hold the block's reservation while it joins this thread. */
static void *cm_race_helper(void *a) {
    cm_race_job *j = a; kmem_ctx_set(j->ctx); kmem_tls_job = 1;
    cm_race_run(j);
    kmem_tls_job = 0; kmem_release(j->unit); return NULL;
}
/* runs trials a..e-1, par at a time, the caller running one of each group (and any that could not start) */
static void cm_race_all(cm_race_job *j, int a, int e, int par) {
    pthread_t th[8]; int started[8];
    if (par > 8) par = 8;
    for (int k0 = a; k0 < e; k0 += par) {
        for (int k = k0 + 1; k < k0 + par && k < e; k++) {
            j[k].ctx = kmem_ctx_get(); j[k].unit = cm_mem_for(cm_bits_for(j[k].n), 4) + 6 * ((uint64_t)j[k].n + 8) + j[k].cap;   /* tables, chain, buffers */
            started[k - k0] = kmem_try_reserve(j[k].unit, j[k].ctx.cap);
            if (started[k - k0] && pthread_create(&th[k - k0], NULL, cm_race_helper, &j[k])) { kmem_release(j[k].unit); started[k - k0] = 0; }
        }
        cm_race_run(&j[k0]);
        for (int k = k0 + 1; k < k0 + par && k < e; k++) { if (started[k - k0]) pthread_join(th[k - k0], NULL); else cm_race_run(&j[k]); }
    }
}
static size_t cm_compress_raced(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, int threads) {
    if (model < 2 || n >= ((size_t)1 << 28) || n < 64) return cm_compress_mx(in, n, out, cap, model, bbcap);
    const size_t L = n <= CM_RACE_SAMPLE ? n : CM_RACE_SAMPLE;
    cm_race_job j[CM_NPRESET];
    for (int k = 0; k < CM_NPRESET; k++) {
        j[k] = (cm_race_job){in, L, L == n ? cap : L + L / 16 + 64, 0, NULL, model, bbcap, cm_presets[k], SIZE_MAX, 0, {0, 0, 0}, 0};
        j[k].out = malloc(j[k].cap);
    }
    cm_race_all(j, 0, CM_NPRESET, threads > CM_NPRESET ? CM_NPRESET : threads > 1 ? threads : 1);   /* par trials at a time */
    int b = -1; for (int k = 0; k < CM_NPRESET; k++) if (j[k].z && (b < 0 || j[k].z < j[b].z)) b = k;
    size_t z = 0;
    if (b >= 0 && L == n) { memcpy(out, j[b].out, j[b].z); z = j[b].z; }
    for (int k = 0; k < CM_NPRESET; k++) free(j[k].out);
    if (L == n) return z ? z : cm_compress_mx(in, n, out, cap, model, bbcap);
    return cm_compress_t(in, n, out, cap, model, bbcap, b >= 0 ? cm_presets[b] : cm_presets[0]);
}

#ifdef KOLMO_CODEC_REGISTRY
/* -x on text (docs/PRIORS.md): the kind of the text (algo/texttype.c) picks the prior every contestant starts from (the
 * shared one when no kind is recognised). Stage 1 races model 3 against model 4 (token-shape contexts) with the first
 * preset on the first CM_RACE_SAMPLE bytes; stage 2 races the three other presets with the winning model. The smallest
 * of the five wins (the earliest on a tie). Depends on the stream's bytes only; threads run the trials side by side. */
#ifndef CM_KP_MAXN
#define CM_KP_MAXN ((size_t)4 << 20)
#endif
#ifndef CM_ABL
#define CM_ABL 0   /* research builds only (benchmark/rc3w3/prior): 1 = the shared prior for every kind, 2 = model 3 only */
#endif
static size_t cm_compress_text(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int bbcap, int threads, int kind) {
    if (n >= ((size_t)1 << 28) || n < 64) return cm_compress_mx(in, n, out, cap, 3, bbcap);
    /* the kind's prior below 4 MiB; beyond, where a start matters little, the shared prior was as good or better (RESULTS) */
    const uint8_t pid = (uint8_t)(CM_ABL != 1 && kind > TT_NONE && kind <= CM_NKPRIOR && n < CM_KP_MAXN ? 1 + kind : 1);
    const size_t L = n <= CM_RACE_SAMPLE ? n : CM_RACE_SAMPLE;
    cm_race_job j[2 + CM_NPRESET - 1];
    for (int k = 0; k < 2 + CM_NPRESET - 1; k++) {
        cm_tune_t t = cm_presets[k < 2 ? 0 : k - 1]; t.b2 = pid;
        /* a stream much longer than the sample is judged by the sample's second half (the rate once the start is past) */
        j[k] = (cm_race_job){in, L, L == n ? cap : L + L / 16 + 64, 0, NULL, k == 1 && CM_ABL != 2 ? 4 : 3, bbcap, t, n >= 4 * L ? L / 2 : SIZE_MAX, 0, {0, 0, 0}, 0};
    }
    const int par = threads > 3 ? 3 : threads > 1 ? threads : 1;   /* at most 3 trials per stage */
    for (int st = 0; st < 2; st++) {                       /* stage 0: jobs 0-1; stage 1: jobs 2-4 with the winner's model */
        const int a = st ? 2 : 0, e = st ? 2 + CM_NPRESET - 1 : 2;
        if (st) { const int win = j[1].z && (!j[0].z || j[1].score < j[0].score) ? 4 : 3; for (int k = a; k < e; k++) j[k].model = win; }
        for (int k = a; k < e; k++) j[k].out = malloc(j[k].cap);
        cm_race_all(j, a, e, par);
    }
    int b = -1; for (int k = 0; k < 2 + CM_NPRESET - 1; k++) if (j[k].z && (b < 0 || j[k].score < j[b].score)) b = k;
    size_t z = 0;
    if (b >= 0 && L == n) { memcpy(out, j[b].out, j[b].z); z = j[b].z; }
    for (int k = 0; k < 2 + CM_NPRESET - 1; k++) free(j[k].out);
    if (L == n) return z ? z : cm_compress_mx(in, n, out, cap, 3, bbcap);
    if (b < 0) return cm_compress_raced(in, n, out, cap, 3, bbcap, threads);
    return cm_compress_t(in, n, out, cap, j[b].model, bbcap, j[b].t);
}
#endif
/* a stream written with learning settings (extended header, model nibble 14 or 15): its frame is version 11 */
static int cm_stream_tuned(const uint8_t *z, size_t zn) { return zn >= 8 && (z[3] >> 4) == 8 && (z[4] >> 4) >= 14; }
static int cm_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 8) return -1;
    uint32_t n32; memcpy(&n32, in, 4);
    int bb = CM_BB, v2 = 0, lx = 0; cm_tune_t tune = {0, 0, 0};
    if (n < ((size_t)1 << 28)) {
        uint32_t f = n32 >> 28; n32 &= 0x0FFFFFFFu;
        if (f == 8) {                                     /* extended header: model nibble 3 (legacy tables), 5, 6, 7 (exact), 14, 15 (exact, tuned) */
            int mb = cn < 9 ? 0 : in[4] >> 4;
            if (mb != 3 && mb != 5 && mb != 6 && mb != 7 && mb != 14 && mb != 15) return -1;
            lx = mb >= 5; v2 = (mb & 3) == 3 ? 3 : (mb & 3) == 2 ? 1 : 0; bb = (in[4] & 15) + 15;
            if (mb >= 14) {                               /* the three tune bytes; the coded bits start after them (cm_dec_loop skips 4) */
                if (cn < 12 || (in[7] & 0x7f) >= CM_NPRIOR || ((in[7] & 0x80) && mb != 15)) return -1;
                tune.b0 = in[5]; tune.b1 = in[6]; tune.b2 = in[7]; in += 3; cn -= 3;
                if (tune.b2 & 0x80) v2 = 4;                /* model 4: model 3 with the token-shape contexts */
            }
            in++; cn--;
        }
        else if (f) { bb = (int)(f & 7) + 15; v2 = (int)(f >> 3); }
    }
    if (n32 != n || bb < 16 || bb > 22) return -1;
    cm_t *m = cm_new_x(out, bb, v2, v2 >= 3 ? (n + 8) * 4 : 0, lx); if (!m) return -1;
    if (cm_set_tune(m, tune)) { cm_free(m); return -1; }
    cm_dec_run(m, in, cn, out, n, v2 >= 3 ? v2 : v2 ? 1 : 0);
    cm_free(m);
    return 0;
}

/* The decode working set of a codec-7 stream of n bytes (tables and model 3's match chain), from its header; 0 when the
 * header is not one cm_decompress accepts. The encoder declares it in the frame (core/blocks.c, blk_dec_need) and the
 * decoder reserves it per block. */
static uint64_t cm_stream_need(const uint8_t *in, size_t cn, size_t n) {
    if (cn < 8) return 0;
    uint32_t n32; memcpy(&n32, in, 4);
    int bb = CM_BB, v2 = 0;
    if (n < ((size_t)1 << 28)) {
        uint32_t f = n32 >> 28;
        if (f == 8) {
            int mb = cn < 9 ? 0 : in[4] >> 4;
            if (mb != 3 && mb != 5 && mb != 6 && mb != 7 && mb != 14 && mb != 15) return 0;
            v2 = (mb & 3) == 3 ? 3 : (mb & 3) == 2 ? 1 : 0; bb = (in[4] & 15) + 15;
            if (mb == 15 && (in[7] & 0x80)) v2 = 4;
        } else if (f) { bb = (int)(f & 7) + 15; v2 = (int)(f >> 3); }
    }
    if (bb < 16 || bb > 22) return 0;
    return cm_mem_for(bb, v2) + (v2 >= 3 ? ((uint64_t)n + 8) * 4 : 0);
}
/* the largest decode working set a codec-7 stream of n bytes can have under the current sizing rules (graph leaves) */
static uint64_t cm_need_bound(size_t n) { int bb = n < ((size_t)1 << 28) ? cm_bits_for(n) : CM_BB; return cm_mem_for(bb, 3) + ((uint64_t)n + 8) * 4; }

/* Codec 7 table (kolmo builds only: tests/test_cm_compact.c includes this file on its own). Model 3 where it gains: text. */
#ifdef KOLMO_CODEC_REGISTRY
static size_t cm_bound(size_t n) { return n + n / 16 + 64; }
static size_t cm_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    /* level 1: model 1, as written before; otherwise model 3 for text, 2 for the rest, with the raced settings */
    if (level == 1) return cm_compress_m(in, n, out, cap, 1);
    /* text (looks_text) only: routing the UTF-8 text it rejects (CLDR XML, tweets) to the text models gained 4-10% but
     * decoded up to 1.56x slower than model 2 (benchmark/rc3w3/prior/RESULTS.md), over the lane's 1.2x rule */
    if (!looks_text(in, n)) return cm_compress_raced(in, n, out, cap, 2, 0, x ? x->threads : 1);
    return cm_compress_text(in, n, out, cap, 0, x ? x->threads : 1, text_kind(in, n));
}
static int cm_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return cm_decompress(in, cn, out, n); }
static const kolmo_codec codec_cm = { C_CM, "cm", cm_bound, cm_codec_compress, cm_codec_decompress, NULL };
#endif
