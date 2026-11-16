/* Codec 19: audio (kaud, container version 11). 16-bit integer PCM, mono or stereo.
 *
 * Ported from the research tool src/experimental/kaud.c (branch claude/kaud-20261116 at 3d4b7ba, merged here;
 * description and measurements in src/experimental/KAUD.md). The tool is built with -fopenmp-simd, which lets the
 * encoder's choices depend on the CPU's vector width; the library port is built without it, so its output is the
 * same on every CPU (the decoded bytes never depended on it). Changes for the library: the command line and the self-test are
 * gone, every file-scope name carries the prefix ka_ (the codecs are still included into src/kolmo.c), the
 * probability adaptation cap is fixed at 7 (the stream field must say 7), the tables are built once, and the
 * encoder takes the sample layout from the caller instead of parsing a WAV header, so a Kolmo block that starts or
 * ends inside a file's header or trailer codes too (those bytes travel verbatim, as KAU1's head and tail).
 *
 *   profile 0 (fast):   per-frame stereo choice (L/R, L/S, R/S, M/S), least-squares LPC up to order 32 with
 *                       quantised coefficients and history carried across frames, partitioned Rice residuals.
 *   profile 1 (strong): profile 0's stereo and LPC stage, then a cascade of integer NLMS filters (256, 32 with
 *                       cross-channel taps, 16) and a sign-sign LMS stage, whose predictions are combined with
 *                       weights adapted by sign-sign LMS; the final residual coded by an adaptive binary range coder
 *                       with magnitude contexts.
 *
 * Everything on the decoding path is integer arithmetic. The LPC analysis in the encoder uses doubles (built with
 * -ffp-contract=off, no OpenMP), only to choose coefficients that are then quantised and transmitted.
 * Payload of a codec-19 block: one KAU1 stream (SPEC.md, "Codec 19"). Written from published descriptions (RFC 9639
 * for the Rice/LPC ideas, the NLMS and sign-sign LMS literature, a carry-propagating binary range coder); no code
 * was taken from other projects. */
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

#define KA_KAUD_VERSION 1
#define KA_MAXORD 32
#define KA_HIST 32             /* history samples kept in front of every frame */
#define KA_RICE_ESC 32         /* unary length that switches to a raw 25-bit value */
#define KA_MAXRES (1 << 24)    /* |residual| bound guaranteed by the encoder */

/* ------------------------------------------------------------------------------------------------ utils */

/* CRC-32C (Castagnoli), with the SSE4.2 instruction when the compiler targets it */
static uint32_t ka_crc_tab[256];
static void ka_crc_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0x82F63B78u ^ (c >> 1) : c >> 1;
        ka_crc_tab[i] = c;
    }
}
static uint32_t ka_crc32_buf(uint32_t c, const uint8_t *p, size_t n)
{
    c = ~c;
#if defined(__SSE4_2__) && defined(__x86_64__)
    for (; n >= 8; n -= 8, p += 8) { uint64_t v; memcpy(&v, p, 8); c = (uint32_t)__builtin_ia32_crc32di(c, v); }
    while (n--) c = __builtin_ia32_crc32qi(c, *p++);
#else
    while (n--) c = ka_crc_tab[(c ^ *p++) & 0xff] ^ (c >> 8);
#endif
    return ~c;
}

static uint32_t ka_rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t ka_rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void ka_wr32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void ka_wr16(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; }

static inline uint32_t ka_zz(int32_t v) { return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31); }
static inline int32_t ka_unzz(uint32_t u) { return (int32_t)(u >> 1) ^ -(int32_t)(u & 1); }
static inline int ka_bitlen(uint32_t v) { return v ? 32 - __builtin_clz(v) : 0; }

static void *ka_xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) abort();
    return p;
}
static void *ka_xcalloc(size_t n, size_t s)
{
    void *p = calloc(n ? n : 1, s ? s : 1);
    if (!p) abort();
    return p;
}


/* growable byte buffer */
typedef struct { uint8_t *p; size_t len, cap; } ka_Buf;
static void ka_buf_need(ka_Buf *b, size_t extra)
{
    if (b->len + extra <= b->cap) return;
    size_t c = b->cap ? b->cap : 4096;
    while (c < b->len + extra) c *= 2;
    b->p = realloc(b->p, c);
    if (!b->p) abort();
    b->cap = c;
}
static void ka_buf_add(ka_Buf *b, const void *d, size_t n) { if (!n) return; ka_buf_need(b, n); memcpy(b->p + b->len, d, n); b->len += n; }

/* ------------------------------------------------------------------------------------------- bit I/O */

typedef struct { ka_Buf b; uint64_t acc; int cnt; } KA_BW;   /* MSB first */

static inline void ka_bw_put(KA_BW *w, uint32_t v, int n)    /* n <= 32 */
{
    if (!n) return;
    w->acc = (w->acc << n) | (v & (uint32_t)((1ull << n) - 1));
    w->cnt += n;
    if (w->cnt >= 32) {
        w->cnt -= 32;
        uint32_t x = (uint32_t)(w->acc >> w->cnt);
        ka_buf_need(&w->b, 4);
        uint8_t *p = w->b.p + w->b.len;
        p[0] = x >> 24; p[1] = x >> 16; p[2] = x >> 8; p[3] = x;
        w->b.len += 4;
    }
}
static void ka_bw_flush(KA_BW *w)
{
    int pad = (8 - (w->cnt & 7)) & 7;
    ka_bw_put(w, 0, pad);
    while (w->cnt) {
        w->cnt -= 8;
        uint8_t x = (uint8_t)(w->acc >> w->cnt);
        ka_buf_add(&w->b, &x, 1);
    }
}
static inline void ka_bw_rice(KA_BW *w, uint32_t u, int k)
{
    uint32_t q = u >> k;
    if (q < KA_RICE_ESC) {
        ka_bw_put(w, 1, (int)q + 1);
        ka_bw_put(w, u, k);
    } else {
        ka_bw_put(w, 0, KA_RICE_ESC);
        ka_bw_put(w, u, 25);
    }
}

typedef struct { const uint8_t *p, *start, *end; uint64_t cache; int cnt; } KA_BR;

static void ka_br_init(KA_BR *r, const uint8_t *p, size_t n) { r->p = r->start = p; r->end = p + n; r->cache = 0; r->cnt = 0; }
static inline void ka_br_fill(KA_BR *r)
{
    while (r->cnt <= 56) {
        uint64_t b = r->p < r->end ? *r->p : 0;
        r->p++;                           /* may run past the end; checked by ka_br_overrun() */
        r->cache |= b << (56 - r->cnt);
        r->cnt += 8;
    }
}
static inline uint32_t ka_br_get(KA_BR *r, int n)  /* n <= 32 */
{
    if (!n) return 0;
    if (r->cnt < n) ka_br_fill(r);
    uint32_t v = (uint32_t)(r->cache >> (64 - n));
    r->cache <<= n;
    r->cnt -= n;
    return v;
}
static inline uint32_t ka_br_rice(KA_BR *r, int k)
{
    if (r->cnt < 33) ka_br_fill(r);
    int z = r->cache ? __builtin_clzll(r->cache) : 64;
    if (z >= KA_RICE_ESC) {
        r->cache <<= KA_RICE_ESC; r->cnt -= KA_RICE_ESC;
        return ka_br_get(r, 25);
    }
    r->cache <<= z + 1; r->cnt -= z + 1;
    return ((uint32_t)z << k) | ka_br_get(r, k);
}
static int ka_br_overrun(const KA_BR *r) { return (size_t)(r->p - r->start) * 8 - (size_t)r->cnt > (size_t)(r->end - r->start) * 8; }

/* ------------------------------------------------------------------------------------ range coder */
/* Binary range coder with 32-bit range, 64-bit low and carry propagation through a cached byte. */

typedef struct { ka_Buf b; uint64_t low; uint32_t range; uint8_t cache; uint64_t pending; } KA_RCE;
typedef struct { const uint8_t *p, *end; uint32_t code, range; int over; } KA_RCD;

#define KA_PBITS 16
#define KA_TOP (1u << 24)

static void ka_rce_init(KA_RCE *e) { memset(e, 0, sizeof *e); e->range = 0xFFFFFFFFu; e->pending = 1; }
static inline void ka_rce_shift(KA_RCE *e)
{
    if ((uint32_t)e->low < 0xFF000000u || (e->low >> 32)) {
        uint8_t carry = (uint8_t)(e->low >> 32);
        ka_buf_need(&e->b, e->pending + 1);
        uint8_t c = e->cache;
        do { e->b.p[e->b.len++] = (uint8_t)(c + carry); c = 0xFF; } while (--e->pending);
        e->cache = (uint8_t)(e->low >> 24);
    }
    e->pending++;
    e->low = (e->low & 0x00FFFFFFu) << 8;
}
static void ka_rce_finish(KA_RCE *e) { for (int i = 0; i < 5; i++) ka_rce_shift(e); }

/* probability of a 1 bit in 16 bits with a hit count: the adaptation shift grows with log2 of the
 * count (close to frequency counting while the context is young), up to ka_g_rate_cap */
typedef struct { uint16_t p; uint16_t n; } ka_Prob;
static const int ka_g_rate_cap = 7;   /* codec 19: fixed (the stream field must be 7) */
static const int ka_g_force_stereo = -1;
static inline void ka_prob_init(ka_Prob *p) { p->p = 1 << 15; p->n = 0; }
static uint8_t ka_shtab[256];          /* ka_shtab[n] = min(ka_bitlen(n + 2), ka_g_rate_cap) */
static void ka_shtab_init(void) { for (int n = 0; n < 256; n++) { int b = 32 - __builtin_clz((uint32_t)n + 2); ka_shtab[n] = (uint8_t)(b > ka_g_rate_cap ? ka_g_rate_cap : b); } }
static inline void ka_prob_upd(ka_Prob *p, int bit)
{
    int sh = ka_shtab[p->n];
    p->n += p->n < 255;
    if (bit) p->p += (65536 - p->p) >> sh; else p->p -= p->p >> sh;
    if (p->p < 32) p->p = 32;
    if (p->p > 65536 - 32) p->p = 65536 - 32;
}
static inline void ka_rce_bit(KA_RCE *e, ka_Prob *pr, int bit)
{
    uint32_t bound = (e->range >> KA_PBITS) * pr->p;
    if (bit) e->range = bound; else { e->low += bound; e->range -= bound; }
    while (e->range < KA_TOP) { e->range <<= 8; ka_rce_shift(e); }
    ka_prob_upd(pr, bit);
}
static inline void ka_rce_bypass(KA_RCE *e, uint32_t v, int n)
{
    while (n--) {
        e->range >>= 1;
        if ((v >> n) & 1) e->low += e->range;
        while (e->range < KA_TOP) { e->range <<= 8; ka_rce_shift(e); }
    }
}

static inline uint8_t ka_rcd_byte(KA_RCD *d) { if (d->p < d->end) return *d->p++; d->over++; return 0; }
static void ka_rcd_init(KA_RCD *d, const uint8_t *p, size_t n)
{
    d->p = p; d->end = p + n; d->over = 0; d->code = 0; d->range = 0xFFFFFFFFu;
    for (int i = 0; i < 5; i++) d->code = (d->code << 8) | ka_rcd_byte(d);
}
static inline int ka_rcd_bit(KA_RCD *d, ka_Prob *pr)
{
    uint32_t bound = (d->range >> KA_PBITS) * pr->p;
    int bit;
    if (d->code < bound) { d->range = bound; bit = 1; }
    else { d->code -= bound; d->range -= bound; bit = 0; }
    while (d->range < KA_TOP) { d->range <<= 8; d->code = (d->code << 8) | ka_rcd_byte(d); }
    ka_prob_upd(pr, bit);
    return bit;
}
static inline uint32_t ka_rcd_bypass(KA_RCD *d, int n)
{
    uint32_t v = 0;
    while (n--) {
        d->range >>= 1;
        uint32_t b = d->code >= d->range;
        if (b) d->code -= d->range;
        v = (v << 1) | b;
        while (d->range < KA_TOP) { d->range <<= 8; d->code = (d->code << 8) | ka_rcd_byte(d); }
    }
    return v;
}

/* ----------------------------------------------------------------------------------------- WAV I/O */

typedef struct {
    int ch;
    uint64_t n;            /* sample frames */
    size_t data_off;       /* first sample byte */
    size_t trl_off;        /* first byte after the coded samples */
} ka_Wav;

static const uint8_t ka_pcm_guid_tail[14] = { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };

static int ka_wav_parse(const uint8_t *b, size_t n, ka_Wav *w, const char **err)
{
    if (n < 12 || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) { *err = "not a RIFF/WAVE file"; return -1; }
    size_t p = 12;
    int have_fmt = 0;
    while (p + 8 <= n) {
        uint32_t sz = ka_rd32(b + p + 4);
        if (!memcmp(b + p, "fmt ", 4)) {
            if (sz < 16 || sz > n - p - 8) { *err = "bad fmt chunk"; return -1; }
            const uint8_t *f = b + p + 8;
            unsigned tag = ka_rd16(f), ch = ka_rd16(f + 2), align = ka_rd16(f + 12), bits = ka_rd16(f + 14);
            if (tag == 0xFFFE) {
                if (sz < 40 || ka_rd16(f + 18) != 16 || ka_rd16(f + 24) != 1 || memcmp(f + 26, ka_pcm_guid_tail, 14)) {
                    *err = "WAVE_FORMAT_EXTENSIBLE that is not 16-bit integer PCM"; return -1;
                }
            } else if (tag != 1) { *err = "not integer PCM"; return -1; }
            if (bits != 16) { *err = "only 16-bit samples are supported"; return -1; }
            if (ch != 1 && ch != 2) { *err = "only mono and stereo are supported"; return -1; }
            if (align != 2 * ch) { *err = "unexpected block align"; return -1; }
            if (have_fmt) { *err = "two fmt chunks"; return -1; }
            have_fmt = 1;
            w->ch = (int)ch;
        } else if (!memcmp(b + p, "data", 4)) {
            if (!have_fmt) { *err = "data chunk before fmt chunk"; return -1; }
            w->data_off = p + 8;
            size_t avail = n - w->data_off, dsz = sz < avail ? sz : avail;
            w->n = dsz / (size_t)(2 * w->ch);
            w->trl_off = w->data_off + (size_t)w->n * 2 * w->ch;
            return 0;
        }
        if (sz > n - p - 8) break;
        p += 8 + (size_t)sz + (sz & 1);
    }
    *err = "no data chunk";
    return -1;
}

/* --------------------------------------------------------------------------------------- options */

typedef struct {
    int profile;     /* 0 fast, 1 strong */
    int effort;      /* 0..3 encoder search */
    int frame;       /* samples per frame */
    int chunk_log2;  /* samples per independent chunk */
    int threads;
    /* profile 1 knobs (stored in the file so the decoder agrees) */
    int mu[3];       /* NLMS step sizes, in 1/4096 */
    int ss_step;     /* sign-sign LMS step */
    int lpc1;        /* max LPC order in profile 1 */
    int depth;       /* block split depth, -1 = from effort */
    int taps[3];     /* NLMS stage lengths */
    int cross[2];    /* cross-channel taps in stage 0 and stage 1 */
    int comb;        /* step of the sign-sign LMS combiner of stage predictions */
} ka_Opts;

/* -------------------------------------------------------------------------------------- LPC analysis */

typedef struct {
    int type;                 /* 0 constant, 1 verbatim, 2 LPC */
    int wasted;
    int32_t cval;
    int order, shift, kc;
    int32_t q[KA_MAXORD];
    int porder;
    uint8_t kp[256];          /* per-partition Rice parameter, 31 = raw with width wp[] */
    uint8_t wp[256];
    int vwidth;
    int64_t bits;
    int32_t *res;             /* residual, n values (owned by caller) */
} ka_Sub;

/* covariance matrix of a block and its Cholesky factorisation */
typedef struct {
    double C[KA_MAXORD + 1][KA_MAXORD + 1], row[KA_MAXORD + 1];
    double L[KA_MAXORD][KA_MAXORD], inv[KA_MAXORD], y[KA_MAXORD], err[KA_MAXORD + 1];
    int maxord, pmax;
} ka_Cov;

enum { KA_ST_CONST = 0, KA_ST_VERB = 1, KA_ST_LPC = 2 };

typedef struct {
    double *xd;               /* KA_HIST + frame */
    int32_t *xs;              /* shifted copy for wasted bits */
    uint32_t *acc;
    int32_t *tmp;             /* candidate residual */
    int64_t *acc64;
    ka_Cov local;
    int cap;
} ka_Work;

static void ka_work_init(ka_Work *w, int frame)
{
    w->cap = frame;
    w->xd = ka_xmalloc(sizeof(double) * (frame + KA_HIST));
    w->xs = ka_xmalloc(sizeof(int32_t) * (frame + KA_HIST));
    w->acc = ka_xmalloc(sizeof(uint32_t) * frame);
    w->tmp = ka_xmalloc(sizeof(int32_t) * frame);
    w->acc64 = ka_xmalloc(sizeof(int64_t) * frame);
}
static void ka_work_free(ka_Work *w) { free(w->xd); free(w->xs); free(w->acc); free(w->tmp); free(w->acc64); }

/* residual of an order-p predictor with coefficients q and shift s; uses wrap-around 32-bit sums */
static void ka_lpc_residual(const int32_t *x, int n, const int32_t *q, int p, int s, int32_t *e, uint32_t *acc)
{
    for (int t = 0; t < n; t++) acc[t] = 0;
    for (int j = 0; j < p; j++) {
        uint32_t c = (uint32_t)q[j];
        const int32_t *xx = x - 1 - j;
        for (int t = 0; t < n; t++) acc[t] += c * (uint32_t)xx[t];
    }
    for (int t = 0; t < n; t++) e[t] = (int32_t)((uint32_t)x[t] - (uint32_t)((int32_t)acc[t] >> s));
}

/* The predictor sum is computed modulo 2^32 by both encoder and decoder, so it may wrap: the decoder
 * repeats exactly the same arithmetic. The encoder only has to keep every residual below KA_MAXRES. */
static int ka_res_ok(const int32_t *e, int n)
{
    int bad = 0;
    for (int t = 0; t < n; t++) bad |= (e[t] >= KA_MAXRES) | (e[t] <= -KA_MAXRES);
    return !bad;
}

/* choose the partition order and Rice parameters for a residual; returns bits (estimate + exact refine) */
static int64_t ka_rice_plan(const int32_t *e, int n, ka_Sub *sb, int maxp)
{
    /* finest partitions */
    int pm = 0;
    while (pm < maxp && (n >> (pm + 1)) >= 16 && !(n & ((2 << pm) - 1))) pm++;
    int np = 1 << pm;
    int plen = n >> pm;
    uint64_t S[256];
    uint32_t M[256];
    for (int i = 0; i < np; i++) {
        int a = i * plen, b = (i == np - 1) ? n : a + plen;
        uint64_t s = 0; uint32_t m = 0;
        for (int t = a; t < b; t++) { uint32_t u = ka_zz(e[t]); s += u; m |= u; }
        S[i] = s; M[i] = m;
    }
    int64_t best = INT64_MAX;
    int bestp = 0;
    uint8_t bk[256], bw[256], ck[256], cw[256];
    for (int p = pm; p >= 0; p--) {
        int cnt = 1 << p;
        int len = n >> p;
        int64_t tot = 4;
        for (int i = 0; i < cnt; i++) {
            int cn = (i == cnt - 1) ? n - i * len : len;
            uint64_t s = S[i]; uint32_t m = M[i];
            /* estimate: n(k+1) + sum(u>>k), with sum(u>>k) ~ (s>>k) - n/2 for k>0 */
            int64_t bb = INT64_MAX; int kb = 0;
            int k0 = 0;
            if (cn) { uint64_t mean = s / (uint64_t)cn; k0 = ka_bitlen((uint32_t)(mean > 0xFFFFFFFFu ? 0xFFFFFFFFu : mean)); }
            for (int k = k0 > 2 ? k0 - 2 : 0; k <= k0 + 1 && k <= 30; k++) {
                int64_t c = (int64_t)cn * (k + 1) + (int64_t)(s >> k) - (k ? cn / 2 : 0);
                if (c < bb) { bb = c; kb = k; }
            }
            int wbits = ka_bitlen(m);
            int64_t raw = 5 + (int64_t)cn * wbits;
            if (raw <= bb) { bb = raw; kb = 31; }
            ck[i] = (uint8_t)kb; cw[i] = (uint8_t)wbits;
            tot += 5 + bb;
        }
        if (tot < best) { best = tot; bestp = p; memcpy(bk, ck, cnt); memcpy(bw, cw, cnt); }
        /* merge to the next coarser level */
        if (p) for (int i = 0; i < (1 << (p - 1)); i++) { S[i] = S[2 * i] + S[2 * i + 1]; M[i] = M[2 * i] | M[2 * i + 1]; }
    }
    sb->porder = bestp;
    int cnt = 1 << bestp, len = n >> bestp;
    int64_t tot = 4;
    /* exact refinement of each Rice parameter */
    for (int i = 0; i < cnt; i++) {
        int a = i * len, b = (i == cnt - 1) ? n : a + len;
        { uint32_t mm = 0; for (int t = a; t < b; t++) mm |= ka_zz(e[t]); bw[i] = (uint8_t)ka_bitlen(mm); }
        if (bk[i] == 31) { sb->kp[i] = 31; sb->wp[i] = bw[i]; tot += 10 + (int64_t)(b - a) * bw[i]; continue; }
        int k0 = bk[i];
        int64_t cbest = INT64_MAX; int kb = k0;
        for (int k = k0 > 0 ? k0 - 1 : 0; k <= k0 + 1 && k <= 30; k++) {
            int64_t c = (int64_t)(b - a) * (k + 1);
            for (int t = a; t < b; t++) { uint32_t q = ka_zz(e[t]) >> k; c += q < KA_RICE_ESC ? (int64_t)q : 25 + KA_RICE_ESC - 1 - k; }
            if (c < cbest) { cbest = c; kb = k; }
        }
        int64_t raw = 5 + (int64_t)(b - a) * bw[i];
        if (raw < cbest) { sb->kp[i] = 31; sb->wp[i] = bw[i]; tot += 5 + raw; }
        else { sb->kp[i] = (uint8_t)kb; tot += 5 + cbest; }
    }
    return tot;
}

/* fast estimate used while searching (no exact refinement) */
static int64_t ka_rice_estimate(const int32_t *e, int n, int maxp)
{
    int pm = 0;
    while (pm < maxp && (n >> (pm + 1)) >= 16 && !(n & ((2 << pm) - 1))) pm++;
    int np = 1 << pm, plen = n >> pm;
    uint64_t S[256];
    for (int i = 0; i < np; i++) {
        int a = i * plen, b = (i == np - 1) ? n : a + plen;
        uint64_t s = 0;
        for (int t = a; t < b; t++) s += ka_zz(e[t]);
        S[i] = s;
    }
    int64_t best = INT64_MAX;
    for (int p = pm; p >= 0; p--) {
        int cnt = 1 << p, len = n >> p;
        int64_t tot = 4;
        for (int i = 0; i < cnt; i++) {
            int cn = (i == cnt - 1) ? n - i * len : len;
            uint64_t s = S[i];
            int k0 = cn ? ka_bitlen((uint32_t)((s / cn) > 0xFFFFFFFFu ? 0xFFFFFFFFu : s / cn)) : 0;
            int64_t bb = INT64_MAX;
            for (int k = k0 > 2 ? k0 - 2 : 0; k <= k0 + 1 && k <= 30; k++) {
                int64_t c = (int64_t)cn * (k + 1) + (int64_t)(s >> k) - (k ? cn / 2 : 0);
                if (c < bb) bb = c;
            }
            tot += 5 + bb;
        }
        if (tot < best) best = tot;
        if (p) for (int i = 0; i < (1 << (p - 1)); i++) S[i] = S[2 * i] + S[2 * i + 1];
    }
    return best;
}

static int ka_coef_bits(const int32_t *q, int p, int *kc_out)
{
    int best = 1 << 30, kb = 0;
    for (int k = 0; k < 16; k++) {
        int c = 0;
        for (int j = 0; j < p; j++) { uint32_t u = ka_zz(q[j]); uint32_t qq = u >> k; c += qq < KA_RICE_ESC ? (int)qq + 1 + k : KA_RICE_ESC + 25; }
        if (c < best) { best = c; kb = k; }
    }
    *kc_out = kb;
    return best;
}

/* ka_quantise real coefficients a[0..p) with `prec` significant bits */
static int ka_quantise(const double *a, int p, int prec, int32_t *q)
{
    double amax = 0;
    for (int j = 0; j < p; j++) if (fabs(a[j]) > amax) amax = fabs(a[j]);
    if (amax <= 0) { for (int j = 0; j < p; j++) q[j] = 0; return 0; }
    int ex;
    frexp(amax, &ex);                       /* amax < 2^ex */
    int s = prec - 1 - ex;
    if (s < 0) s = 0;
    if (s > 30) s = 30;
    double sc = ldexp(1.0, s), err = 0;
    for (int j = 0; j < p; j++) {
        double v = a[j] * sc + err;
        double r = floor(v + 0.5);
        if (r > 1e9) r = 1e9;
        if (r < -1e9) r = -1e9;
        q[j] = (int32_t)r;
        err = v - r;
    }
    return s;
}

/* Covariance (least-squares) matrix of a block: C[i][j] = sum_{t<n} x[t-i] x[t-j], built from one exact
 * integer row and the diagonal recurrence C[i+1][j+1] = C[i][j] + x[-1-i]x[-1-j] - x[n-1-i]x[n-1-j]. */

/* row C[0][j] of a block; xd is the same signal as doubles. Products and sums of 17-bit integers are
 * exact in double precision, so the vectorised reduction gives the same value in any order. */
__attribute__((optimize("fp-contract=fast"))) static void cov_row(const double *xd, int n, int maxord, double *row)
{
    for (int j = 0; j <= maxord; j++) {
        double s = 0;
        const double *b = xd - j;
#pragma omp simd reduction(+ : s)
        for (int t = 0; t < n; t++) s += xd[t] * b[t];
        row[j] = s;
    }
}
/* fill the matrix from row 0 (already in cv->C[0][*]) */
static void ka_cov_fill(const int32_t *x, int n, int maxord, ka_Cov *cv)
{
    cv->maxord = maxord;
    for (int j = 0; j <= maxord; j++) cv->C[j][0] = cv->C[0][j];
    for (int i = 0; i < maxord; i++)
        for (int j = i; j < maxord; j++) {
            int64_t d = (int64_t)x[-1 - i] * x[-1 - j] - (int64_t)x[n - 1 - i] * x[n - 1 - j];
            double v = cv->C[i][j] + (double)d;
            cv->C[i + 1][j + 1] = cv->C[j + 1][i + 1] = v;
        }
}
static int ka_clamp_order(int maxord, int n) { return maxord > n - 1 ? (n > 1 ? n - 1 : 0) : maxord; }
static void ka_cov_build(const int32_t *x, int n, int maxord, ka_Cov *cv)
{
    maxord = ka_clamp_order(maxord, n);
    for (int j = 0; j <= maxord; j++) {
        int64_t s = 0;
        for (int t = 0; t < n; t++) s += (int64_t)x[t] * x[t - j];
        cv->C[0][j] = (double)s;
    }
    ka_cov_fill(x, n, maxord, cv);
}

/* Cholesky factorisation C[1..p][1..p] = L L^T for every order at once, with y = L^-1 C[1..p][0].
 * err[p] = residual energy of order p. Fills cv->L, cv->inv, cv->y, cv->err, cv->pmax. */
static void ka_cov_solve(ka_Cov *cv)
{
    const int maxord = cv->maxord;
    double (*L)[KA_MAXORD] = cv->L, *y = cv->y, *inv = cv->inv, *err = cv->err;
    double ridge = 1e-9 * (cv->C[0][0] + 1.0) + 1e-3;
    err[0] = cv->C[0][0];
    int pmax = 0;
    for (int i = 0; i < maxord; i++) {
        double *li = L[i];
        for (int j = 0; j < i; j++) {
            double s = 0;
            const double *lj = L[j];
#pragma omp simd reduction(+ : s)
            for (int k = 0; k < j; k++) s += li[k] * lj[k];
            li[j] = (cv->C[i + 1][j + 1] - s) * inv[j];
        }
        double d = 0, t = 0;
#pragma omp simd reduction(+ : d, t)
        for (int k = 0; k < i; k++) { d += li[k] * li[k]; t += li[k] * y[k]; }
        d = cv->C[i + 1][i + 1] + ridge - d;
        if (d <= ridge * 1e-3) break;
        li[i] = sqrt(d);
        inv[i] = 1.0 / li[i];
        y[i] = (cv->C[i + 1][0] - t) * inv[i];
        err[i + 1] = err[i] - y[i] * y[i];
        if (err[i + 1] < 0) err[i + 1] = 0;
        pmax = i + 1;
    }
    cv->pmax = pmax;
}

/* real coefficients of order p (p <= cv->pmax) by back substitution L_p^T a = y */
static void ka_cov_coef(const ka_Cov *cv, int p, double *a)
{
    for (int i = p - 1; i >= 0; i--) {
        double s = cv->y[i];
        for (int k = i + 1; k < p; k++) s -= cv->L[k][i] * a[k];
        a[i] = s * cv->inv[i];
    }
}

#define KA_COEF_EST 13.0   /* estimated bits per transmitted coefficient */
#define KA_SUB_HDR 20.0    /* estimated subframe header + partition overhead */

static inline double ka_est_order(const double *err, int p, int n)
{
    double e = err[p] / n;
    if (e < 0.05) e = 0.05;
    return 0.5 * n * log2(e) + p * KA_COEF_EST;
}
static double ka_est_best(const double *err, int pmax, int n)
{
    double b = 1e300;
    for (int p = 0; p <= pmax; p++) { double c = ka_est_order(err, p, n); if (c < b) b = c; }
    return b + KA_SUB_HDR;
}

/* Find the best subframe for signal x (x[-KA_HIST..-1] valid). sb->res must hold n values.
 * cv, when not NULL, is the covariance of x over this block. */
static void ka_analyse_sub(ka_Work *w, const int32_t *x, int n, int effort, int maxord, ka_Sub *sb, const ka_Cov *cv)
{
    int isconst = 1;
    for (int t = 1; t < n; t++) if (x[t] != x[0]) { isconst = 0; break; }
    if (isconst) { sb->type = KA_ST_CONST; sb->cval = x[0]; sb->bits = 2 + 17; return; }

    uint32_t orv = 0;
    for (int t = 0; t < n; t++) orv |= (uint32_t)x[t];
    int wasted = orv ? __builtin_ctz(orv) : 0;
    if (wasted > 15) wasted = 15;
    const int32_t *xs = x;
    if (wasted) {
        for (int t = -KA_HIST; t < n; t++) w->xs[t + KA_HIST] = x[t] >> wasted;
        xs = w->xs + KA_HIST;
    }
    int hdr = 2 + 1 + (wasted ? 4 : 0) + 6;

    uint32_t m = 0;
    for (int t = 0; t < n; t++) m |= ka_zz(x[t]);
    int64_t vbits = 2 + 5 + (int64_t)n * ka_bitlen(m);

    if (!cv || wasted || cv->maxord < ka_clamp_order(maxord, n)) { ka_cov_build(xs, n, maxord, &w->local); ka_cov_solve(&w->local); cv = &w->local; }
    const double *err = cv->err;
    int pmax = cv->pmax;
    if (pmax > maxord) pmax = maxord;

    int ncand = effort <= 1 ? 1 : effort == 2 ? 3 : 5;
    int cand[8], nc = 0;
    {
        int used[KA_MAXORD + 1] = { 0 };
        for (int c = 0; c < ncand && c <= pmax; c++) {
            int b = -1;
            for (int p = 0; p <= pmax; p++) if (!used[p] && (b < 0 || ka_est_order(err, p, n) < ka_est_order(err, b, n))) b = p;
            if (b < 0) break;
            used[b] = 1; cand[nc++] = b;
        }
    }
    int precs[3] = { 15, 13, 12 };
    int nprec = effort >= 2 ? 3 : 1;
    int64_t best = INT64_MAX;
    int bo = 0, bs = 0, bk = 0;
    int32_t bq[KA_MAXORD];
    for (int c = 0; c < nc; c++) {
        int p = cand[c];
        for (int pi = 0; pi < nprec; pi++) {
            int32_t q[KA_MAXORD];
            int s = 0, kc = 0, cb = 0;
            if (p) {
                double a[KA_MAXORD];
                ka_cov_coef(cv, p, a);
                s = ka_quantise(a, p, precs[pi], q);
                cb = 5 + 4 + ka_coef_bits(q, p, &kc);
            } else if (pi) continue;
            ka_lpc_residual(xs, n, q, p, s, w->tmp, w->acc);
            if (!ka_res_ok(w->tmp, n)) continue;
            if (nc * nprec > 1) {
                int64_t bits = hdr + cb + ka_rice_estimate(w->tmp, n, 8);
                if (bits >= best) continue;
                best = bits;
            }
            bo = p; bs = s; bk = kc; memcpy(bq, q, sizeof(int32_t) * p);
            memcpy(sb->res, w->tmp, sizeof(int32_t) * n);
        }
    }
    sb->type = KA_ST_LPC;
    sb->wasted = wasted;
    sb->order = bo; sb->shift = bs; sb->kc = bk;
    memcpy(sb->q, bq, sizeof(int32_t) * bo);
    if (bo == 0) memcpy(sb->res, xs, sizeof(int32_t) * n);
    int cb = 0;
    if (bo) { int kc; cb = 5 + 4 + ka_coef_bits(bq, bo, &kc); }
    sb->bits = hdr + cb + ka_rice_plan(sb->res, n, sb, 8);
    if (vbits < sb->bits) { sb->type = KA_ST_VERB; sb->vwidth = ka_bitlen(m); sb->bits = vbits; }
}

/* ------------------------------------------------------------------------------- subframe bitstream */

static void ka_put_sub_header(KA_BW *w, const ka_Sub *sb)
{
    ka_bw_put(w, (uint32_t)sb->type, 2);
    if (sb->type == KA_ST_CONST) { ka_bw_put(w, (uint32_t)sb->cval & 0x1FFFF, 17); return; }
    if (sb->type == KA_ST_VERB) { ka_bw_put(w, (uint32_t)sb->vwidth, 5); return; }
    ka_bw_put(w, sb->wasted ? 1 : 0, 1);
    if (sb->wasted) ka_bw_put(w, (uint32_t)sb->wasted, 4);
    ka_bw_put(w, (uint32_t)sb->order, 6);
    if (sb->order) {
        ka_bw_put(w, (uint32_t)sb->shift, 5);
        ka_bw_put(w, (uint32_t)sb->kc, 4);
        for (int j = 0; j < sb->order; j++) ka_bw_rice(w, ka_zz(sb->q[j]), sb->kc);
    }
}

static void ka_put_sub(KA_BW *w, const ka_Sub *sb, const int32_t *x, int n)
{
    ka_put_sub_header(w, sb);
    if (sb->type == KA_ST_CONST) return;
    if (sb->type == KA_ST_VERB) { for (int t = 0; t < n; t++) ka_bw_put(w, ka_zz(x[t]), sb->vwidth); return; }
    ka_bw_put(w, (uint32_t)sb->porder, 4);
    int cnt = 1 << sb->porder, len = n >> sb->porder;
    for (int i = 0; i < cnt; i++) {
        int a = i * len, b = (i == cnt - 1) ? n : a + len;
        int k = sb->kp[i];
        ka_bw_put(w, (uint32_t)k, 5);
        if (k == 31) {
            int wd = sb->wp[i];
            ka_bw_put(w, (uint32_t)wd, 5);
            for (int t = a; t < b; t++) ka_bw_put(w, ka_zz(sb->res[t]), wd);
        } else {
            for (int t = a; t < b; t++) ka_bw_rice(w, ka_zz(sb->res[t]), k);
        }
    }
}

/* reconstruct x[0..n) from residual e with predictor (x[-KA_HIST..-1] valid), wrap-around sums */
static void ka_lpc_restore(int32_t *x, const int32_t *e, int n, const int32_t *q, int p, int s)
{
    if (p == 0) { memcpy(x, e, sizeof(int32_t) * n); return; }
    uint32_t c[KA_MAXORD] = { 0 };
    for (int j = 0; j < p; j++) c[j] = (uint32_t)q[j];
#define KA_RESTORE(P)                                                                     \
    for (int t = 0; t < n; t++) {                                                      \
        uint32_t a = 0;                                                                \
        for (int j = 0; j < P; j++) a += c[j] * (uint32_t)x[t - 1 - j];                \
        x[t] = (int32_t)((uint32_t)e[t] + (uint32_t)((int32_t)a >> s));                \
    }
    if (p <= 2) { KA_RESTORE(2) }
    else if (p <= 4) { KA_RESTORE(4) }
    else if (p <= 8) { KA_RESTORE(8) }
    else if (p <= 12) { KA_RESTORE(12) }
    else if (p <= 16) { KA_RESTORE(16) }
    else if (p <= 24) { KA_RESTORE(24) }
    else { KA_RESTORE(32) }
#undef KA_RESTORE
}

/* read a subframe header; returns -1 on error */
typedef struct { int type, wasted, order, shift, vwidth; int32_t cval, q[KA_MAXORD]; } ka_SubHdr;

static int ka_get_sub_header_br(KA_BR *r, ka_SubHdr *h)
{
    h->type = (int)ka_br_get(r, 2);
    if (h->type == KA_ST_CONST) { h->cval = ((int32_t)(ka_br_get(r, 17) << 15)) >> 15; return 0; }
    if (h->type == KA_ST_VERB) { h->vwidth = (int)ka_br_get(r, 5); return h->vwidth > 26 ? -1 : 0; }
    if (h->type != KA_ST_LPC) return -1;
    h->wasted = ka_br_get(r, 1) ? (int)ka_br_get(r, 4) : 0;
    h->order = (int)ka_br_get(r, 6);
    if (h->order > KA_MAXORD) return -1;
    h->shift = 0;
    if (h->order) {
        h->shift = (int)ka_br_get(r, 5);
        if (h->shift > 30) return -1;
        int kc = (int)ka_br_get(r, 4);
        for (int j = 0; j < h->order; j++) h->q[j] = ka_unzz(ka_br_rice(r, kc));
    }
    return 0;
}

/* decode one subframe into x[0..n) (x[-KA_HIST..-1] valid, in unshifted units). lim = |x| bound. */
static int ka_get_sub(KA_BR *r, int32_t *x, int n, int32_t *e, int32_t *xs, int lim)
{
    ka_SubHdr h;
    if (ka_get_sub_header_br(r, &h)) return -1;
    if (h.type == KA_ST_CONST) {
        if (h.cval > lim || h.cval < -lim) return -1;
        for (int t = 0; t < n; t++) x[t] = h.cval;
        return 0;
    }
    if (h.type == KA_ST_VERB) {
        for (int t = 0; t < n; t++) x[t] = ka_unzz(ka_br_get(r, h.vwidth));
    } else {
        int po = (int)ka_br_get(r, 4);
        if (po > 8 || (n >> po) < 1) return -1;
        int cnt = 1 << po, len = n >> po;
        for (int i = 0; i < cnt; i++) {
            int a = i * len, b = (i == cnt - 1) ? n : a + len;
            int k = (int)ka_br_get(r, 5);
            if (k == 31) {
                int wd = (int)ka_br_get(r, 5);
                if (wd > 26) return -1;
                for (int t = a; t < b; t++) e[t] = ka_unzz(ka_br_get(r, wd));
            } else {
                if (k > 30) return -1;
                for (int t = a; t < b; t++) e[t] = ka_unzz(ka_br_rice(r, k));
            }
        }
        if (h.wasted) {
            for (int t = -KA_HIST; t < 0; t++) xs[t] = x[t] >> h.wasted;
            ka_lpc_restore(xs, e, n, h.q, h.order, h.shift);
            for (int t = 0; t < n; t++) x[t] = (int32_t)((uint32_t)xs[t] << h.wasted);
        } else ka_lpc_restore(x, e, n, h.q, h.order, h.shift);
    }
    for (int t = 0; t < n; t++) if (x[t] > lim || x[t] < -lim) return -1;
    return 0;
}

/* --------------------------------------------------------------------------------------- stereo */

enum { KA_SM_LR = 0, KA_SM_LS = 1, KA_SM_RS = 2, KA_SM_MS = 3 };

/* derive L,R (with history) into the four signals */
static void ka_make_side_mid(const int32_t *L, const int32_t *R, int n, int32_t *S, int32_t *M)
{
    for (int t = -KA_HIST; t < n; t++) { S[t] = L[t] - R[t]; M[t] = (L[t] + R[t]) >> 1; }
}

/* ------------------------------------------------------------------------------- profile 1: cascade */

#define KA_WQ 20     /* fraction bits of the adapted weights */
#define KA_PQ 14     /* fraction bits of the int16 weights used for prediction */

/* NLMS filter: int32 weights (Q20) are adapted; an int16 copy (Q14, saturated) predicts. The dot
 * product of int16 weights and int16 history is summed modulo 2^32, identically in encoder and decoder. */
typedef struct {
    int n, m;              /* own taps, cross taps */
    int32_t *w;            /* n + m adapted weights: own then cross */
    int16_t *w16;          /* prediction weights */
    int16_t *h;            /* own history buffer (sliding) */
    int16_t *hc;           /* cross history buffer (sliding) */
    int pos, posc, cap;
    int64_t pow;
    int mu;                /* step in 1/4096 */
} ka_Nlms;

#define KA_SLIDE 4096

static void ka_nlms_init(ka_Nlms *f, int n, int m, int mu)
{
    f->n = n; f->m = m; f->mu = mu;
    f->w = ka_xcalloc((size_t)(n + m), sizeof(int32_t));
    f->w16 = ka_xcalloc((size_t)(n + m), sizeof(int16_t));
    f->cap = KA_SLIDE;
    f->h = ka_xcalloc((size_t)(n + KA_SLIDE), sizeof(int16_t));
    f->hc = ka_xcalloc((size_t)(m + KA_SLIDE + 1), sizeof(int16_t));
    f->pos = 0; f->posc = 0; f->pow = 0;
}
static void ka_nlms_free(ka_Nlms *f) { free(f->w); free(f->w16); free(f->h); free(f->hc); }

static inline int16_t ka_clip16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); }

__attribute__((optimize("wrapv"))) static inline int32_t dot16(const int16_t *restrict a, const int16_t *restrict b, int n)
{
    int32_t s = 0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

/* own history: h[pos .. pos+n) oldest..newest; cross: hc[posc .. posc+m) */
static inline int32_t ka_nlms_pred(const ka_Nlms *f)
{
    uint32_t s = (uint32_t)dot16(f->w16, f->h + f->pos, f->n);
    if (f->m) s += (uint32_t)dot16(f->w16 + f->n, f->hc + f->posc, f->m);
    return (int32_t)(s + (1u << (KA_PQ - 1))) >> KA_PQ;
}

/* weight update: w += (g2 * h + 2^14) >> 15 (the rounding of a 16x16 high multiply), then the int16
 * prediction copy w16 = sat16((w + 32) >> 6). The AVX2 path and the C path give identical results. */
static inline void ka_upd16(int32_t *restrict w, int16_t *restrict w16, const int16_t *restrict h, int n, int16_t g2, int sh)
{
    int i = 0;
#if defined(__AVX2__)
    const __m256i g = _mm256_set1_epi16(g2), r = _mm256_set1_epi32(1 << (KA_WQ - KA_PQ - 1));
    for (; i + 16 <= n; i += 16) {
        __m256i d = _mm256_mulhrs_epi16(_mm256_loadu_si256((const __m256i *)(h + i)), g);
        __m256i d0 = _mm256_slli_epi32(_mm256_cvtepi16_epi32(_mm256_castsi256_si128(d)), sh);
        __m256i d1 = _mm256_slli_epi32(_mm256_cvtepi16_epi32(_mm256_extracti128_si256(d, 1)), sh);
        __m256i w0 = _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(w + i)), d0);
        __m256i w1 = _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(w + i + 8)), d1);
        _mm256_storeu_si256((__m256i *)(w + i), w0);
        _mm256_storeu_si256((__m256i *)(w + i + 8), w1);
        __m256i q0 = _mm256_srai_epi32(_mm256_add_epi32(w0, r), KA_WQ - KA_PQ);
        __m256i q1 = _mm256_srai_epi32(_mm256_add_epi32(w1, r), KA_WQ - KA_PQ);
        __m256i p = _mm256_permute4x64_epi64(_mm256_packs_epi32(q0, q1), 0xD8);
        p = _mm256_max_epi16(p, _mm256_set1_epi16(-32767));
        _mm256_storeu_si256((__m256i *)(w16 + i), p);
    }
#endif
    for (; i < n; i++) {
        int32_t v = (int32_t)((uint32_t)w[i] + ((uint32_t)(((int32_t)g2 * h[i] + (1 << 14)) >> 15) << sh));
        w[i] = v;
        int32_t q = (v + (1 << (KA_WQ - KA_PQ - 1))) >> (KA_WQ - KA_PQ);
        w16[i] = (int16_t)(q > 32767 ? 32767 : q < -32767 ? -32767 : q);
    }
}

/* reciprocal table: ka_rtab[i] = round(2^24 / (2048 + i)), so 1/d ~ ka_rtab[(d >> e) - 2048] / 2^(24 + e) */
static uint32_t ka_rtab[2048];
static void ka_rtab_init(void) { for (uint32_t i = 0; i < 2048; i++) ka_rtab[i] = ((1u << 25) / (2048 + i) + 1) >> 1; }

static inline void ka_nlms_update(ka_Nlms *f, int32_t err, int32_t in)
{
    /* g ~ mu * err * 2^KA_WQ / (pow + eps), represented as g / 2^G with |g| < 2^15 */
    if (err) {
        /* g / 2^G = mu/4096 * err * 2^KA_WQ / den, with den >= 2^12 so that e >= 1 */
        uint64_t den = (uint64_t)f->pow + 4096 + 64 * (uint64_t)(f->n + f->m);
        int e = 64 - __builtin_clzll(den) - 12;
        uint32_t r = ka_rtab[(den >> e) - 2048];
        int64_t num = (int64_t)err * f->mu;                 /* |num| < 2^34 */
        /* g2 = mu/4096 * err * 2^(KA_WQ+15) / den = num * 2^23 / den = num * r >> (e + 1) */
        int64_t g = (num * (int64_t)r) >> (e + 1);
        int sh = 0;
        uint64_t ag = (uint64_t)(g < 0 ? -g : g);
        if (ag > 32767) {
            sh = 64 - __builtin_clzll(ag) - 15;         /* |g >> sh| < 2^15 */
            if (sh > 8) sh = 8;
            g >>= sh;
        }
        int16_t g2 = (int16_t)(g > 32767 ? 32767 : g < -32767 ? -32767 : g);
        ka_upd16(f->w, f->w16, f->h + f->pos, f->n, g2, sh);
        if (f->m) ka_upd16(f->w + f->n, f->w16 + f->n, f->hc + f->posc, f->m, g2, sh);
    }
    /* push input */
    int16_t v = ka_clip16(in);
    int16_t old = f->h[f->pos];
    f->pow += (int64_t)v * v - (int64_t)old * old;
    f->h[f->pos + f->n] = v;
    f->pos++;
    if (f->pos == f->cap) { memmove(f->h, f->h + f->pos, sizeof(int16_t) * f->n); f->pos = 0; }
}
static inline void ka_nlms_push_cross(ka_Nlms *f, int32_t in)
{
    if (!f->m) return;
    int16_t v = ka_clip16(in);
    int16_t old = f->hc[f->posc];
    f->pow += (int64_t)v * v - (int64_t)old * old;
    f->hc[f->posc + f->m] = v;
    f->posc++;
    if (f->posc == f->cap) { memmove(f->hc, f->hc + f->posc, sizeof(int16_t) * f->m); f->posc = 0; }
}

#define KA_SSN 16
#define KA_SSLIDE 1024
/* sign-sign LMS: w += step * sign(err) * sign(h); history and its signs in sliding buffers */
typedef struct { int32_t w[KA_SSN]; int32_t h[KA_SSN + KA_SSLIDE]; int32_t sg[KA_SSN + KA_SSLIDE]; int pos; int step; } ka_Sslms;

static inline int32_t ka_ss_pred(const ka_Sslms *s)
{
    const int32_t *h = s->h + s->pos;
    int64_t a = 0;
    for (int i = 0; i < KA_SSN; i++) a += (int64_t)s->w[i] * h[i];
    a = (a + (1 << (KA_WQ - 1))) >> KA_WQ;
    if (a > (1 << 20)) a = 1 << 20;
    if (a < -(1 << 20)) a = -(1 << 20);
    return (int32_t)a;
}
static inline void ka_ss_update(ka_Sslms *s, int32_t err, int32_t in)
{
    if (err) {
        int32_t st = err > 0 ? s->step : -s->step;
        const int32_t *sg = s->sg + s->pos;
        for (int i = 0; i < KA_SSN; i++) s->w[i] += st * sg[i];
    }
    int32_t v = in > 32767 ? 32767 : in < -32767 ? -32767 : in;
    s->h[s->pos + KA_SSN] = v;
    s->sg[s->pos + KA_SSN] = (v > 0) - (v < 0);
    if (++s->pos == KA_SSLIDE) {
        memmove(s->h, s->h + KA_SSLIDE, sizeof(int32_t) * KA_SSN);
        memmove(s->sg, s->sg + KA_SSLIDE, sizeof(int32_t) * KA_SSN);
        s->pos = 0;
    }
}

#define KA_KCTX 25
#define KA_DCTX 9
typedef struct {
    ka_Nlms f[3];
    ka_Sslms ss;
    int32_t cw[4];
    int comb;                 /* combiner step, 0 = plain sum */
    /* entropy model */
    uint32_t afast, aslow;
    int psign;
    int kpart;                /* magnitude parameter of the current partition (sent by the encoder) */
    ka_Prob tree[KA_KCTX * KA_DCTX][28];
    ka_Prob m1[27][KA_KCTX];
    ka_Prob m2[27][2];
    ka_Prob sg[3];
} ka_Chan1;

static void ka_chan1_init(ka_Chan1 *c, const ka_Opts *o, int stereo)
{
    memset(c, 0, sizeof *c);
    ka_nlms_init(&c->f[0], o->taps[0], stereo ? o->cross[0] : 0, o->mu[0]);
    ka_nlms_init(&c->f[1], o->taps[1], stereo ? o->cross[1] : 0, o->mu[1]);
    ka_nlms_init(&c->f[2], o->taps[2], 0, o->mu[2]);
    c->ss.step = o->ss_step;
    for (int k = 0; k < 4; k++) c->cw[k] = 1 << 14;
    c->comb = o->comb;
    for (size_t i = 0; i < sizeof c->tree / sizeof(ka_Prob); i++) ka_prob_init(&c->tree[0][0] + i);
    for (size_t i = 0; i < sizeof c->m1 / sizeof(ka_Prob); i++) ka_prob_init(&c->m1[0][0] + i);
    for (size_t i = 0; i < sizeof c->m2 / sizeof(ka_Prob); i++) ka_prob_init(&c->m2[0][0] + i);
    for (int i = 0; i < 3; i++) ka_prob_init(&c->sg[i]);
    c->afast = 16 * 16; c->aslow = 16 * 16;
}
static void ka_chan1_free(ka_Chan1 *c) { for (int i = 0; i < 3; i++) ka_nlms_free(&c->f[i]); }


/* predictions of every stage; returns total, fills p[] */
typedef struct { int32_t p0, p1, p2, p3, pb; } ka_Preds;

/* The cascade runs stage by stage: each stage's prediction depends only on its own history, but
 * its history is the previous stage's error, so the stages are evaluated in order. */
/* The final prediction combines the stage predictions with adaptive weights cw (Q14, start at 1),
 * adapted by sign-sign LMS on the final residual; each stage still adapts on its own error. */
static inline int32_t ka_comb_pred(const ka_Chan1 *c, const int32_t *p)
{
    if (!c->comb) return p[0] + p[1] + p[2] + p[3];
    int64_t s = 0;
    for (int k = 0; k < 4; k++) s += (int64_t)c->cw[k] * p[k];
    s = (s + (1 << 13)) >> 14;
    if (s > (1 << 21)) s = 1 << 21;
    if (s < -(1 << 21)) s = -(1 << 21);
    return (int32_t)s;
}
static inline void ka_comb_upd(ka_Chan1 *c, const int32_t *p, int32_t r)
{
    if (!c->comb || !r) return;
    int st = r > 0 ? c->comb : -c->comb;
    for (int k = 0; k < 4; k++) c->cw[k] += p[k] > 0 ? st : p[k] < 0 ? -st : 0;
}
static inline void ka_casc_upd(ka_Chan1 *c, const int32_t *p, int32_t v, int32_t r)
{
    int32_t e0 = v - p[0], e1 = e0 - p[1], e2 = e1 - p[2], e3 = e2 - p[3];
    ka_nlms_update(&c->f[0], e0, v);
    ka_nlms_update(&c->f[1], e1, e0);
    ka_nlms_update(&c->f[2], e2, e1);
    ka_ss_update(&c->ss, e3, e2);
    ka_comb_upd(c, p, r);
}
/* stage k predicts the error of stage k-1 from its own history, so the predictions are taken stage
 * by stage from the histories alone and do not need the current value */
static inline int32_t ka_casc_encode(ka_Chan1 *c, int32_t v)
{
    int32_t p[4] = { ka_nlms_pred(&c->f[0]), ka_nlms_pred(&c->f[1]), ka_nlms_pred(&c->f[2]), ka_ss_pred(&c->ss) };
    int32_t r = v - ka_comb_pred(c, p);
    ka_casc_upd(c, p, v, r);
    return r;
}
static inline int32_t ka_casc_decode(ka_Chan1 *c, int32_t r)
{
    int32_t p[4] = { ka_nlms_pred(&c->f[0]), ka_nlms_pred(&c->f[1]), ka_nlms_pred(&c->f[2]), ka_ss_pred(&c->ss) };
    int32_t v = r + ka_comb_pred(c, p);
    ka_casc_upd(c, p, v, r);
    return v;
}

/* note: p1 of the decoder must be computed after f[0]'s prediction only (histories are independent),
 * so evaluating all predictions first is identical to the encoder order. */

/* context: the partition's magnitude parameter k, and where the recent mean |r| sits relative to it */
static inline int ka_model_ctx(const ka_Chan1 *c)
{
    uint32_t a = c->afast;   /* Q4 mean |r| */
    int l = ka_bitlen(a);
    int q = l < 2 ? l : 2 * l - 2 + (int)((a >> (l - 2)) & 1);
    int d = q - 2 * c->kpart;
    if (d < 0) d = 0;
    if (d > KA_DCTX - 1) d = KA_DCTX - 1;
    return c->kpart * KA_DCTX + d;
}
static inline void ka_model_upd(ka_Chan1 *c, uint32_t m)
{
    if (m > (1u << 22)) m = 1u << 22;
    c->afast += (int32_t)(m * 16 - c->afast) >> 4;
    c->aslow += (int32_t)(m * 16 - c->aslow) >> 7;
}

/* The bit length b of |r| is coded as its rank in the order k+1, k, k+2, k-1, k+3, k-2, ... (k is
 * the partition parameter): one adaptive "is it this one?" decision per rank. Below the MSB, two
 * mantissa bits are modelled and the rest go to the raw bit stream; the sign is modelled. */
#define KA_MAXB 26
static inline int ka_rank_of(int b, int k)
{
    int d = b - (k + 1);                       /* 0, -1, +1, -2, +2 ... -> 0, 1, 2, 3, 4 ... */
    int r;
    int lo = k + 1, hi = KA_MAXB - (k + 1);       /* available steps below and above */
    int ad = d < 0 ? -d : d;
    int m = lo < hi ? lo : hi;                 /* symmetric part */
    if (ad <= m) r = d == 0 ? 0 : d < 0 ? 2 * ad - 1 : 2 * ad;
    else r = 2 * m + (ad - m);                  /* only one side left */
    return r;
}
static inline int ka_b_of_rank(int r, int k)
{
    int lo = k + 1, hi = KA_MAXB - (k + 1), m = lo < hi ? lo : hi;
    if (r == 0) return k + 1;
    if (r <= 2 * m) { int ad = (r + 1) / 2; return r & 1 ? k + 1 - ad : k + 1 + ad; }
    int ad = m + (r - 2 * m);
    return lo > hi ? k + 1 - ad : k + 1 + ad;
}

static inline void ka_code_res(KA_RCE *e, KA_BW *raw, ka_Chan1 *c, int32_t r)
{
    uint32_t m = r < 0 ? -(uint32_t)r : (uint32_t)r;
    int b = ka_bitlen(m);
    int ctx = ka_model_ctx(c);
    ka_Prob *t = c->tree[ctx];
    int rk = ka_rank_of(b, c->kpart);
    for (int i = 0; i < rk; i++) ka_rce_bit(e, &t[i], 0);
    if (rk < KA_MAXB) ka_rce_bit(e, &t[rk], 1);
    if (b >= 2) {
        int k = b - 1;                      /* bits below the MSB */
        int top = (m >> (k - 1)) & 1;
        ka_rce_bit(e, &c->m1[b][c->kpart], top);
        if (k >= 2) {
            int sec = (m >> (k - 2)) & 1;
            ka_rce_bit(e, &c->m2[b][top], sec);
            if (k > 2) ka_bw_put(raw, m, k - 2);
        }
    }
    if (m) { ka_rce_bit(e, &c->sg[c->psign], r < 0); c->psign = r < 0 ? 0 : 2; }
    else c->psign = 1;
    ka_model_upd(c, m);
}
static inline int ka_decode_res(KA_RCD *d, KA_BR *raw, ka_Chan1 *c, int32_t *out)
{
    int ctx = ka_model_ctx(c);
    ka_Prob *t = c->tree[ctx];
    int rk = 0;
    while (rk < KA_MAXB && !ka_rcd_bit(d, &t[rk])) rk++;
    int b = ka_b_of_rank(rk, c->kpart);
    if (b < 0 || b > KA_MAXB) return -1;
    uint32_t m = b ? 1 : 0;
    if (b >= 2) {
        int k = b - 1;
        int top = ka_rcd_bit(d, &c->m1[b][c->kpart]);
        m = (m << 1) | (uint32_t)top;
        if (k >= 2) {
            int sec = ka_rcd_bit(d, &c->m2[b][top]);
            m = (m << 1) | (uint32_t)sec;
            if (k > 2) m = (m << (k - 2)) | ka_br_get(raw, k - 2);
        }
    }
    int32_t r = (int32_t)m;
    if (m) { int neg = ka_rcd_bit(d, &c->sg[c->psign]); c->psign = neg ? 0 : 2; if (neg) r = -r; }
    else c->psign = 1;
    ka_model_upd(c, m);
    *out = r;
    return 0;
}

/* profile 1 partitions: order po (partitions of n >> po samples, the last takes the rest) and one
 * magnitude parameter per partition, chosen with the Rice cost estimate */
#define KA_P1_MAXPO 5
static int ka_p1_parts(const int32_t *r, int n, uint8_t *ks)
{
    int pm = 0;
    while (pm < KA_P1_MAXPO && (n >> (pm + 1)) >= 128) pm++;
    int64_t best = INT64_MAX;
    int bp = 0;
    uint8_t tk[1 << KA_P1_MAXPO];
    for (int p = 0; p <= pm; p++) {
        int cnt = 1 << p, len = n >> p;
        int64_t tot = 0;
        for (int i = 0; i < cnt; i++) {
            int a = i * len, b = i == cnt - 1 ? n : a + len;
            uint64_t sum = 0;
            for (int t = a; t < b; t++) sum += ka_zz(r[t]);
            int cn = b - a;
            int k0 = ka_bitlen((uint32_t)((sum / cn) > 0xFFFFFFFFu ? 0xFFFFFFFFu : sum / cn));
            int64_t bb = INT64_MAX; int kb = 0;
            for (int k = k0 > 2 ? k0 - 2 : 0; k <= k0 + 1 && k < KA_KCTX; k++) {
                int64_t c = (int64_t)cn * (k + 1) + (int64_t)(sum >> k) - (k ? cn / 2 : 0);
                if (c < bb) { bb = c; kb = k; }
            }
            tk[i] = (uint8_t)kb;
            tot += 5 + bb;
        }
        if (tot < best) { best = tot; bp = p; memcpy(ks, tk, cnt); }
    }
    return bp;
}

/* --------------------------------------------------------------------------------- chunk encoder */

static void ka_range_hdr(KA_RCE *e, const ka_Sub *sb)
{
    ka_rce_bypass(e, (uint32_t)sb->type, 2);
    if (sb->type == KA_ST_CONST) { ka_rce_bypass(e, (uint32_t)sb->cval & 0x1FFFF, 17); return; }
    ka_rce_bypass(e, sb->wasted ? 1 : 0, 1);
    if (sb->wasted) ka_rce_bypass(e, (uint32_t)sb->wasted, 4);
    ka_rce_bypass(e, (uint32_t)sb->order, 6);
    if (sb->order) {
        ka_rce_bypass(e, (uint32_t)sb->shift, 5);
        ka_rce_bypass(e, (uint32_t)sb->kc, 4);
        for (int j = 0; j < sb->order; j++) {
            /* Rice code of the zigzag value through bypass bits, as in profile 0 */
            uint32_t u = ka_zz(sb->q[j]), q = u >> sb->kc;
            if (q < KA_RICE_ESC) { ka_rce_bypass(e, 1, (int)q + 1); ka_rce_bypass(e, u, sb->kc); }
            else { ka_rce_bypass(e, 0, KA_RICE_ESC); ka_rce_bypass(e, u, 25); }
        }
    }
}

#define KA_MINBLK 256          /* a block is split only if both halves have at least this many samples */
#define KA_MAXNODE 64          /* split tree nodes per frame (depth <= 5) */

static const int ka_pairs[4][2] = { { 0, 1 }, { 0, 2 }, { 1, 2 }, { 3, 2 } };

static inline int ka_can_split(int n, int depth, int maxdepth) { return depth < maxdepth && n >= 2 * KA_MINBLK; }

typedef struct {
    const ka_Opts *o;
    int ch, maxord, maxdepth;
    int32_t *sig[4];          /* L, R, S, M with KA_HIST history */
    double *sigd[4];          /* the same as doubles */
    ka_Work w;
    ka_Sub sb[4];
    ka_Cov *cov;                 /* [KA_MAXNODE][4] */
    uint8_t split[KA_MAXNODE], mode[KA_MAXNODE];
    int fixed_mode;           /* profile 1: one stereo mode for the whole chunk, -1 = per block */
    double est4[4];           /* signal estimates of the last top-level block */
    KA_BW bw;
    KA_RCE rc;
    ka_Chan1 *c1;
    int32_t *rr[2];
} ka_Enc;

/* choose the split tree and stereo modes of a block from least-squares estimates; returns est. bits.
 * Children are planned first, and a parent's covariance row is the sum of its children's rows. */
static double ka_plan_block(ka_Enc *E, int off, int n, int depth, int idx)
{
    int ns = E->ch == 2 ? 4 : 1;
    int split = ka_can_split(n, depth, E->maxdepth), h = n / 2;
    double csplit = 0;
    if (split) csplit = ka_plan_block(E, off, h, depth + 1, 2 * idx + 1) + ka_plan_block(E, off + h, n - h, depth + 1, 2 * idx + 2);
    int mo = ka_clamp_order(E->maxord, n);
    double est[4];
    for (int i = 0; i < ns; i++) {
        const int32_t *x = E->sig[i] + off;
        ka_Cov *cv = &E->cov[idx * 4 + i];
        if (split) {
            const ka_Cov *l = &E->cov[(2 * idx + 1) * 4 + i], *r = &E->cov[(2 * idx + 2) * 4 + i];
            for (int j = 0; j <= mo; j++) cv->C[0][j] = l->row[j] + r->row[j];
        } else cov_row(E->sigd[i] + off, n, mo, cv->C[0]);
        memcpy(cv->row, cv->C[0], sizeof(double) * (mo + 1));
        int c = 1;
        for (int t = 1; t < n; t++) if (x[t] != x[0]) { c = 0; break; }
        if (c) { est[i] = 19; continue; }
        ka_cov_fill(x, n, mo, cv);
        ka_cov_solve(cv);
        est[i] = ka_est_best(cv->err, cv->pmax, n);
    }
    double single = est[0];
    int mode = 0;
    if (ns == 4) {
        single = 1e300;
        for (int m = 0; m < 4; m++) {
            double c = est[ka_pairs[m][0]] + est[ka_pairs[m][1]];
            if (c < single) { single = c; mode = m; }
        }
        single += 2;
    }
    if (idx == 0) memcpy(E->est4, est, sizeof est);
    if (ns == 4 && ka_g_force_stereo >= 0) mode = ka_g_force_stereo;
    else if (ns == 4 && E->fixed_mode >= 0) mode = E->fixed_mode;
    E->mode[idx] = (uint8_t)mode;
    E->split[idx] = 0;
    if (!split) return single;
    if (csplit < single) { E->split[idx] = 1; return csplit + 1; }
    return single + 1;
}

static void ka_emit_leaf(ka_Enc *E, int off, int n, int idx)
{
    const ka_Opts *o = E->o;
    int mode = E->mode[idx];
    ka_Sub *use[2];
    const int32_t *xs[2];
    if (E->ch == 1) {
        ka_analyse_sub(&E->w, E->sig[0] + off, n, o->effort, E->maxord, &E->sb[0], &E->cov[idx * 4]);
        use[0] = &E->sb[0]; xs[0] = E->sig[0] + off;
    } else {
        if (o->effort >= 2 && ka_g_force_stereo < 0 && E->fixed_mode < 0) {
            /* exact choice among the four ka_pairs */
            for (int i = 0; i < 4; i++)
                ka_analyse_sub(&E->w, E->sig[i] + off, n, o->effort, E->maxord, &E->sb[i], &E->cov[idx * 4 + i]);
            int64_t best = INT64_MAX;
            for (int m = 0; m < 4; m++) {
                int64_t c = E->sb[ka_pairs[m][0]].bits + E->sb[ka_pairs[m][1]].bits;
                if (c < best) { best = c; mode = m; }
            }
        } else {
            for (int k = 0; k < 2; k++) {
                int i = ka_pairs[mode][k];
                ka_analyse_sub(&E->w, E->sig[i] + off, n, o->effort, E->maxord, &E->sb[i], &E->cov[idx * 4 + i]);
            }
        }
        for (int k = 0; k < 2; k++) { use[k] = &E->sb[ka_pairs[mode][k]]; xs[k] = E->sig[ka_pairs[mode][k]] + off; }
    }
    if (o->profile == 0) {
        if (E->ch == 2) ka_bw_put(&E->bw, (uint32_t)mode, 2);
        for (int c = 0; c < E->ch; c++) ka_put_sub(&E->bw, use[c], xs[c], n);
        return;
    }
    if (E->ch == 2) ka_rce_bypass(&E->rc, (uint32_t)mode, 2);
    for (int c = 0; c < E->ch; c++) {
        /* profile 1 has no verbatim subframes: use LPC order 0 */
        if (use[c]->type == KA_ST_VERB) { use[c]->type = KA_ST_LPC; use[c]->wasted = 0; use[c]->order = 0; memcpy(use[c]->res, xs[c], sizeof(int32_t) * n); }
        ka_range_hdr(&E->rc, use[c]);
    }
    /* run the cascade over the block, then choose and send the partition parameters, then code */
    for (int t = 0; t < n; t++)
        for (int c = 0; c < E->ch; c++) {
            int32_t v = use[c]->type == KA_ST_CONST ? 0 : use[c]->res[t];
            if (use[c]->type != KA_ST_CONST) E->rr[c][t] = ka_casc_encode(&E->c1[c], v);
            if (E->ch == 2) { ka_nlms_push_cross(&E->c1[c ^ 1].f[0], v); ka_nlms_push_cross(&E->c1[c ^ 1].f[1], v); }
        }
    uint8_t ks[2][1 << KA_P1_MAXPO];
    int po[2] = { 0, 0 };
    for (int c = 0; c < E->ch; c++) {
        if (use[c]->type == KA_ST_CONST) continue;
        po[c] = ka_p1_parts(E->rr[c], n, ks[c]);
        ka_rce_bypass(&E->rc, (uint32_t)po[c], 3);
        for (int i = 0; i < (1 << po[c]); i++) ka_rce_bypass(&E->rc, ks[c][i], 5);
    }
    for (int t = 0; t < n; t++)
        for (int c = 0; c < E->ch; c++) {
            if (use[c]->type == KA_ST_CONST) continue;
            int len = n >> po[c], i = t / len;
            if (i >= (1 << po[c])) i = (1 << po[c]) - 1;
            E->c1[c].kpart = ks[c][i];
            ka_code_res(&E->rc, &E->bw, &E->c1[c], E->rr[c][t]);
        }
}

static void ka_emit_block(ka_Enc *E, int off, int n, int depth, int idx)
{
    if (ka_can_split(n, depth, E->maxdepth)) {
        if (E->o->profile == 0) ka_bw_put(&E->bw, E->split[idx], 1); else ka_rce_bypass(&E->rc, E->split[idx], 1);
        if (E->split[idx]) {
            int h = n / 2;
            ka_emit_block(E, off, h, depth + 1, 2 * idx + 1);
            ka_emit_block(E, off + h, n - h, depth + 1, 2 * idx + 2);
            return;
        }
    }
    ka_emit_leaf(E, off, n, idx);
}

static int ka_depth_for(const ka_Opts *o)
{
    if (o->depth >= 0) return o->depth;
    if (o->profile == 1) return 0;    /* fewer LPC changes suit the adaptive stages */
    return o->effort == 0 ? 0 : o->effort == 1 ? 2 : 3;
}

static void ka_encode_chunk(const ka_Opts *o, int ch, const uint8_t *pcm, uint64_t start, int len, ka_Buf *out)
{
    ka_Enc E;
    memset(&E, 0, sizeof E);
    E.o = o; E.ch = ch;
    E.maxord = o->profile == 1 ? o->lpc1 : KA_MAXORD;
    E.maxdepth = ka_depth_for(o);
    int F = o->frame;
    for (int i = 0; i < 4; i++) E.sig[i] = (int32_t *)ka_xcalloc((size_t)len + KA_HIST, sizeof(int32_t)) + KA_HIST;
    const uint8_t *p = pcm + start * 2 * ch;
    for (int t = 0; t < len; t++) {
        E.sig[0][t] = (int16_t)ka_rd16(p + (size_t)t * 2 * ch);
        if (ch == 2) E.sig[1][t] = (int16_t)ka_rd16(p + (size_t)t * 4 + 2);
    }
    if (ch == 2) ka_make_side_mid(E.sig[0], E.sig[1], len, E.sig[2], E.sig[3]);
    for (int i = 0; i < (ch == 2 ? 4 : 1); i++) {
        E.sigd[i] = (double *)ka_xmalloc(sizeof(double) * ((size_t)len + KA_HIST)) + KA_HIST;
        for (int t = -KA_HIST; t < len; t++) E.sigd[i][t] = E.sig[i][t];
    }
    ka_work_init(&E.w, F);
    for (int i = 0; i < 4; i++) E.sb[i].res = ka_xmalloc(sizeof(int32_t) * F);
    E.cov = ka_xmalloc(sizeof(ka_Cov) * KA_MAXNODE * 4);
    if (o->profile == 1) {
        E.rr[0] = ka_xmalloc(sizeof(int32_t) * F); E.rr[1] = ka_xmalloc(sizeof(int32_t) * F);
        ka_rce_init(&E.rc);
        E.c1 = ka_xmalloc(sizeof(ka_Chan1) * ch);
        for (int c = 0; c < ch; c++) ka_chan1_init(&E.c1[c], o, ch == 2);
    }
    E.fixed_mode = -1;
    if (o->profile == 1 && ch == 2 && ka_g_force_stereo < 0) {
        /* one stereo mode per chunk: switching modes disturbs the adaptive filters */
        double tot[4] = { 0, 0, 0, 0 };
        int md = E.maxdepth;
        E.maxdepth = 0;
        for (int f0 = 0; f0 < len; f0 += F) {
            int n = len - f0 < F ? len - f0 : F;
            ka_plan_block(&E, f0, n, 0, 0);
            for (int i = 0; i < 4; i++) tot[i] += E.est4[i];
        }
        E.maxdepth = md;
        double best = 1e300;
        for (int m = 0; m < 4; m++) { double c = tot[ka_pairs[m][0]] + tot[ka_pairs[m][1]]; if (c < best) { best = c; E.fixed_mode = m; } }
    }
    for (int f0 = 0; f0 < len; f0 += F) {
        int n = len - f0 < F ? len - f0 : F;
        ka_plan_block(&E, f0, n, 0, 0);
        ka_emit_block(&E, f0, n, 0, 0);
    }
    if (o->profile == 0) { ka_bw_flush(&E.bw); *out = E.bw.b; }
    else {
        ka_rce_finish(&E.rc);
        ka_bw_flush(&E.bw);
        uint8_t l4[4];
        ka_wr32(l4, (uint32_t)E.rc.b.len);
        ka_Buf o1 = { 0 };
        ka_buf_add(&o1, l4, 4); ka_buf_add(&o1, E.rc.b.p, E.rc.b.len); ka_buf_add(&o1, E.bw.b.p, E.bw.b.len);
        free(E.rc.b.p); free(E.bw.b.p);
        *out = o1;
        for (int c = 0; c < ch; c++) ka_chan1_free(&E.c1[c]);
        free(E.c1); free(E.rr[0]); free(E.rr[1]);
    }
    for (int i = 0; i < 4; i++) { free(E.sig[i] - KA_HIST); free(E.sb[i].res); if (E.sigd[i]) free(E.sigd[i] - KA_HIST); }
    free(E.cov);
    ka_work_free(&E.w);
}

/* --------------------------------------------------------------------------------- chunk decoder */

typedef struct {
    const ka_Opts *o;
    int ch, maxdepth, err;
    int32_t *a, *b, *L, *R, *e, *e2, *xs;
    KA_BR br;
    KA_RCD rd;
    ka_Chan1 *c1;
} ka_Dec;

static int ka_read_hdr1(KA_RCD *rd, ka_SubHdr *hh)
{
    hh->type = (int)ka_rcd_bypass(rd, 2);
    if (hh->type == KA_ST_CONST) { hh->cval = ((int32_t)(ka_rcd_bypass(rd, 17) << 15)) >> 15; return 0; }
    if (hh->type != KA_ST_LPC) return -1;
    hh->wasted = ka_rcd_bypass(rd, 1) ? (int)ka_rcd_bypass(rd, 4) : 0;
    hh->order = (int)ka_rcd_bypass(rd, 6);
    if (hh->order > KA_MAXORD) return -1;
    hh->shift = 0;
    if (hh->order) {
        hh->shift = (int)ka_rcd_bypass(rd, 5);
        if (hh->shift > 30) return -1;
        int kc = (int)ka_rcd_bypass(rd, 4);
        for (int j = 0; j < hh->order; j++) {
            uint32_t q = 0;
            while (q < KA_RICE_ESC && !ka_rcd_bypass(rd, 1)) q++;
            uint32_t u = q < KA_RICE_ESC ? (q << kc) | ka_rcd_bypass(rd, kc) : ka_rcd_bypass(rd, 25);
            hh->q[j] = ka_unzz(u);
        }
    }
    return 0;
}

static int ka_decode_leaf(ka_Dec *D, int f0, int n)
{
    const ka_Opts *o = D->o;
    int ch = D->ch, mode = KA_SM_LR;
    int32_t *slot[2];
    if (ch == 2) {
        mode = o->profile == 0 ? (int)ka_br_get(&D->br, 2) : (int)ka_rcd_bypass(&D->rd, 2);
        int32_t *A = D->a + f0, *B = D->b + f0;
        for (int t = -KA_HIST; t < 0; t++) {
            int32_t l = D->L[f0 + t], r = D->R[f0 + t];
            int32_t s = l - r, m = (l + r) >> 1;
            A[t] = mode == KA_SM_LR || mode == KA_SM_LS ? l : mode == KA_SM_RS ? r : m;
            B[t] = mode == KA_SM_LR ? r : s;
        }
        slot[0] = A; slot[1] = B;
    } else slot[0] = D->L + f0;
    int lim[2] = { 32768, mode == KA_SM_LR ? 32768 : 65535 };
    if (o->profile == 0) {
        for (int c = 0; c < ch; c++) if (ka_get_sub(&D->br, slot[c], n, D->e, D->xs, lim[c])) return -1;
    } else {
        ka_SubHdr h[2];
        for (int c = 0; c < ch; c++) if (ka_read_hdr1(&D->rd, &h[c])) return -1;
        uint8_t ks[2][1 << KA_P1_MAXPO];
        int po[2] = { 0, 0 };
        for (int c = 0; c < ch; c++) {
            if (h[c].type == KA_ST_CONST) continue;
            po[c] = (int)ka_rcd_bypass(&D->rd, 3);
            if (po[c] > KA_P1_MAXPO || (n >> po[c]) < 1) return -1;
            for (int i = 0; i < (1 << po[c]); i++) { ks[c][i] = (uint8_t)ka_rcd_bypass(&D->rd, 5); if (ks[c][i] >= KA_KCTX) return -1; }
        }
        int32_t *ev[2] = { D->e, D->e2 };
        for (int t = 0; t < n; t++)
            for (int c = 0; c < ch; c++) {
                int32_t v = 0;
                if (h[c].type != KA_ST_CONST) {
                    int32_t r;
                    int len = n >> po[c], i = t / len;
                    if (i >= (1 << po[c])) i = (1 << po[c]) - 1;
                    D->c1[c].kpart = ks[c][i];
                    if (ka_decode_res(&D->rd, &D->br, &D->c1[c], &r)) return -1;
                    v = ka_casc_decode(&D->c1[c], r);
                    if (v >= KA_MAXRES || v <= -KA_MAXRES) return -1;
                    ev[c][t] = v;
                }
                if (ch == 2) { ka_nlms_push_cross(&D->c1[c ^ 1].f[0], v); ka_nlms_push_cross(&D->c1[c ^ 1].f[1], v); }
            }
        for (int c = 0; c < ch; c++) {
            int32_t *x = slot[c];
            if (h[c].type == KA_ST_CONST) {
                if (h[c].cval > lim[c] || h[c].cval < -lim[c]) return -1;
                for (int t = 0; t < n; t++) x[t] = h[c].cval;
                continue;
            }
            if (h[c].wasted) {
                for (int t = -KA_HIST; t < 0; t++) D->xs[t] = x[t] >> h[c].wasted;
                ka_lpc_restore(D->xs, ev[c], n, h[c].q, h[c].order, h[c].shift);
                for (int t = 0; t < n; t++) x[t] = (int32_t)((uint32_t)D->xs[t] << h[c].wasted);
            } else ka_lpc_restore(x, ev[c], n, h[c].q, h[c].order, h[c].shift);
            for (int t = 0; t < n; t++) if (x[t] > lim[c] || x[t] < -lim[c]) return -1;
        }
    }
    if (ch == 2) {
        int32_t *A = slot[0], *B = slot[1], *L = D->L + f0, *R = D->R + f0;
        int bad = 0;
        switch (mode) {
        case KA_SM_LR: for (int t = 0; t < n; t++) { L[t] = A[t]; R[t] = B[t]; } break;
        case KA_SM_LS: for (int t = 0; t < n; t++) { L[t] = A[t]; R[t] = A[t] - B[t]; } break;
        case KA_SM_RS: for (int t = 0; t < n; t++) { R[t] = A[t]; L[t] = A[t] + B[t]; } break;
        default:
            for (int t = 0; t < n; t++) { int32_t s = B[t], l = (2 * A[t] + (s & 1) + s) >> 1; L[t] = l; R[t] = l - s; }
        }
        for (int t = 0; t < n; t++) bad |= (L[t] < -32768) | (L[t] > 32767) | (R[t] < -32768) | (R[t] > 32767);
        if (bad) return -1;
    } else {
        int bad = 0;
        for (int t = 0; t < n; t++) bad |= (D->L[f0 + t] < -32768) | (D->L[f0 + t] > 32767);
        if (bad) return -1;
    }
    return 0;
}

static int ka_decode_block(ka_Dec *D, int off, int n, int depth)
{
    if (ka_can_split(n, depth, D->maxdepth)) {
        int s = D->o->profile == 0 ? (int)ka_br_get(&D->br, 1) : (int)ka_rcd_bypass(&D->rd, 1);
        if (s) {
            int h = n / 2;
            return ka_decode_block(D, off, h, depth + 1) || ka_decode_block(D, off + h, n - h, depth + 1) ? -1 : 0;
        }
    }
    return ka_decode_leaf(D, off, n);
}

/* decode a chunk into interleaved LE16 at dst; returns 0 on success */
static int ka_decode_chunk(const ka_Opts *o, int ch, const uint8_t *src, size_t srclen, int len, uint8_t *dst)
{
    ka_Dec D;
    memset(&D, 0, sizeof D);
    D.o = o; D.ch = ch; D.maxdepth = o->depth;
    int F = o->frame;
    D.a = (int32_t *)ka_xcalloc((size_t)len + KA_HIST, sizeof(int32_t)) + KA_HIST;
    D.b = (int32_t *)ka_xcalloc((size_t)len + KA_HIST, sizeof(int32_t)) + KA_HIST;
    D.L = (int32_t *)ka_xcalloc((size_t)len + KA_HIST, sizeof(int32_t)) + KA_HIST;
    D.R = (int32_t *)ka_xcalloc((size_t)len + KA_HIST, sizeof(int32_t)) + KA_HIST;
    D.e = ka_xmalloc(sizeof(int32_t) * F); D.e2 = ka_xmalloc(sizeof(int32_t) * F);
    D.xs = (int32_t *)ka_xmalloc(sizeof(int32_t) * (F + KA_HIST)) + KA_HIST;
    if (o->profile == 0) ka_br_init(&D.br, src, srclen);
    else {
        uint32_t rl = srclen >= 4 ? ka_rd32(src) : 0;
        if (srclen < 4 || rl > srclen - 4) rl = 0, srclen = 4;   /* corrupt: decoding will fail cleanly */
        ka_rcd_init(&D.rd, src + 4, rl);
        ka_br_init(&D.br, src + 4 + rl, srclen - 4 - rl);
        D.c1 = ka_xmalloc(sizeof(ka_Chan1) * ch);
        for (int c = 0; c < ch; c++) ka_chan1_init(&D.c1[c], o, ch == 2);
    }
    int rcv = 0;
    for (int f0 = 0; f0 < len && !rcv; f0 += F) {
        int n = len - f0 < F ? len - f0 : F;
        rcv = ka_decode_block(&D, f0, n, 0);
        if (!rcv && o->profile == 0 && ka_br_overrun(&D.br)) rcv = -1;
        if (!rcv && o->profile == 1 && (D.rd.over > 5 || ka_br_overrun(&D.br))) rcv = -1;
    }
    if (!rcv) {
        if (ch == 1) for (int t = 0; t < len; t++) ka_wr16(dst + 2 * (size_t)t, (uint32_t)D.L[t]);
        else for (int t = 0; t < len; t++) { ka_wr16(dst + 4 * (size_t)t, (uint32_t)D.L[t]); ka_wr16(dst + 4 * (size_t)t + 2, (uint32_t)D.R[t]); }
    }
    if (D.c1) { for (int c = 0; c < ch; c++) ka_chan1_free(&D.c1[c]); free(D.c1); }
    free(D.a - KA_HIST); free(D.b - KA_HIST); free(D.L - KA_HIST); free(D.R - KA_HIST); free(D.e); free(D.e2); free(D.xs - KA_HIST);
    return rcv;
}

/* ---------------------------------------------------------------------------------- container */
/* Header (little endian):
 *   "KAU1" | u8 version | u8 profile | u8 channels | u8 log2 chunk | u16 frame | u8 effort | u8 lpc1
 *   | u16 mu0 | u16 mu1 | u16 mu2 | u16 ss_step | u8 rate_cap | u8 0 | u64 sample frames
 *   | u32 head length | head bytes | u32 tail length | tail bytes | u32 CRC-32 of the original file
 *   | per chunk: u32 payload bytes, u32 CRC-32 of the chunk's PCM bytes | payloads             */

typedef struct {
    const ka_Opts *o; int ch; const uint8_t *pcm; uint64_t n; int nchunks;
    ka_Buf *out; int next; pthread_mutex_t mu;
    /* decode */
    const uint8_t **src; const uint32_t *srclen; const uint32_t *crc; uint8_t *dst; int bad;
} ka_Pool;

static int ka_pool_take(ka_Pool *p)
{
    pthread_mutex_lock(&p->mu);
    int i = p->next < p->nchunks ? p->next++ : -1;
    pthread_mutex_unlock(&p->mu);
    return i;
}
static int ka_chunk_len(const ka_Pool *p, int i)
{
    uint64_t cs = 1ull << p->o->chunk_log2, a = (uint64_t)i * cs;
    return (int)(p->n - a < cs ? p->n - a : cs);
}
static void *ka_enc_worker(void *arg)
{
    ka_Pool *p = arg;
    int i;
    while ((i = ka_pool_take(p)) >= 0)
        ka_encode_chunk(p->o, p->ch, p->pcm, (uint64_t)i << p->o->chunk_log2, ka_chunk_len(p, i), &p->out[i]);
    return NULL;
}
static void *ka_dec_worker(void *arg)
{
    ka_Pool *p = arg;
    int i;
    while ((i = ka_pool_take(p)) >= 0) {
        int len = ka_chunk_len(p, i);
        uint8_t *d = p->dst + ((size_t)i << p->o->chunk_log2) * 2 * p->ch;
        if (ka_decode_chunk(p->o, p->ch, p->src[i], p->srclen[i], len, d) ||
            ka_crc32_buf(0, d, (size_t)len * 2 * p->ch) != p->crc[i]) {
            pthread_mutex_lock(&p->mu); p->bad = 1; pthread_mutex_unlock(&p->mu);
        }
    }
    return NULL;
}
static void ka_run_pool(ka_Pool *p, int threads, void *(*fn)(void *))
{
    p->next = 0;
    pthread_mutex_init(&p->mu, NULL);
    if (threads > p->nchunks) threads = p->nchunks;
    if (threads <= 1) { fn(p); }
    else {
        pthread_t *t = ka_xmalloc(sizeof(pthread_t) * threads);
        for (int i = 0; i < threads; i++) pthread_create(&t[i], NULL, fn, p);
        for (int i = 0; i < threads; i++) pthread_join(t[i], NULL);
        free(t);
    }
    pthread_mutex_destroy(&p->mu);
}

static int ka_kaud_encode_layout(const ka_Opts *o, const uint8_t *in, size_t inlen, ka_Wav w, ka_Buf *out)
{
    ka_Pool p = { 0 };
    p.o = o; p.ch = w.ch; p.pcm = in + w.data_off; p.n = w.n;
    uint64_t cs = 1ull << o->chunk_log2;
    p.nchunks = (int)((w.n + cs - 1) / cs);
    p.out = ka_xcalloc((size_t)p.nchunks, sizeof(ka_Buf));
    ka_run_pool(&p, o->threads, ka_enc_worker);

    uint8_t h[40];
    uint8_t hx[7];
    ka_wr16(hx, (uint32_t)o->taps[0]); hx[2] = (uint8_t)o->taps[1]; hx[3] = (uint8_t)o->taps[2]; hx[4] = (uint8_t)o->cross[0]; hx[5] = (uint8_t)o->cross[1]; hx[6] = (uint8_t)o->comb;
    memcpy(h, "KAU1", 4);
    h[4] = KA_KAUD_VERSION; h[5] = (uint8_t)o->profile; h[6] = (uint8_t)w.ch; h[7] = (uint8_t)o->chunk_log2;
    ka_wr16(h + 8, (uint32_t)o->frame); h[10] = (uint8_t)o->effort; h[11] = (uint8_t)o->lpc1;
    ka_wr16(h + 12, (uint32_t)o->mu[0]); ka_wr16(h + 14, (uint32_t)o->mu[1]); ka_wr16(h + 16, (uint32_t)o->mu[2]);
    ka_wr16(h + 18, (uint32_t)o->ss_step); h[20] = (uint8_t)ka_g_rate_cap; h[21] = (uint8_t)ka_depth_for(o);
    ka_wr32(h + 22, (uint32_t)w.n); ka_wr32(h + 26, (uint32_t)(w.n >> 32));
    out->len = 0;
    ka_buf_add(out, h, 30);
    ka_buf_add(out, hx, 7);
    uint8_t b4[4];
    ka_wr32(b4, (uint32_t)w.data_off); ka_buf_add(out, b4, 4); ka_buf_add(out, in, w.data_off);
    ka_wr32(b4, (uint32_t)(inlen - w.trl_off)); ka_buf_add(out, b4, 4); ka_buf_add(out, in + w.trl_off, inlen - w.trl_off);
    ka_wr32(b4, ka_crc32_buf(0, in, inlen)); ka_buf_add(out, b4, 4);
    for (int i = 0; i < p.nchunks; i++) {
        int len = ka_chunk_len(&p, i);
        ka_wr32(b4, (uint32_t)p.out[i].len); ka_buf_add(out, b4, 4);
        ka_wr32(b4, ka_crc32_buf(0, p.pcm + ((size_t)i << o->chunk_log2) * 2 * w.ch, (size_t)len * 2 * w.ch)); ka_buf_add(out, b4, 4);
    }
    for (int i = 0; i < p.nchunks; i++) { ka_buf_add(out, p.out[i].p, p.out[i].len); free(p.out[i].p); }
    free(p.out);
    return 0;
}

static int ka_kaud_decode(const uint8_t *in, size_t inlen, int threads, ka_Buf *out, const char **err, size_t expect)
{
    *err = "corrupt or truncated KAU1 stream";
    if (inlen < 37 || memcmp(in, "KAU1", 4)) { *err = "not a KAU1 stream"; return -1; }
    if (in[4] != KA_KAUD_VERSION) { *err = "unsupported KAU1 version"; return -1; }
    ka_Opts o = { 0 };
    o.profile = in[5];
    int ch = in[6];
    o.chunk_log2 = in[7];
    o.frame = ka_rd16(in + 8);
    o.effort = in[10]; o.lpc1 = in[11];
    o.mu[0] = ka_rd16(in + 12); o.mu[1] = ka_rd16(in + 14); o.mu[2] = ka_rd16(in + 16); o.ss_step = ka_rd16(in + 18);
    int rate_cap = in[20];
    o.depth = in[21];
    uint64_t n = ka_rd32(in + 22) | (uint64_t)ka_rd32(in + 26) << 32;
    if (o.profile > 1 || (ch != 1 && ch != 2) || o.chunk_log2 < 8 || o.chunk_log2 > 30 || o.frame < 16 ||
        o.frame > 32768 || rate_cap < 1 || rate_cap > 12 || o.depth > 5) return -1;
    if (o.profile == 1 && rate_cap != ka_g_rate_cap) return -1;
    o.taps[0] = ka_rd16(in + 30); o.taps[1] = in[32]; o.taps[2] = in[33]; o.cross[0] = in[34]; o.cross[1] = in[35]; o.comb = in[36];
    if (o.taps[0] < 1 || o.taps[0] > 4096 || o.taps[1] < 1 || o.taps[2] < 1) return -1;
    size_t p = 37;
    if (inlen - p < 4) return -1;
    size_t hl = ka_rd32(in + p); p += 4;
    if (hl > inlen - p) return -1;
    const uint8_t *head = in + p; p += hl;
    if (inlen - p < 4) return -1;
    size_t tl = ka_rd32(in + p); p += 4;
    if (tl > inlen - p) return -1;
    const uint8_t *tail = in + p; p += tl;
    if (inlen - p < 4) return -1;
    uint32_t fcrc = ka_rd32(in + p); p += 4;
    uint64_t cs = 1ull << o.chunk_log2;
    uint64_t nch = (n + cs - 1) / cs;
    if (n > ((uint64_t)1 << 40) / (size_t)(2 * ch) || nch > (inlen - p) / 8) return -1;
    ka_Pool pl = { 0 };
    pl.o = &o; pl.ch = ch; pl.n = n; pl.nchunks = (int)nch;
    pl.src = ka_xmalloc(sizeof(uint8_t *) * (nch + 1));
    uint32_t *sl = ka_xmalloc(sizeof(uint32_t) * (nch + 1)), *cr = ka_xmalloc(sizeof(uint32_t) * (nch + 1));
    size_t q = p + 8 * nch;
    for (uint64_t i = 0; i < nch; i++) {
        sl[i] = ka_rd32(in + p + 8 * i); cr[i] = ka_rd32(in + p + 8 * i + 4);
        uint64_t clen = n - i * cs < cs ? n - i * cs : cs;
        if (sl[i] > inlen - q || (clen + o.frame - 1) / o.frame > (uint64_t)sl[i] * 8 / 19 + 2) { free(pl.src); free(sl); free(cr); return -1; }
        pl.src[i] = in + q; q += sl[i];
    }
    pl.srclen = sl; pl.crc = cr;
    size_t pcmlen = (size_t)n * 2 * ch;
    if (expect && (pcmlen > expect || hl + tl != expect - pcmlen)) { free(pl.src); free(sl); free(cr); return -1; }
    out->len = 0;
    ka_buf_need(out, hl + pcmlen + tl);
    memcpy(out->p, head, hl);
    pl.dst = out->p + hl;
    ka_run_pool(&pl, threads, ka_dec_worker);
    free(pl.src); free(sl); free(cr);
    if (pl.bad) return -1;
    memcpy(out->p + hl + pcmlen, tail, tl);
    out->len = hl + pcmlen + tl;
    if (ka_crc32_buf(0, out->p, out->len) != fcrc) { *err = "CRC mismatch"; return -1; }
    return 0;
}


/* ---------------------------------------------------------------------------------------- Kolmo codec 19 */
static pthread_once_t ka_once = PTHREAD_ONCE_INIT;
static void ka_init(void) { ka_crc_init(); ka_rtab_init(); ka_shtab_init(); }

/* code len bytes: head bytes, then frames sample frames of ch (1 or 2) channels of 16-bit little-endian PCM, then the
 * rest of the block. profile 0 or 1. Returns the payload length, or 0 when it does not fit in cap. */
static size_t aud_compress(int profile, int ch, const uint8_t *in, size_t len, size_t head, uint64_t frames, uint8_t *out, size_t cap) {
    if ((ch != 1 && ch != 2) || head > len || frames > (len - head) / (size_t)(2 * ch) || frames == 0) return 0;
    pthread_once(&ka_once, ka_init);
    ka_Opts o; memset(&o, 0, sizeof o);
    o.profile = profile; o.effort = 1; o.frame = 0; o.chunk_log2 = 18; o.threads = 1;
    o.mu[0] = 256; o.mu[1] = 512; o.mu[2] = 1024; o.ss_step = 1 << 8; o.lpc1 = 16; o.depth = -1;
    o.taps[0] = 256; o.taps[1] = 32; o.taps[2] = 16; o.cross[0] = 32; o.cross[1] = 8; o.comb = 64;
    if (!o.frame) o.frame = profile == 1 ? 32768 : 4096;
    ka_Wav w; w.ch = ch; w.n = frames; w.data_off = head; w.trl_off = head + (size_t)frames * 2 * (size_t)ch;
    ka_Buf b = {0};
    if (ka_kaud_encode_layout(&o, in, len, w, &b) || b.len > cap) { free(b.p); return 0; }
    memcpy(out, b.p, b.len); free(b.p); return b.len;
}

static int aud_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    pthread_once(&ka_once, ka_init);
    ka_Buf b = { out, 0, n }; const char *err;
    if (!n || ka_kaud_decode(in, cn, 1, &b, &err, n) || b.p != out || b.len != n) { if (b.p != out) free(b.p); return -1; }
    return 0;
}

/* coded and decoded at block level by core/blocks.c and core/decode.c (never a graph leaf): a name only */
static const kolmo_codec codec_audio = { C_AUD, "audio", NULL, NULL, NULL, NULL };
