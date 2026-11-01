/* czn — native core of the cz compressor. See czn.h. */
#define _GNU_SOURCE
#include "czn.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#if defined(CZN_TRACE) || defined(CZN_PROF)
#include <stdio.h>
#endif
#ifdef CZN_PROF
#include <x86intrin.h>
#endif
#include <stdint.h>
unsigned long long czn_xxh3(const void *p, size_t n, unsigned long long seed);   /* vendor/xxhash/czn_xxh3.c */
static _Atomic int cz_newmodes = 0;   /* bitmask of extended predictors used (bit0=mode4 dod -> v4, bit1=mode5 3rd-order -> v5) */
#include "zstd.h"
#include "zdict.h"
#include "lz4.h"
#include "lz4hc.h"
#include "common/xxhash.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#ifndef CZN_NO_LZMA
#include <lzma.h>
#endif
#include "libbsc/libbsc.h"

/* ------------------------------------------------------------------ CRC32C (hardware where available) */
#if defined(__SSE4_2__)
#include <nmmintrin.h>
uint32_t czn_crc32c(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = buf; uint64_t c = ~crc;
    while (len >= 8) { uint64_t v; memcpy(&v, p, 8); c = _mm_crc32_u64(c, v); p += 8; len -= 8; }
    while (len--) c = _mm_crc32_u8((uint32_t)c, *p++);
    return ~(uint32_t)c;
}
#elif defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
uint32_t czn_crc32c(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = buf; uint32_t c = ~crc;
    while (len >= 8) { uint64_t v; memcpy(&v, p, 8); c = __crc32cd(c, v); p += 8; len -= 8; }
    while (len--) c = __crc32cb(c, *p++);
    return ~c;
}
#else
static uint32_t crc_tab[8][256];
static pthread_once_t crc_once = PTHREAD_ONCE_INIT;
static void crc_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0x82F63B78u : c >> 1;
        crc_tab[0][i] = c;
    }
    for (int t = 1; t < 8; t++)
        for (int i = 0; i < 256; i++) crc_tab[t][i] = (crc_tab[t - 1][i] >> 8) ^ crc_tab[0][crc_tab[t - 1][i] & 0xFF];
}
uint32_t czn_crc32c(uint32_t crc, const void *buf, size_t len) {
    pthread_once(&crc_once, crc_init);
    const uint8_t *p = buf; uint32_t c = ~crc;
    while (len >= 8) {
        uint64_t v; memcpy(&v, p, 8); v ^= c;
        c = crc_tab[7][v & 0xFF] ^ crc_tab[6][(v >> 8) & 0xFF] ^ crc_tab[5][(v >> 16) & 0xFF] ^ crc_tab[4][(v >> 24) & 0xFF] ^
            crc_tab[3][(v >> 32) & 0xFF] ^ crc_tab[2][(v >> 40) & 0xFF] ^ crc_tab[1][(v >> 48) & 0xFF] ^ crc_tab[0][v >> 56];
        p += 8; len -= 8;
    }
    while (len--) c = (c >> 8) ^ crc_tab[0][(c ^ *p++) & 0xFF];
    return ~c;
}
#endif

/* ------------------------------------------------------------------ transforms (shared with cz's czfast.c) */
#define TP_STEP(c) do { \
    if (quotes) { if (c == '"' && !inbr) inq ^= 1; else if (c == '[' && !inq) inbr = 1; else if (c == ']' && !inq) inbr = 0; } \
    if (c == '\n') { j = 0; inq = inbr = 0; } \
    else if (c == delim && !inq && !inbr && j < maxc - 1) j++; } while (0)

static void tp_classes(uint8_t *cls, uint8_t delim, int quotes) {
    memset(cls, 0, 256); cls['\n'] = 1; cls[delim] = 1;
    if (quotes) { cls['"'] = 1; cls['['] = 1; cls[']'] = 1; }
}

static void transpose_fwd(const uint8_t *in, int64_t n, uint8_t delim, int maxc, int quotes, uint8_t *out, int64_t *colsz) {
    int64_t cnt[256] = {0}; uint8_t cls[256]; tp_classes(cls, delim, quotes);
    { int j = 0, inq = 0, inbr = 0; int64_t i = 0;
      while (i < n) { int64_t s = i; while (i < n && !cls[in[i]]) i++; if (i < n) i++; cnt[j] += i - s; uint8_t c = in[i - 1]; TP_STEP(c); } }
    uint8_t *dst[256]; int64_t pos = 0;
    for (int k = 0; k < maxc; k++) { dst[k] = out + pos; colsz[k] = cnt[k]; pos += cnt[k]; }
    { int j = 0, inq = 0, inbr = 0; uint8_t *o = dst[0];
      for (int64_t i = 0; i < n; i++) { uint8_t c = in[i]; *o++ = c; if (cls[c]) { dst[j] = o; TP_STEP(c); o = dst[j]; } } }
}

static int transpose_inv(const uint8_t *in, int64_t n, uint8_t delim, int maxc, int quotes, const int64_t *colsz, uint8_t *out) {
    int64_t off[256], end[256], pos = 0; uint8_t cls[256]; tp_classes(cls, delim, quotes);
    for (int k = 0; k < maxc; k++) { if (colsz[k] < 0 || colsz[k] > n - pos) return -1; off[k] = pos; pos += colsz[k]; end[k] = pos; }
    if (pos != n) return -1;
    int j = 0, inq = 0, inbr = 0; int64_t i = 0;
    while (i < n) {
        const uint8_t *src = in + off[j]; int64_t lim = end[j] - off[j], r = 0;
        while (r < lim && !cls[src[r]]) r++;
        if (r < lim) r++;
        if (r == 0 || i + r > n) return -1;
        memcpy(out + i, src, r); off[j] += r; i += r;
        uint8_t c = out[i - 1];
        if (!cls[c]) continue;
        TP_STEP(c);
    }
    return 0;
}

/* Quote-free fast path: terminator positions found by a branchless scan, fields moved with short copies.
 * Byte-identical to transpose_fwd/inv with quotes = 0. pos needs n + 1 entries. */
#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
/* bit masks of the bytes equal to a or '\n' (t) and to '\n' (nl) among the 64 at p */
static inline void term_masks64(const uint8_t *p, uint8_t a, uint64_t *t, uint64_t *nl) {
#if defined(__SSE2__)
    const __m128i va = _mm_set1_epi8((char)a), vn = _mm_set1_epi8('\n'); uint64_t mt = 0, mn = 0;
    for (int q = 0; q < 4; q++) {
        __m128i x = _mm_loadu_si128((const __m128i *)(p + 16 * q)), en = _mm_cmpeq_epi8(x, vn);
        mn |= (uint64_t)(uint16_t)_mm_movemask_epi8(en) << (16 * q);
        mt |= (uint64_t)(uint16_t)_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(x, va), en)) << (16 * q);
    }
    *t = mt; *nl = mn;
#elif defined(__ARM_NEON) && defined(__aarch64__)
    static const uint8_t bw[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const uint8x16_t va = vdupq_n_u8(a), vn = vdupq_n_u8('\n'), w = vld1q_u8(bw);
    uint8x16_t n0 = vceqq_u8(vld1q_u8(p), vn), n1 = vceqq_u8(vld1q_u8(p + 16), vn), n2 = vceqq_u8(vld1q_u8(p + 32), vn), n3 = vceqq_u8(vld1q_u8(p + 48), vn);
    uint8x16_t t0 = vorrq_u8(n0, vceqq_u8(vld1q_u8(p), va)), t1 = vorrq_u8(n1, vceqq_u8(vld1q_u8(p + 16), va));
    uint8x16_t t2 = vorrq_u8(n2, vceqq_u8(vld1q_u8(p + 32), va)), t3 = vorrq_u8(n3, vceqq_u8(vld1q_u8(p + 48), va));
#define CZN_MM64(r0, r1, r2, r3) ({ uint8x16_t s0 = vpaddq_u8(vandq_u8(r0, w), vandq_u8(r1, w)), s1 = vpaddq_u8(vandq_u8(r2, w), vandq_u8(r3, w)); \
        s0 = vpaddq_u8(s0, s1); s0 = vpaddq_u8(s0, s0); vgetq_lane_u64(vreinterpretq_u64_u8(s0), 0); })
    *t = CZN_MM64(t0, t1, t2, t3); *nl = CZN_MM64(n0, n1, n2, n3);
#undef CZN_MM64
#else
    uint64_t mt = 0, mn = 0;
    for (int q = 0; q < 64; q++) { mt |= (uint64_t)(p[q] == a || p[q] == '\n') << q; mn |= (uint64_t)(p[q] == '\n') << q; }
    *t = mt; *nl = mn;
#endif
}
/* positions of bytes equal to a or '\n' (SIMD compare + bit iteration) */
static size_t scan_terms(const uint8_t *s, size_t n, uint8_t a, uint32_t *pos) {
    size_t c = 0, i = 0;
#if defined(__SSE2__)
    const __m128i va = _mm_set1_epi8((char)a), vn = _mm_set1_epi8('\n');
    for (; i + 64 <= n; i += 64) {
        uint64_t m = 0;
        for (int q = 0; q < 4; q++) {
            __m128i x = _mm_loadu_si128((const __m128i *)(s + i + 16 * q));
            m |= (uint64_t)(uint16_t)_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi8(x, va), _mm_cmpeq_epi8(x, vn))) << (16 * q);
        }
        while (m) { pos[c++] = (uint32_t)(i + __builtin_ctzll(m)); m &= m - 1; }
    }
#elif defined(__ARM_NEON)
    const uint8x16_t va = vdupq_n_u8(a), vn = vdupq_n_u8('\n');
    for (; i + 16 <= n; i += 16) {
        uint8x16_t x = vld1q_u8(s + i);
        uint8x16_t e = vorrq_u8(vceqq_u8(x, va), vceqq_u8(x, vn));
        uint64_t m = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(e), 4)), 0);
        while (m) { int t = __builtin_ctzll(m); pos[c++] = (uint32_t)(i + (t >> 2)); m &= ~(0xFull << (t & ~3)); }
    }
#endif
    for (; i < n; i++) { pos[c] = (uint32_t)i; c += (s[i] == a) | (s[i] == '\n'); }
    return c;
}
static inline void copy_field(uint8_t *d, const uint8_t *s, size_t len, int wild) {
    if (wild && len <= 16) { uint64_t a, b; memcpy(&a, s, 8); memcpy(&b, s + 8, 8); memcpy(d, &a, 8); memcpy(d + 8, &b, 8); }
    else memcpy(d, s, len);
}
static void tpq_fwd(const uint8_t *in, size_t n, uint8_t delim, int maxc, uint8_t *out, int64_t *colsz, uint32_t *pos) {
    size_t T = scan_terms(in, n, delim, pos);
    int64_t cnt[256] = {0}; int j = 0; size_t st = 0;
    for (size_t t = 0; t < T; t++) { size_t p = pos[t]; cnt[j] += p + 1 - st; st = p + 1; if (in[p] == '\n') j = 0; else if (j < maxc - 1) j++; }
    cnt[j] += n - st;
    uint8_t *dst[256], *cend[256]; int64_t o = 0;
    for (int k = 0; k < maxc; k++) { dst[k] = out + o; colsz[k] = cnt[k]; o += cnt[k]; cend[k] = out + o; }
    j = 0; st = 0;
    for (size_t t = 0; t < T; t++) {
        size_t p = pos[t], len = p + 1 - st;
        copy_field(dst[j], in + st, len, st + 16 <= n && dst[j] + 16 <= cend[j]);
        dst[j] += len; st = p + 1;
        if (in[p] == '\n') j = 0; else if (j < maxc - 1) j++;
    }
    memcpy(dst[j], in + st, n - st);
}
static int tpq_inv(const uint8_t *in, size_t n, uint8_t delim, int maxc, const int64_t *colsz, uint8_t *out, uint32_t *pos) {
    size_t off[256], end[256], pi[256], pe[256], o = 0, tot = 0;
    for (int k = 0; k < maxc; k++) { if (colsz[k] < 0 || (size_t)colsz[k] > n - o) return -1; off[k] = o; end[k] = o + colsz[k]; o += colsz[k]; }
    if (o != n) return -1;                                               /* sizes validated before any read */
    for (int k = 0; k < maxc; k++) { pi[k] = tot; tot += scan_terms(in + off[k], colsz[k], delim, pos + tot); pe[k] = tot; }
    size_t w = 0; int j = 0;
    while (w < n) {
        if (pi[j] < pe[j]) {
            size_t base = end[j] - colsz[j], q = base + pos[pi[j]++], len = q + 1 - off[j];
            if (w + len > n) return -1;
            copy_field(out + w, in + off[j], len, w + 16 <= n && off[j] + 16 <= n);
            w += len; off[j] = q + 1;
            if (in[q] == '\n') j = 0; else if (j < maxc - 1) j++;
        } else {                                                     /* last field of the block, no terminator */
            size_t len = end[j] - off[j];
            if (!len || w + len > n) return -1;
            memcpy(out + w, in + off[j], len); w += len; off[j] += len;
        }
    }
    return 0;
}

/* Column-major inverse for uniform blocks (every row has exactly k fields, block ends with a newline).
 * Rows are independent inside each column pass, so the CPU overlaps many field copies.
 * Returns 0 done, 1 not uniform (caller falls back), -1 corrupt. rowbuf needs 2 * (n / 2 + 2) entries. */
static int tpq_inv_cols(const uint8_t *in, size_t n, uint8_t delim, int k, const int64_t *colsz, uint8_t *out,
                        uint32_t *pos, uint32_t *rowbuf) {
    size_t base[256], pb[256], tot = 0, o = 0;
    for (int j = 0; j < k; j++) { if (colsz[j] < 0 || (size_t)colsz[j] > n - o) return -1; base[j] = o; o += colsz[j]; }
    if (o != n || n == 0 || colsz[k - 1] == 0 || in[n - 1] != '\n') return 1;
    size_t R = 0;
    for (int j = 0; j < k; j++) {
        size_t c = scan_terms(in + base[j], colsz[j], j < k - 1 ? delim : '\n', pos + tot);
        if (j == 0) R = c; else if (c != R) return 1;
        if (!c || pos[tot + c - 1] != colsz[j] - 1) return 1;                 /* column must end on a terminator */
        pb[j] = tot; tot += c;
    }
    if (R > n / 2 + 1) return 1;
    uint32_t *rowoff = rowbuf, *cur = rowbuf + R + 1;
    memset(cur, 0, R * sizeof(uint32_t));
    for (int j = 0; j < k; j++) {
        const uint32_t *p = pos + pb[j]; const uint8_t *col = in + base[j]; uint32_t st = 0;
        uint8_t want = j < k - 1 ? delim : '\n';
        for (size_t r = 0; r < R; r++) {
            uint32_t q = p[r];
            if (col[q] != want) return 1;                                        /* short row: newline inside a field column */
            cur[r] += q + 1 - st; st = q + 1;
        }
    }
    uint32_t acc = 0;
    for (size_t r = 0; r < R; r++) { rowoff[r] = acc; acc += cur[r]; cur[r] = 0; }
    rowoff[R] = acc;
    if (acc != n) return -1;
    for (int j = 0; j < k; j++) {
        const uint32_t *p = pos + pb[j]; const uint8_t *col = in + base[j]; uint32_t st = 0;
        for (size_t r = 0; r < R; r++) {
            uint32_t q = p[r], len = q + 1 - st;
            uint8_t *d = out + rowoff[r] + cur[r];
            copy_field(d, col + st, len, d + 16 <= out + rowoff[r + 1] && base[j] + st + 16 <= n);
            cur[r] += len; st = q + 1;
        }
    }
    return 0;
}

#define TSLACK 16384            /* table transform may exceed the block by this much (headers, exceptions) */
#define ONES 0x0101010101010101ull
#define HIGHS 0x8080808080808080ull
/* first index of byte a or b in p[0..lim), or lim (SWAR, 8 bytes per step; lowest flagged byte is always exact) */
static inline size_t scan2(const uint8_t *p, size_t lim, uint8_t a, uint8_t b) {
    uint64_t ma = ONES * a, mb = ONES * b; size_t i = 0;
    for (; i + 8 <= lim; i += 8) {
        uint64_t v; memcpy(&v, p + i, 8); uint64_t x = v ^ ma, y = v ^ mb;
        uint64_t t = (((x - ONES) & ~x) | ((y - ONES) & ~y)) & HIGHS;
        if (t) return i + (__builtin_ctzll(t) >> 3);
    }
    for (; i < lim; i++) if (p[i] == a || p[i] == b) return i;
    return lim;
}
static inline size_t scan1(const uint8_t *p, size_t lim, uint8_t a) { const uint8_t *q = memchr(p, a, lim); return q ? (size_t)(q - p) : lim; }
static int has_quotes(const uint8_t *p, size_t n) { return memchr(p, '"', n) || memchr(p, '[', n) || memchr(p, ']', n); }


/* ---------------------------------------------------------------- table transform (v0.3)
 * Rows are split into fields (quote-aware, same rules as cz). Each column becomes one section:
 *   text    : field lengths (u8, 255 = long, then u32) + the bytes, no delimiters
 *   numeric : canonical decimals (-?(0|[1-9]d*)(.d+)?, <= 18 digits) as int64 at the column's largest scale,
 *             delta-coded down the column, zigzag, byte planes; per value a kind (0 empty, 1 number, 2 other text)
 *             and its scale; "other" values stored as text. Decoding needs no parsing or scanning.
 * Layout: k | nrows | nexc | noNL (u32) | exceptions (row,cnt) u32 | per column: type u8, size u32 | sections */
static const int64_t P10[19] = {1LL,10LL,100LL,1000LL,10000LL,100000LL,1000000LL,10000000LL,100000000LL,1000000000LL,
    10000000000LL,100000000000LL,1000000000000LL,10000000000000LL,100000000000000LL,1000000000000000LL,
    10000000000000000LL,100000000000000000LL,1000000000000000000LL};

static inline size_t field_end_q(const uint8_t *in, size_t i, size_t n, uint8_t delim, int split) {
    int inq = 0, inbr = 0;
    for (; i < n; i++) {
        uint8_t c = in[i];
        if (c == '\n') return i;
        if (c == '"' && !inbr) inq ^= 1; else if (c == '[' && !inq) inbr = 1; else if (c == ']' && !inq) inbr = 0;
        else if (c == delim && split && !inq && !inbr) return i;
    }
    return n;
}

/* canonical decimal -> (mantissa, scale); 0 if not canonical (then stored as text) */
/* parse_dec for fields with at most 8 digits before and after the dot, when 24 bytes can be read at p: SWAR
 * validation and conversion, no per-byte loop. Same result as parse_dec (checked by the tests); others go to it. */
static inline uint64_t swar_digits_ok(uint64_t x, uint64_t m) {   /* all bytes selected by m are '0'..'9' */
    uint64_t hi = x & 0xF0F0F0F0F0F0F0F0ull, lo = x & 0x0F0F0F0F0F0F0F0Full;
    return (((hi ^ 0x3030303030303030ull) | ((lo + 0x0606060606060606ull) & 0xF0F0F0F0F0F0F0F0ull)) & m) == 0;
}
static inline uint64_t swar_parse8(uint64_t x, int L) {           /* value of the first L (1..8) digits of x */
    x <<= 8 * (8 - L);                                           /* leading zero bytes act as leading zeros */
    x &= 0x0F0F0F0F0F0F0F0Full;
    x = (x * 2561) >> 8; x &= 0x00FF00FF00FF00FFull;
    x = (x * 6553601) >> 16; x &= 0x0000FFFF0000FFFFull;
    return (x * 42949672960001ull) >> 32;
}
static inline int parse_dec_fast(const uint8_t *p, uint32_t len, size_t avail, int64_t *m, int *sc);
static inline int parse_dec(const uint8_t *p, uint32_t len, int64_t *m, int *sc) {
    if (!len || len > 20) return 0;
    uint32_t i = 0; int neg = 0;
    if (p[0] == '-') { neg = 1; i = 1; if (len == 1) return 0; }
    if (p[i] < '0' || p[i] > '9') return 0;
    if (p[i] == '0' && i + 1 < len && p[i + 1] != '.') return 0;              /* leading zero */
    int64_t v = 0; int digits = 0, s = 0, dot = 0;
    for (; i < len; i++) {
        uint8_t c = p[i];
        if (c == '.') { if (dot || i + 1 == len) return 0; dot = 1; continue; }
        if (c < '0' || c > '9') return 0;
        if (++digits > 18) return 0;
        v = v * 10 + (c - '0'); s += dot;
    }
    if (neg && v == 0) return 0;                                              /* "-0", "-0.00" */
    *m = neg ? -v : v; *sc = s; return 1;
}
static inline int parse_dec_fast(const uint8_t *p, uint32_t len, size_t avail, int64_t *m, int *sc) {
    if (len - 1u >= 18u || avail < 24) return parse_dec(p, len, m, sc);
    int neg = p[0] == '-'; const uint8_t *q = p + neg; int L = (int)len - neg;
    if (!L) return 0;
    uint64_t w0, w1; memcpy(&w0, q, 8); memcpy(&w1, q + 8, 8);
    uint64_t d0 = w0 ^ 0x2E2E2E2E2E2E2E2Eull, d1 = w1 ^ 0x2E2E2E2E2E2E2E2Eull;            /* '.' -> zero byte */
    uint64_t z0 = ~(((d0 & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | d0 | 0x7F7F7F7F7F7F7F7Full);
    uint64_t z1 = ~(((d1 & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | d1 | 0x7F7F7F7F7F7F7F7Full);
    uint64_t m0 = L >= 8 ? ~0ull : ((1ull << (8 * L)) - 1), m1 = L <= 8 ? 0 : L >= 16 ? ~0ull : ((1ull << (8 * (L - 8))) - 1);
    z0 &= m0 & HIGHS; z1 &= m1 & HIGHS;
    int ndot = __builtin_popcountll(z0) + __builtin_popcountll(z1);
    if (ndot > 1) return 0;
    int ip = ndot ? (z0 ? __builtin_ctzll(z0) >> 3 : 8 + (__builtin_ctzll(z1) >> 3)) : L;
    int fl = ndot ? L - ip - 1 : 0;
    if (ip == 0 || (ndot && fl == 0)) return 0;
    if (ip > 8 || fl > 8) return parse_dec(p, len, m, sc);
    if (q[0] == '0' && ip > 1) return 0;                                      /* leading zero */
    if (!swar_digits_ok(w0, ip >= 8 ? ~0ull : ((1ull << (8 * ip)) - 1))) return 0;
    uint64_t v = swar_parse8(w0, ip);
    if (fl) {
        uint64_t f; memcpy(&f, q + ip + 1, 8);
        if (!swar_digits_ok(f, fl >= 8 ? ~0ull : ((1ull << (8 * fl)) - 1))) return 0;
        v = v * (uint64_t)P10[fl] + swar_parse8(f, fl);
    }
    if (neg && v == 0) return 0;
    *m = neg ? -(int64_t)v : (int64_t)v; *sc = fl; return 1;
}

static const char DIG2[201] = "00010203040506070809101112131415161718192021222324252627282930313233343536373839"
    "404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899";
static uint64_t INV5[19];                         /* modular inverses of 5^k mod 2^64: exact division without divide */
__attribute__((constructor)) static void inv5_init(void) {
    for (int k = 0; k < 19; k++) { uint64_t a = 1; for (int i = 0; i < k; i++) a *= 5; uint64_t x = a; for (int i = 0; i < 6; i++) x *= 2 - a * x; INV5[k] = x; }
}
static inline int64_t exact_div_p10(int64_t v, int k) {    /* v is a multiple of 10^k */
    return (int64_t)((uint64_t)(v >> k) * INV5[k]);
}
/* writes the decimal; o must have 24 bytes of room */
static inline uint8_t *fmt_dec(uint8_t *o, int64_t m, int s) {
    char buf[24]; char *e = buf + 24, *q = e; uint64_t a = m < 0 ? -(uint64_t)m : (uint64_t)m;
    while (a >= 100) { uint64_t r = a % 100; a /= 100; q -= 2; memcpy(q, DIG2 + 2 * r, 2); }
    if (a >= 10) { q -= 2; memcpy(q, DIG2 + 2 * a, 2); } else *--q = (char)('0' + a);
    int nd = (int)(e - q);
    while (nd < s + 1) { *--q = '0'; nd++; }
    if (m < 0) *o++ = '-';
    if (!s) { memcpy(o, q, nd); return o + nd; }
    int ip = nd - s; memcpy(o, q, ip); o += ip; *o++ = '.'; memcpy(o, q + ip, s); return o + s;
}
static inline __attribute__((always_inline)) int ndigits(uint64_t a) {
    if (!a) return 1;
    int d = ((64 - __builtin_clzll(a)) * 1233) >> 12;        /* floor(log10) estimate */
    return d + (a >= (uint64_t)P10[d < 18 ? d : 18] ? 1 : 0) + (d >= 19);
}
/* same output as fmt_dec, written straight to its final place (back to front): no re-read of fresh stores */
static inline uint8_t *fmt_dec_fast(uint8_t *o, int64_t m, int s) {
    uint64_t a = m < 0 ? -(uint64_t)m : (uint64_t)m;
    *o = '-'; o += m < 0;
    int nd = ndigits(a); if (nd < s + 1) nd = s + 1;
    int tot = nd + (s > 0);
    uint8_t *p = o + tot;
    int f = s;
    while (f >= 2) { uint64_t r = a % 100; a /= 100; p -= 2; memcpy(p, DIG2 + 2 * r, 2); f -= 2; }
    if (f) { *--p = (uint8_t)('0' + a % 10); a /= 10; }
    if (s) *--p = '.';
    while (p - o >= 2) { uint64_t r = a % 100; a /= 100; p -= 2; memcpy(p, DIG2 + 2 * r, 2); }
    if (p > o) *--p = (uint8_t)('0' + a % 10);
    return o + tot;
}

/* 8 decimal digits of x < 1e8 as ASCII, most significant first in memory (little-endian SWAR) */
static inline __attribute__((always_inline)) uint64_t swar8(uint64_t x) {
    uint64_t hi = x / 10000, lo = x % 10000;
    uint64_t merged = hi | (lo << 32);
    uint64_t top = ((merged * 10486) >> 20) & ((0x7FULL << 32) | 0x7FULL);
    uint64_t bot = merged - 100 * top;
    uint64_t hundreds = (bot << 16) + top;
    uint64_t tens = (hundreds * 103) >> 10;
    tens &= (0xFULL << 48) | (0xFULL << 32) | (0xFULL << 16) | 0xFULL;
    tens += (hundreds - 10 * tens) << 8;
    return tens + 0x3030303030303030ULL;
}
/* same output as fmt_dec_fast for up to 16 digits: SWAR digit conversion and overlapping 8-byte stores, no loads;
 * needs 40 bytes of room at o (bytes past the result are scratch) */
static inline __attribute__((always_inline)) void st8(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint8_t *fmt_dec_swar(uint8_t *o, int64_t m, int s) {
    uint64_t a = m < 0 ? -(uint64_t)m : (uint64_t)m;
    int D = ndigits(a); if (D < s + 1) D = s + 1;
    if (D > 16) return fmt_dec_fast(o, m, s);
    *o = '-'; o += m < 0;
    if (D <= 8) {
        uint64_t l = swar8(a);
        st8(o, l >> (8 * (8 - D)));
        if (s) { o[D - s] = '.'; st8(o + D - s + 1, l >> (8 * (8 - s))); }
    } else {
        uint64_t h = swar8(a / 100000000), l = swar8(a % 100000000);
        st8(o, h >> (8 * (16 - D))); st8(o + D - 8, l);
        if (s) {
            o[D - s] = '.';
            if (s <= 8) st8(o + D - s + 1, l >> (8 * (8 - s)));
            else { st8(o + D - s + 1, h >> (8 * (16 - s))); st8(o + D - 7, l); }
        }
    }
    return o + D + (s > 0);
}

/* A run of n values of one numeric column at scale S, trailing zeros cut per value (ds), written back to back at o
 * with their lengths in lv. Same bytes as fmt_dec_swar. x86 with AVX2: 4 values per step for |v| < 1e8 and S < 8
 * (digits by vector multiplies, decimal point placed by a byte shuffle); other values and CPUs use fmt_dec_swar.
 * Needs 40 bytes of room past the end. */
#if defined(__AVX2__)
#include <immintrin.h>
static uint8_t FMT_SH[9 * 8][16] __attribute__((aligned(16))), FMT_DOT[9 * 8][16] __attribute__((aligned(16)));
__attribute__((constructor)) static void fmt_luts_init(void) {
    for (int D = 1; D <= 8; D++) for (int S = 0; S < 8 && S < D; S++) {
        uint8_t *m = FMT_SH[D * 8 + S], *d = FMT_DOT[D * 8 + S]; memset(m, 0x80, 16); memset(d, 0, 16);
        int o = 0;
        for (int i = 0; i < D - S; i++) m[o++] = (uint8_t)(8 - D + i);
        if (S) { d[o] = '.'; o++; for (int i = 0; i < S; i++) m[o++] = (uint8_t)(8 - S + i); }
    }
}
#endif
/* Same values, each written at o + FMT_STRIDE * i (fixed slots: no dependency between values); lengths to lv.
 * Values longer than 16 bytes still fit their slot (at most 22 bytes); needs 40 bytes of room past the last slot. */
#define FMT_STRIDE 24
#if defined(__AVX2__)
static uint8_t FMT_SHN[2][9 * 8][16] __attribute__((aligned(16)));    /* [neg][D * 8 + S]: shuffle with '-' slot */
__attribute__((constructor)) static void fmt_luts2_init(void) {
    for (int ng = 0; ng < 2; ng++) for (int D = 1; D <= 8; D++) for (int S = 0; S < 8 && S < D; S++) {
        uint8_t *m = FMT_SHN[ng][D * 8 + S]; memset(m, 0x80, 16);
        int o = ng;                                                    /* '-' and '.' come from the OR mask */
        for (int i = 0; i < D - S; i++) m[o++] = (uint8_t)(8 - D + i);
        if (S) { o++; for (int i = 0; i < S; i++) m[o++] = (uint8_t)(8 - S + i); }
    }
}
static uint8_t FMT_ORN[2][9 * 8][16] __attribute__((aligned(16)));
__attribute__((constructor)) static void fmt_luts3_init(void) {
    for (int ng = 0; ng < 2; ng++) for (int D = 1; D <= 8; D++) for (int S = 0; S < 8 && S < D; S++) {
        uint8_t *d = FMT_ORN[ng][D * 8 + S]; memset(d, 0, 16);
        if (ng) d[0] = '-';
        if (S) d[ng + D - S] = '.';
    }
}
#endif
#if defined(__ARM_NEON) && defined(__aarch64__)
static uint8_t FMT_SHA[2][9 * 8][16] __attribute__((aligned(16))), FMT_ORA[2][9 * 8][16] __attribute__((aligned(16)));
__attribute__((constructor)) static void fmt_luts_neon_init(void) {
    for (int ng = 0; ng < 2; ng++) for (int D = 1; D <= 8; D++) for (int S = 0; S < 8 && S < D; S++) {
        uint8_t *m = FMT_SHA[ng][D * 8 + S], *d = FMT_ORA[ng][D * 8 + S]; memset(m, 0xFF, 16); memset(d, 0, 16);
        int o = ng; if (ng) d[0] = '-';
        for (int i = 0; i < D - S; i++) m[o++] = (uint8_t)(8 - D + i);
        if (S) { d[o] = '.'; o++; for (int i = 0; i < S; i++) m[o++] = (uint8_t)(8 - S + i); }
    }
}
#endif
static void fmt_slots(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0;
#if defined(__ARM_NEON) && defined(__aarch64__)
    if (S < 8) {                                     /* 2 values per step: digits by 32/16-bit lane arithmetic, table lookup */
        for (; i + 2 <= n; i += 2) {
            int64_t x0 = v[i], x1 = v[i + 1];
            uint64_t a0 = x0 < 0 ? -(uint64_t)x0 : (uint64_t)x0, a1 = x1 < 0 ? -(uint64_t)x1 : (uint64_t)x1;
            if ((a0 | a1) > 99999999u && (a0 > 99999999u || a1 > 99999999u)) {
                for (int j = 0; j < 2; j++) { int d = ds[i + j]; uint8_t *q = o + (size_t)FMT_STRIDE * (i + j); lv[i + j] = (uint8_t)(fmt_dec_swar(q, v[i + j], S) - d - (d == S && d) - q); }
                continue;
            }
            uint32x2_t a32 = vcreate_u32(a0 | (a1 << 32));
            uint32x2_t hi = vmovn_u64(vshrq_n_u64(vmull_u32(a32, vdup_n_u32(0xD1B71759u)), 45));
            uint32x2_t lo = vmls_u32(a32, hi, vdup_n_u32(10000));
            uint32x4_t mg = vcombine_u32(vzip1_u32(hi, lo), vzip2_u32(hi, lo));      /* [hi0 lo0 hi1 lo1] */
            uint32x4_t top = vshrq_n_u32(vmulq_n_u32(mg, 10486), 20);
            uint32x4_t bot = vmlsq_n_u32(mg, top, 100);
            uint16x8_t hd = vreinterpretq_u16_u32(vorrq_u32(top, vshlq_n_u32(bot, 16)));
            uint16x8_t tn = vshrq_n_u16(vmulq_n_u16(hd, 103), 10);
            uint16x8_t un = vmlsq_n_u16(hd, tn, 10);
            uint8x16_t dig = vaddq_u8(vreinterpretq_u8_u16(vorrq_u16(tn, vshlq_n_u16(un, 8))), vdupq_n_u8('0'));
            uint64_t aa[2] = {a0, a1}; int64_t xx[2] = {x0, x1};
            for (int j = 0; j < 2; j++) {
                uint64_t a = aa[j];
                int D = 1 + (a > 9) + (a > 99) + (a > 999) + (a > 9999) + (a > 99999) + (a > 999999) + (a > 9999999);
                D = D < S + 1 ? S + 1 : D; int ng = xx[j] < 0;
                uint8x16_t dj = j ? vextq_u8(dig, dig, 8) : dig;
                uint8x16_t r = vorrq_u8(vqtbl1q_u8(dj, vld1q_u8(FMT_SHA[ng][D * 8 + S])), vld1q_u8(FMT_ORA[ng][D * 8 + S]));
                vst1q_u8(o + (size_t)FMT_STRIDE * (i + j), r);
                int d = ds[i + j];
                lv[i + j] = (uint8_t)(ng + D + (S > 0) - d - (d == S && d));
            }
        }
    }
#endif
#if defined(__AVX2__)
    if (S < 8) {
        const __m256i M = _mm256_set1_epi64x(0xD1B71759), K10k = _mm256_set1_epi64x(10000), z = _mm256_setzero_si256();
        const __m256i lim = _mm256_set1_epi64x(99999999);
        const __m256i d100 = _mm256_set1_epi16(0x147B), c100 = _mm256_set1_epi16(100), d10 = _mm256_set1_epi16(0x199A), c10 = _mm256_set1_epi16(10);
        const __m256i asc = _mm256_set1_epi8('0'), S1 = _mm256_set1_epi64x(S + 1), SS = _mm256_set1_epi64x(S);
        for (; i + 4 <= n; i += 4) {
            __m256i x = _mm256_loadu_si256((const __m256i *)(v + i));
            __m256i neg = _mm256_cmpgt_epi64(z, x);
            __m256i a = _mm256_sub_epi64(_mm256_xor_si256(x, neg), neg);
            __m256i big = _mm256_cmpgt_epi64(a, lim);
            if (!_mm256_testz_si256(big, big)) {
                for (int j = 0; j < 4; j++) { int d = ds[i + j]; uint8_t *q = o + (size_t)FMT_STRIDE * (i + j); lv[i + j] = (uint8_t)(fmt_dec_swar(q, v[i + j], S) - d - (d == S && d) - q); }
                continue;
            }
            __m256i hi = _mm256_srli_epi64(_mm256_mul_epu32(a, M), 45);
            __m256i lo = _mm256_sub_epi64(a, _mm256_mul_epu32(hi, K10k));
            __m256i mg = _mm256_or_si256(hi, _mm256_slli_epi64(lo, 32));
            __m256i top = _mm256_srli_epi16(_mm256_mulhi_epu16(mg, d100), 3);
            __m256i bot = _mm256_sub_epi16(mg, _mm256_mullo_epi16(top, c100));
            __m256i hd = _mm256_or_si256(top, _mm256_slli_epi32(bot, 16));
            __m256i tn = _mm256_mulhi_epu16(hd, d10);
            __m256i un = _mm256_sub_epi16(hd, _mm256_mullo_epi16(tn, c10));
            __m256i dig = _mm256_add_epi8(_mm256_or_si256(tn, _mm256_slli_epi16(un, 8)), asc);
            __m256i D = _mm256_set1_epi64x(1);
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(9)));
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(99)));
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(999)));
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(9999)));
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(99999)));
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(999999)));
            D = _mm256_sub_epi64(D, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(9999999)));
            D = _mm256_blendv_epi8(D, S1, _mm256_cmpgt_epi64(S1, D));                     /* at least S + 1 digits */
            __m256i idx = _mm256_add_epi64(_mm256_slli_epi64(D, 3), SS);
            idx = _mm256_add_epi64(idx, _mm256_and_si256(neg, _mm256_set1_epi64x(9 * 8)));   /* negative: second table */
            /* lanes stay in registers (no store and reload: forwarding from a 256-bit store to narrow loads stalls) */
            __m256i len = _mm256_add_epi64(_mm256_sub_epi64(D, neg), _mm256_set1_epi64x(S > 0));
            const uint8_t (*SH)[16] = &FMT_SHN[0][0], (*OR)[16] = &FMT_ORN[0][0];
            __m128i dl = _mm256_castsi256_si128(dig), dh = _mm256_extracti128_si256(dig, 1);
            __m128i il = _mm256_castsi256_si128(idx), ih = _mm256_extracti128_si256(idx, 1);
            __m128i ll = _mm256_castsi256_si128(len), lh = _mm256_extracti128_si256(len, 1);
            __m128i dv[4] = {dl, _mm_srli_si128(dl, 8), dh, _mm_srli_si128(dh, 8)};
            uint32_t qv[4] = {(uint32_t)_mm_cvtsi128_si64(il), (uint32_t)_mm_extract_epi64(il, 1), (uint32_t)_mm_cvtsi128_si64(ih), (uint32_t)_mm_extract_epi64(ih, 1)};
            int lw[4] = {(int)_mm_cvtsi128_si64(ll), (int)_mm_extract_epi64(ll, 1), (int)_mm_cvtsi128_si64(lh), (int)_mm_extract_epi64(lh, 1)};
            for (int j = 0; j < 4; j++) {
                uint32_t q = qv[j];
                __m128i r = _mm_or_si128(_mm_shuffle_epi8(dv[j], _mm_load_si128((const __m128i *)SH[q])), _mm_load_si128((const __m128i *)OR[q]));
                _mm_storeu_si128((__m128i *)(o + (size_t)FMT_STRIDE * (i + j)), r);
                int d = ds[i + j];
                lv[i + j] = (uint8_t)(lw[j] - d - (d == S && d));
            }
        }
    }
#endif
    for (; i < n; i++) { int d = ds[i]; uint8_t *q = o + (size_t)FMT_STRIDE * i; lv[i] = (uint8_t)(fmt_dec_swar(q, v[i], S) - d - (d == S && d) - q); }
}
static uint8_t *fmt_run(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0;
#if defined(__AVX2__)
    if (S < 8) {
        const __m256i M = _mm256_set1_epi64x(0xD1B71759), K10k = _mm256_set1_epi64x(10000), z = _mm256_setzero_si256();
        const __m256i lim = _mm256_set1_epi64x(99999999);
        const __m256i d100 = _mm256_set1_epi16(0x147B), c100 = _mm256_set1_epi16(100), d10 = _mm256_set1_epi16(0x199A), c10 = _mm256_set1_epi16(10);
        const __m256i asc = _mm256_set1_epi8('0');
        for (; i + 4 <= n; i += 4) {
            __m256i x = _mm256_loadu_si256((const __m256i *)(v + i));
            __m256i neg = _mm256_cmpgt_epi64(z, x);
            __m256i a = _mm256_sub_epi64(_mm256_xor_si256(x, neg), neg);
            if (!_mm256_testz_si256(_mm256_cmpgt_epi64(a, lim), _mm256_cmpgt_epi64(a, lim))) {
                for (int j = 0; j < 4; j++) { int d = ds[i + j]; uint8_t *e = fmt_dec_swar(o, v[i + j], S) - d - (d == S && d); lv[i + j] = (uint8_t)(e - o); o = e; }
                continue;
            }
            __m256i hi = _mm256_srli_epi64(_mm256_mul_epu32(a, M), 45);
            __m256i lo = _mm256_sub_epi64(a, _mm256_mul_epu32(hi, K10k));
            __m256i mg = _mm256_or_si256(hi, _mm256_slli_epi64(lo, 32));
            __m256i top = _mm256_srli_epi16(_mm256_mulhi_epu16(mg, d100), 3);
            __m256i bot = _mm256_sub_epi16(mg, _mm256_mullo_epi16(top, c100));
            __m256i hd = _mm256_or_si256(top, _mm256_slli_epi32(bot, 16));
            __m256i tn = _mm256_mulhi_epu16(hd, d10);
            __m256i un = _mm256_sub_epi16(hd, _mm256_mullo_epi16(tn, c10));
            __m256i dig = _mm256_add_epi8(_mm256_or_si256(tn, _mm256_slli_epi16(un, 8)), asc);
            __m256i cnt = _mm256_set1_epi64x(1);
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(9)));
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(99)));
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(999)));
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(9999)));
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(99999)));
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(999999)));
            cnt = _mm256_sub_epi64(cnt, _mm256_cmpgt_epi64(a, _mm256_set1_epi64x(9999999)));
            uint64_t dg[4], cn[4], ng[4];
            _mm256_storeu_si256((__m256i *)dg, dig); _mm256_storeu_si256((__m256i *)cn, cnt); _mm256_storeu_si256((__m256i *)ng, neg);
            for (int j = 0; j < 4; j++) {
                int D = (int)cn[j]; D = D < S + 1 ? S + 1 : D;
                __m128i r = _mm_or_si128(_mm_shuffle_epi8(_mm_cvtsi64_si128((long long)dg[j]), _mm_load_si128((const __m128i *)FMT_SH[D * 8 + S])),
                                         _mm_load_si128((const __m128i *)FMT_DOT[D * 8 + S]));
                uint8_t *q = o; *q = '-'; q += ng[j] & 1;
                _mm_storeu_si128((__m128i *)q, r);
                int d = ds[i + j];
                uint8_t *e = q + D + (S > 0) - d - (d == S && d);
                lv[i + j] = (uint8_t)(e - o); o = e;
            }
        }
    }
#endif
    for (; i < n; i++) { int d = ds[i]; uint8_t *e = fmt_dec_swar(o, v[i], S) - d - (d == S && d); lv[i] = (uint8_t)(e - o); o = e; }
    return o;
}

/* Per-thread reuse of large scratch blocks in the transforms. They allocate and free blocks of similar sizes block
 * after block; handing the same pages back avoids the page faults of fresh memory, which on the fast paths cost as
 * much as the work itself. Workers release the cache when they finish a call. */
typedef struct { size_t n, cap; } zs_hdr;                 /* 16 bytes: keeps malloc alignment */
typedef struct { zs_hdr *h; } zs_ent;
static _Thread_local zs_ent zs_cache[24]; static _Thread_local int zs_n;
#define ZS_MIN ((size_t)64 << 10)
static void *zs_malloc(size_t n) {
    if (n >= ZS_MIN) {
        int bi = -1;
        for (int i = 0; i < zs_n; i++) { size_t c = zs_cache[i].h->cap; if (c >= n && c <= 4 * n && (bi < 0 || c < zs_cache[bi].h->cap)) bi = i; }
        if (bi >= 0) { zs_hdr *h = zs_cache[bi].h; zs_cache[bi] = zs_cache[--zs_n]; h->n = n; return h + 1; }
    }
    if (n > SIZE_MAX - sizeof(zs_hdr)) return NULL;
    zs_hdr *h = malloc(sizeof(zs_hdr) + n); if (!h) return NULL;
    h->n = n; h->cap = n; return h + 1;
}
static void zs_free(void *p) {
    if (!p) return;
    zs_hdr *h = (zs_hdr *)p - 1;
    if (h->cap >= ZS_MIN && zs_n < 24) { zs_cache[zs_n++].h = h; return; }
    free(h);
}
static void *zs_calloc(size_t a, size_t b) {
    if (b && a > SIZE_MAX / b) return NULL;
    void *p = zs_malloc(a * b); if (p) memset(p, 0, a * b); return p;
}
static void *zs_realloc(void *p, size_t n) {
    if (!p) return zs_malloc(n);
    zs_hdr *h = (zs_hdr *)p - 1;
    if (n <= h->cap) { h->n = n; return p; }
    void *q = zs_malloc(n); if (!q) return NULL;
    memcpy(q, p, h->n); zs_free(p); return q;
}
static void zs_trim(void) { for (int i = 0; i < zs_n; i++) free(zs_cache[i].h); zs_n = 0; }
typedef struct { uint8_t *p; size_t n, cap; } Buf;
static void bput(Buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = zs_realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n); b->n += n;
}

/* encode one column given its fields (offsets/lengths into in); appends type+size+section to out */
static void col_encode(const uint8_t *in, const uint32_t *off, const uint32_t *len, uint32_t cnt, Buf *out) {
    /* try numeric */
    int64_t *v = malloc(8 * (cnt + 1)); uint8_t *kind = malloc(cnt + 1), *sc = malloc(cnt + 1);
    uint32_t nnum = 0, nexc = 0, nempty = 0; int S = 0;
    for (uint32_t i = 0; i < cnt; i++) {
        int64_t m; int s;
        if (len[i] == 0) { kind[i] = 0; nempty++; }
        else if (parse_dec(in + off[i], len[i], &m, &s)) { kind[i] = 1; v[nnum] = m; sc[nnum] = (uint8_t)s; nnum++; if (s > S) S = s; }
        else { kind[i] = 2; nexc++; }
    }
    int numeric = nnum > 0 && nexc * 20 <= cnt && nnum * 4 >= cnt - nempty;
    if (numeric) {                                          /* common scale; overflow -> text */
        for (uint32_t i = 0; i < nnum && numeric; i++) if (__builtin_mul_overflow(v[i], P10[S - sc[i]], &v[i]) || v[i] > (1LL << 62) || v[i] < -(1LL << 62)) numeric = 0;
    }
    if (!numeric) {
        uint32_t nlong = 0; for (uint32_t i = 0; i < cnt; i++) nlong += len[i] >= 255;
        size_t data = 0; for (uint32_t i = 0; i < cnt; i++) data += len[i];
        uint8_t t = 0; uint32_t size = 8 + cnt + 4 * nlong + data; bput(out, &t, 1); bput(out, &size, 4);
        bput(out, &cnt, 4); bput(out, &nlong, 4);
        size_t at = out->n; bput(out, kind, 0);
        if (out->n + cnt > out->cap) { out->cap = (out->n + cnt) * 2; out->p = zs_realloc(out->p, out->cap); }
        for (uint32_t i = 0; i < cnt; i++) out->p[at + i] = len[i] >= 255 ? 255 : (uint8_t)len[i];
        out->n += cnt;
        for (uint32_t i = 0; i < cnt; i++) if (len[i] >= 255) bput(out, &len[i], 4);
        for (uint32_t i = 0; i < cnt; i++) bput(out, in + off[i], len[i]);
    } else {
        uint64_t mx = 0; int64_t prev = 0;
        uint64_t *z = malloc(8 * (nnum + 1));
        for (uint32_t i = 0; i < nnum; i++) { uint64_t d = (uint64_t)v[i] - (uint64_t)prev; prev = v[i]; int64_t sd = (int64_t)d; z[i] = ((uint64_t)sd << 1) ^ (uint64_t)(sd >> 63); if (z[i] > mx) mx = z[i]; }
        int w = 1; while (w < 8 && (mx >> (8 * w))) w++;
        uint32_t elen = 0; for (uint32_t i = 0; i < cnt; i++) if (kind[i] == 2) elen += len[i];
        uint8_t t = 1; uint32_t size = 4 * 3 + 2 + cnt + nnum + (size_t)nnum * w + 4 * nexc + elen;
        bput(out, &t, 1); bput(out, &size, 4); bput(out, &cnt, 4); bput(out, &nnum, 4); bput(out, &nexc, 4);
        uint8_t Sw[2] = {(uint8_t)S, (uint8_t)w}; bput(out, Sw, 2);
        bput(out, kind, cnt);
        for (uint32_t i = 0; i < nnum; i++) sc[i] = (uint8_t)(S - sc[i]);
        bput(out, sc, nnum);
        size_t at = out->n; if (out->n + (size_t)nnum * w > out->cap) { out->cap = (out->n + (size_t)nnum * w) * 2 + 16; out->p = zs_realloc(out->p, out->cap); }
        for (int b = 0; b < w; b++) for (uint32_t i = 0; i < nnum; i++) out->p[at + (size_t)b * nnum + i] = (uint8_t)(z[i] >> (8 * b));
        out->n += (size_t)nnum * w;
        for (uint32_t i = 0; i < cnt; i++) if (kind[i] == 2) bput(out, &len[i], 4);
        for (uint32_t i = 0; i < cnt; i++) if (kind[i] == 2) bput(out, in + off[i], len[i]);
        free(z);
    }
    free(v); free(kind); free(sc);
}

static int64_t tbl_encode(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, uint8_t *dst) {
    if (quotes && !has_quotes(in, n)) quotes = 0;
    uint32_t *rec = malloc(4 * (n + 2)), *exc = malloc(8 * (n + 2));
    uint32_t colcnt[256] = {0}; int64_t nf = 0, nr = 0, nexc = 0; int noNL = 0;
    int64_t i = 0;
    while (i < n) {
        int j = 0;
        for (;;) {
            int split = j < k - 1; size_t e;
            if (quotes) e = field_end_q(in, i, n, delim, split);
            else e = i + (split ? scan2(in + i, n - i, delim, '\n') : scan2(in + i, n - i, '\n', '\n'));
            rec[nf++] = (uint32_t)(e - i); colcnt[j]++;
            if (e >= (size_t)n) { noNL = 1; nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } i = n; break; }
            if (in[e] == '\n') { nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } i = e + 1; break; }
            j++; i = e + 1;
            if (i == n) { rec[nf++] = 0; colcnt[j]++; noNL = 1; nr++; if (j + 1 != k) { exc[2 * nexc] = nr - 1; exc[2 * nexc + 1] = j + 1; nexc++; } break; }
        }
    }
    /* column-major (offset, length) lists */
    uint32_t *coff = malloc(4 * (nf + 1)), *clen = malloc(4 * (nf + 1)); int64_t lpos[256], pos = 0, base[256];
    for (int j = 0; j < k; j++) { base[j] = lpos[j] = pos; pos += colcnt[j]; }
    int64_t f = 0, ei = 0, src = 0;
    for (int64_t r = 0; r < nr; r++) {
        int cnt = (ei < nexc && exc[2 * ei] == r) ? (int)exc[2 * ei++ + 1] : k;
        for (int j = 0; j < cnt; j++) { uint32_t L = rec[f++]; coff[lpos[j]] = (uint32_t)src; clen[lpos[j]++] = L; src += L + 1; }
    }
    Buf out = {0}; uint32_t h[4] = {(uint32_t)k, (uint32_t)nr, (uint32_t)nexc, (uint32_t)noNL};
    bput(&out, h, 16); bput(&out, exc, 8 * nexc);
    for (int j = 0; j < k; j++) col_encode(in, coff + base[j], clen + base[j], colcnt[j], &out);
    free(rec); free(exc); free(coff); free(clen);
    int64_t total = out.n;
    if (total > n + TSLACK - 64) { zs_free(out.p); return -1; }
    memcpy(dst, out.p, total); zs_free(out.p);
    return total;
}

typedef struct { int type; uint32_t cnt, nnum, nexc, nlong; int S, w;
                 const uint8_t *lens, *longs, *data, *dend, *kind, *sd, *pl, *elen, *edata; uint32_t ki, ni, li, xi; int64_t prev; int64_t *vals; } Col;

static int tbl_decode(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n) {
    if (tn < 16) return -1;
    uint32_t h[4]; memcpy(h, in, 16); int k = h[0]; int64_t nr = h[1], nexc = h[2], noNL = h[3];
    if (k < 1 || k > 256 || 16 + 8 * nexc > tn) return -1;
    const uint8_t *exc = in + 16, *p = in + 16 + 8 * nexc, *end = in + tn;
    Col C[256];
    for (int j = 0; j < k; j++) {
        if (p + 5 > end) return -1;
        Col *c = &C[j]; memset(c, 0, sizeof *c); c->type = p[0]; uint32_t size; memcpy(&size, p + 1, 4); p += 5;
        if (c->type > 1) return -1;
        const uint8_t *q = p, *qe = p + size; if (qe > end) return -1;
        if (c->type == 0) {
            if (size < 8) return -1;
            memcpy(&c->cnt, q, 4); memcpy(&c->nlong, q + 4, 4); q += 8;
            c->lens = q; q += c->cnt; c->longs = q; q += 4ull * c->nlong; c->data = q; c->dend = qe;
            if (q > qe) return -1;
        } else {
            if (size < 14) return -1;
            memcpy(&c->cnt, q, 4); memcpy(&c->nnum, q + 4, 4); memcpy(&c->nexc, q + 8, 4); c->S = q[12]; c->w = q[13]; q += 14;
            if (c->S > 18 || c->w < 1 || c->w > 8) return -1;
            c->kind = q; q += c->cnt; c->sd = q; q += c->nnum; c->pl = q; q += (size_t)c->nnum * c->w;
            c->elen = q; q += 4ull * c->nexc; c->edata = q; c->dend = qe;
            if (q > qe) return -1;
        }
        p = qe;
    }
    /* numeric columns decoded column-at-a-time (sequential plane reads), mantissas ready for formatting */
    size_t totnum = 0; for (int j = 0; j < k; j++) if (C[j].type == 1) totnum += C[j].nnum;
    int64_t *vals = malloc(8 * (totnum + 1)), *vp = vals;
    if (!vals) return -1;
    for (int j = 0; j < k; j++) {
        Col *c = &C[j]; if (c->type != 1) continue;
        c->vals = vp; uint32_t m = c->nnum;
        memset(vp, 0, 8 * (size_t)m);
        for (int b = 0; b < c->w; b++) { const uint8_t *pl = c->pl + (size_t)b * m; for (uint32_t i = 0; i < m; i++) vp[i] |= (int64_t)((uint64_t)pl[i] << (8 * b)); }
        int64_t prev = 0;
        for (uint32_t i = 0; i < m; i++) {
            uint64_t zz = (uint64_t)vp[i]; int64_t d = (int64_t)(zz >> 1) ^ -(int64_t)(zz & 1);
            prev = (int64_t)((uint64_t)prev + (uint64_t)d);
            int ds = c->sd[i]; if (ds > c->S) goto fail;
            vp[i] = ds ? exact_div_p10(prev, ds) : prev;
        }
        vp += m;
    }
    /* phase A: every column as contiguous bytes + per-field lengths (numeric columns formatted here, column by column) */
    uint32_t *L = NULL, *rowoff = NULL, *cur = NULL; uint8_t *arena = NULL, *rcnt = NULL;
    const uint8_t *cb[256]; uint32_t *cl[256]; size_t nf = 0, ar = 0;
    for (int j = 0; j < k; j++) {
        nf += C[j].cnt;
        if (C[j].type == 1) ar += (size_t)C[j].nnum * 21 + (size_t)(C[j].dend - C[j].edata);
    }
    L = malloc(4 * (nf + 1)); arena = malloc(ar + 128);
    rowoff = malloc(4 * ((size_t)nr + 1)); cur = malloc(4 * ((size_t)nr + 1)); rcnt = malloc((size_t)nr + 1);
    if (!L || !arena || !rowoff || !cur || !rcnt) goto fail2;
    {
        uint32_t *lp = L; uint8_t *ap = arena;
        for (int j = 0; j < k; j++) {
            Col *c = &C[j]; cl[j] = lp;
            if (c->type == 0) {
                cb[j] = c->data; size_t tot = 0;
                for (uint32_t i2 = 0; i2 < c->cnt; i2++) {
                    uint32_t len = c->lens[i2];
                    if (len == 255) { if (c->li >= c->nlong) goto fail2; memcpy(&len, c->longs + 4ull * c->li++, 4); }
                    lp[i2] = len; tot += len;
                }
                if (c->data + tot > c->dend) goto fail2;
            } else {
                cb[j] = ap; uint32_t ni = 0, xi = 0; const uint8_t *ed = c->edata;
                for (uint32_t i2 = 0; i2 < c->cnt; i2++) {
                    uint8_t kd = c->kind[i2]; uint8_t *st = ap;
                    if (kd == 1) { if (ni >= c->nnum) goto fail2; int ds = c->sd[ni]; ap = fmt_dec_fast(ap, c->vals[ni], c->S - ds); ni++; }
                    else if (kd == 2) {
                        if (xi >= c->nexc) goto fail2;
                        uint32_t len; memcpy(&len, c->elen + 4ull * xi++, 4);
                        if (ed + len > c->dend || (size_t)(ap - arena) + len > ar) goto fail2;
                        memcpy(ap, ed, len); ap += len; ed += len;
                    } else if (kd != 0) goto fail2;
                    lp[i2] = (uint32_t)(ap - st);
                }
            }
            lp += c->cnt;
        }
    }
    /* phase B: fields per row, row lengths, row offsets */
    {
        memset(rcnt, (uint8_t)0, (size_t)nr); int64_t ei = 0;
        for (int64_t r = 0; r < nr; r++) rcnt[r] = 0;
        for (int64_t r = 0, e = 0; r < nr; r++) {
            int cnt = k;
            if (e < nexc) { uint32_t er; memcpy(&er, exc + 8 * e, 4); if (er == r) { uint32_t cc; memcpy(&cc, exc + 8 * e + 4, 4); cnt = (int)cc; e++; if (cnt < 1 || cnt > k) goto fail2; } }
            rcnt[r] = (uint8_t)(cnt - 1);                 /* k <= 256 */
            cur[r] = (uint32_t)(cnt - 1) + ((noNL && r == nr - 1) ? 0 : 1);
        }
        (void)ei;
        for (int j = 0; j < k; j++) {
            const uint32_t *lp = cl[j]; uint32_t i2 = 0, cntj = C[j].cnt;
            for (int64_t r = 0; r < nr; r++) {
                if (rcnt[r] < j) continue;
                if (i2 >= cntj) goto fail2;
                cur[r] += lp[i2++];
            }
            if (i2 != cntj) goto fail2;
        }
        uint64_t acc = 0;
        for (int64_t r = 0; r < nr; r++) { rowoff[r] = (uint32_t)acc; acc += cur[r]; cur[r] = 0; }
        rowoff[nr] = (uint32_t)acc;
        if (acc != (uint64_t)n) goto fail2;
    }
    /* phase C: column-major copy in row chunks that stay in cache; rows are independent inside a pass */
    {
        const uint8_t *sp[256], *se[256]; uint32_t ip[256];
        for (int j = 0; j < k; j++) { sp[j] = cb[j]; se[j] = C[j].type == 0 ? C[j].dend : arena + ar; ip[j] = 0; }
        const int64_t CH = 1024;
        for (int64_t r0 = 0; r0 < nr; r0 += CH) {
            int64_t r1 = r0 + CH < nr ? r0 + CH : nr;
            for (int j = 0; j < k; j++) {
                const uint32_t *lp = cl[j]; const uint8_t *src = sp[j], *send = se[j]; uint32_t i2 = ip[j];
                for (int64_t r = r0; r < r1; r++) {
                    int last = rcnt[r];
                    if (last < j) continue;
                    uint32_t len = lp[i2++]; uint8_t *d = out + rowoff[r] + cur[r], *rend = out + rowoff[r + 1];
                    if (len <= 16 && d + 16 <= rend && src + 16 <= send) memcpy(d, src, 16); else memcpy(d, src, len);
                    src += len; d += len;
                    if (j < last) *d = delim; else if (d < rend) *d = '\n';
                    cur[r] += len + 1;
                }
                sp[j] = src; ip[j] = i2;
            }
        }
    }
    free(L); free(arena); free(rowoff); free(cur); free(rcnt);
    free(vals); return 0;
fail2:
    free(L); free(arena); free(rowoff); free(cur); free(rcnt);
fail:
    free(vals); return -1;
}

/* exported for tests */
int64_t tbl_encode_pub(const uint8_t *in, int64_t n, uint8_t delim, int k, int quotes, uint8_t *dst) { return tbl_encode(in, n, delim, k, quotes, dst); }
int tbl_decode_pub(const uint8_t *in, int64_t tn, uint8_t delim, uint8_t *out, int64_t n) { return tbl_decode(in, tn, delim, out, n); }

/* tunable settings (defaults are the hand-tuned values; tune/ga.py searches them with -D overrides) */
#ifndef CZ_TBL_GAIN_PCT
#define CZ_TBL_GAIN_PCT 97      /* keep the table transform if the zstd -1 trial is below this % of plain */
#endif
#ifndef CZ_LOG_GAIN_PCT
#define CZ_LOG_GAIN_PCT 92
#endif
#ifndef CZ_WORD_GAIN_PCT
#define CZ_WORD_GAIN_PCT 99
#endif
#ifndef CZ_STRUCT_GAIN_PCT
#define CZ_STRUCT_GAIN_PCT 97
#endif
#ifndef CZ_HC_TEXT_LEVEL
#define CZ_HC_TEXT_LEVEL 22
#endif
#ifndef CZ_ECO_LEVEL
#define CZ_ECO_LEVEL 8
#endif
#ifndef CZ_FAST_LEVEL
#define CZ_FAST_LEVEL 3
#endif
#define malloc(n) zs_malloc(n)
#define calloc(a, b) zs_calloc(a, b)
#define realloc(p, n) zs_realloc(p, n)
#define free(p) zs_free(p)
int cz_plan_threads = 1;   /* threads for the formula search in make_plan (set per czn_compress call) */
#include "tbl2.inc"
#include "log.inc"
#include "sci.inc"
#include "word.inc"
#undef malloc
#undef calloc
#undef realloc
#undef free
#include "cm.inc"

static int tp_fieldcounts(const uint8_t *in, int64_t n, uint8_t delim, int quotes, int *cnt, int maxlines) {
    int j = 0, inq = 0, inbr = 0, maxc = 1 << 30, L = 0;
    for (int64_t i = 0; i < n && L < maxlines; i++) { uint8_t c = in[i]; if (c == '\n') cnt[L++] = j + 1; TP_STEP(c); }
    return L;
}

static inline uint64_t ldw(const uint8_t *p, int w) {
    switch (w) { case 1: return p[0]; case 2: { uint16_t v; memcpy(&v, p, 2); return v; }
                 case 4: { uint32_t v; memcpy(&v, p, 4); return v; } default: { uint64_t v; memcpy(&v, p, 8); return v; } }
}
static inline void stw(uint8_t *p, uint64_t v, int w) {
    switch (w) { case 1: p[0] = (uint8_t)v; break; case 2: { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); break; }
                 case 4: { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); break; } default: memcpy(p, &v, 8); }
}
#define SB 4096
static void shuffle_fwd(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out) {
    uint64_t prev[64] = {0}, tmp[SB]; uint64_t mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r0 = 0; r0 < rows; r0 += SB) {
        int64_t nb = rows - r0 < SB ? rows - r0 : SB;
        for (int ch = 0; ch < c; ch++) {
            uint64_t pv = prev[ch]; const uint8_t *p = in + (r0 * c + ch) * w;
            for (int64_t r = 0; r < nb; r++, p += (int64_t)c * w) { uint64_t v = ldw(p, w); tmp[r] = delta ? ((v - pv) & mask) : v; pv = v; }
            prev[ch] = pv;
            for (int b = 0; b < w; b++) { uint8_t *o = out + ((int64_t)b * c + ch) * rows + r0; for (int64_t r = 0; r < nb; r++) o[r] = (uint8_t)(tmp[r] >> (8 * b)); }
        }
    }
}
static void shuffle_inv(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out) {
    uint64_t prev[64] = {0}, tmp[SB]; uint64_t mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r0 = 0; r0 < rows; r0 += SB) {
        int64_t nb = rows - r0 < SB ? rows - r0 : SB;
        for (int ch = 0; ch < c; ch++) {
            memset(tmp, 0, nb * 8);
            for (int b = 0; b < w; b++) { const uint8_t *q = in + ((int64_t)b * c + ch) * rows + r0; for (int64_t r = 0; r < nb; r++) tmp[r] |= (uint64_t)q[r] << (8 * b); }
            uint64_t pv = prev[ch]; uint8_t *p = out + (r0 * c + ch) * w;
            for (int64_t r = 0; r < nb; r++, p += (int64_t)c * w) { uint64_t v = delta ? ((pv + tmp[r]) & mask) : tmp[r]; stw(p, v, w); pv = v; }
            prev[ch] = pv;
        }
    }
}

/* ------------------------------------------------------------------ codecs */
enum { C_STORE = 0, C_LZ4 = 1, C_LZ4HC = 2, C_ZSTD = 3, C_BROTLI = 4, C_BSC = 5, C_LZMA = 6, C_CM = 7 };
enum { T_NONE = 0, T_TP = 1, T_SHUF = 2, T_TBL = 3, T_LOG = 4, T_IMG = 5, T_REC = 6, T_X86 = 7, T_WORD = 8 };
/* x86 call/jump transform (E8/E9 with a rel32 whose top byte is 00 or FF): the low 24 bits become position-absolute,
 * so repeated calls to one target produce identical bytes. Encoded back to front and decoded front to back (as in
 * zpaq), so every decision sees the same bytes on both sides. */
/* x86 call/jump transform (zpaq order: encoder backward, decoder forward). The candidates (E8/E9 opcode with a
 * 0x00/0xFF top address byte four bytes on) are rare, so 16 positions are screened at a time with SIMD; after each
 * applied candidate the screen restarts behind it, because the rewritten bytes can create or remove candidates. */
static inline uint32_t e8_mask16(const uint8_t *b) {          /* bit k: position k is a candidate (b has 20 readable bytes) */
#if defined(__SSE2__)
    __m128i v = _mm_loadu_si128((const __m128i *)b), w = _mm_loadu_si128((const __m128i *)(b + 4));
    __m128i c1 = _mm_cmpeq_epi8(_mm_and_si128(v, _mm_set1_epi8((char)0xFE)), _mm_set1_epi8((char)0xE8));
    __m128i c2 = _mm_or_si128(_mm_cmpeq_epi8(w, _mm_setzero_si128()), _mm_cmpeq_epi8(w, _mm_set1_epi8((char)0xFF)));
    return (uint32_t)_mm_movemask_epi8(_mm_and_si128(c1, c2));
#elif defined(__ARM_NEON) && defined(__aarch64__)
    uint8x16_t v = vld1q_u8(b), w = vld1q_u8(b + 4);
    uint8x16_t c = vandq_u8(vceqq_u8(vandq_u8(v, vdupq_n_u8(0xFE)), vdupq_n_u8(0xE8)), vorrq_u8(vceqq_u8(w, vdupq_n_u8(0)), vceqq_u8(w, vdupq_n_u8(0xFF))));
    uint64_t m4 = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(c), 4)), 0);
    uint32_t m = 0; while (m4) { int t = __builtin_ctzll(m4); m |= 1u << (t >> 2); m4 &= ~(0xFull << (t & ~3)); }
    return m;
#else
    uint32_t m = 0; for (int k = 0; k < 16; k++) m |= (uint32_t)((b[k] & 0xFE) == 0xE8 && (uint8_t)(b[k + 4] + 1) <= 1) << k;
    return m;
#endif
}
static void e8e9_fwd(uint8_t *b, size_t n) {
    if (n < 5) return;
    size_t i = n - 5 + 1;                                     /* positions i - 1 down to 0 remain */
    while (i > 0) {
        if (i >= 16) {
            uint32_t m = e8_mask16(b + i - 16);
            if (!m) { i -= 16; continue; }
            int t = 31 - __builtin_clz(m); size_t k = i - 16 + (size_t)t;
            uint32_t a = (uint32_t)(b[k + 1] | b[k + 2] << 8 | b[k + 3] << 16) + (uint32_t)k;
            b[k + 1] = (uint8_t)a; b[k + 2] = (uint8_t)(a >> 8); b[k + 3] = (uint8_t)(a >> 16);
            i = k; continue;
        }
        size_t k = --i;
        if ((b[k] & 0xFE) == 0xE8 && (uint8_t)(b[k + 4] + 1) <= 1) {
            uint32_t a = (uint32_t)(b[k + 1] | b[k + 2] << 8 | b[k + 3] << 16) + (uint32_t)k;
            b[k + 1] = (uint8_t)a; b[k + 2] = (uint8_t)(a >> 8); b[k + 3] = (uint8_t)(a >> 16);
        }
    }
}
static void e8e9_inv(uint8_t *b, size_t n) {
    if (n < 5) return;
    size_t i = 0;
    while (i + 5 <= n) {
        if (i + 20 <= n) {
            uint32_t m = e8_mask16(b + i);
            if (!m) { i += 16; continue; }
            i += (size_t)__builtin_ctz(m);
        } else if (!((b[i] & 0xFE) == 0xE8 && (uint8_t)(b[i + 4] + 1) <= 1)) { i++; continue; }
        uint32_t a = (uint32_t)(b[i + 1] | b[i + 2] << 8 | b[i + 3] << 16) - (uint32_t)i;
        b[i + 1] = (uint8_t)a; b[i + 2] = (uint8_t)(a >> 8); b[i + 3] = (uint8_t)(a >> 16);
        i++;
    }
}

static void bsc_once(void) { bsc_init(LIBBSC_FEATURE_FASTMODE); }
static int looks_text(const uint8_t *s, size_t n) {          /* >= 95% printable in 4 slices of 16 KB */
    if (n < 4096) return 0;
    size_t tot = 0, pr = 0;
    for (int k = 0; k < 4; k++) {
        size_t st = n / 4 * k, m = n - st < 16384 ? n - st : 16384;
        for (size_t i = st; i < st + m; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 0xC2; }
        tot += m;
    }
    return pr * 100 >= tot * 95;
}
static size_t codec_bound(int codec, size_t n) {
    switch (codec) {
        case C_LZ4: case C_LZ4HC: return (size_t)LZ4_compressBound((int)n);
        case C_ZSTD: return ZSTD_compressBound(n);
        case C_BROTLI: return BrotliEncoderMaxCompressedSize(n) ? BrotliEncoderMaxCompressedSize(n) : n + 1024;
        case C_BSC: return n + LIBBSC_HEADER_SIZE + 64;
#ifndef CZN_NO_LZMA
        case C_LZMA: return lzma_stream_buffer_bound(n) + 64;
#else
        case C_LZMA: return n + n / 8 + 64;
#endif
        case C_CM: return n + n / 16 + 64;
        default: return n;
    }
}

typedef struct { ZSTD_CCtx *zc; ZSTD_DCtx *zd; void *lz4hc_state; const void *dict; size_t dictlen; } codec_ctx;

static size_t codec_compress(codec_ctx *x, int codec, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    switch (codec) {
        case C_LZ4: { int r = LZ4_compress_fast((const char *)in, (char *)out, (int)n, (int)cap, level > 0 ? level : 1); return r > 0 ? (size_t)r : 0; }
        case C_LZ4HC: {
            if (!x->lz4hc_state) x->lz4hc_state = malloc(LZ4_sizeofStateHC());
            int r = LZ4_compress_HC_extStateHC(x->lz4hc_state, (const char *)in, (char *)out, (int)n, (int)cap, level);
            return r > 0 ? (size_t)r : 0; }
        case C_ZSTD: {
            if (!x->zc) x->zc = ZSTD_createCCtx();
            size_t r = x->dict ? ZSTD_compress_usingDict(x->zc, out, cap, in, n, x->dict, x->dictlen, level)
                               : ZSTD_compressCCtx(x->zc, out, cap, in, n, level);
            return ZSTD_isError(r) ? 0 : r; }
        case C_BROTLI: {
            size_t o = cap;
            int ok = BrotliEncoderCompress(level, 24, BROTLI_MODE_GENERIC, n, in, &o, out);
            return ok ? o : 0; }
        case C_BSC: {                       /* payload: XXH32 of the bsc stream | bsc stream (checked before decoding) */
            static pthread_once_t once = PTHREAD_ONCE_INIT; pthread_once(&once, bsc_once);
            if (n > 0x7FFFFFF0 || cap < n + LIBBSC_HEADER_SIZE + 4) return 0;
            int r = bsc_compress(in, out + 4, (int)n, LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, LIBBSC_BLOCKSORTER_BWT,
                                 LIBBSC_CODER_QLFC_STATIC, LIBBSC_FEATURE_FASTMODE);
            if (r <= 0) return 0;
            uint32_t h = (uint32_t)XXH64(out + 4, (size_t)r, 0x62736331); memcpy(out, &h, 4);
            return (size_t)r + 4; }
#ifdef CZN_NO_LZMA
        case C_LZMA: return 0;                /* built without liblzma: block stored or another codec used */
#else
        case C_LZMA: {                      /* payload: dict u32 | lc lp pb | raw LZMA2 */
            lzma_options_lzma opt; if (lzma_lzma_preset(&opt, 9 | LZMA_PRESET_EXTREME)) return 0;
            uint32_t d = 4096; while (d < n && d < (64u << 20)) d <<= 1; opt.dict_size = d;
            if (level >= 100) { opt.lc = (level / 100) % 10 - 1; opt.lp = (level / 10) % 10; opt.pb = level % 10; }
            lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
            if (cap < 8) return 0;
            memcpy(out, &d, 4); out[4] = (uint8_t)opt.lc; out[5] = (uint8_t)opt.lp; out[6] = (uint8_t)opt.pb; out[7] = 0;
            size_t pos = 8;
            if (lzma_raw_buffer_encode(f, NULL, in, n, out, &pos, cap) != LZMA_OK) return 0;
            return pos; }
#endif
        case C_CM: return cm_compress(in, n, out, cap);
    }
    return 0;
}

static int codec_decompress(codec_ctx *x, int codec, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    switch (codec) {
        case C_STORE: if (cn != n) return -1; memcpy(out, in, n); return 0;
        case C_LZ4: case C_LZ4HC: { int r = LZ4_decompress_safe((const char *)in, (char *)out, (int)cn, (int)n); return r == (int)n ? 0 : -1; }
        case C_ZSTD: { if (!x->zd) x->zd = ZSTD_createDCtx();
            size_t r = x->dict ? ZSTD_decompress_usingDict(x->zd, out, n, in, cn, x->dict, x->dictlen)
                               : ZSTD_decompressDCtx(x->zd, out, n, in, cn);
            return (!ZSTD_isError(r) && r == n) ? 0 : -1; }
        case C_BROTLI: { size_t o = n; int r = BrotliDecoderDecompress(cn, in, &o, out); return (r == BROTLI_DECODER_RESULT_SUCCESS && o == n) ? 0 : -1; }
        case C_BSC: {
            static pthread_once_t once = PTHREAD_ONCE_INIT; pthread_once(&once, bsc_once);
            if (cn < 4 + LIBBSC_HEADER_SIZE || n > 0x7FFFFFF0 || cn > 0x7FFFFFF0) return -1;
            uint32_t h; memcpy(&h, in, 4); if (h != (uint32_t)XXH64(in + 4, cn - 4, 0x62736331)) return -1;
            int bs, ds; if (bsc_block_info(in + 4, LIBBSC_HEADER_SIZE, &bs, &ds, LIBBSC_FEATURE_FASTMODE) != LIBBSC_NO_ERROR) return -1;
            if ((size_t)bs != cn - 4 || (size_t)ds != n) return -1;
            return bsc_decompress(in + 4, (int)(cn - 4), out, (int)n, LIBBSC_FEATURE_FASTMODE) == LIBBSC_NO_ERROR ? 0 : -1; }
#ifdef CZN_NO_LZMA
        case C_LZMA: return -1;
#else
        case C_LZMA: {
            if (cn < 8) return -1;
            uint32_t d; memcpy(&d, in, 4);
            if (d < 4096 || d > (64u << 20) || in[4] > 4 || in[5] > 4 || in[6] > 4 || in[4] + in[5] > 4) return -1;
            lzma_options_lzma opt; lzma_lzma_preset(&opt, 6); opt.dict_size = d; opt.lc = in[4]; opt.lp = in[5]; opt.pb = in[6];
            lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
            size_t ip = 8, op = 0;
            if (lzma_raw_buffer_decode(f, NULL, in, &ip, cn, out, &op, n) != LZMA_OK) return -1;
            return (ip == cn && op == n) ? 0 : -1; }
#endif
        case C_CM: return cm_decompress(in, cn, out, n);
    }
    return -1;
}

static void codec_ctx_free(codec_ctx *x) { ZSTD_freeCCtx(x->zc); ZSTD_freeDCtx(x->zd); free(x->lz4hc_state); }

/* per-thread state kept across calls (pool threads persist): codec contexts and scratch buffers */
typedef struct { codec_ctx x; uint8_t *tbuf, *slot, *rbuf; uint32_t *pos, *rowbuf; size_t tcap, scap, rbcap, pcap, rcap; } tls_t;
static __thread tls_t TLS;
static void *grow(void *p, size_t *cap, size_t need) {
    if (need <= *cap) return p;
    free(p); p = malloc(need); *cap = p ? need : 0; return p;
}

/* ------------------------------------------------------------------ container layout
 * header (32 B): "CZN1" | ver u8 | mode u8 | transform u8 | a u8 | b u8 | c u8 | flags u8 | reserved u8 |
 *                orig u64 | nblocks u32 | block u32 | crc32c(header[0..27] + index) u32
 * index: nblocks x { raw u32 | comp u32 | block checksum u32 | codec u8 | tflag u8 | reserved u16 }
 * block checksum: low 32 bits of XXH3-64(raw) from version 3, of XXH64(raw) in versions 1 and 2. Flags bit 0 (rt): the
 * checksum instead covers the stored payload after its 4 plan bytes, seeded with them, and is checked before decoding
 * blocks: payloads back to back; a transposed block starts with k x u32 column sizes */
#define HDR 32
#define IDX 16
static inline uint32_t blk_sum(int ver, const uint8_t *p, size_t n) {
    return ver >= 3 ? (uint32_t)czn_xxh3(p, n, 0x637a6e31) : (uint32_t)XXH64(p, n, 0x637a6e31);
}

static inline void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static inline void put64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t get64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

typedef struct { int t, a, b, c; fset_t *fs; img_plan_t img; rec_plan_t *rec; } plan_t;   /* tp/tbl: a=delim b=quotes c=k-1 | shuf: a=w b=ch c=delta; fs: formulas (encoder) */

static int default_threads(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1; }

static void mode_codec(const czn_params *p, int *codec, int *level) {
    switch (p->mode) {
        case CZN_RT:  *codec = C_LZ4;    *level = p->level ? p->level : 1; break;
        case CZN_FAST:*codec = C_ZSTD;   *level = p->level ? p->level : CZ_FAST_LEVEL; break;
        case CZN_ECO: *codec = C_BROTLI; *level = p->level ? p->level : CZ_ECO_LEVEL; break;
        case CZN_HC:  *codec = C_ZSTD;   *level = p->level ? p->level : 19; break;
        case CZN_MAX: *codec = C_LZMA;   *level = p->level ? p->level : 9; break;     /* max: per block BWT or LZMA2 */
        default:      *codec = C_CM;     *level = 0; break;                            /* ultra: context mixing */
    }
}

/* file-level analysis: one plan for every block */
static plan_t make_plan(const uint8_t *s, size_t n, int effort, int logs, int structured) {
    plan_t pl = {T_NONE, 0, 0, 0};
    if (n < 4096) return pl;
    size_t h = n < 65536 ? n : 65536, pr = 0;
    for (size_t i = 0; i < h; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13; }
    if (pr * 100 > h * 95) {                                     /* text: field transposition if lines have a fixed field count */
        static const struct { uint8_t d; int q; } D[] = {{',', 1}, {'\t', 0}, {';', 1}, {'|', 0}, {' ', 1}};
        int *cnt = malloc(4001 * sizeof(int)); size_t head = n < (1 << 16) ? n : (1 << 16);
        for (int di = 0; di < 5 && pl.t == T_NONE; di++) {
            int L = tp_fieldcounts(s, (int64_t)head, D[di].d, D[di].q, cnt, 4000);
            if (L < 51) continue;
            int best = 0, bc = 0;
            for (int i = 1; i < L; i++) { int k = cnt[i], c = 0; if (k == best) continue; for (int j = 1; j < L; j++) c += cnt[j] == k; if (c > bc) { bc = c; best = k; } }
            if (best >= 2 && best <= 256 && bc * 100 > (L - 1) * 95) { pl.t = T_TP; pl.a = D[di].d; pl.b = D[di].q; pl.c = best - 1; }
        }
        free(cnt);
        if (pl.t == T_TP) {                       /* typed table transform, kept only if it helps on a sample */
            size_t mm = head; while (mm > 0 && s[mm - 1] != '\n') mm--; if (!mm) mm = head;
            uint8_t *o = malloc(mm + TSLACK); int64_t tn = tbl_encode(s, mm, (uint8_t)pl.a, pl.c + 1, pl.b, o);
            ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm + TSLACK); uint8_t *z = malloc(cap);
            size_t a = tn > 0 ? ZSTD_compressCCtx(zc, z, cap, o, tn, 1) : (size_t)-1, b = ZSTD_compressCCtx(zc, z, cap, s, mm, 1);
            if (tn > 0 && !ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_TBL_GAIN_PCT * b) {
                pl.t = T_TBL; pl.fs = malloc(sizeof(fset_t));
                if (pl.fs) fset_discover(s, n, (uint8_t)pl.a, pl.c + 1, pl.b, effort, pl.fs);
            }
            free(o); free(z); ZSTD_freeCCtx(zc);
        }
        if (logs && pl.t == T_NONE && n >= 65536) {        /* logs: template transform, kept if it helps on a sample */
            size_t mm = n < (1u << 20) ? n : (1u << 20); while (mm > 0 && s[mm - 1] != '\n') mm--;
            if (mm >= 16384) {
                uint8_t *o = malloc(mm + 64); int64_t tn = log_encode(s, mm, o, mm + 64);
                if (tn > 0) {
                    ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm); uint8_t *z = malloc(cap);
                    size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 1), b = ZSTD_compressCCtx(zc, z, cap, s, mm, 1);
                    if (!ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_LOG_GAIN_PCT * b) pl.t = T_LOG;
                    free(z); ZSTD_freeCCtx(zc);
                }
                free(o);
            }
        }
        if (logs && effort && pl.t == T_NONE && n >= 65536) {   /* prose and markup (eco and above): word dictionary */
            size_t mm = n < (2u << 20) ? n : (2u << 20);
            uint8_t *o = malloc(mm + 64); int64_t tn = word_encode(s, mm, o, mm + 64);
            if (tn > 0) {
                ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm); uint8_t *z = malloc(cap);
                size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 3), b = ZSTD_compressCCtx(zc, z, cap, s, mm, 3);
                if (!ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_WORD_GAIN_PCT * b) pl.t = T_WORD;
                free(z); ZSTD_freeCCtx(zc);
            }
            free(o);
        }
        return pl;
    }
    /* binary: numeric byte-plane shuffle, chosen by a zstd -1 trial on a 32 KB slice */
    size_t mid = n / 2, sl = n - mid < 32768 ? n - mid : 32768;
    const uint8_t *smp = s + mid;
    ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(sl);
    uint8_t *o = malloc(cap), *t = malloc(sl);
    size_t base = ZSTD_compressCCtx(zc, o, cap, smp, sl, 1);
    if (!ZSTD_isError(base) && base * 4 >= sl) {
        double best = base * 0.95;
        for (int w = 2; w <= 8; w *= 2) for (int c = 1; c <= 8; c++) {
            if (c == 5 || c == 6 || c == 7 || w * c > 32) continue;
            for (int dl = 1; dl >= 0; dl--) {
                size_t rows = sl / (w * c); if (!rows) continue;
                shuffle_fwd(smp, rows, w, c, dl, t);
                size_t z = ZSTD_compressCCtx(zc, o, cap, t, rows * w * c, 1);
                if (!ZSTD_isError(z) && z < best) { best = z; pl.t = T_SHUF; pl.a = w; pl.b = c; pl.c = dl; }
            }
        }
    }
    free(o); free(t);
    /* structure-aware candidates (16-bit image, fixed records) against the shuffle and plain coding, scored by zstd -3
     * on a slice of up to 1 MiB from the middle of the input; kept only when at least 3% smaller */
    if (structured && n >= (1u << 18)) {
        size_t L = n / 4 < (1u << 20) ? n / 4 : (1u << 20), st = (n / 2 - L / 2) & ~(size_t)7;
        size_t bc = L * 2 + 1024; uint8_t *buf = malloc(bc), *zb = malloc(ZSTD_compressBound(bc));
        size_t zcap = ZSTD_compressBound(bc);
        double plain = (double)zcost(zc, s + st, L, zb, zcap) / L, best = plain * CZ_STRUCT_GAIN_PCT / 100.0; plan_t keep = pl; int chosen = 0;
        if (pl.t == T_SHUF) {
            size_t u = (size_t)pl.a * pl.b, rows = L / u; shuffle_fwd(s + st, rows, pl.a, pl.b, pl.c, buf);
            double c = (double)zcost(zc, buf, rows * u, zb, zcap) / (rows * u);
            if (c < best) { best = c; chosen = 1; }
        }
        img_plan_t im;
        if (img16_detect(s, n, &im)) {
            int64_t e = img16_encode(s + st, L, st, &im, buf);
            if (e > 0) { double c = (double)zcost(zc, buf, (size_t)e, zb, zcap) / L; if (c < best) { best = c; keep = pl; keep.t = T_IMG; keep.img = im; chosen = 2; } }
        }
        rec_plan_t *R = malloc(sizeof(rec_plan_t));
        if (R && rec_detect(s, n, effort, R)) {
            size_t rs = (size_t)R->h + (st > (size_t)R->h ? (st - R->h) / R->P * R->P : 0); if (!rs) rs = R->P;
            size_t rl = L / R->P * R->P;
            if (rs + rl <= n) {
                int64_t e = rec_encode(s + rs, rl, rs, R, buf, bc);
                if (e > 0) { double c = (double)zcost(zc, buf, (size_t)e, zb, zcap) / rl; if (c < best) { best = c; keep = pl; keep.t = T_REC; keep.rec = R; R = NULL; chosen = 3; } }
            }
        }
        free(R);
        {   /* executables: x86 call/jump transform */
            memcpy(buf, s + st, L); e8e9_fwd(buf, L);
            double c = (double)zcost(zc, buf, L, zb, zcap) / L;
            if (c < best && chosen < 2 && pl.t != T_SHUF) { best = c; keep = pl; keep.t = T_X86; chosen = 4; }
            else if (c < best && chosen == 0) { best = c; keep = pl; keep.t = T_X86; chosen = 4; }
        }
        if (chosen >= 2) pl = keep;
        else if (!chosen && pl.t == T_SHUF && plain * 0.97 <= best) { /* shuffle did not beat plain coding at zstd -3 */ }
        free(buf); free(zb);
    }
    ZSTD_freeCCtx(zc);
    return pl;
}

/* block boundaries: newline-aligned for transposed text, element-aligned for shuffled arrays */
static uint32_t make_blocks(const uint8_t *s, size_t n, uint32_t B, plan_t pl, uint64_t **starts_out) {
    size_t cap = n / (B / 2 + 1) + 2; uint64_t *st = malloc(cap * sizeof(uint64_t)); uint32_t nb = 0; size_t pos = 0;
    if (pl.t == T_SHUF) { uint32_t u = pl.a * pl.b; B = B / u * u; if (!B) B = u; }
    if (pl.t == T_IMG) B &= ~1u;
    if (pl.t == T_REC) { B = B / pl.rec->P * pl.rec->P; if (!B) B = pl.rec->P; }
    while (pos < n) {
        size_t end = pos + B < n ? pos + B : n;
        if (pl.t == T_REC && pos == 0) end = (size_t)pl.rec->h + B < n ? (size_t)pl.rec->h + B : n;
        if ((pl.t == T_TP || pl.t == T_TBL || pl.t == T_LOG) && end < n) {
            size_t lo = pos + B / 2, e = end;
            while (e > lo && s[e - 1] != '\n') e--;
            if (e > lo) end = e;
        }
        if (nb + 1 >= cap) { cap *= 2; st = realloc(st, cap * sizeof(uint64_t)); }
        st[nb++] = pos; pos = end;
    }
    if (n == 0) st[nb++] = 0;
    st = realloc(st, (nb + 1) * sizeof(uint64_t)); st[nb] = n;
    *starts_out = st; return nb;
}

static size_t slot_size(uint32_t B, int codec) { size_t a = codec_bound(codec, (size_t)B + TSLACK), b = codec_bound(C_BSC, (size_t)B + TSLACK); return (a > b ? a : b) + 256 * 4 + 64; }

/* Speed-tier (rt/fast) default block size: aim for ~8 balanced blocks so decode and
   compress parallelise across cores, with a 1 MiB floor (below it ratio drops faster
   than speed rises) and the 4 MiB ceiling (so large inputs are unchanged). A pure
   function of n, so output stays byte-identical across machines regardless of cores. */
static uint32_t czn_fast_block(size_t n) {
    uint64_t b = (uint64_t)n / 8;
    if (b < (1u << 20)) b = 1u << 20;
    if (b > (4u << 20)) b = 4u << 20;
    return (uint32_t)b;
}
/* worst case: every block stored raw (+ index entry); blocks are >= B/2 except the last */
size_t czn_bound(size_t n, const czn_params *p) {
    uint32_t B = p && p->block ? p->block : (p && p->mode >= CZN_MAX ? (128u << 20) : p && p->mode >= CZN_ECO ? (16u << 20) : czn_fast_block(n));
    size_t nb = n / (B / 2 > 0 ? B / 2 : 1) + 2 + 2 * (n >> 20);     /* + archive segments (>= 1 MiB each) */
    return HDR + nb * (IDX + 4) + n + 64 + 2 * IDX;
}

/* ------------------------------------------------------------------ parallel driver */
typedef struct {
    const uint8_t *src; uint8_t *dst; const uint64_t *starts; uint32_t nb; size_t slot; uint8_t *idx;
    int codec, level; plan_t pl; atomic_uint next; atomic_int err; size_t B;
    const plan_t *plans; const uint32_t *bplan; const uint64_t *rstart;   /* per-block plan (container v2) */
    const uint64_t *poff;   /* decode: payload offsets */
    uint64_t rlo, rhi; uint8_t *rdst;   /* range read */
    uint8_t **out;          /* compressed block copies */
    size_t *outlen;         /* their sizes */
    pthread_mutex_t wmu; uint32_t wnext; size_t wpos, cap; int werr;   /* blocks are written to dst in order as they finish */
    int psum;               /* checksums over stored bytes (rt) instead of decoded bytes */
    const void *dict; size_t dictlen;   /* optional zstd dictionary, read-only by workers */
} job_t;
/* block i is finished: write it at the frontier if it is next (then any buffered successors), else buffer a copy */
static void emit_block(job_t *J, uint32_t i, const uint8_t *hdr4, const uint8_t *body, size_t tot) {
    pthread_mutex_lock(&J->wmu);
    if (i == J->wnext) {
        if (J->wpos + tot > J->cap) J->werr = CZN_ERR_DSTSIZE;
        else { memcpy(J->dst + J->wpos, hdr4, 4); memcpy(J->dst + J->wpos + 4, body, tot - 4); J->wpos += tot; }
        J->wnext++;
        while (J->wnext < J->nb && J->out[J->wnext]) {
            uint32_t q = J->wnext; size_t l = J->outlen[q];
            if (J->wpos + l > J->cap) J->werr = CZN_ERR_DSTSIZE; else { memcpy(J->dst + J->wpos, J->out[q], l); J->wpos += l; }
            free(J->out[q]); J->out[q] = NULL; J->wnext++;
        }
        pthread_mutex_unlock(&J->wmu); return;
    }
    pthread_mutex_unlock(&J->wmu);
    uint8_t *b = malloc(tot);
    if (!b) { atomic_store(&J->err, CZN_ERR_ALLOC); return; }
    memcpy(b, hdr4, 4); memcpy(b + 4, body, tot - 4);
    pthread_mutex_lock(&J->wmu);
    J->outlen[i] = tot; J->out[i] = b;
    while (J->wnext < J->nb && J->out[J->wnext]) {                /* the frontier may have reached i meanwhile */
        uint32_t q = J->wnext; size_t l = J->outlen[q];
        if (J->wpos + l > J->cap) J->werr = CZN_ERR_DSTSIZE; else { memcpy(J->dst + J->wpos, J->out[q], l); J->wpos += l; }
        free(J->out[q]); J->out[q] = NULL; J->wnext++;
    }
    pthread_mutex_unlock(&J->wmu);
}

static void *compress_worker(void *arg) {
    job_t *J = arg; tls_t *S = &TLS; codec_ctx *xp = &S->x;
    xp->dict = J->dict; xp->dictlen = J->dictlen;                          /* per-call dictionary (read-only) */
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->B + TSLACK + 64);
    uint8_t *slot = S->slot = grow(S->slot, &S->scap, J->slot + 4);
    if (!tbuf || !slot) { atomic_store(&J->err, CZN_ERR_ALLOC); return NULL; }
    for (;;) {
        uint32_t i = atomic_fetch_add(&J->next, 1); if (i >= J->nb) break;
        const plan_t pl = J->plans[J->bplan[i]]; const uint64_t rs = J->rstart[i];
        const uint8_t *raw = J->src + J->starts[i]; size_t len = J->starts[i + 1] - J->starts[i];
        const uint8_t *in = raw; size_t meta = 0; int tflag = 0;
        uint8_t *sl = slot + 4;                                             /* slot: plan (4 bytes) | meta | codec data */
        slot[0] = (uint8_t)pl.t; slot[1] = (uint8_t)pl.a; slot[2] = (uint8_t)pl.b; slot[3] = (uint8_t)pl.c;
        if (pl.t == T_TP && len) {
            uint32_t *pos = S->pos = grow(S->pos, &S->pcap, (J->B + 1) * sizeof(uint32_t));
            if (!pos) { atomic_store(&J->err, CZN_ERR_ALLOC); break; }
            int k = pl.c + 1; int64_t colsz[256];
            int quoted = pl.b && (memchr(raw, '"', len) || memchr(raw, '[', len) || memchr(raw, ']', len));
            if (quoted) { transpose_fwd(raw, len, (uint8_t)pl.a, k, pl.b, tbuf, colsz); tflag = 1; }
            else { tpq_fwd(raw, len, (uint8_t)pl.a, k, tbuf, colsz, pos); tflag = 2; }
            for (int j = 0; j < k; j++) put32(sl + 4 * j, (uint32_t)colsz[j]);
            meta = 4 * k; in = tbuf;
        } else if (pl.t == T_TBL && len) {
            int64_t tn = tbl2_encode(raw, len, (uint8_t)pl.a, pl.c + 1, pl.b, pl.fs, tbuf);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 4; }
        } else if (pl.t == T_LOG && len) {
            int64_t tn = log_encode(raw, len, tbuf, len + TSLACK);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 5; }
        } else if (pl.t == T_IMG && len) {
            int64_t tn = img16_encode(raw, len, rs, &pl.img, tbuf);
            if (tn > 0 && (size_t)tn <= len + TSLACK - 64) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 6; }
        } else if (pl.t == T_REC && len) {
            int64_t tn = rec_encode(raw, len, rs, pl.rec, tbuf, J->B + TSLACK);
            if (tn > 0 && (size_t)tn <= len + TSLACK - 64) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 7; }
        } else if (pl.t == T_SHUF && len) {
            size_t u = pl.a * pl.b, rows = len / u;
            shuffle_fwd(raw, rows, pl.a, pl.b, pl.c, tbuf);
            memcpy(tbuf + rows * u, raw + rows * u, len - rows * u);
            in = tbuf; tflag = 1;
        } else if (pl.t == T_WORD && len) {
            int64_t tn = word_encode(raw, len, tbuf, len + TSLACK);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 9; }
        } else if (pl.t == T_X86 && len) {
            memcpy(tbuf, raw, len); e8e9_fwd(tbuf, len); in = tbuf; tflag = 8;
        }
        size_t inlen = (tflag >= 3 && tflag <= 7) || tflag == 9 ? get32(sl) : len;
        int codec = J->codec, level = J->level;
        if (codec == C_CM && tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; }                    /* ultra: CM models words itself */
        if (codec == C_LZMA && (!tflag || tflag == 9) && looks_text(raw, len)) { codec = C_BSC; if (tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; } }   /* max: text to BWT */
        if (codec == C_ZSTD && level == 19 && tflag == 9) level = CZ_HC_TEXT_LEVEL;                                  /* hc text: ultra level */
        size_t cl = codec_compress(xp, codec, level, in, inlen, sl + meta, J->slot - meta);
        if (codec == C_CM) {                          /* ultra: best of context mixing, LZMA2 and (text) BWT per block */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            if (alt) {
                int alts[2] = {C_LZMA, C_BSC};
                for (int k = 0; k < 2; k++) {
                    int text = !tflag && looks_text(in, inlen);
                    if (alts[k] == C_BSC && !text) continue;
                    if (alts[k] == C_LZMA && text) continue;  /* text: BWT always beat LZMA2 in tests; saves ~25% of the time */
                    size_t c2 = codec_compress(xp, alts[k], 9, in, inlen, alt, J->slot - meta);
#if defined(CZN_TRACE) || defined(CZN_PROF)
                    fprintf(stderr, "len %zu t%d text %d cm %zu alt%d %zu\n", (size_t)inlen, tflag, looks_text(in, inlen), cl, alts[k], c2);
#endif
                    if (c2 && (!cl || c2 < cl)) { memcpy(sl + meta, alt, c2); cl = c2; codec = alts[k]; }
                }
            }
        }
        size_t tot = 4 + meta + cl;
        uint8_t *e = J->idx + (size_t)i * IDX;
        if (cl == 0 || cl + meta >= len) {                                  /* store raw */
            codec = C_STORE; tflag = 0; tot = 4 + len;
            static const uint8_t h0[4] = {T_NONE, 0, 0, 0};
            emit_block(J, i, h0, raw, tot);
        } else emit_block(J, i, slot, slot + 4, tot);
        put32(e, (uint32_t)len); put32(e + 4, (uint32_t)tot);
        if (J->psum) put32(e + 8, codec == C_STORE ? (uint32_t)czn_xxh3(raw, len, 0x637a6e31u ^ (uint32_t)T_NONE)
                                                   : (uint32_t)czn_xxh3(slot + 4, tot - 4, 0x637a6e31u ^ get32(slot)));
        else put32(e + 8, blk_sum(3, raw, len));
        e[12] = (uint8_t)codec; e[13] = (uint8_t)tflag; e[14] = e[15] = 0;
    }
    zs_trim();
    return NULL;
}

/* persistent worker pool: threads (and their contexts) live across calls; the caller works too */
static pthread_mutex_t pool_mu = PTHREAD_MUTEX_INITIALIZER, pool_job = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pool_go = PTHREAD_COND_INITIALIZER, pool_done = PTHREAD_COND_INITIALIZER;
static int pool_n = 0, pool_want = 0, pool_active = 0; static uint64_t pool_gen = 0;
static void *(*pool_fn)(void *); static void *pool_arg; static uint64_t pool_start_gen[256];
static void *pool_main(void *a) {
    int idx = (int)(intptr_t)a;
    pthread_mutex_lock(&pool_mu);
    uint64_t seen = pool_start_gen[idx];          /* generation at creation: a job posted before we start is not missed */
    for (;;) {
        while (seen == pool_gen) pthread_cond_wait(&pool_go, &pool_mu);
        seen = pool_gen;
        if (idx >= pool_want) continue;
        void *(*fn)(void *) = pool_fn; void *arg = pool_arg;
        pthread_mutex_unlock(&pool_mu);
        fn(arg);
        pthread_mutex_lock(&pool_mu);
        if (--pool_active == 0) pthread_cond_signal(&pool_done);
    }
    return NULL;
}
static void run_pool(void *(*fn)(void *), void *J, int threads, uint32_t units) {
    if (threads > 256) threads = 256;
    if ((uint32_t)threads > units) threads = (int)units;
    if (threads <= 1) { fn(J); return; }
    if (pthread_mutex_trylock(&pool_job)) {               /* pool busy (concurrent caller): private threads */
        pthread_t th[256];
        for (int t = 1; t < threads; t++) pthread_create(&th[t], NULL, fn, J);
        fn(J);
        for (int t = 1; t < threads; t++) pthread_join(th[t], NULL);
        return;
    }
    pthread_mutex_lock(&pool_mu);
    while (pool_n < threads - 1) {
        pthread_t th; pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
        pool_start_gen[pool_n] = pool_gen;
        if (pthread_create(&th, &at, pool_main, (void *)(intptr_t)pool_n)) { pthread_attr_destroy(&at); break; }
        pthread_attr_destroy(&at); pool_n++;
    }
    int helpers = threads - 1 < pool_n ? threads - 1 : pool_n;
    pool_fn = fn; pool_arg = J; pool_want = helpers; pool_active = helpers; pool_gen++;
    pthread_cond_broadcast(&pool_go);
    pthread_mutex_unlock(&pool_mu);
    fn(J);
    pthread_mutex_lock(&pool_mu);
    while (pool_active) pthread_cond_wait(&pool_done, &pool_mu);
    pthread_mutex_unlock(&pool_mu);
    pthread_mutex_unlock(&pool_job);
}

/* ultra: the context-mixing coder models text, logs and CSV directly (column and word contexts), so a text transform is
 * kept only if CM on a 1 MiB transformed sample beats CM on the raw sample */
static int ultra_keep_transform(const uint8_t *s, size_t n, const plan_t *pl) {
    if (pl->t != T_TBL && pl->t != T_LOG && pl->t != T_WORD && pl->t != T_TP) return 1;
    size_t m = n < (1u << 20) ? n : (1u << 20); while (m > 0 && s[m - 1] != '\n') m--;
    if (m < 65536) return 1;
    uint8_t *t = malloc(m + TSLACK + 64), *z = malloc(m + m / 8 + 64); int64_t tn = -1;
    if (!t || !z) { free(t); free(z); return 1; }
    if (pl->t == T_TBL) tn = tbl2_encode(s, m, (uint8_t)pl->a, pl->c + 1, pl->b, pl->fs, t);
    else if (pl->t == T_LOG) tn = log_encode(s, m, t, m + TSLACK);
    else if (pl->t == T_WORD) tn = word_encode(s, m, t, m + TSLACK);
    int keep = 1;
    if (tn > 0) { size_t a = cm_compress(t, (size_t)tn, z, m + m / 8 + 64), b = cm_compress(s, m, z, m + m / 8 + 64); keep = a && (!b || a < b); }
    else if (pl->t == T_TP) keep = 0;
    free(t); free(z);
    return keep;
}

/* tar archives: regular-file members of at least 1 MiB become their own segments (own plan); else one segment */
static uint32_t segments(const uint8_t *s, size_t n, uint64_t *ss, uint64_t *se, uint32_t maxs) {
    uint32_t k = 0; size_t off = 0, last = 0;
    if (n >= 1024 && !memcmp(s + 257, "ustar", 5)) {
        while (off + 512 <= n && k + 2 < maxs) {
            const uint8_t *h = s + off; int zero = 1; for (int i = 0; i < 512 && zero; i++) zero = !h[i];
            if (zero) break;
            if (h[124] & 0x80) break;
            uint64_t size = 0; int ok = 1;
            for (int i = 124; i < 136; i++) { uint8_t c = h[i]; if (c == 0 || c == ' ') continue; if (c < '0' || c > '7') { ok = 0; break; } size = size * 8 + (c - '0'); }
            if (!ok || off + 512 + size > n) break;
            if ((h[156] == '0' || h[156] == 0) && size >= (1u << 20)) {
                if (off + 512 > last) { ss[k] = last; se[k] = off + 512; k++; }
                ss[k] = off + 512; se[k] = off + 512 + size; k++; last = off + 512 + size;
            }
            off += 512 + (size + 511) / 512 * 512;
        }
    }
    if (last < n || k == 0) { ss[k] = last; se[k] = n; k++; }
    return k;
}

static int64_t compress_impl(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const czn_params *p,
                             const void *dict, size_t dictlen, uint32_t dictid) {
    czn_params d = {CZN_FAST, 0, 0, 0, 1}; if (!p) p = &d;
    atomic_store_explicit(&cz_newmodes, 0, memory_order_relaxed);   /* reset per call; table encoder ORs in extended predictors, then the version byte reflects the highest */
    uint32_t B = p->block ? p->block : (p->mode >= CZN_MAX ? (128u << 20) : p->mode >= CZN_ECO ? (16u << 20) : czn_fast_block(n));   /* slow modes: larger blocks; speed modes: ~8 balanced blocks */
    if (B < 4096) B = 4096;
    int codec, level; mode_codec(p, &codec, &level);
    enum { MAXSEG = 4096 };
    uint64_t *ss = malloc(8 * MAXSEG), *se = malloc(8 * MAXSEG);
#ifdef CZN_PROF
    uint64_t pt0 = __rdtsc();
#endif
    uint32_t nseg = segments(src, n, ss, se, MAXSEG);
    plan_t *plans = calloc(nseg, sizeof(plan_t));
    uint64_t *starts = malloc(8 * 2); uint32_t *bplan = malloc(4 * 2); uint64_t *rstart = malloc(8 * 2); uint32_t nb = 0, bcap = 2;
    cz_plan_threads = p->threads > 0 ? p->threads : default_threads();
    for (uint32_t g = 0; g < nseg; g++) {
        const uint8_t *s0 = src + ss[g]; size_t len = (size_t)(se[g] - ss[g]);
        plan_t pl = p->transforms ? make_plan(s0, len, p->mode >= CZN_ECO, p->mode != CZN_RT, p->mode != CZN_RT) : (plan_t){T_NONE, 0, 0, 0};
        if (p->mode == CZN_RT && (pl.t == T_TP || pl.t == T_LOG)) pl.t = T_NONE;   /* rt keeps LZ4 decode speed */
        if (p->mode == CZN_ULTRA && !ultra_keep_transform(s0, len, &pl)) { free(pl.fs); pl.fs = NULL; pl.t = T_NONE; }
        plans[g] = pl;
        uint32_t Bs = B;
        if (!p->block && p->mode == CZN_HC && pl.t == T_WORD) Bs = 128u << 20;   /* hc text: one large window */
        uint64_t *st; uint32_t m = make_blocks(s0, len, Bs, pl, &st);
        if (len == 0) m = 0;
        while (nb + m + 1 > bcap) { bcap *= 2; starts = realloc(starts, 8 * bcap); bplan = realloc(bplan, 4 * bcap); rstart = realloc(rstart, 8 * bcap); }
        for (uint32_t i = 0; i < m; i++) { starts[nb] = ss[g] + st[i]; rstart[nb] = st[i]; bplan[nb] = g; nb++; }
        free(st);
    }
    if (nb == 0) { starts[0] = 0; rstart[0] = 0; bplan[0] = 0; nb = 1; }
    starts[nb] = n;
    size_t maxb = 0; for (uint32_t i = 0; i < nb; i++) if (starts[i + 1] - starts[i] > maxb) maxb = starts[i + 1] - starts[i];
    job_t J = {0};
    J.src = src; J.starts = starts; J.nb = nb; J.slot = slot_size((uint32_t)maxb, codec);
    J.idx = dst + HDR; J.codec = codec; J.level = level; J.B = maxb; J.plans = plans; J.bplan = bplan; J.rstart = rstart;
    J.out = calloc(nb, sizeof(uint8_t *)); J.outlen = calloc(nb, sizeof(size_t));
    J.psum = p->mode == CZN_RT;
    J.dict = dict; J.dictlen = dictlen;
    J.dst = dst; J.cap = cap; J.wnext = 0; J.wpos = HDR + (size_t)nb * IDX; J.werr = 0; pthread_mutex_init(&J.wmu, NULL);
    atomic_init(&J.next, 0); atomic_init(&J.err, 0);
    int64_t ret = 0;
    if (HDR + (size_t)nb * IDX > cap) ret = CZN_ERR_DSTSIZE;
#ifdef CZN_PROF
    uint64_t pt1 = __rdtsc();
#endif
    if (!ret) run_pool(compress_worker, &J, p->threads > 0 ? p->threads : default_threads(), nb);
#ifdef CZN_PROF
    fprintf(stderr, "compress: plan %.1f Mcyc, blocks %.1f Mcyc\n", (pt1 - pt0) / 1e6, (__rdtsc() - pt1) / 1e6);
#endif
    if (!ret) ret = atomic_load(&J.err);
    if (!ret) ret = J.werr;
    if (!ret && J.wnext != nb) ret = CZN_ERR_ALLOC;
    size_t w = J.wpos;
    for (uint32_t i = 0; i < nb; i++) free(J.out[i]);
    free(J.outlen); pthread_mutex_destroy(&J.wmu);
    for (uint32_t g = 0; g < nseg; g++) { free(plans[g].fs); free(plans[g].rec); }
    free(J.out); free(starts); free(bplan); free(rstart); free(plans); free(ss); free(se);
    if (ret) return ret;
    { int nm = atomic_load_explicit(&cz_newmodes, memory_order_relaxed); memcpy(dst, "CZN1", 4); dst[4] = (nm & 2) ? 5 : (nm & 1) ? 4 : 3; } dst[5] = (uint8_t)p->mode; dst[6] = dst[7] = dst[8] = dst[9] = 0; dst[10] = (uint8_t)J.psum; dst[11] = 0;
    if (dict) { dst[10] |= 2; put32(dst + 6, dictid); }   /* flag bit1 = zstd dictionary; dst[6..9] = dict id for validation */
    put64(dst + 12, n); put32(dst + 20, nb); put32(dst + 24, B);
    uint32_t hc = czn_crc32c(0, dst, 28); hc = czn_crc32c(hc, dst + HDR, (size_t)nb * IDX); put32(dst + 28, hc);
    return (int64_t)w;
}

int64_t czn_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const czn_params *p) {
    return compress_impl(src, n, dst, cap, p, NULL, 0, 0);
}

/* ------------------------------------------------------------------ decoding */
typedef struct { uint64_t orig; uint32_t nb; int ver, psum, needdict; uint32_t dictid; plan_t pl; const uint8_t *idx; uint64_t *poff; uint64_t *roff; } parsed_t;

static int parse(const uint8_t *s, size_t n, parsed_t *P) {
    if (n < HDR || memcmp(s, "CZN1", 4) || s[4] < 1 || s[4] > 5) return CZN_ERR_CORRUPT;
    P->ver = s[4]; P->psum = P->ver >= 3 && (s[10] & 1);
    P->needdict = P->ver >= 3 && (s[10] & 2); P->dictid = P->needdict ? get32(s + 6) : 0;
    P->orig = get64(s + 12); P->nb = get32(s + 20);
    P->pl = (plan_t){s[6], s[7], s[8], s[9]};
    if ((uint64_t)P->nb * IDX + HDR > n) return CZN_ERR_CORRUPT;
    uint32_t hc = czn_crc32c(0, s, 28); hc = czn_crc32c(hc, s + HDR, (size_t)P->nb * IDX);
    if (hc != get32(s + 28)) return CZN_ERR_CHECKSUM;
    P->idx = s + HDR;
    P->poff = malloc((P->nb + 1) * sizeof(uint64_t)); P->roff = malloc((P->nb + 1) * sizeof(uint64_t));
    uint64_t po = HDR + (uint64_t)P->nb * IDX, ro = 0;
    for (uint32_t i = 0; i < P->nb; i++) {
        P->poff[i] = po; P->roff[i] = ro;
        po += get32(P->idx + (size_t)i * IDX + 4); ro += get32(P->idx + (size_t)i * IDX);
    }
    P->poff[P->nb] = po; P->roff[P->nb] = ro;
    if (po > n || ro != P->orig) { free(P->poff); free(P->roff); return CZN_ERR_CORRUPT; }
    return 0;
}

typedef struct { const uint8_t *src; parsed_t *P; uint8_t *dst; uint32_t lo, hi; atomic_uint next; atomic_int err;
                 uint64_t base; size_t maxraw; uint64_t clip_lo, clip_hi; const void *dict; size_t dictlen; } djob_t;

static int decode_block(const djob_t *J, uint32_t i, codec_ctx *x, tls_t *S, uint8_t *tbuf, uint8_t *out) {
    const parsed_t *P = J->P; const uint8_t *e = P->idx + (size_t)i * IDX;
    size_t len = get32(e), cl = get32(e + 4); int codec = e[12], tflag = e[13];
    const uint8_t *pay = J->src + P->poff[i]; size_t meta = 0;
    plan_t pl = P->pl;
    if (P->psum) {                                             /* rt: the checksum covers the stored bytes; checked first */
        if (cl < 4 || (uint32_t)czn_xxh3(pay + 4, cl - 4, 0x637a6e31u ^ get32(pay)) != get32(e + 8)) return CZN_ERR_CHECKSUM;
    }
    if (P->ver >= 2) {                                         /* per-block plan */
        if (cl < 4) return CZN_ERR_CORRUPT;
        pl.t = pay[0]; pl.a = pay[1]; pl.b = pay[2]; pl.c = pay[3]; pay += 4; cl -= 4;
        if (pl.t > T_WORD) return CZN_ERR_CORRUPT;
    }
    if (tflag && pl.t == T_TP) meta = 4 * (size_t)(pl.c + 1);
    if ((tflag >= 3 && tflag <= 7) || tflag == 9) meta = 4;
    int maxf = pl.t == T_TP ? 2 : pl.t == T_TBL ? 4 : pl.t == T_LOG ? 5 : pl.t == T_IMG ? 6 : pl.t == T_REC ? 7 : pl.t == T_X86 ? 8 : pl.t == T_WORD ? 9 : 1;
    if (tflag > maxf || (pl.t == T_TBL && tflag && tflag < 3) || (pl.t == T_LOG && tflag && tflag != 5)
        || (pl.t == T_IMG && tflag && tflag != 6) || (pl.t == T_REC && tflag && tflag != 7) || (pl.t == T_X86 && tflag && tflag != 8) || (pl.t == T_WORD && tflag && tflag != 9)
        || (pl.t == T_NONE && tflag)) return CZN_ERR_CORRUPT;
    if (meta > cl) return CZN_ERR_CORRUPT;
    uint8_t *tgt = (tflag && tflag != 8) ? tbuf : out;
    size_t dlen = len;
    if ((tflag >= 3 && tflag <= 7) || tflag == 9) { dlen = get32(pay); if (dlen > len + TSLACK) return CZN_ERR_CORRUPT; }
    if (codec_decompress(x, codec, pay + meta, cl - meta, tgt, dlen)) return CZN_ERR_CORRUPT;
    if (tflag == 3) { if (tbl_decode(tbuf, dlen, (uint8_t)pl.a, out, len)) return CZN_ERR_CORRUPT; }
    else if (tflag == 4) { if (tbl2_decode(tbuf, dlen, (uint8_t)pl.a, out, len)) return CZN_ERR_CORRUPT; }
    else if (tflag == 5) { if (log_decode(tbuf, dlen, out, len)) return CZN_ERR_CORRUPT; }
    else if (tflag == 6) { if (img16_decode(tbuf, dlen, out, len)) return CZN_ERR_CORRUPT; }
    else if (tflag == 7) { if (rec_decode(tbuf, dlen, out, len)) return CZN_ERR_CORRUPT; }
    else if (tflag == 8) e8e9_inv(out, len);
    else if (tflag == 9) { if (word_decode(tbuf, dlen, out, len)) return CZN_ERR_CORRUPT; }
    else if (tflag && pl.t == T_TP) {
        uint32_t *pos = S->pos = grow(S->pos, &S->pcap, (J->maxraw + 1) * sizeof(uint32_t));
        uint32_t *rowbuf = S->rowbuf = grow(S->rowbuf, &S->rcap, (J->maxraw + 8) * sizeof(uint32_t));
        if (!pos || !rowbuf) return CZN_ERR_ALLOC;
        int k = pl.c + 1; int64_t colsz[256];
        if (cl < 4 * (size_t)k) return CZN_ERR_CORRUPT;
        for (int j = 0; j < k; j++) colsz[j] = get32(pay + 4 * j);
        if (tflag == 2) {
            int u = tpq_inv_cols(tbuf, len, (uint8_t)pl.a, k, colsz, out, pos, rowbuf);
            if (u < 0 || (u > 0 && tpq_inv(tbuf, len, (uint8_t)pl.a, k, colsz, out, pos))) return CZN_ERR_CORRUPT;
        }
        else if (transpose_inv(tbuf, len, (uint8_t)pl.a, k, pl.b, colsz, out)) return CZN_ERR_CORRUPT;
    } else if (tflag && pl.t == T_SHUF) {
        size_t u = (size_t)pl.a * pl.b, rows = u ? len / u : 0;
        if (!u || u > 64 * 8 || (pl.a != 2 && pl.a != 4 && pl.a != 8) || pl.b < 1 || pl.b > 8) return CZN_ERR_CORRUPT;
        shuffle_inv(tbuf, rows, pl.a, pl.b, pl.c, out);
        memcpy(out + rows * u, tbuf + rows * u, len - rows * u);
    } else if (tflag) return CZN_ERR_CORRUPT;
    if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return CZN_ERR_CHECKSUM;
    return 0;
}

static void *decompress_worker(void *arg) {
    djob_t *J = arg; tls_t *S = &TLS;
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->maxraw + TSLACK + 64);
    uint8_t *rbuf = J->clip_hi ? (S->rbuf = grow(S->rbuf, &S->rbcap, J->maxraw + 64)) : NULL;
    if (!tbuf || (J->clip_hi && !rbuf)) { atomic_store(&J->err, CZN_ERR_ALLOC); return NULL; }
    codec_ctx *xp = &S->x;
    xp->dict = J->dict; xp->dictlen = J->dictlen;                          /* per-call dictionary (read-only) */
    for (;;) {
        uint32_t i = J->lo + atomic_fetch_add(&J->next, 1); if (i >= J->hi) break;
        if (atomic_load(&J->err)) break;
        int r;
        if (!rbuf) r = decode_block(J, i, xp, S, tbuf, J->dst + J->P->roff[i]);
        else {                                               /* range read: decode block, copy the overlap */
            r = decode_block(J, i, xp, S, tbuf, rbuf);
            if (!r) {
                uint64_t b0 = J->P->roff[i], b1 = J->P->roff[i + 1];
                uint64_t a = J->clip_lo > b0 ? J->clip_lo : b0, z = J->clip_hi < b1 ? J->clip_hi : b1;
                memcpy(J->dst + (a - J->clip_lo), rbuf + (a - b0), z - a);
            }
        }
        if (r) atomic_store(&J->err, r);
    }
    zs_trim();
    return NULL;
}

int64_t czn_original_size(const uint8_t *src, size_t n) { return n >= HDR && !memcmp(src, "CZN1", 4) ? (int64_t)get64(src + 12) : CZN_ERR_CORRUPT; }

static int64_t run_decode(const uint8_t *src, size_t n, uint8_t *dst, int threads, uint64_t lo, uint64_t hi, int range,
                          const void *dict, size_t dictlen, uint32_t dictid) {
    parsed_t P; int r = parse(src, n, &P); if (r) return r;
    if (P.needdict && (!dict || dictid != P.dictid)) { free(P.poff); free(P.roff); return CZN_ERR_DICT; }
    djob_t J = {0}; J.src = src; J.P = &P; J.dst = dst; J.dict = dict; J.dictlen = dictlen;
    for (uint32_t i = 0; i < P.nb; i++) { size_t l = get32(P.idx + (size_t)i * IDX); if (l > J.maxraw) J.maxraw = l; }
    if (range) {
        uint32_t a = 0, b = P.nb;
        while (a < P.nb && P.roff[a + 1] <= lo) a++;
        b = a; while (b < P.nb && P.roff[b] < hi) b++;
        J.lo = a; J.hi = b; J.clip_lo = lo; J.clip_hi = hi;
    } else { J.lo = 0; J.hi = P.nb; }
    atomic_init(&J.next, 0); atomic_init(&J.err, 0);
    run_pool(decompress_worker, &J, threads > 0 ? threads : default_threads(), J.hi - J.lo);
    free(P.poff); free(P.roff);
    r = atomic_load(&J.err);
    return r ? r : (int64_t)(range ? hi - lo : P.orig);
}

int64_t czn_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads) {
    int64_t o = czn_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return CZN_ERR_DSTSIZE;
    return run_decode(src, n, dst, threads, 0, 0, 0, NULL, 0, 0);
}

int64_t czn_read_range(const uint8_t *src, size_t n, uint64_t off, size_t len, uint8_t *dst, int threads) {
    int64_t o = czn_original_size(src, n); if (o < 0) return o;
    if (off > (uint64_t)o) return CZN_ERR_CORRUPT;
    if (off + len > (uint64_t)o) len = (size_t)((uint64_t)o - off);
    if (!len) return 0;
    return run_decode(src, n, dst, threads, off, off + len, 1, NULL, 0, 0);
}

const char *czn_strerror(int64_t c) {
    switch (c) {
        case CZN_ERR_CORRUPT: return "corrupt or unsupported input";
        case CZN_ERR_CHECKSUM: return "checksum mismatch (data corrupted)";
        case CZN_ERR_DSTSIZE: return "destination buffer too small";
        case CZN_ERR_ALLOC: return "out of memory";
        case CZN_ERR_CODEC: return "codec error";
        case CZN_ERR_DICT: return "dictionary required or mismatched (compressed with a different dictionary)";
    }
    return c >= 0 ? "ok" : "unknown error";
}

/* ------------------------------------------------------------------ dictionary */

int64_t czn_train_dict(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                       uint8_t *dict, size_t dict_cap) {
    if (!samples || !sizes || !nsamples || !dict || dict_cap < 256) return CZN_ERR_CORRUPT;
    size_t r = ZDICT_trainFromBuffer(dict, dict_cap, samples, sizes, (unsigned)nsamples);
    if (ZDICT_isError(r)) return CZN_ERR_CODEC;
    return (int64_t)r;
}

int64_t czn_compress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                          const czn_params *p, const void *dict, size_t dictlen) {
    if (!dict || !dictlen) return czn_compress(src, n, dst, cap, p);
    uint32_t id = (uint32_t)czn_xxh3(dict, dictlen, 0x637a6e44u);   /* 'cznD' */
    return compress_impl(src, n, dst, cap, p, dict, dictlen, id);
}

int64_t czn_decompress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                            const void *dict, size_t dictlen) {
    if (!dict || !dictlen) return czn_decompress(src, n, dst, cap, threads);
    int64_t o = czn_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return CZN_ERR_DSTSIZE;
    uint32_t id = (uint32_t)czn_xxh3(dict, dictlen, 0x637a6e44u);
    return run_decode(src, n, dst, threads, 0, 0, 0, dict, dictlen, id);
}

#include "stream.inc"
