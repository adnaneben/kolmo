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
#ifdef KOLMO_IMPACT
#define CM_NSLOT 32             /* research build (benchmark/impact): contexts 30 and 31, the token classes of the rc3w3 prior
                                 * lane (a hand-written lexer), as model 4 mask bits 30 and 31 */
#else
#define CM_NSLOT CM_NM3         /* per-context arrays */
#endif
#ifndef CM_MCAND
#define CM_MCAND 16              /* model 3: match candidates examined */
#endif
#define CM_NMOD(v) ((v) == 3 ? CM_NM3 : (v) ? CM_NMX : CM_NM)
#define CM4_NIP 64              /* model 4: mixer inputs per row, padded (2 * 30 + 3 at most) */
#define CM4_RA2 (CM_W1 + CM_W2 + CM_W3 + CM_W4)   /* model 4: the APM rows' flags after the mixers' */
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
static uint8_t cm4_pri[256];   /* model 4: bits seen by a state (bucket replacement priority) */
static int16_t cm4_dx[256];    /* model 4: second input of a state: its run, signed by the bit (0 when both bits were seen) */
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
    for (int st = 0; st < 256; st++) {
        const int n0 = st < bh_ns ? bh_n0[st] : 0, n1 = st < bh_ns ? bh_n1[st] : 0, n = n0 + n1 > 7 ? 7 : n0 + n1;
        cm4_pri[st] = (uint8_t)(n0 + n1);
        cm4_dx[st] = (int16_t)(n0 && n1 ? 0 : n1 ? 128 * (n + 1) : n0 ? -128 * (n + 1) : 0);
    }
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
    cm_bucket *t[CM_NSLOT];
    const int16_t *sq, *st;                  /* logistic tables: cm_sq/cm_st (legacy) or cm_sqx/cm_stx (exact) */
#ifdef KOLMO_CM_COMPACT
    /* High 16 bits hold the compact slot, low 16 hold the original tag. */
    uint32_t one_byte[2][CM_BYTE_VALUES][CM_NIBBLES];
#endif
    int sh, v2; size_t mem, tb; uint8_t shi[CM_NSLOT]; size_t tbi[CM_NSLOT];                                  /* 32 - log2 buckets; model 2?; bytes held */
    uint32_t o1[256 * 256];
    uint32_t h[CM_NSLOT]; uint32_t *slot[CM_NSLOT]; cm_bucket *cur[CM_NSLOT];
    int32_t w1[CM_W1][CM_NIX3], w2[CM_W2][CM_NIX3];
    int32_t (*w3)[CM_NIX3], (*w4)[CM_NIX3], wf[CM_WF][5];        /* model 2 */
    int xf[5], pr3, pr4, prf, w3i, w4i, wfi, a3i; uint16_t (*a3)[33];
    uint32_t sm[CM_NSLOT][256 * 8]; uint16_t sst[CM_NSLOT];           /* state -> probability (22 bits) | count (10 bits) */
    int x[CM_NIX3];
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
#ifdef KOLMO_IMPACT
    uint32_t k_cls, k_tok, k_kp, k_pn; uint64_t k_sr;     /* research, token classes: class of the last byte, token index in the
                                                           * line, bytes into the token, tokens of the previous line, classes of the last 8 tokens */
    uint32_t k_st[128], k_pst[128];                       /* token starts (offsets in the line) of this line and the line before */
#endif
    uint32_t iw, ich, ih, i_pred, i_cls, i_act, i_n, i_med, i_wn;   /* image model: row bytes, channels, head; mixer selectors */
    uint32_t mc[64];
    int mci;
    const kolmo_kernels *kk;                              /* dispatched kernels (src/arch) */
    /* learning settings (cm_tune_t; the defaults are the compile-time constants, a tuned stream carries its own) */
    int lr, lrk, lrd, apm_sh, mc_sh; uint32_t smlim; int fw[4]; uint32_t rcp[16];
    /* model 4: the hashed contexts in use, a subset of model 3's (the stream's mask). Slot k of the per-model arrays
     * (t, shi, cur, slot, sm, ...) holds context mid[k]; h[] stays indexed by context, hc[k] = h[mid[k]]. Models 0-3:
     * mid[k] = k and nmod = CM_NMOD(v2). */
    int nmod; uint8_t mid[CM_NSLOT]; uint32_t hc[CM_NSLOT], mask;
    int64_t mwin;                                         /* model 4: the match chain's ring size (0: one entry per byte) */
    uint32_t nl[CM_NSLOT];                                  /* model 4: lines per context table */
    uint8_t *cur4[CM_NSLOT], *st4[CM_NSLOT];                  /* model 4: the nibble bucket and the bit's state byte, per slot */
    /* model 4: the 16-bit mixer (algo/mixer.h): inputs, and the four weight sets' rows of CM4_NIP (4096 = 1.0) */
    int16_t x16[CM4_NIP];   /* (cm_t comes from calloc: no over-alignment) */
    int16_t (*q1)[CM4_NIP], (*q2)[CM4_NIP], (*q3)[CM4_NIP], (*q4)[CM4_NIP];
    /* model 4 initialises a row of the mixers and the two large APMs when it is first used (the same values as setting
     * every row at the start, without writing megabytes for a short stream): the row templates and the rows' flags */
    int16_t q0[4][CM4_NIP]; uint16_t a0[33]; uint8_t *rdy;   /* rdy: CM_W1 + CM_W2 + CM_W3 + CM_W4 + 2 * 65536 flags */
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
static int kmem_try_reserve(uint64_t b, uint64_t cap) { (void)cap; cm_used += b; return 1; }
static void kmem_release(uint64_t b) { cm_used -= b; }
typedef struct { uint64_t cap, dec_limit; size_t raw; } kmem_ctx;
static kmem_ctx kmem_ctx_get(void) { kmem_ctx c = {kmem_tls_cap, kmem_tls_dec_limit, kmem_tls_raw}; return c; }
static void kmem_ctx_set(kmem_ctx c) { kmem_tls_cap = c.cap; kmem_tls_dec_limit = c.dec_limit; kmem_tls_raw = c.raw; }
static uint64_t kmem_buffers(uint64_t raw) { return 2 * raw + ((uint64_t)1 << 20); }
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
#ifndef CM_SCALED_BYTES
#define CM_SCALED_BYTES 4   /* input bytes per bucket in the default ("scaled") sizing; see cm_bits_for */
#endif
#ifndef CM_SCALED_MAX
#define CM_SCALED_MAX 21    /* and at most 2^21 buckets per model there (about 2.5 GiB to decode) */
#endif
static int cm_bits_for(size_t n);
#ifdef KOLMO_IMPACT
#define CM_MASK3 (((((uint32_t)1 << CM_NM3) - 1) & ~(uint32_t)1) | 0xC0000000u)   /* research: + the token-class contexts 30, 31 */
#else
#define CM_MASK3 ((((uint32_t)1 << CM_NM3) - 1) & ~(uint32_t)1)   /* model 4 masks: a subset of model 3's hashed contexts 1..29 */
#endif
/* model 4's contexts, chosen by backward elimination on 1 MB samples (benchmark/w4/cm/RESULTS.md): text (and logs, CSV,
 * JSON) and everything else */
#define CM4_MASK_TEXT 0x3f60f758u   /* orders 3, 4, 5, 7; word, column, words; indirect; orders 12, 20; expected byte; structure */
#define CM4_MASK_BIN  0x019f74d8u   /* orders 3, 4, 7; word; column; sparse; indirect; record contexts; expected byte */
/* log2 buckets of hashed context i (1..29) for tables of 2^bb: the model-2 contexts (16+) half, the structure contexts
 * (25+) a quarter, from 2^19 */
static int cm_ctx_bits(int i, int bb) { return i >= CM_NM && bb > 18 ? bb - CM_SMALL - (i >= CM_NM3 - 5) : bb; }
/* Model 4's tables: 64-byte lines of four 16-byte nibble buckets (check byte, 15 bit-history states). Sizes: a context's table has n >> s lines for an n-byte stream (s from the header: s = 2 + its low nibble), half
 * that for the model-2 contexts (16-24), a quarter for the structure contexts (25+), between 2^12 and 2^22 lines; any
 * count (the line is h * lines >> 32). */
enum { CM4_MINL = 1 << 12, CM4_MAXL = 1 << 22 };
static uint32_t cm4_lines(int i, size_t n, int s) {
    uint64_t l = (uint64_t)n >> s; l >>= i >= CM_NM3 - 5 ? 2 : i >= CM_NM ? 1 : 0;
    return (uint32_t)(l < CM4_MINL ? CM4_MINL : l > CM4_MAXL ? CM4_MAXL : l);
}
static inline uint32_t cm4_line(uint32_t h, uint32_t nl) { return (uint32_t)(((uint64_t)h * nl) >> 32); }
static size_t cm_mem4(size_t n, int s, uint32_t mask) {
    /* model 3's 48 MiB less its 32-bit mixer rows (16.5 MiB) and match table (16 MiB), plus the 16-bit rows (8.6 MiB) and
     * a match table of 2^16..2^22 slots */
    size_t b = (size_t)25 << 20; int mb = 16; while (mb < 22 && ((size_t)1 << mb) < n) mb++; b += (size_t)4 << mb;
    for (int i = 1; i < CM_NSLOT; i++) if (mask >> i & 1) b += (size_t)64 * cm4_lines(i, n, s);
    return b;
}
#ifndef CM4_S
#define CM4_S 5   /* model 4's size shift for streams up to 48 MiB: n / 32 lines (2 bytes per input byte) per context */
#endif
static int cm4_s_default = CM4_S;   /* research harnesses may change it */
/* model 4's size shift for an n-byte stream: the default, raised under an explicit memory cap until the decode working
 * set (tables, match chain, the block's buffers) fits (a format decision from n, the setting and the cap only) */
static int cm4_shift_for(size_t n, uint32_t mask);
/* model 4's match chain: one entry per byte up to 2^24 bytes, then a ring of the last 2^24 positions (64 MiB) */
#define CM4_MWIN ((int64_t)1 << 24)
static size_t cm4_chain(size_t n) { return 4 * (n + 8 > (size_t)CM4_MWIN ? (size_t)CM4_MWIN : n + 8); }
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
    return (size_t)(CM_NM - 1) * t + (v2 ? (size_t)(CM_NMX - CM_NM) * ts : 0) + (v2 == 3 ? (size_t)(CM_NM3 - 5 - CM_NMX) * ts + 5 * (bb > 18 ? ts >> 1 : t) : 0) + ((size_t)(v2 ? 48 : 16) << 20);
#endif
}

/* Table sizing (encoder; the stream header carries the bits, so the decoder needs exactly what the encoder chose).
 * A format decision: it depends on the stream's length, this setting and the call's explicit memory cap only, never on
 * the memory free at the time (docs/MEMORY-MODEL.md, "memory changes speed, never bytes").
 * -1 ("scaled", the default from rc3): one bucket per CM_SCALED_BYTES input bytes, 2^16 to 2^CM_BB_MAX per model.
 *    Measured on the M5 (1 thread) against full tables: tables within 16 times the stream (rc3 wave 2's first rule,
 *    ~200 MiB to decode 10 MB) cost +1.7% (dickens 1 MB), +3.5% (4 MB), +5.1% (10 MB). One bucket per 4 bytes costs
 *    +0.35%, +0.28%, +0.25% there (samba 1 MB +0.08%, mozilla 4 MB +0.04%), decoding 1 MB in ~0.5 GiB, 4 MB in
 *    ~1.3 GiB; from 8 MiB the tables stop at 2^21 (~2.5 GiB to decode; 10 MB text +0.25% against 2^22);
 * 0 ("full", rc2's sizing): one bucket per input byte, 2^16 to 2^CM_BB_MAX per model; 16..22: full, capped at 2^bits.
 * Then, with an explicit cap (kmem_tls_cap), the largest size whose decode working set fits the cap (2^16 at least). */
static int cm_tables_mode = -1;
int kolmo_set_x_tables(int mode) {
    if (mode != 0 && mode != -1 && (mode < 16 || mode > CM_BB_MAX)) return -1;
    __atomic_store_n(&cm_tables_mode, mode, __ATOMIC_RELAXED); return 0;
}
static int cm_bits_for(size_t n) {
    const int mode = __atomic_load_n(&cm_tables_mode, __ATOMIC_RELAXED), cap = mode >= 16 ? mode : mode == -1 ? CM_SCALED_MAX : CM_BB_MAX;
    int b = 16; while (b < cap && ((size_t)1 << b) < (mode == -1 ? n / CM_SCALED_BYTES : n)) b++;
    if (kmem_tls_cap && kmem_tls_cap != KMEM_UNLIMITED) {
        size_t raw = kmem_tls_raw > n ? kmem_tls_raw : n;
        uint64_t fixed = kmem_buffers(raw) + 4 * ((uint64_t)n + 8);   /* the block's buffers and model 3's match chain */
        while (b > 16 && cm_mem_for(b, 3) + fixed > kmem_tls_cap) b--;
    }
    return b;
}
static int cm4_shift_for(size_t n, uint32_t mask) {
    /* n / 32 lines per context up to 48 MiB, n / 64 up to 128 MiB, n / 128 beyond: about 26, 13 and 6 table bytes per input
     * byte with the text contexts, so that -x needs at most about 1.5 times the memory of zpaq -m5, whose need per byte
     * also falls with the size (benchmark/w4/cm/RESULTS.md) */
    int s = cm4_s_default + (n > ((size_t)48 << 20)) + (n > ((size_t)128 << 20));
    if (__atomic_load_n(&cm_tables_mode, __ATOMIC_RELAXED) == 0 && s > 2) s -= 2;   /* --x-tables=full: four times the lines */
    if (kmem_tls_cap && kmem_tls_cap != KMEM_UNLIMITED) {
        size_t raw = kmem_tls_raw > n ? kmem_tls_raw : n;
        uint64_t fixed = kmem_buffers(raw) + cm4_chain(n);
        while (s < 17 && cm_mem4(n, s, mask) + fixed > kmem_tls_cap) s++;
    }
    return s;
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
    for (int i = 1; i < CM_NSLOT; i++) if (m->tbi[i]) cm_big_free(m->t[i], m->tbi[i]);
    cm_big_free(m->w3, sizeof(*m->w3) * CM_W3); cm_big_free(m->w4, sizeof(*m->w4) * CM_W4);
    cm_big_free(m->rdy, CM4_RA2 + 2 * 65536); cm_big_free(m->q1, sizeof(*m->q1) * CM_W1); cm_big_free(m->q2, sizeof(*m->q2) * CM_W2); cm_big_free(m->q3, sizeof(*m->q3) * CM_W3); cm_big_free(m->q4, sizeof(*m->q4) * CM_W4);
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
/* every row of the five mixers starts from the prior's row for that mixer (weights in 16.16, stored / 4) */
static void cm_apply_prior(cm_t *m, const int16_t p[5][CM_NIX3]) {
    /* model 4: input j of the subset takes the prior of the model-3 input it stands for (slot k is context mid[k]) */
    int jm[CM_NIX3]; const int nm = m->nmod;
    for (int j = 0; j < CM_NIX3; j++) jm[j] = j;
    if (m->v2 == 4) {
        for (int j = 0; j < CM_NIX3; j++) jm[j] = -1;
        for (int k = 0; k < nm; k++) { const int c = m->mid[k] < CM_NM3; jm[k] = c ? m->mid[k] : -1; jm[nm + 3 + k] = c ? CM_NM3 + 3 + m->mid[k] : -1; }   /* research contexts 30, 31 keep the start value */
        for (int k = 0; k < 3; k++) jm[nm + k] = CM_NM3 + k;
    }
    for (int j = 0; j < CM_NIX3; j++) {
        const int q = jm[j]; if (q < 0) continue;
        const int32_t a = p[0][q] * 4, b = p[1][q] * 4, c = p[2][q] * 4, d = p[3][q] * 4;
        if (m->v2 == 4) {   /* 16-bit row templates: 4096 = 1.0 */
#define CM4_Q(v) (int16_t)((v) >> 4 > 32767 ? 32767 : (v) >> 4 < -32768 ? -32768 : (v) >> 4)
            if (j < CM4_NIP) { m->q0[0][j] = CM4_Q(a); m->q0[1][j] = CM4_Q(b); m->q0[2][j] = CM4_Q(c); m->q0[3][j] = CM4_Q(d); }
#undef CM4_Q
            continue;
        }
        for (int i = 0; i < CM_W1; i++) m->w1[i][j] = a;
        for (int i = 0; i < CM_W2; i++) m->w2[i][j] = b;
        if (m->v2) { for (int i = 0; i < CM_W3; i++) m->w3[i][j] = c; for (int i = 0; i < CM_W4; i++) m->w4[i][j] = d; }
    }
    if (m->v2) for (int i = 0; i < CM_WF; i++) for (int j = 0; j < 5; j++) m->wf[i][j] = p[4][j] * 4;
}
static const kolmo_kernels *cm_kk_override;            /* tests only (tests/test_cm4.c): the kernels of one ISA level */
static const int16_t (*cm_prior_override)[CM_NIX3];   /* research harnesses only (benchmark/rc3w2/twopass): replaces prior 1 */
#ifdef KOLMO_IMPACT
#define CM4_B2MAX 10            /* research: 3..10 = the per-kind prior of kind 1..8 (algo/texttype.c) */
static void cm_apply_kprior4(cm_t *m, int k);
#else
#define CM4_B2MAX 2
#endif
static void cm_set_tune(cm_t *m, cm_tune_t t) {
#ifdef KOLMO_IMPACT
    if (m->v2 == 4 && t.b2 >= 3) { cm_apply_kprior4(m, t.b2 - 3); t.b2 = 0; }
    else
#endif
    if (m->v2 == 4 && t.b2) cm_apply_prior(m, cm_prior_override ? cm_prior_override : cm_prior4[t.b2 - 1]);   /* model 4: 1 text, 2 binary */
    else if (t.b2 == 1 && m->v2) cm_apply_prior(m, cm_prior_override ? cm_prior_override : cm_prior[m->v2 >= 3]);
    m->lr = CM_LR + cm_tv_lr[t.b0 & 3]; const int lim = cm_tv_lim[t.b0 >> 2 & 3]; m->apm_sh = cm_tv_apm[t.b0 >> 4 & 3]; m->mc_sh = cm_tv_mc[t.b0 >> 6];
    m->lrk = cm_tv_lrk[t.b1 & 3]; m->lrd = cm_tv_lrd[t.b1 >> 2 & 3]; m->smlim = cm_tv_sm[t.b1 >> 4 & 3];
    for (int k = 0; k < 4; k++) m->fw[k] = cm_tv_fin[t.b1 >> 6][k];
    for (int n = 0; n < 16; n++) m->rcp[n] = cm_rcp[n * lim];
}
#ifdef KOLMO_IMPACT
/* Research port of the rc3w3 prior lane's per-kind priors (claude/rc3w3-prior-20261118, docs/PRIORS.md there) onto
 * model 4. The lane trained them on model 3: every row of mixers w1-w3 and the final mixer, w4's mean row (4-bit
 * weights, steps of 1/8), the state maps and order-1 table (stretched probabilities in 17 levels with a count class),
 * the match counters. Here the blobs are read unpacked from KOLMO_CM_KPRIOR_FILE (benchmark/impact/kprior.py writes it
 * from the lane's cm_kprior.inc: 8 kinds x CM_KP_RAW bytes) and mapped onto model 4: a slot takes the rows of the
 * context it stands for, the 16-bit mixer rows are the 16.16 weights / 16. The token-class contexts keep their start. */
#define CM_KP_RAW 373391
typedef struct {
    int8_t w1[CM_W1][CM_NIX3], w2[CM_W2][CM_NIX3], w3[CM_W3][CM_NIX3], w4m[CM_NIX3], wf[CM_WF][5];
    uint8_t sm[CM_NM3][8][256], o1q[65536], o1s[65536], mc[64];
} cm_kprior_t;
static cm_kprior_t *cm_kp_cache[8]; static int cm_kp_loaded;
static pthread_mutex_t cm_kp_mu = PTHREAD_MUTEX_INITIALIZER;
static void cm_kp_load(void) {
    const char *path = getenv("KOLMO_CM_KPRIOR_FILE"); FILE *f = path ? fopen(path, "rb") : NULL;
    uint8_t *raw = malloc(CM_KP_RAW);
    for (int k = 0; f && raw && k < 8; k++) {
        if (fread(raw, 1, CM_KP_RAW, f) != CM_KP_RAW) break;
        cm_kprior_t *p = calloc(1, sizeof *p); if (!p) break;
        const uint8_t *q = raw;
        memcpy(p->w1, q, sizeof p->w1); q += sizeof p->w1; memcpy(p->w2, q, sizeof p->w2); q += sizeof p->w2;
        memcpy(p->w3, q, sizeof p->w3); q += sizeof p->w3; memcpy(p->w4m, q, sizeof p->w4m); q += sizeof p->w4m;
        memcpy(p->wf, q, sizeof p->wf); q += sizeof p->wf;
        for (int i = 0; i < CM_NM3; i++) for (int b = 0; b < 8; b++) for (int st = 0; st < bh_ns; st++) p->sm[i][b][st] = *q++;
        memcpy(p->o1q, q, 65536); q += 65536; memcpy(p->o1s, q, 65536); q += 65536; memcpy(p->mc, q, 64); q += 64;
        if ((size_t)(q - raw) != CM_KP_RAW) { free(p); break; }   /* the lane's layout and this build's state count agree */
        cm_kp_cache[k] = p;
    }
    free(raw); if (f) fclose(f);
}
static const cm_kprior_t *cm_kprior_get(int k) {
    pthread_mutex_lock(&cm_kp_mu); if (!cm_kp_loaded) { cm_kp_load(); cm_kp_loaded = 1; } pthread_mutex_unlock(&cm_kp_mu);
    return k >= 0 && k < 8 ? cm_kp_cache[k] : NULL;
}
static void cm_apply_kprior4(cm_t *m, int k) {
    const cm_kprior_t *p = cm_kprior_get(k);
    if (!p) return;   /* not loaded: the encoder never picks it (cm_impact_b2); a decoder without it fails the block checksum */
    const int nm = m->nmod; int jm[CM4_NIP];
    for (int j = 0; j < CM4_NIP; j++) jm[j] = -1;
    for (int i = 0; i < nm; i++) if (m->mid[i] < CM_NM3) { jm[i] = m->mid[i]; jm[nm + 3 + i] = CM_NM3 + 3 + m->mid[i]; }
    for (int i = 0; i < 3; i++) jm[nm + i] = CM_NM3 + i;
#define KPQ(row, j) (int16_t)(jm[j] < 0 ? 1024 : (int)(row)[jm[j]] * 512)
    for (int i = 0; i < CM_W1; i++) { for (int j = 0; j < CM4_NIP; j++) m->q1[i][j] = KPQ(p->w1[i], j); m->rdy[i] = 1; }
    for (int i = 0; i < CM_W2; i++) { for (int j = 0; j < CM4_NIP; j++) m->q2[i][j] = KPQ(p->w2[i], j); m->rdy[CM_W1 + i] = 1; }
    for (int i = 0; i < CM_W3; i++) { for (int j = 0; j < CM4_NIP; j++) m->q3[i][j] = KPQ(p->w3[i], j); m->rdy[CM_W1 + CM_W2 + i] = 1; }
    for (int j = 0; j < CM4_NIP; j++) m->q0[3][j] = KPQ(p->w4m, j);       /* w4: the mean row, set lazily from the template */
#undef KPQ
    for (int i = 0; i < CM_WF; i++) for (int j = 0; j < 5; j++) m->wf[i][j] = (int32_t)p->wf[i][j] * 8192;
    static const uint16_t smn[4] = {0, 4, 12, 30}, o1n[4] = {0, 2, 6, 15};
    for (int i = 0; i < nm; i++) {
        const int c = m->mid[i]; if (c >= CM_NM3) continue;
        for (int b = 0; b < 8; b++) for (int st = 0; st < bh_ns; st++) {
            const uint8_t v = p->sm[c][b][st]; const uint32_t p12 = (uint32_t)cm_SQ(m->sq, ((v & 31) - 8) * 256);
            m->sm[i][b << 8 | st] = p12 << 20 | smn[v >> 5];
        }
    }
    for (int i = 0; i < 65536; i++) { const uint8_t v = p->o1q[i]; const uint32_t p12 = (uint32_t)cm_SQ(m->sq, ((v & 31) - 8) * 256); m->o1[i] = p12 << 20 | (uint32_t)p->o1s[i] << 8 | o1n[v >> 5]; }
    for (int i = 0; i < 64; i++) m->mc[i] = (uint32_t)cm_SQ(m->sq, (p->mc[i] - 32) * 64) << 4;
}
/* KOLMO_CM_KPRIOR=1: a text stream under 4 MiB whose kind the detector knows (1-8) starts from that kind's prior */
static uint8_t cm_impact_b2(const uint8_t *in, size_t n, uint8_t b2) {
    const char *e = getenv("KOLMO_CM_KPRIOR"); if (!e || atoi(e) != 1 || n >= ((size_t)4 << 20)) return b2;
    const int kind = text_kind(in, n);
    return kind > TT_NONE && kind <= 8 && cm_kprior_get(kind - 1) ? (uint8_t)(2 + kind) : b2;
}
#endif
/* mask: model 4's contexts (v2 = 4), ignored otherwise */
static cm_t *cm_new_xm(uint8_t *buf, int bb, int v2, size_t chain, int lx, uint32_t mask, size_t n) {   /* model 4: bb = the size shift s, n the stream's length */   /* lx: exact logistic tables */   /* chain: bytes for model 3's match chain (4 per input byte) */
    pthread_once(&cm_once, cm_init_tables);
    size_t mem = (v2 == 4 ? cm_mem4(n, bb, mask) : cm_mem_for(bb, v2)) + chain;
    if (kmem_tls_dec_limit && mem > kmem_tls_dec_limit) return NULL;   /* decoding: more than the frame declares */
    size_t resv = cm_acquire(mem);
    atomic_fetch_add_explicit(&cm_models_made, 1, memory_order_relaxed);
    cm_t *m = calloc(1, sizeof(cm_t)); if (!m) { cm_release(resv); return NULL; }
    int nm = 0;
    if (v2 >= 4) { m->mid[nm++] = 0; for (int i = 1; i < CM_NSLOT; i++) if (mask >> i & 1) m->mid[nm++] = (uint8_t)i; }
    else for (nm = 0; nm < CM_NMOD(v2); nm++) m->mid[nm] = (uint8_t)nm;
    m->nmod = nm; m->mask = mask;
    m->kk = cm_kk_override ? cm_kk_override : kolmo_kernels_get();
    m->sh = 32 - bb; m->mem = resv; m->v2 = v2; m->sq = lx ? cm_sqx : cm_sq; m->st = lx ? cm_stx : cm_st;
    m->tb = sizeof(cm_bucket) << bb;
    for (int i = 1; i < nm; i++) {                            /* the model-2 contexts (16+) have fewer distinct values */
        int b = cm_ctx_bits(m->mid[i], bb);                   /* model 2 and 3 contexts: half; structure contexts: a quarter */
        m->shi[i] = (uint8_t)(32 - b); m->tbi[i] = sizeof(cm_bucket) << b;
        if (v2 == 4) { m->nl[i] = cm4_lines(m->mid[i], n, bb); m->tbi[i] = (size_t)64 * m->nl[i]; }
#ifdef KOLMO_CM_COMPACT
        if (v2 != 4 && (m->mid[i] == 1 || m->mid[i] == 17)) {   /* model 4 has no compact tables */
            m->tbi[i] = cm_build_byte_table(m->one_byte[m->mid[i] == 17], m->mid[i], m->shi[i]);
            if (!m->tbi[i]) goto fail;
        }
#endif
        m->t[i] = cm_big(m->tbi[i]); if (!m->t[i]) goto fail;
    }
    if (v2 == 4) {   /* the 16-bit mixer's rows, each weight 1/4 (rows are set from the templates when first used) */
        m->q1 = cm_big(sizeof(*m->q1) * CM_W1); m->q2 = cm_big(sizeof(*m->q2) * CM_W2); m->q3 = cm_big(sizeof(*m->q3) * CM_W3); m->q4 = cm_big(sizeof(*m->q4) * CM_W4);
        m->rdy = cm_big(CM4_RA2 + 2 * 65536);
        if (!m->q1 || !m->q2 || !m->q3 || !m->q4 || !m->rdy) goto fail;
        for (int k = 0; k < 4; k++) for (int j = 0; j < CM4_NIP; j++) m->q0[k][j] = 1024;
        for (int j = 0; j < 33; j++) m->a0[j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    }
    if (v2) {
        if (v2 != 4) { m->w3 = cm_big(sizeof(*m->w3) * CM_W3); m->w4 = cm_big(sizeof(*m->w4) * CM_W4); }
        m->a3 = cm_big(sizeof(*m->a3) * CM_A3);
        if ((v2 != 4 && (!m->w3 || !m->w4)) || !m->a3) goto fail;
        if (v2 != 4) for (int i = 0; i < CM_W3; i++) for (int j = 0; j < CM_NIX3; j++) m->w3[i][j] = 1 << 14;
        if (v2 != 4) for (int i = 0; i < CM_W4; i++) for (int j = 0; j < CM_NIX3; j++) m->w4[i][j] = 1 << 14;
        for (int i = 0; i < CM_WF; i++) { for (int j = 0; j < 4; j++) m->wf[i][j] = 1 << 14; m->wf[i][4] = 0; }
        if (v2 != 4) for (int c = 0; c < CM_A3; c++) for (int j = 0; j < 33; j++) m->a3[c][j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    }
    for (int i = 0; i < 65536; i++) m->o1[i] = 0x80000000u;
    for (int i = 0; i < nm; i++) for (int j = 0; j < 256 * 8; j++) {
        int st = j & 255; uint32_t n0 = st < bh_ns ? bh_n0[st] : 0, n1 = st < bh_ns ? bh_n1[st] : 0;
        m->sm[i][j] = (uint32_t)(((2ull * n1 + 1) << 22) / (2ull * (n0 + n1) + 2)) << 10;
    }
    for (int i = 0; i < CM_W1; i++) for (int j = 0; j < CM_NIX3; j++) m->w1[i][j] = 1 << 14;
    for (int i = 0; i < CM_W2; i++) for (int j = 0; j < CM_NIX3; j++) m->w2[i][j] = 1 << 14;
    for (int c = 0; c < 256; c++) for (int j = 0; j < 33; j++) m->a1[c][j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    if (v2 != 4) for (int c = 0; c < 65536; c++) for (int j = 0; j < 33; j++) m->a2[c][j] = (uint16_t)(cm_SQ(m->sq, (j - 16) * 128) * 16);
    m->buf = buf; m->c0 = 1;
    m->mmask = (1u << 22) - 1;
    if (v2 == 4) { int b = 16; while (b < 22 && ((size_t)1 << b) < n) b++; m->mmask = (1u << b) - 1; }   /* model 4: about one slot per byte */
    m->mt = cm_big(((size_t)m->mmask + 1) * 4); if (!m->mt) goto fail;
    for (int i = 0; i < 64; i++) m->mc[i] = 1u << 15;
    if (chain) { m->mprev_n = chain; m->mprev = cm_big(chain); if (!m->mprev) goto fail; }
    if (v2 == 4 && n + 8 > CM4_MWIN) m->mwin = CM4_MWIN;   /* model 4: the chain is a ring of the last CM4_MWIN positions */
    cm_set_tune(m, (cm_tune_t){0, 0, 0});
    return m;
fail:
    cm_free_parts(m); cm_release(resv); return NULL;
}
static cm_t *cm_new_x(uint8_t *buf, int bb, int v2, size_t chain, int lx) { return cm_new_xm(buf, bb, v2, chain, lx, 0, 0); }
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
static inline cm_bucket *cm_find(cm_bucket *t, uint32_t h, int sh) { return cm_find_at(t, h >> sh, h & 0xffff); }   /* (models 1-3 use the compact path) */
static inline uint32_t cm_byte_entry(const cm_t *m, int o, unsigned nib) {
    unsigned c = o == 1 ? m->c4 & 255 : (m->c4 >> 8) & 255;
    return m->one_byte[o == 17][m->pos ? c : 256][nib];
}
static inline cm_bucket *cm_find_model(cm_t *m, int i, unsigned nib, uint32_t salt) {   /* i: slot, mid[i]: context */
    const int o = m->mid[i];
    if (o == 1 || o == 17) {
        uint32_t entry = cm_byte_entry(m, o, nib);
        return cm_find_at(m->t[i], entry >> 16, entry & 65535);
    }
    uint32_t h = cm_hmix(m->h[o] + salt);
    return cm_find_at(m->t[i], h >> m->shi[i], h & 65535);
}
static inline uint32_t cm_prefetch_slot(const cm_t *m, int i, unsigned nib, uint32_t salt) {
    const int o = m->mid[i];
    if (o == 1 || o == 17) return cm_byte_entry(m, o, nib) >> 16;
    return cm_hmix(m->h[o] + salt) >> m->shi[i];
}
#endif
/* model 4: the 16-byte bucket of hash h in its 64-byte line: the one with h's check byte, or else the one whose first
 * state has seen the fewest bits, emptied (lpaq's replacement) */
/* A line: the four buckets' check bytes, then their 15 states each (bucket j's node k at 4 + 15 j + k - 1). Returns the
 * bucket's base so that base[k] is node k. */
static inline uint8_t *cm4_find(cm_bucket *t, uint32_t h, uint32_t nl) {
    uint8_t *l = (uint8_t *)&t[cm4_line(h, nl)]; const uint8_t chk = (uint8_t)h;
    int j;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint32_t w; memcpy(&w, l, 4); w ^= chk * 0x01010101u;
    const uint32_t z = (w - 0x01010101u) & ~w & 0x80808080u;   /* exact for the lowest zero byte: the first match */
    if (z) return l + 3 + 15 * (__builtin_ctz(z) >> 3);
#else
    for (j = 0; j < 4; j++) if (l[j] == chk) return l + 3 + 15 * j;
#endif
    int bp = cm4_pri[l[4]]; j = 0;
    for (int k = 1; k < 4; k++) { int p = cm4_pri[l[4 + 15 * k]]; if (p < bp) { bp = p; j = k; } }
    uint8_t *v = l + 4 + 15 * j; memset(v, 0, 15); l[j] = chk;
    return v - 1;
}
/* the length, up to lmax, of the common run of bytes ending just before positions a and b (a < b): what the byte loop
 * while (l < lmax && buf[a - 1 - l] == buf[b - 1 - l]) l++ returns, 8 bytes at a time on little-endian hosts */
static inline int cm_match_len(const uint8_t *buf, int64_t a, int64_t b, int lmax) {
    int l = 0;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (; l + 8 <= lmax; l += 8) {
        uint64_t x, y; memcpy(&x, buf + a - 8 - l, 8); memcpy(&y, buf + b - 8 - l, 8);
        if (x != y) return l + (__builtin_clzll(x ^ y) >> 3);   /* the highest differing byte is the first one met */
    }
#endif
    while (l < lmax && buf[a - 1 - l] == buf[b - 1 - l]) l++;
    return l;
}
static inline int cm_expected(cm_t *m) {
    if (!m->mlen) return -1;
    int b = m->buf[m->mptr];
    if (((b + 256) >> (8 - m->bitpos)) != (int)m->c0) return -1;
    return (b >> (7 - m->bitpos)) & 1;
}
/* slot: high 16 bits adaptive probability, bits 8..15 bit-history state, low 8 bits count */
static inline int cm_stc(const int16_t *st, uint32_t v) { return st[v >> 20]; }

/* v2 is a compile-time constant at each call site, so model 1 and model 2 each get their own specialized loop. */
static inline void cm4_row(int16_t *r, const int16_t *t, uint8_t *f) { if (!*f) { *f = 1; memcpy(r, t, CM4_NIP * sizeof *r); } }
static inline void cm4_apm(uint16_t *r, const uint16_t *t, uint8_t *f) { if (!*f) { *f = 1; memcpy(r, t, 33 * sizeof *r); } }
#define CM_HON(k) (v2 < 4 || (m->mask >> (k) & 1))       /* context k's hash is needed (model 4: only the stream's) */
#define CM_HSRC(m, v2) ((v2) >= 4 ? (m)->hc : (m)->h)   /* the context hashes in slot order */
static inline __attribute__((always_inline)) int cm_predict(cm_t *m, const int v2) {
    const int nm = v2 >= 4 ? m->nmod : CM_NMOD(v2), ni = 2 * nm + 3;
    uint32_t c0 = m->c0; int bp = (int)m->bitpos;
    if (bp == 0 || bp == 4) {
        uint32_t nib = bp ? (c0 & 15) * 0x5BD1E995u : 0;
#ifdef KOLMO_CM_COMPACT
        if (v2 != 4) for (int i = 1; i < nm; i++) m->cur[i] = cm_find_model(m, i, bp ? (c0 & 15) : 0, nib);
        else
#endif
        {
        uint32_t hv[CM_NSLOT]; m->kk->cm_hash(CM_HSRC(m, v2) + 1, nm - 1, nib, hv + 1);       /* the context hashes, 8 at a time */
        if (v2 == 4) for (int i = 1; i < nm; i++) m->cur4[i] = cm4_find(m->t[i], hv[i], m->nl[i]);
        else for (int i = 1; i < nm; i++) m->cur[i] = cm_find(m->t[i], hv[i], m->shi[i]);
        }
    }
    int node = bp < 4 ? (int)((c0 & ((1u << bp) - 1)) | (1u << bp)) : (int)((c0 & ((1u << (bp - 4)) - 1)) | (1u << (bp - 4)));
    const int img = v2 == 2;
    m->slot[0] = &m->o1[c0 | (img ? m->i_pred : (m->c4 & 0xff)) << 8];
    if (v2 == 4) {
        {   uint32_t v = *m->slot[0]; uint16_t st = (uint16_t)((v >> 8 & 255) | bp << 8); m->sst[0] = st;
            m->x16[0] = (int16_t)cm_stc(m->st, v); m->x16[nm + 3] = m->st[m->sm[0][st] >> 20]; }
        for (int i = 1; i < nm; i++) {
            uint8_t *sp = m->st4[i] = m->cur4[i] + node; const uint32_t sv = *sp; const uint16_t st = (uint16_t)(sv | bp << 8); m->sst[i] = st;
            m->x16[nm + 3 + i] = m->st[m->sm[i][st] >> 20]; m->x16[i] = cm4_dx[sv];
        }
    } else {
    for (int i = 1; i < nm; i++) m->slot[i] = &m->cur[i]->s[node];
    for (int i = 0; i < nm; i++) {
        uint32_t v = *m->slot[i]; uint16_t st = (uint16_t)((v >> 8 & 255) | bp << 8); m->sst[i] = st;
        m->x[i] = cm_stc(m->st, v); m->x[nm + 3 + i] = m->st[m->sm[i][st] >> 20];
    }
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
    if (v2 == 4) { m->x16[nm] = (int16_t)m->x[nm]; m->x16[nm + 1] = (int16_t)m->x[nm + 1]; m->x16[nm + 2] = 64; }
    int ml = m->mlen == 0 ? 0 : m->mlen < 16 ? 1 : m->mlen < 32 ? 2 : 3;
    uint32_t c1 = m->c4 & 0xff;
    int cls = ((c1 | 32) >= 'a' && (c1 | 32) <= 'z') ? 0 : (c1 >= '0' && c1 <= '9') ? 1 : 2;
    if (img) { cls = (int)m->i_cls; c1 = m->i_pred; }
    m->w1i = (int)c0 + 256 * cls;
    m->w2i = (ml * 8 + bp) * 4 + (int)(img ? m->i_act : c1 >> 6);
    const int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];   /* models 1-3 (model 4: q1, q2) */
    int st;
    if (!v2) {
        int64_t sd[4]; m->kk->cm_dot(m->x, ni, w1, w2, NULL, NULL, sd);
        int d1 = (int)(sd[0] >> 16), d2 = (int)(sd[1] >> 16);
        m->pr1 = cm_SQ(m->sq, d1); m->pr2 = cm_SQ(m->sq, d2);
        st = (d1 + d2) >> 1;
    } else {                                                  /* four mixers, then a small learned mixer of their outputs */
        m->w3i = (int)((img ? m->i_n : (m->c4 >> 8 & 0xff)) << 3 | (uint32_t)bp); m->w4i = (int)(c0 | (img ? m->i_med : c1) << 8); m->wfi = (int)c0 | ml << 8;
        const int32_t *wf = m->wf[m->wfi];
        int d[4];
        if (v2 == 4) {
            cm4_row(m->q1[m->w1i], m->q0[0], m->rdy + m->w1i); cm4_row(m->q2[m->w2i], m->q0[1], m->rdy + CM_W1 + m->w2i);
            cm4_row(m->q3[m->w3i], m->q0[2], m->rdy + CM_W1 + CM_W2 + m->w3i); cm4_row(m->q4[m->w4i], m->q0[3], m->rdy + CM_W1 + CM_W2 + CM_W3 + m->w4i);
            int32_t s4[4]; m->kk->cm_dot16(m->x16, (ni + 15) & ~15, m->q1[m->w1i], m->q2[m->w2i], m->q3[m->w3i], m->q4[m->w4i], s4);
            for (int k = 0; k < 4; k++) d[k] = s4[k] >> 4;
        } else {
            const int32_t *w3 = m->w3[m->w3i], *w4 = m->w4[m->w4i];
            int64_t sd[4]; m->kk->cm_dot(m->x, ni, w1, w2, w3, w4, sd);
            for (int k = 0; k < 4; k++) d[k] = (int)(sd[k] >> 16);
        }
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
    if (v2 == 4) cm4_apm(m->a2[m->a2i], m->a0, m->rdy + CM4_RA2 + m->a2i);
    int lo = m->ai, wt = m->aw;
    int p1 = (m->a1[c0][lo] * (128 - wt) + m->a1[c0][lo + 1] * wt) >> 11;
    int p2 = (m->a2[m->a2i][lo] * (128 - wt) + m->a2[m->a2i][lo + 1] * wt) >> 11;
    int p;
    if (!v2) p = (mixed + p1 + p2) / 3;
    else {
        m->a3i = (int)(cm_hmix((img ? m->i_wn : (m->c4 & 0xffff)) * 0x9E3779B1u + c0 * 0x85EBCA6Bu) >> 16);
        if (v2 == 4) cm4_apm(m->a3[m->a3i], m->a0, m->rdy + CM4_RA2 + 65536 + m->a3i);
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
    const int nm = v2 >= 4 ? m->nmod : CM_NMOD(v2), ni = 2 * nm + 3;
    {   /* the next bit's rows of the large selector tables (order-1 mixer, order-1 and order-2 APMs) are known now:
         * prefetch them so their cache misses overlap this update (memory access only; output unchanged). The image
         * model's selectors change at the byte's end, after this point, so its last bit is not prefetched. */
        const int img = v2 == 2, last = m->bitpos == 7;
        if (!(img && last)) {
            const uint32_t nc0 = last ? 1 : m->c0 << 1 | (uint32_t)y, c4n = last ? (m->c4 << 8) | ((m->c0 << 1 | (uint32_t)y) & 255) : m->c4;
            const uint32_t c1 = img ? m->i_pred : c4n & 0xff;
            const char *a2 = (const char *)m->a2[(nc0 | c1 << 8) & 0xffff]; __builtin_prefetch(a2); __builtin_prefetch(a2 + 64);
            if (v2) {
                if (v2 == 4) { const char *q4 = (const char *)m->q4[nc0 | c1 << 8]; __builtin_prefetch(q4); __builtin_prefetch(q4 + 64); }
                else { const char *w4 = (const char *)m->w4[nc0 | (img ? m->i_med : c1) << 8];
                       __builtin_prefetch(w4); __builtin_prefetch(w4 + 64); __builtin_prefetch(w4 + 128); __builtin_prefetch(w4 + 192); }
                const char *a3 = (const char *)m->a3[cm_hmix((img ? m->i_wn : (c4n & 0xffff)) * 0x9E3779B1u + nc0 * 0x85EBCA6Bu) >> 16];
                __builtin_prefetch(a3); __builtin_prefetch(a3 + 64);
            }
        }
    }
    if (v2 == 4) {
        cm_upd_sm(&m->sm[0][m->sst[0]], y, m->smlim); cm_upd_ctr(m->slot[0], y, m->rcp);
        for (int i = 1; i < nm; i++) { cm_upd_sm(&m->sm[i][m->sst[i]], y, m->smlim); *m->st4[i] = bh_next[*m->st4[i]][y]; }
    } else {
    for (int i = 0; i < nm; i++) { cm_upd_sm(&m->sm[i][m->sst[i]], y, m->smlim); cm_upd_ctr(m->slot[i], y, m->rcp); }
    for (int i = 1; i < nm; i++) if ((m->cur[i]->s[0] & 0xffff) < 0xffff) m->cur[i]->s[0]++;
    }
    if (m->mci >= 0) { uint32_t *c = &m->mc[m->mci]; *c = (uint32_t)((int64_t)*c + ((((int64_t)y << 16) - (int64_t)*c) >> m->mc_sh)); }
    int e1 = (y << 12) - m->pr1, e2 = (y << 12) - m->pr2;                  /* learning rate 1/2048 in 16.16 weights */
    int32_t *w1 = m->w1[m->w1i], *w2 = m->w2[m->w2i];
    if (!v2) { const int e[4] = {e1, e2, 0, 0}; m->kk->cm_train(m->x, ni, w1, w2, NULL, NULL, e, m->lr); }
    else {
        int k = 64 - (int)(m->pos >> m->lrd); if (k < m->lrk) k = m->lrk;
        e1 = e1 * k >> 4; e2 = e2 * k >> 4;
        int e3 = ((y << 12) - m->pr3) * k >> 4, e4 = ((y << 12) - m->pr4) * k >> 4, ef = (y << 12) - m->prf;
        int32_t *wf = m->wf[m->wfi];
        if (v2 == 4) { const int sh = m->lr - 11, e[4] = {e1 >> sh, e2 >> sh, e3 >> sh, e4 >> sh};   /* (x e) >> (lr + 4) at 4096 = 1.0 */
                       m->kk->cm_train16(m->x16, (ni + 15) & ~15, m->q1[m->w1i], m->q2[m->w2i], m->q3[m->w3i], m->q4[m->w4i], e); }
        else { int32_t *w3 = m->w3[m->w3i], *w4 = m->w4[m->w4i]; const int e[4] = {e1, e2, e3, e4}; m->kk->cm_train(m->x, ni, w1, w2, w3, w4, e, m->lr); }
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
    if (m->bitpos == 6 && m->pos >= 5) {                      /* the match table's slot for either value of the byte's last bit */
        for (uint32_t b = 0; b < 2; b++) {
            const uint32_t c4n = m->c4 << 8 | ((m->c0 << 1 | b) & 255), c8n = m->c8 << 8 | m->c4 >> 24;
            __builtin_prefetch(&m->mt[cm_hmix(c4n ^ ((c8n & 0xffff) * 0x9E3779B1u)) & m->mmask]);
        }
    }
    if (m->bitpos == 2) {                                     /* prefetch both candidate buckets for the second nibble */
        for (int b = 0; b < 2; b++) {
            uint32_t nib = (((m->c0 << 1) | (uint32_t)b) & 15) * 0x5BD1E995u;
#ifdef KOLMO_CM_COMPACT
            if (v2 != 4) { for (int i = 1; i < nm; i++) { uint32_t idx = cm_prefetch_slot(m, i, ((m->c0 << 1) | (uint32_t)b) & 15, nib); __builtin_prefetch(&m->t[i][idx]); } continue; }
#endif
            uint32_t hv[CM_NSLOT]; m->kk->cm_hash(CM_HSRC(m, v2) + 1, nm - 1, nib, hv + 1);
            for (int i = 1; i < nm; i++) { uint32_t idx = v2 == 4 ? cm4_line(hv[i], m->nl[i]) : hv[i] >> m->shi[i]; __builtin_prefetch(&m->t[i][idx]); }
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
        if (CM_HON(1)) m->h[1] = cm_hmix((c4 & 0xff) + 0x1000000u);
        if (CM_HON(2)) m->h[2] = cm_hmix((c4 & 0xffff) + 0x2000000u);
        if (CM_HON(3)) m->h[3] = cm_hmix((c4 & 0xffffff) * 0x9E3779B1u + 3);
        if (CM_HON(4)) m->h[4] = cm_hmix(c4 * 0x9E3779B1u + 4);
        if (CM_HON(5)) m->h[5] = cm_hmix(cm_hmix(c4) + (m->c8 & 0xffff) * 0x85EBCA6Bu + 5);
        if (CM_HON(6)) m->h[6] = cm_hmix(m->word * 0x27D4EB2Fu + m->word1 * 0x165667B1u + 6);
        if (CM_HON(7)) m->h[7] = cm_hmix((c4 & 0xff00ff00u) * 0x9E3779B1u + 7);
        if (CM_HON(16)) m->h[16] = cm_hmix((c4 & 0xffff0000u) * 0x9E3779B1u + 16);                                     /* model 2: bytes 3-4 */
        if (CM_HON(17)) m->h[17] = cm_hmix((c4 & 0x0000ff00u) * 0x85EBCA6Bu + 17);                                     /* model 2: byte 2 */
        {   uint32_t pos = (uint32_t)m->pos, d = pos - m->rl_last[c];                 /* record length: a repeated byte-to-byte distance */
            if (m->rl_last[c] && d == m->rl_d[c] && d >= 2 && d < 65536) {
                if (d == m->rcand) { if (m->rcnt < 255) m->rcnt++; if (m->rcnt >= 8 && m->rlen != d) m->rlen = d; }
                else if (m->rcnt) m->rcnt--; else { m->rcand = d; m->rcnt = 1; }
            }
            m->rl_d[c] = d; m->rl_last[c] = pos;
            uint32_t r = m->rlen, ab = r && pos >= r ? m->buf[pos - r] : 0, ab2 = r && pos >= 2 * r ? m->buf[pos - 2 * r] : 0;
            if (CM_HON(18)) m->h[18] = cm_hmix((ab << 16 | ab2 << 8 | (r ? 1u : 0u)) * 0x9E3779B1u + r * 0x85EBCA6Bu + 18);
            if (CM_HON(19)) m->h[19] = cm_hmix((ab << 8 | (c4 & 0xff)) * 0xC2B2AE35u + r * 0x9E3779B1u + 19);
            uint32_t col = r ? pos % r : 0; if (col > 4095) col = 4095;
            if (CM_HON(20)) m->h[20] = cm_hmix((col << 8 | (c4 & 0xff)) * 0x2545F491u + r * 0x85EBCA6Bu + 20);
        }
#if CM_NM > 14
        {   uint32_t c1 = c4 >> 8 & 0xff, c2 = c4 >> 8 & 0xffff;                           /* what followed the previous context last time */
            m->ind1[c1] = (uint16_t)(m->ind1[c1] << 8 | c); m->ind2[c2] = (uint16_t)(m->ind2[c2] << 8 | c);
            if (CM_HON(14)) m->h[14] = cm_hmix(((uint32_t)m->ind1[c] << 8 | c) * 0x9E3779B1u + 14);
            if (CM_HON(15)) m->h[15] = cm_hmix(((uint32_t)m->ind2[c4 & 0xffff] << 16 | (c4 & 0xffff)) * 0x85EBCA6Bu + 15);
        }
#endif
#if CM_NM > 12
        if (CM_HON(12)) m->h[12] = cm_hmix(m->word * 0x2545F491u + 12);                                              /* current word */
        if (CM_HON(13)) m->h[13] = cm_hmix(m->word * 0x9E3779B1u + m->word1 * 0x85EBCA6Bu + m->word2 * 0xC2B2AE35u + 13);  /* word trigram */
#endif
#if CM_NM > 10
        if (c == '\n') { m->line1 = m->line0; m->line0 = m->pos; }
        {   int64_t col = m->pos - m->line0, plen = m->line0 - m->line1;
            uint32_t above = (col < plen) ? m->buf[m->line1 + col] : 0, above2 = (col + 1 < plen) ? m->buf[m->line1 + col + 1] : 0;
            uint32_t cc = (uint32_t)(col < 255 ? col : 255);
            if (CM_HON(10)) m->h[10] = cm_hmix((above << 8 | above2) * 0x9E3779B1u + (cc << 20) + 10);          /* column, bytes above */
            if (CM_HON(11)) m->h[11] = cm_hmix((above << 16 | (c4 & 0xffff)) * 0x85EBCA6Bu + 11);               /* byte above + order 2 */
        }
#endif
#if CM_NM > 8
        if (CM_HON(8)) m->h[8] = cm_hmix(cm_hmix(c4 * 0x9E3779B1u) + (m->c8 & 0xff) * 0x85EBCA6Bu + 8);          /* order 5 */
        if (CM_HON(9)) m->h[9] = cm_hmix(cm_hmix(c4 + 0x51ED27u) + (m->c8 & 0xffffff) * 0xC2B2AE35u + 9);     /* order 7 */
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
            if (CM_HON(25)) m->h[25] = cm_hmix(m->s_key * 0x9E3779B1u + c1 * 0x85EBCA6Bu + 25);
            if (CM_HON(26)) m->h[26] = cm_hmix(m->s_key * 0xC2B2AE35u + m->s_tok * 0x9E3779B1u + 26);
            if (CM_HON(27)) m->h[27] = cm_hmix((m->s_fidx << 16 | (c4 & 0xffff)) * 0x9E3779B1u + 27);
            if (CM_HON(28)) m->h[28] = cm_hmix((m->s_fidx + 1) * 0x85EBCA6Bu + m->s_tok * 0xC2B2AE35u + 28);
            if (CM_HON(29)) m->h[29] = cm_hmix((ab << 16 | ab1 << 8 | m->s_fidx) * 0x9E3779B1u + c1 * 0x2545F491u + 29);
            (void)kq;
        }
#ifdef KOLMO_IMPACT
        if (v2 == 4 && (CM_HON(30) || CM_HON(31))) {
            /* research (rc3w3 prior lane, docs/PRIORS.md on claude/rc3w3-prior-20261118): token shapes, what a regular
             * expression calls character classes: a digit, a letter (or '_', or a byte >= 128), a space or tab, or a
             * punctuation byte of its own; a token is a run of one class. Contexts: the classes of the last 8 tokens with
             * the position in the current one; the token index with the bytes at the same place of the same token of the
             * line before. Same code as the lane, on model 4's context slots. */
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
            if (m->k_tok < m->k_pn) {
                const int64_t a = m->s_pls + m->k_pst[m->k_tok] + (m->k_cls ? m->k_kp + 1 : 0);
                if (a >= m->s_pls && a < m->s_ls) { a0 = m->buf[a]; a1 = a + 1 < m->s_ls ? m->buf[a + 1] : 0; }
            }
            m->h[30] = cm_hmix((uint32_t)(m->k_sr & 0xffffffffffffull) * 0x9E3779B1u + (uint32_t)(m->k_sr >> 32 & 0xffff) * 0x85EBCA6Bu + kq * 0xC2B2AE35u + 30);
            m->h[31] = cm_hmix((a0 << 16 | a1 << 8 | kq) * 0x9E3779B1u + (m->k_tok << 8 | (c4 & 0xff)) * 0x2545F491u + 31);
        }
#endif
        if (v2 >= 3 && (CM_HON(21) || CM_HON(22))) {          /* model 3: orders 12 and 20 */
            const uint8_t *bf = m->buf + m->pos; uint32_t hh = 0; const int lim = m->pos < 20 ? (int)m->pos : 20;
            m->h[21] = 21; m->h[22] = 22;
            for (int k = 1; k <= lim; k++) { hh = (hh + bf[-k] + 1) * 0x2F0B3A55u; if (k == 12) m->h[21] = cm_hmix(hh + 21); if (k == 20) m->h[22] = cm_hmix(hh + 22); }
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
        if (v2 >= 4) for (int k = 1; k < nm; k++) m->hc[k] = m->h[m->mid[k]];
        uint32_t hv[CM_NSLOT];
#ifdef KOLMO_CM_COMPACT
        if (v2 == 4)
#endif
        m->kk->cm_hash(CM_HSRC(m, v2) + 1, nm - 1, 0, hv + 1);
        for (int i = 1; i < nm; i++) {                        /* prefetch the buckets for the first nibble */
#ifdef KOLMO_CM_COMPACT
            uint32_t idx = v2 != 4 ? cm_prefetch_slot(m, i, 0, 0) : cm4_line(hv[i], m->nl[i]);
#else
            uint32_t idx = v2 == 4 ? cm4_line(hv[i], m->nl[i]) : hv[i] >> m->shi[i];
#endif
            __builtin_prefetch(&m->t[i][idx]); if (v2 != 4) __builtin_prefetch(&m->t[i][idx ^ 1]);
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
                        int l = cm_match_len(m->buf, cand, m->pos, cand < 400 ? (int)cand : 400);
                        if (l > best) { best = l; bc = cand; if (l >= 400) break; }
                        cand = v2 != 4 || !m->mwin ? m->mprev[cand] : m->pos - cand < m->mwin ? m->mprev[cand & (m->mwin - 1)] : 0;
                    }
                    if (best >= 6) { m->mlen = best; m->mptr = bc; found = 1; }
                } else if (cand > 0) {
                    int l = cm_match_len(m->buf, cand, m->pos, cand < 400 ? (int)cand : 400);
                    if (l >= 6) { m->mlen = l; m->mptr = cand; found = 1; }
                }
                if (v2 >= 3 && !found && m->mrec > 0 && m->mptr + 1 < m->pos) { m->mptr++; m->mlen = 1; }   /* carry on past the difference */
            }
            if (v2 >= 3) m->mprev[v2 == 4 && m->mwin ? m->pos & (m->mwin - 1) : m->pos] = m->mt[hh];
            m->mt[hh] = (uint32_t)m->pos;
            if (m->mlen && m->mptr >= m->pos) m->mlen = 0;
        }
        if (v2 >= 3) {                                        /* model 3: the byte the match model expects, with orders 1 and 2 */
            uint32_t eb = m->mlen ? m->buf[m->mptr] : 256, lq = m->mlen == 0 ? 0 : m->mlen < 8 ? 1 : m->mlen < 12 ? 2 : m->mlen < 16 ? 3 : m->mlen < 24 ? 4 : m->mlen < 32 ? 5 : m->mlen < 64 ? 6 : 7;
            if (CM_HON(23)) m->h[23] = cm_hmix((eb << 3 | lq) * 0x9E3779B1u + (m->c4 & 0xff) * 0x85EBCA6Bu + 23);
            if (CM_HON(24)) m->h[24] = cm_hmix((eb << 3 | lq) * 0xC2B2AE35u + (m->c4 & 0xffff) * 0x9E3779B1u + 24);
        }
        if (v2 >= 4) for (int k = 1; k < m->nmod; k++) m->hc[k] = m->h[m->mid[k]];   /* h[23], h[24] are new */
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
/* The decoder consumes exactly the bytes the encoder wrote (4 at the start for the 4 it writes at the end, then one per
 * shift on both sides), so a valid stream never reads past cn. Past it, the decoder reads 0xff; more than CM_TAIL_SLACK
 * such bytes means the stream is truncated or forged, and decoding stops (-1) instead of modelling padding until the
 * claimed size, which for a forged size costs the full decode time and memory of that size. */
#define CM_TAIL_SLACK 8
static inline __attribute__((always_inline)) int cm_dec_loop(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, const int v2) {
    (void)out;
    cm_ac ac = {0, 0xffffffffu}; uint32_t code = 0; size_t ip = 4, pad = 0;   /* code: the next 32 bits of the stream */
    for (int k = 0; k < 4; k++) code = (code << 8) | (ip < cn ? in[ip++] : (pad++, 0xffu));
    for (size_t i = 0; i < n; i++) {
        if (pad > CM_TAIL_SLACK) return -1;
        for (int b = 7; b >= 0; b--) {
            int p = cm_predict(m, v2);
            uint32_t mid = cm_ac_mid(&ac, p);
            int y = code <= mid;
            if (y) ac.high = mid; else ac.low = mid + 1;
            cm_update(m, y, v2);
            for (; cm_ac_settled(&ac); cm_ac_shift(&ac)) code = (code << 8) | (ip < cn ? in[ip++] : (pad++, 0xffu));
        }
    }
    return pad > CM_TAIL_SLACK ? -1 : 0;
}
/* The coding loops are instantiated once per model and, on x86-64 builds for the baseline ISA, a second time for AVX2
 * (the mixer dot products and weight updates vectorize; integer arithmetic, so both write the same bytes), chosen at
 * run time by kolmo_have_avx2(). */
#define CM_LOOPS(SUF, ATTR) \
    ATTR __attribute__((noinline)) static int cm_enc##SUF(cm_t *m, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *po, int v2) { \
        switch (v2) { case 0: return cm_enc_loop(m, in, n, out, cap, po, 0); case 1: return cm_enc_loop(m, in, n, out, cap, po, 1); \
                      case 2: return cm_enc_loop(m, in, n, out, cap, po, 2); case 3: return cm_enc_loop(m, in, n, out, cap, po, 3); \
                      default: return cm_enc_loop(m, in, n, out, cap, po, 4); } } \
    ATTR __attribute__((noinline)) static int cm_dec##SUF(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, int v2) { \
        switch (v2) { case 0: return cm_dec_loop(m, in, cn, out, n, 0); case 1: return cm_dec_loop(m, in, cn, out, n, 1); \
                      case 2: return cm_dec_loop(m, in, cn, out, n, 2); case 3: return cm_dec_loop(m, in, cn, out, n, 3); \
                      default: return cm_dec_loop(m, in, cn, out, n, 4); } }
CM_LOOPS(_gen, )
#if defined(KOLMO_X86_DISPATCH) && !defined(__AVX2__) && !defined(KOLMO_CM_NO_DISPATCH)
CM_LOOPS(_avx2, KOLMO_TARGET_AVX2)
static int cm_enc_run(cm_t *m, const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t *po, int v2) {
    return kolmo_have_avx2() ? cm_enc_avx2(m, in, n, out, cap, po, v2) : cm_enc_gen(m, in, n, out, cap, po, v2);
}
static int cm_dec_run(cm_t *m, const uint8_t *in, size_t cn, uint8_t *out, size_t n, int v2) {
    return kolmo_have_avx2() ? cm_dec_avx2(m, in, cn, out, n, v2) : cm_dec_gen(m, in, cn, out, n, v2);
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
static size_t cm_compress_tm(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, cm_tune_t tune, uint32_t mask);
static size_t cm_compress_mx(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap) { return cm_compress_t(in, n, out, cap, model, bbcap, (cm_tune_t){0, 0, 0}); }
static size_t cm_compress_m(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model) { return cm_compress_mx(in, n, out, cap, model, 0); }
/* bbcap: at most 2^bbcap buckets per model (0: no cap beyond cm_bits_for); tune: learning settings (models 2 and 3;
 * {0, 0, 0} writes the stream every earlier encoder wrote) */
static size_t cm_compress_t(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, cm_tune_t tune) {
    return cm_compress_tm(in, n, out, cap, model, bbcap, tune, 0);
}
/* Model 4 (stream header model nibble 9): model 3's contexts restricted to those in mask (bits 1..29), 16-byte buckets
 * of bit-history states, the 16-bit mixer, exact logistic tables and learning settings (SPEC.md, "Model 4"):
 * size u32 (flags 8) | 9 << 4 | size shift - 2 | tune b0 b1 b2 | mask u32 LE | coded bytes */
/* model 4's prior for a context set: the binary one for masks that hold the record contexts and not the structure ones */
static uint8_t cm4_prior_for(uint32_t mask) { return (mask >> 18 & 1) && !(mask >> 29 & 1) ? 2 : 1; }
static size_t cm_compress_m4(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int bbcap, cm_tune_t tune, uint32_t mask) {
    if (cap < 16 || n >= ((size_t)1 << 28) || (mask & ~CM_MASK3) || tune.b2 > CM4_B2MAX) return 0;
    int bb = cm4_shift_for(n, mask); (void)bbcap;
    uint8_t *hist = malloc(n + 8); cm_t *m = hist ? cm_new_xm(hist, bb, 4, cm4_chain(n), 1, mask, n) : NULL;
    if (!m) { free(hist); return 0; }
    if (tune.b2) tune.b2 = cm4_prior_for(mask);
#ifdef KOLMO_IMPACT
    if (tune.b2) tune.b2 = cm_impact_b2(in, n, tune.b2);   /* research: a per-kind prior (KOLMO_CM_KPRIOR) */
#endif
    cm_set_tune(m, tune);
    uint32_t n32 = (uint32_t)n | (uint32_t)8 << 28; memcpy(out, &n32, 4); out[4] = (uint8_t)(9 << 4 | (bb - 2));
    out[5] = tune.b0; out[6] = tune.b1; out[7] = tune.b2;
    for (int k = 0; k < 4; k++) out[8 + k] = (uint8_t)(mask >> 8 * k);
    size_t o = 4; int r = cm_enc_run(m, in, n, out + 8, cap - 8, &o, 4);
    cm_free(m); free(hist);
    return r ? 0 : o + 8;
}
static size_t cm_compress_tm(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, cm_tune_t tune, uint32_t mask) {
    if (model == 4) return cm_compress_m4(in, n, out, cap, bbcap, tune, mask);
    if (cap < 8 || n > 0xFFFFFFF0u) return 0;
    int small = n < ((size_t)1 << 28), v2 = small && model > 1 ? (model == 3 ? 3 : 1) : 0;
    const int tuned = v2 && (tune.b0 | tune.b1 | tune.b2);
    /* exact logistic tables: model 3 always (its header has the model byte anyway); models 1 and 2 from 64 KiB, below
     * which the extra header byte can cost more than the better tables gain */
    int lx = small && (v2 == 3 || n >= ((size_t)1 << 16) || tuned);   /* a tuned stream's header always says exact tables */
    int bb = small ? cm_bits_for(n) : CM_BB;
    if (small && bbcap >= 16 && bb > bbcap) bb = bbcap;
    uint8_t *hist = malloc(n + 8); cm_t *m = hist ? cm_new_x(hist, bb, v2, v2 == 3 ? (n + 8) * 4 : 0, lx) : NULL;
    if (!m) { free(hist); return 0; }
    size_t o = 4; int r;
    if (tuned) {              /* extended header with learning settings: nibble 8 | 4 | model, then the two tune bytes */
        cm_set_tune(m, tune);
        uint32_t n32 = (uint32_t)n | (uint32_t)8 << 28; memcpy(out, &n32, 4); out[4] = (uint8_t)((v2 == 3 ? 15 : 14) << 4 | (bb - 15));
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
    cm_free(m); free(hist);
    return r ? 0 : o;
}
static size_t cm_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { return cm_compress_m(in, n, out, cap, 2); }

/* -x: learning settings chosen per stream by a race (docs/TWOPASS.md). Four presets, each with the shared prior
 * (byte 2 = 1), code the stream's first CM_RACE_SAMPLE bytes; the smallest wins (the first on a tie) and codes the
 * whole stream. A stream no longer than the sample is coded whole by every preset and the smallest is kept; a stream
 * longer than CM_RACE_MAX takes the first preset without a race. Presets:
 * the prior with the default rates; and three settings that the full search (49 trials per file) found most often on
 * text, logs, tables, numbers and images: faster APMs, a slower match-model counter, more weight on the APMs, a
 * faster-settling or slower mixer. The choice depends on the stream's bytes only; threads only run the trials side
 * by side. Decoding costs the same as before. */
#ifndef CM_RACE_SAMPLE
#define CM_RACE_SAMPLE (256u << 10)
#endif
#ifndef CM_RACE_MAX
#define CM_RACE_MAX (16u << 20)  /* longer streams take the first preset (the prior, default rates) without a race */
#endif
enum { CM_NPRESET = 4 };
static const cm_tune_t cm_presets[CM_NPRESET] = {{0x00, 0x00, 1}, {0x1c, 0xa5, 1}, {0x52, 0x04, 1}, {0xdc, 0xb4, 1}};
static cm_tune_t cm4_tune = {0xdc, 0xb4, 1};   /* model 4's learning settings (research harnesses may change them) */
typedef struct { const uint8_t *in; size_t n, cap, z; uint8_t *out; int model, bbcap; cm_tune_t t; kmem_ctx ctx; uint32_t mask; } cm_race_job;
static void *cm_race_run(void *a) { cm_race_job *j = a; j->z = j->out ? cm_compress_tm(j->in, j->n, j->out, j->cap, j->model, j->bbcap, j->t, j->mask) : 0; return NULL; }
/* a preset on a thread of its own: the caller reserved its tables (kmem_try_reserve), so the thread is a covered unit of
 * work, and it codes under the caller's memory context (the explicit cap sizes the tables: same bytes either way) */
static void *cm_race_thread(void *a) { cm_race_job *j = a; kmem_ctx_set(j->ctx); kmem_tls_job = 1; return cm_race_run(j); }
static uint64_t cm_need_bound(size_t n);
static uint64_t cm4_need_bound(size_t n, uint32_t mask);
/* model 4: mask, the stream's contexts (CM4_MASK_TEXT or CM4_MASK_BIN); streams of 2^28 bytes and more take model 1 */
static size_t cm_compress_raced(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int model, int bbcap, int threads, uint32_t mask) {
    if (model == 4 && n >= ((size_t)1 << 28)) model = 1;
    /* model 4 is not raced: the race gained it 0.05-0.15% on 1 MB samples for twice the encoding time at one thread;
     * one preset (the race's fourth) does nearly as well (benchmark/w4/cm/RESULTS.md) */
    if (model == 4) return cm_compress_tm(in, n, out, cap, 4, bbcap, cm4_tune, mask);
    if (model < 2 || n >= ((size_t)1 << 28) || n < 64) return cm_compress_mx(in, n, out, cap, model, bbcap);
    /* a prefix tells how the rates do early, not over tens of MB: on 100 MB of doubles the preset that won on the first
     * 256 KiB (slower mixer, faster rate decay) was 1.2% larger over the whole stream than the default rates */
    if (n > CM_RACE_MAX) return cm_compress_tm(in, n, out, cap, model, bbcap, cm_presets[0], mask);
    const size_t L = n <= CM_RACE_SAMPLE ? n : CM_RACE_SAMPLE;
    cm_race_job j[CM_NPRESET]; pthread_t th[CM_NPRESET]; int started[CM_NPRESET] = {0};
    for (int k = 0; k < CM_NPRESET; k++) {
        j[k] = (cm_race_job){in, L, L == n ? cap : L + L / 16 + 64, 0, NULL, model, bbcap, cm_presets[k], {0, 0, 0}, mask};
        j[k].out = malloc(j[k].cap);
    }
    const int par = threads > CM_NPRESET ? CM_NPRESET : threads > 1 ? threads : 1;
    const kmem_ctx ctx = kmem_ctx_get(); const uint64_t need = (model == 4 ? cm4_need_bound(L, mask) : cm_need_bound(L)) + (L + L / 16 + 64);
    uint64_t held[CM_NPRESET] = {0};
    for (int k0 = 0; k0 < CM_NPRESET; k0 += par) {           /* par trials at a time; the caller runs one of them */
        /* an extra thread only when its tables fit the memory budget now (optional work never waits: the caller may
         * hold a reservation the waiting thread needs); otherwise the caller codes that preset after its own */
        for (int k = k0 + 1; k < k0 + par && k < CM_NPRESET; k++) {
            j[k].ctx = ctx;
            if (!kmem_try_reserve(need, ctx.cap)) continue;
            held[k] = need; started[k] = !pthread_create(&th[k], NULL, cm_race_thread, &j[k]);
            if (!started[k]) { kmem_release(need); held[k] = 0; }
        }
        cm_race_run(&j[k0]);
        for (int k = k0 + 1; k < k0 + par && k < CM_NPRESET; k++) {
            if (started[k]) { pthread_join(th[k], NULL); kmem_release(held[k]); } else cm_race_run(&j[k]);
        }
    }
    int b = -1; for (int k = 0; k < CM_NPRESET; k++) if (j[k].z && (b < 0 || j[k].z < j[b].z)) b = k;
    size_t z = 0;
    if (b >= 0 && L == n) { memcpy(out, j[b].out, j[b].z); z = j[b].z; }
    for (int k = 0; k < CM_NPRESET; k++) free(j[k].out);
    if (L == n) return z ? z : cm_compress_tm(in, n, out, cap, model, bbcap, cm_presets[0], mask);
    return cm_compress_tm(in, n, out, cap, model, bbcap, b >= 0 ? cm_presets[b] : cm_presets[0], mask);
}

/* a stream written with learning settings (extended header, model nibble 14 or 15): its frame is version 11 */
static int cm_stream_tuned(const uint8_t *z, size_t zn) { return zn >= 8 && (z[3] >> 4) == 8 && ((z[4] >> 4) >= 14 || (z[4] >> 4) == 9); }
static int cm_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
#ifdef KOLMO_FUZZ_NO_CM   /* fuzz-only builds: skip context-mixing model setup (~300 ms per call; fuzz/campaign3) */
    (void)in; (void)cn; (void)out; (void)n; return -1;
#endif
    if (cn < 8) return -1;
    uint32_t n32; memcpy(&n32, in, 4);
    int bb = CM_BB, v2 = 0, lx = 0; cm_tune_t tune = {0, 0, 0}; uint32_t mask = 0;
    if (n < ((size_t)1 << 28)) {
        uint32_t f = n32 >> 28; n32 &= 0x0FFFFFFFu;
        if (f == 8 && cn >= 16 && in[4] >> 4 == 9) {      /* model 4: tune bytes and the context mask */
            v2 = 4;
            if (in[7] > CM4_B2MAX) return -1;
            tune.b0 = in[5]; tune.b1 = in[6]; tune.b2 = in[7];
            for (int k = 0; k < 4; k++) mask |= (uint32_t)in[8 + k] << 8 * k;
            if (mask & ~CM_MASK3) return -1;
            lx = 1; bb = (in[4] & 15) + 2; in += 8; cn -= 8;
        } else if (f == 8) {                              /* extended header: model nibble 3 (legacy tables), 5, 6, 7 (exact), 14, 15 (exact, tuned) */
            int mb = cn < 9 ? 0 : in[4] >> 4;
            if (mb != 3 && mb != 5 && mb != 6 && mb != 7 && mb != 14 && mb != 15) return -1;
            lx = mb >= 5; v2 = (mb & 3) == 3 ? 3 : (mb & 3) == 2 ? 1 : 0; bb = (in[4] & 15) + 15;
            if (mb >= 14) {                               /* the three tune bytes; the coded bits start after them (cm_dec_loop skips 4) */
                if (cn < 12 || in[7] > 1) return -1;
                tune.b0 = in[5]; tune.b1 = in[6]; tune.b2 = in[7]; in += 3; cn -= 3;
            }
            in++; cn--;
        }
        else if (f) { bb = (int)(f & 7) + 15; v2 = (int)(f >> 3); }
    }
    if (n32 != n || (v2 != 4 && (bb < 16 || bb > 22))) return -1;
    cm_t *m = cm_new_xm(out, bb, v2, v2 == 4 ? cm4_chain(n) : v2 >= 3 ? (n + 8) * 4 : 0, lx, mask, n); if (!m) return -1;
    cm_set_tune(m, tune);
    int r = cm_dec_run(m, in, cn, out, n, v2 >= 3 ? v2 : v2 ? 1 : 0);
    cm_free(m);
    return r;
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
        if (f == 8 && cn >= 16 && in[4] >> 4 == 9) {      /* model 4 */
            uint32_t mask = 0; for (int k = 0; k < 4; k++) mask |= (uint32_t)in[8 + k] << 8 * k;
            if (in[7] > CM4_B2MAX || (mask & ~CM_MASK3)) return 0;
            return cm_mem4(n, (in[4] & 15) + 2, mask) + cm4_chain(n);
        }
        if (f == 8) {
            int mb = cn < 9 ? 0 : in[4] >> 4;   /* the models cm_decompress accepts, the tuned ones (14, 15) included */
            if (mb != 3 && mb != 5 && mb != 6 && mb != 7 && mb != 14 && mb != 15) return 0;
            if (mb >= 14 && (cn < 12 || in[7] > 1)) return 0;
            v2 = (mb & 3) == 3 ? 3 : (mb & 3) == 2 ? 1 : 0; bb = (in[4] & 15) + 15;
        } else if (f) { bb = (int)(f & 7) + 15; v2 = (int)(f >> 3); }
    }
    if (bb < 16 || bb > 22) return 0;
    return cm_mem_for(bb, v2) + (v2 == 3 ? ((uint64_t)n + 8) * 4 : 0);
}
/* the largest decode working set a codec-7 stream of n bytes can have under the current sizing rules (graph leaves) */
static uint64_t cm_need_bound(size_t n) {
    int bb = n < ((size_t)1 << 28) ? cm_bits_for(n) : CM_BB; uint64_t a = cm_mem_for(bb, 3) + ((uint64_t)n + 8) * 4;
    if (n < ((size_t)1 << 28)) { uint64_t b = cm4_need_bound(n, CM4_MASK_TEXT); if (b > a) a = b; }
    return a;
}
/* model 4 with these contexts, as the encoder sizes it now */
static uint64_t cm4_need_bound(size_t n, uint32_t mask) { return cm_mem4(n, cm4_shift_for(n, mask), mask) + cm4_chain(n); }

/* Codec 7 table (kolmo builds only: tests/test_cm_compact.c includes this file on its own). Model 3 where it gains: text. */
#ifdef KOLMO_CODEC_REGISTRY
static size_t cm_bound(size_t n) { return n + n / 16 + 64; }
#ifdef KOLMO_IMPACT
/* research: KOLMO_CM_LEX=1 adds the token-class contexts (30, 31) to text streams, =2 to every stream */
static uint32_t cm4_mask_for(const uint8_t *in, size_t n) {
    const char *e = getenv("KOLMO_CM_LEX"); const int lex = e ? atoi(e) : 0, txt = looks_text(in, n);
    return (txt ? CM4_MASK_TEXT : CM4_MASK_BIN) | ((lex == 1 && txt) || lex == 2 ? 0xC0000000u : 0);
}
#else
static uint32_t cm4_mask_for(const uint8_t *in, size_t n) { return looks_text(in, n) ? CM4_MASK_TEXT : CM4_MASK_BIN; }
#endif
/* the planner's probe: model 4 with six contexts (orders 3 and 4, word, column, current word, field and byte above), about
 * three times as fast, to compare a transform with the plain bytes on a sample */
#define CM4_MASK_PROBE 0x2000145cu
static size_t cm_compress_probe(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    return n < ((size_t)1 << 28) ? cm_compress_tm(in, n, out, cap, 4, 0, cm4_tune, CM4_MASK_PROBE) : cm_compress_m(in, n, out, cap, 1);
}
/* model 4 as codec 7 codes a stream at -x: the coder's estimate for the planner's samples */
static size_t cm_compress_auto(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    return n < ((size_t)1 << 28) ? cm_compress_tm(in, n, out, cap, 4, 0, cm4_tune, cm4_mask_for(in, n)) : cm_compress_m(in, n, out, cap, 1);
}
static size_t cm_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    /* level 1: model 1, as written before; otherwise model 3 for text, 2 for the rest, with the raced settings */
    return level == 1 ? cm_compress_m(in, n, out, cap, 1) : cm_compress_raced(in, n, out, cap, 4, 0, x ? x->threads : 1, cm4_mask_for(in, n));
}
/* Ultra+ competes only on raw text already won by CM4. Exchange two full and two quarter-sized
 * context tables for one full and three half-sized tables. The match, prior and wire model stay v12.
 * Reserve no more model memory than the already-admitted baseline and reuse the caller's scratch slot. */
static size_t cm4_plus_compete(const uint8_t *in, size_t n, uint8_t *alt, size_t cap,
                              const uint8_t *coded, size_t current) {
    enum { SAMPLE = 16 << 10 };
    const uint32_t mask = (CM4_MASK_TEXT & ~((1u << 10) | (1u << 15) | (1u << 27) | (1u << 28)))
                          | (1u << 2) | (1u << 16) | (1u << 17) | (1u << 23);
    if (n < (1u << 20) || n >= (1u << 28) || current < 16 || (coded[4] >> 4) != 9 || cap < 2 * SAMPLE)
        return 0;
    const uint8_t *m = coded + 8;
    uint32_t encoded_mask = (uint32_t)m[0] | (uint32_t)m[1] << 8 | (uint32_t)m[2] << 16 | (uint32_t)m[3] << 24;
    if (encoded_mask != CM4_MASK_TEXT || cm4_need_bound(n, mask) > cm_stream_need(coded, current, n))
        return 0;

    for (size_t part = 1; part <= 3; part += 2) {
        const uint8_t *sample = in + part * (n - SAMPLE) / 4;
        size_t base = cm_compress_m4(sample, SAMPLE, alt, 2 * SAMPLE, 0, cm4_tune, CM4_MASK_TEXT);
        size_t trial = cm_compress_m4(sample, SAMPLE, alt, 2 * SAMPLE, 0, cm4_tune, mask);
        if (!base || !trial || trial * 100 > base * 99) return 0;
    }

    /* The range coder needs tail room beyond its reported payload, so compare sizes after encoding. */
    size_t z = cm_compress_m4(in, n, alt, cap, 0, cm4_tune, mask);
    return z && z * 100 <= current * 99 ? z : 0;
}
static int cm_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return cm_decompress(in, cn, out, n); }
static const kolmo_codec codec_cm = { C_CM, "cm", cm_bound, cm_codec_compress, cm_codec_decompress, NULL };
#endif
