/* kolmo — native core of the kolmo compressor. See kolmo.h. */
#define _GNU_SOURCE
#include "kolmo.h"
#ifdef KOLMO_FRONTIER
#include "experimental/frontier_dna.h"
#include "experimental/frontier_table.h"
#include "experimental/frontier_prefix.h"
#include "experimental/frontier_runs.h"
#include "experimental/frontier_pcm.h"
#include "experimental/frontier_lag.h"
#include "experimental/frontier_shuffle.h"
#define KOLMO_MAX_VERSION 8
#else
#define KOLMO_MAX_VERSION 8   /* 6: minified JSON split (log plan flag 10); 7: kolmo graph blocks; 8: line classes (flag 12) */
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#if defined(KOLMO_TRACE) || defined(KOLMO_PROF) || defined(KOLMO_TIMING)
#include <stdio.h>
#endif
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
#include <time.h>
static double prof_now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
#endif
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
#include <x86intrin.h>
#endif
#include <stdint.h>
unsigned long long kolmo_xxh3(const void *p, size_t n, unsigned long long seed);   /* vendor/xxhash/kolmo_xxh3.c */
#include "zstd.h"
#include "zdict.h"
#include "lz4.h"
#include "lz4hc.h"
#include "common/xxhash.h"
#include <brotli/encode.h>
#include <brotli/decode.h>
#ifndef KOLMO_NO_LZMA
#include <lzma.h>
#endif
#include "libbsc/libbsc.h"

/* ------------------------------------------------------------------ CRC32C (hardware where available) */
#if defined(__SSE4_2__)
#include <nmmintrin.h>
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = buf; uint64_t c = ~crc;
    while (len >= 8) { uint64_t v; memcpy(&v, p, 8); c = _mm_crc32_u64(c, v); p += 8; len -= 8; }
    while (len--) c = _mm_crc32_u8((uint32_t)c, *p++);
    return ~(uint32_t)c;
}
#elif defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) {
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
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) {
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

/* ------------------------------------------------------------------ transforms (shared with the first kolmo prototype) */
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
#define KOLMO_MM64(r0, r1, r2, r3) ({ uint8x16_t s0 = vpaddq_u8(vandq_u8(r0, w), vandq_u8(r1, w)), s1 = vpaddq_u8(vandq_u8(r2, w), vandq_u8(r3, w)); \
        s0 = vpaddq_u8(s0, s1); s0 = vpaddq_u8(s0, s0); vgetq_lane_u64(vreinterpretq_u64_u8(s0), 0); })
    *t = KOLMO_MM64(t0, t1, t2, t3); *nl = KOLMO_MM64(n0, n1, n2, n3);
#undef KOLMO_MM64
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
/* The first 255 delimiters partition a row. The rest of that row is a
 * single field, including any further delimiters. Two scans avoid retaining
 * a position for every field and copy the long suffix just once. */
static void tpq_wide_pass(const uint8_t *in, size_t n, uint8_t delim, int64_t *sizes,
                          uint8_t **dst, uint8_t **end) {
    unsigned lane = 0;
    size_t start = 0;
    for (size_t off = 0; off < n; off += 64) {
        const uint8_t *p = in + off;
        uint8_t tail[64];
        if (n - off < 64) {
            memset(tail, 0, sizeof(tail)); memcpy(tail, p, n - off); p = tail;
        }
        uint64_t terms, lines;
        term_masks64(p, delim, &terms, &lines);
        if (n - off < 64) terms &= (UINT64_C(1) << (n - off)) - 1;
        while (terms) {
            uint64_t relevant = lane == 255 ? terms & lines : terms;
            if (!relevant) break;
            unsigned bit = (unsigned)__builtin_ctzll(relevant);
            size_t stop = off + bit + 1, len = stop - start;
            if (!dst) sizes[lane] += len;
            else {
                copy_field(dst[lane], in + start, len, n - start >= 16 && end[lane] - dst[lane] >= 16);
                dst[lane] += len;
            }
            lane = (lines & (UINT64_C(1) << bit)) ? 0 : lane + 1;
            start = stop;
            /* Remove every terminator up to and including the one consumed. */
            terms &= ~((relevant & -relevant) - 1);
            terms &= terms - 1;
        }
    }
    if (!dst) sizes[lane] += n - start;
    else memcpy(dst[lane], in + start, n - start);
}

static void tpq_wide_fwd(const uint8_t *in, size_t n, uint8_t delim, uint8_t *out, int64_t *sizes) {
    uint8_t *dst[256], *end[256];
    memset(sizes, 0, 256 * sizeof(*sizes));
    tpq_wide_pass(in, n, delim, sizes, NULL, NULL);
    for (int j = 0; j < 256; j++) { dst[j] = out; out += sizes[j]; end[j] = out; }
    tpq_wide_pass(in, n, delim, sizes, dst, end);
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

/* The last lane of a full-width transpose contains the remaining suffix
 * of each row. Copy straight from lane cursors into the row being restored. */
static int tpq_inv_wide(const uint8_t *in, size_t n, uint8_t delim,
                        const size_t *base, uint8_t *out) {
    enum { LAST = 255 };
    if (memchr(in, '\n', base[LAST])) return 1;  /* a short row; use the generic inverse */
    const uint8_t *next[LAST], *end[LAST];
    for (int j = 0; j < LAST; j++) { next[j] = in + base[j]; end[j] = in + base[j + 1]; }
    const uint8_t *suffix = in + base[LAST], *limit = in + n;
    uint8_t *dst = out;
    while (suffix < limit) {
        for (int j = 0; j < LAST; j++) {
            const uint8_t *field = next[j];
            size_t available = (size_t)(end[j] - field);
            if (!available) return 1;
#if defined(__SSE2__) || (defined(__ARM_NEON) && defined(__aarch64__))
            if ((size_t)(limit - field) >= 16 && (size_t)(out + n - dst) >= 16) {
#if defined(__SSE2__)
                __m128i v = _mm_loadu_si128((const __m128i *)field);
                unsigned mask = _mm_movemask_epi8(_mm_cmpeq_epi8(v, _mm_set1_epi8((char)delim)));
                unsigned len = mask ? (unsigned)__builtin_ctz(mask) + 1 : 0;
#else
                uint8x16_t v = vld1q_u8(field);
                uint8x16_t eq = vceqq_u8(v, vdupq_n_u8(delim));
                uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
                unsigned len = mask ? ((unsigned)__builtin_ctzll(mask) >> 2) + 1 : 0;
#endif
                if (len && len <= available) {
                    memcpy(dst, &v, 16); next[j] += len; dst += len; continue;
                }
            }
#endif
            const uint8_t *sep = memchr(field, delim, available);
            if (!sep) return 1;
            size_t len = (size_t)(sep - field) + 1;
            memcpy(dst, field, len); dst += len; next[j] += len;
        }
        const uint8_t *nl = memchr(suffix, '\n', (size_t)(limit - suffix));
        if (!nl) return 1;
        size_t len = (size_t)(nl - suffix) + 1;
        memcpy(dst, suffix, len); dst += len; suffix += len;
    }
    return dst == out + n ? 0 : 1;
}

/* Column-major inverse for uniform blocks (every row has exactly k fields, block ends with a newline).
 * Rows are independent inside each column pass, so the CPU overlaps many field copies.
 * Returns 0 done, 1 not uniform (caller falls back), -1 corrupt. rowbuf needs 2 * (n / 2 + 2) entries. */
static int tpq_inv_cols(const uint8_t *in, size_t n, uint8_t delim, int k, const int64_t *colsz, uint8_t *out,
                        uint32_t *pos, uint32_t *rowbuf) {
    size_t base[256], pb[256], tot = 0, o = 0;
    for (int j = 0; j < k; j++) { if (colsz[j] < 0 || (size_t)colsz[j] > n - o) return -1; base[j] = o; o += colsz[j]; }
    if (o != n || n == 0 || colsz[k - 1] == 0 || in[n - 1] != '\n') return 1;
    if (k == 256) return tpq_inv_wide(in, n, delim, base, out);
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
 * Rows are split into fields (quote-aware, same rules as kolmo). Each column becomes one section:
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
#ifdef KOLMO_FRONTIER
    if (w == 2 && frontier_shuffle16(in, (size_t)rows, (unsigned)c, delta, out, 0)) return;
#endif
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
#ifdef KOLMO_FRONTIER
    if (w == 2 && frontier_shuffle16(in, (size_t)rows, (unsigned)c, delta, out, 1)) return;
#endif
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
enum { C_STORE = 0, C_LZ4 = 1, C_LZ4HC = 2, C_ZSTD = 3, C_BROTLI = 4, C_BSC = 5, C_LZMA = 6, C_CM = 7, C_DNA = 8, C_DNA_PREFIX = 9, C_PCM = 10, C_DNA_RUNS = 11,
       C_GRAPH = 12, C_HUF = 13, C_RANS = 14, C_CMI = 15 };   /* 15: context mixing with the image model (ultra; carries its own row length, channels and head) */   /* 8-11: experimental frontier codecs (opt-in build); 12: kolmo graph block; 13: Huffman only, 14: rANS order 0 (graph leaves) */
enum { T_NONE = 0, T_TP = 1, T_SHUF = 2, T_TBL = 3, T_LOG = 4, T_IMG = 5, T_REC = 6, T_X86 = 7, T_WORD = 8, T_KG = 9, T_LINES = 10 };   /* 9: kolmo graph (a=delim b=decimal separator) */
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
static pthread_once_t bsc_init_once = PTHREAD_ONCE_INIT;
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
#ifndef KOLMO_NO_LZMA
        case C_LZMA: return lzma_stream_buffer_bound(n) + 64;
#else
        case C_LZMA: return n + n / 8 + 64;
#endif
        case C_CM: return n + n / 16 + 64;
        default: return n;
    }
}

typedef struct { ZSTD_CCtx *zc; ZSTD_DCtx *zd; void *lz4hc_state; const void *dict; size_t dictlen; } codec_ctx;

static int kgu_get(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int sh = 0;
    while (*i < n && sh < 64) { uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << sh; if (!(c & 128)) { *v = r; return 0; } sh += 7; }
    return -1;
}
/* Huffman-only coding (zstd's HUF, no matches): for byte streams with a skewed distribution and no repeats, such as
 * the exponent plane of float arrays. Chunks of up to 128 KiB: mode u8 (0 stored, 1 one repeated byte, 2 Huffman),
 * raw length uv, then for mode 0 the bytes, mode 1 the byte, mode 2 coded length uv and the coded bytes. */
#include "common/huf.h"
static size_t huf_compress_all(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    U64 wk[HUF_WORKSPACE_SIZE_U64]; HUF_CREATE_STATIC_CTABLE(ct, HUF_SYMBOLVALUE_MAX); size_t o = 0;
    for (size_t i = 0; i < n;) {
        size_t m = n - i < HUF_BLOCKSIZE_MAX ? n - i : HUF_BLOCKSIZE_MAX;
        if (cap - o < m + 16) return 0;
        uint8_t *h = out + o; size_t hp = 1;
        for (uint64_t v = m;; v >>= 7) { h[hp++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        HUF_repeat rp = HUF_repeat_none; uint8_t tmp[10];
        size_t z = HUF_compress4X_repeat(h + hp + 4, cap - o - hp - 4, in + i, m, HUF_SYMBOLVALUE_MAX, HUF_TABLELOG_DEFAULT, wk, sizeof wk, ct, &rp, 0);
        if (HUF_isError(z)) return 0;
        if (z == 1) { h[0] = 1; h[hp] = in[i]; o += hp + 1; }                        /* one repeated byte */
        else if (z == 0 || z + 4 >= m) { h[0] = 0; memcpy(h + hp, in + i, m); o += hp + m; }
        else {
            size_t tn = 0; for (uint64_t v = z;; v >>= 7) { tmp[tn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
            memmove(h + hp + tn, h + hp + 4, z); memcpy(h + hp, tmp, tn); h[0] = 2; o += hp + tn + z;
        }
        i += m;
    }
    return o;
}
static int huf_decompress_all(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX); U32 wk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32]; size_t i = 0, o = 0;
    while (o < n) {
        uint64_t m, z; if (i >= cn) return -1;
        uint8_t mode = in[i++]; if (kgu_get(in, cn, &i, &m) || m == 0 || m > HUF_BLOCKSIZE_MAX || m > n - o) return -1;
        if (mode == 0) { if (m > cn - i) return -1; memcpy(out + o, in + i, m); i += m; }
        else if (mode == 1) { if (i >= cn) return -1; memset(out + o, in[i++], m); }
        else if (mode == 2) {
            if (kgu_get(in, cn, &i, &z) || z > cn - i) return -1;
            size_t r = HUF_decompress4X_hufOnly_wksp(dt, out + o, (size_t)m, in + i, (size_t)z, wk, sizeof wk, 0);
            if (HUF_isError(r) || r != m) return -1;
            i += z;
        } else return -1;
        o += m;
    }
    return i == cn ? 0 : -1;
}

/* Static order-0 rANS (graph leaves): near-entropy coding of skewed bytes, where Huffman's whole-bit code lengths
 * lose (an exponent plane at 2.6 bits per byte). Chunks of up to 1 MiB: raw length uv, frequency table, coded length uv, then the bytes: a 32-bit state (4 bytes, little-endian) and the
 * renormalisation bytes, read forwards. Table: per symbol its frequency uv; a 0 is followed by the count of further
 * absent symbols uv. Frequencies are scaled to 2^16. */
#define RANS_SB 16
#define RANS_M (1u << RANS_SB)
#define RANS_L (1u << 23)
#define RANS_CHUNK (1u << 20)
static int rans_norm(const uint32_t *cnt, size_t n, uint32_t *f) {   /* scale counts to sum RANS_M, present symbols >= 1 */
    uint64_t tot = 0; int used = 0;
    for (int s = 0; s < 256; s++) { f[s] = cnt[s] ? (uint32_t)(((uint64_t)cnt[s] * RANS_M) / n) : 0; if (cnt[s] && !f[s]) f[s] = 1; tot += f[s]; used += cnt[s] != 0; }
    if (used < 2) return -1;
    while (tot != RANS_M) {                                   /* adjust the largest (or the cheapest to change) */
        int b = -1; double bc = 1e300;
        for (int s = 0; s < 256; s++) {
            if (!cnt[s]) continue;
            if (tot > RANS_M && f[s] <= 1) continue;
            double c = tot > RANS_M ? (double)cnt[s] * log2((double)f[s] / (f[s] - 1)) : -(double)cnt[s] * log2((double)(f[s] + 1) / f[s]);
            if (c < bc) { bc = c; b = s; }
        }
        if (b < 0) return -1;
        if (tot > RANS_M) { uint64_t d = tot - RANS_M, room = f[b] - 1, k = d < room ? d : room; if (k > (cnt[b] / 64) + 1) k = (cnt[b] / 64) + 1; if (k > room) k = room; f[b] -= (uint32_t)k; tot -= k; }
        else { uint64_t d = RANS_M - tot, k = d < (cnt[b] / 64) + 1 ? d : (cnt[b] / 64) + 1; f[b] += (uint32_t)k; tot += k; }
    }
    return 0;
}
/* rans64 (ryg): 64-bit state, 32-bit renormalisation words, reciprocal encoding; four interleaved states */
#define R64_L (1ull << 31)
typedef struct { uint64_t rcp; uint32_t freq, bias, cmpl, shift; } r64sym;
static void r64sym_init(r64sym *e, uint32_t start, uint32_t f) {
    e->freq = f; e->cmpl = RANS_M - f;
    if (f < 2) { e->rcp = ~0ull; e->shift = 0; e->bias = start + RANS_M - 1; }
    else {
        uint32_t sh = 0; while (f > (1u << sh)) sh++;
        uint64_t x0 = f - 1, x1 = 1ull << (sh + 31), t1 = x1 / f; x0 += (x1 % f) << 32; uint64_t t0 = x0 / f;
        e->rcp = t0 + (t1 << 32); e->shift = sh - 1; e->bias = start;
    }
}
static inline uint64_t r64_mulhi(uint64_t a, uint64_t b) { return (uint64_t)(((unsigned __int128)a * b) >> 64); }
static size_t rans_compress_all(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    size_t o = 0, cw = RANS_CHUNK + 64; uint32_t *tmp = malloc(cw * 4); if (!tmp) return 0;   /* 32-bit words, filled backwards */
    for (size_t i = 0; i < n;) {
        size_t m = n - i < RANS_CHUNK ? n - i : RANS_CHUNK; const uint8_t *src = in + i;
        uint32_t cnt[256] = {0}, f[256], c = 0; r64sym es[256]; for (size_t q = 0; q < m; q++) cnt[src[q]]++;
        if (rans_norm(cnt, m, f)) { free(tmp); return 0; }         /* one symbol: leave it to the other codecs */
        for (int k = 0; k < 256; k++) { if (f[k]) r64sym_init(&es[k], c, f[k]); c += f[k]; }
        uint32_t *e = tmp + cw; uint64_t x0 = R64_L, x1 = R64_L;   /* states for even and odd positions, back to front */
#define R64_PUT(X, Q) do { const r64sym *y = &es[src[Q]]; uint64_t xmax = ((R64_L >> RANS_SB) << 32) * y->freq; \
            if (X >= xmax) { *--e = (uint32_t)X; X >>= 32; } X = X + y->bias + (r64_mulhi(X, y->rcp) >> y->shift) * y->cmpl; } while (0)
        uint64_t x2 = R64_L, x3 = R64_L; size_t q = m;            /* four states (position mod 4); at most one word per symbol */
        while (q & 3) { q--; switch (q & 3) { case 0: R64_PUT(x0, q); break; case 1: R64_PUT(x1, q); break; default: R64_PUT(x2, q); } }
        while (q) { q -= 4; R64_PUT(x3, q + 3); R64_PUT(x2, q + 2); R64_PUT(x1, q + 1); R64_PUT(x0, q); }
#undef R64_PUT
        uint64_t x[4] = {x0, x1, x2, x3};
        if (e - tmp < 8) { free(tmp); return 0; }
        for (int k = 3; k >= 0; k--) { *--e = (uint32_t)(x[k] >> 32); *--e = (uint32_t)x[k]; }   /* x0 lo, x0 hi, x1 lo, ... */
        size_t zw = (size_t)(tmp + cw - e), z = zw * 4;
        if (cap - o < 16 + 256 * 3 + z) { free(tmp); return 0; }
        uint8_t *h = out + o;
        for (uint64_t v = m;; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (int k = 0; k < 256;) {
            if (f[k]) { for (uint64_t v = f[k];; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; } k++; continue; }
            int r = 1; while (k + r < 256 && !f[k + r]) r++;
            *h++ = 0; *h++ = (uint8_t)(r - 1); k += r;              /* r - 1 < 256 */
        }
        for (uint64_t v = z;; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (size_t q = 0; q < zw; q++) { uint32_t w = e[q]; h[4 * q] = (uint8_t)w; h[4 * q + 1] = (uint8_t)(w >> 8); h[4 * q + 2] = (uint8_t)(w >> 16); h[4 * q + 3] = (uint8_t)(w >> 24); }
        o = (size_t)(h - out) + z; i += m;
    }
    free(tmp); return o;
}
static inline uint32_t r64_rd(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int rans_decompress_all(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0, o = 0; uint8_t *sym = malloc(RANS_M); if (!sym) return -1;     /* slot -> symbol: 64 KiB, stays in cache */
    while (o < n) {
        uint64_t m, v, z; uint32_t f[256], c[257]; c[0] = 0;
        if (kgu_get(in, cn, &i, &m) || m == 0 || m > RANS_CHUNK || m > n - o) goto bad;
        for (int k = 0; k < 256;) {
            if (kgu_get(in, cn, &i, &v) || v > RANS_M) goto bad;
            if (v) { f[k] = (uint32_t)v; c[k + 1] = c[k] + f[k]; if (c[k + 1] > RANS_M) goto bad; k++; continue; }
            if (i >= cn) goto bad;
            int r = 1 + in[i++]; if (k + r > 256) goto bad;
            for (int q = 0; q < r; q++) { f[k] = 0; c[k + 1] = c[k]; k++; }
        }
        if (c[256] != RANS_M || kgu_get(in, cn, &i, &z) || z < 32 || z % 4 || z > cn - i) goto bad;
        for (int k = 0; k < 256; k++) if (f[k]) memset(sym + c[k], k, f[k]);
        const uint8_t *p = in + i, *pe = p + z; uint64_t x[4];
        for (int k = 0; k < 4; k++) { x[k] = (uint64_t)r64_rd(p) | (uint64_t)r64_rd(p + 4) << 32; p += 8; if (x[k] < R64_L) goto bad; }
        uint8_t *w = out + o; uint64_t x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
#define R64_GET(X, Q) do { uint32_t sl = (uint32_t)X & (RANS_M - 1); uint8_t k = sym[sl]; w[Q] = k; \
            X = (uint64_t)f[k] * (X >> RANS_SB) + sl - c[k]; if (X < R64_L) { if (p >= pe) goto bad; X = (X << 32) | r64_rd(p); p += 4; } } while (0)
        size_t q = 0;
        for (; q + 3 < m; q += 4) { R64_GET(x0, q); R64_GET(x1, q + 1); R64_GET(x2, q + 2); R64_GET(x3, q + 3); }
        if (q < m) R64_GET(x0, q);
        if (q + 1 < m) R64_GET(x1, q + 1);
        if (q + 2 < m) R64_GET(x2, q + 2);
#undef R64_GET
        if (p != pe || x0 != R64_L || x1 != R64_L || x2 != R64_L || x3 != R64_L) goto bad;   /* back where the encoder started */
        i += z; o += m;
    }
    free(sym); return i == cn ? 0 : -1;
bad: free(sym); return -1;
}

static size_t codec_compress(codec_ctx *x, int codec, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    switch (codec) {
        case C_HUF: return huf_compress_all(in, n, out, cap);
        case C_RANS: return rans_compress_all(in, n, out, cap);
        case C_LZ4: { int r = LZ4_compress_fast((const char *)in, (char *)out, (int)n, (int)cap, level > 0 ? level : 1); return r > 0 ? (size_t)r : 0; }
        case C_LZ4HC: {
            if (!x->lz4hc_state) x->lz4hc_state = malloc(LZ4_sizeofStateHC());
            if (!x->lz4hc_state) return 0;
            int r = LZ4_compress_HC_extStateHC(x->lz4hc_state, (const char *)in, (char *)out, (int)n, (int)cap, level);
            return r > 0 ? (size_t)r : 0; }
        case C_ZSTD: {
            if (!x->zc) x->zc = ZSTD_createCCtx();
            if (!x->zc) return 0;
            size_t r = x->dict ? ZSTD_compress_usingDict(x->zc, out, cap, in, n, x->dict, x->dictlen, level)
                               : ZSTD_compressCCtx(x->zc, out, cap, in, n, level);
            return ZSTD_isError(r) ? 0 : r; }
        case C_BROTLI: {
            size_t o = cap;
            int ok = BrotliEncoderCompress(level, 24, BROTLI_MODE_GENERIC, n, in, &o, out);
            return ok ? o : 0; }
        case C_BSC: {                       /* payload: XXH32 of the bsc stream | bsc stream (checked before decoding) */
            pthread_once(&bsc_init_once, bsc_once);
            if (n > 0x7FFFFFF0 || cap < n + LIBBSC_HEADER_SIZE + 4) return 0;
            int r = bsc_compress(in, out + 4, (int)n, LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, LIBBSC_BLOCKSORTER_BWT,
                                 LIBBSC_CODER_QLFC_STATIC, LIBBSC_FEATURE_FASTMODE);
            if (r <= 0) return 0;
            uint32_t h = (uint32_t)XXH64(out + 4, (size_t)r, 0x62736331); memcpy(out, &h, 4);
            return (size_t)r + 4; }
#ifdef KOLMO_NO_LZMA
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
        case C_CM: return cm_compress_m(in, n, out, cap, level == 1 ? 1 : looks_text(in, n) ? 3 : 2);   /* model 3 where it gains: text */
    }
    return 0;
}

static int codec_decompress(codec_ctx *x, int codec, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    switch (codec) {
        case C_STORE: if (cn != n) return -1; memcpy(out, in, n); return 0;
        case C_LZ4: case C_LZ4HC: { int r = LZ4_decompress_safe((const char *)in, (char *)out, (int)cn, (int)n); return r == (int)n ? 0 : -1; }
        case C_ZSTD: { if (!x->zd) x->zd = ZSTD_createDCtx();
            if (!x->zd) return -1;
            size_t r = x->dict ? ZSTD_decompress_usingDict(x->zd, out, n, in, cn, x->dict, x->dictlen)
                               : ZSTD_decompressDCtx(x->zd, out, n, in, cn);
            return (!ZSTD_isError(r) && r == n) ? 0 : -1; }
        case C_BROTLI: { size_t o = n; int r = BrotliDecoderDecompress(cn, in, &o, out); return (r == BROTLI_DECODER_RESULT_SUCCESS && o == n) ? 0 : -1; }
        case C_BSC: {
            pthread_once(&bsc_init_once, bsc_once);
            if (cn < 4 + LIBBSC_HEADER_SIZE || n > 0x7FFFFFF0 || cn > 0x7FFFFFF0) return -1;
            uint32_t h; memcpy(&h, in, 4); if (h != (uint32_t)XXH64(in + 4, cn - 4, 0x62736331)) return -1;
            int bs, ds; if (bsc_block_info(in + 4, LIBBSC_HEADER_SIZE, &bs, &ds, LIBBSC_FEATURE_FASTMODE) != LIBBSC_NO_ERROR) return -1;
            if ((size_t)bs != cn - 4 || (size_t)ds != n) return -1;
            return bsc_decompress(in + 4, (int)(cn - 4), out, (int)n, LIBBSC_FEATURE_FASTMODE) == LIBBSC_NO_ERROR ? 0 : -1; }
#ifdef KOLMO_NO_LZMA
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
        case C_CMI: return cm_decompress_img(in, cn, out, n);
        case C_HUF: return huf_decompress_all(in, cn, out, n);
        case C_RANS: return rans_decompress_all(in, cn, out, n);
    }
    return -1;
}

#include "kgraph.inc"

static void codec_ctx_free(codec_ctx *x) { ZSTD_freeCCtx(x->zc); ZSTD_freeDCtx(x->zd); free(x->lz4hc_state); }

/* per-thread state kept across calls (pool threads persist): codec contexts and scratch buffers */
typedef struct { codec_ctx x; uint8_t *tbuf, *slot, *rbuf; uint32_t *pos, *rowbuf; size_t tcap, scap, rbcap, pcap, rcap; int retained; } tls_t;
static __thread tls_t *TLS;
static pthread_key_t tls_key;
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;
static int tls_key_valid;
static void tls_destroy(void *arg) {
    tls_t *s = arg;
    codec_ctx_free(&s->x);
    free(s->tbuf); free(s->slot); free(s->rbuf); free(s->pos); free(s->rowbuf);
    free(s);
}
static void tls_init(void) { tls_key_valid = pthread_key_create(&tls_key, tls_destroy) == 0; }
static tls_t *tls_begin(void) {
    pthread_once(&tls_once, tls_init);
    if (!TLS) {
        TLS = calloc(1, sizeof(*TLS));
        if (!TLS) return NULL;
        if (tls_key_valid && !pthread_setspecific(tls_key, TLS)) TLS->retained = 1;
    }
    return TLS;
}
static void tls_end(tls_t *s) {
    zs_trim();
    /* Without a destructor, retain nothing after a completed call. */
    if (!s->retained) { TLS = NULL; tls_destroy(s); }
}
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
    return ver >= 3 ? (uint32_t)kolmo_xxh3(p, n, 0x637a6e31) : (uint32_t)XXH64(p, n, 0x637a6e31);
}

static inline void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static inline void put64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t get64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
#include "lines.inc"

typedef struct { int t, a, b, c; fset_t *fs; img_plan_t img; rec_plan_t *rec; int wide; int experimental_dna, experimental_table, experimental_pcm; uint32_t kgw; int verify;   /* kgw: IMG8 or DEC64 row bytes; verify: code graph blocks plainly too, keep the smaller */
#ifdef KOLMO_FRONTIER
    frontier_lags lags;
#endif
} plan_t;   /* tp/tbl: a=delim b=quotes c=k-1 | shuf: a=w b=ch c=delta; fs: formulas (encoder) */

static int default_threads(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1; }

/* Container allocations use checked element counts and retain the old pointer
 * on growth failure. The test-only counter exercises each cleanup path. */
#ifdef KOLMO_TESTING
static _Thread_local long kolmo_alloc_fail_after = -1;
static int kolmo_alloc_fail(void) {
    if (kolmo_alloc_fail_after < 0) return 0;
    if (!kolmo_alloc_fail_after) return 1;
    kolmo_alloc_fail_after--; return 0;
}
#else
#define kolmo_alloc_fail() 0
#endif
static void *array_alloc(size_t count, size_t width, int zero) {
    if (!width || count > SIZE_MAX / width || kolmo_alloc_fail()) return NULL;
    return zero ? calloc(count ? count : 1, width) : malloc((count ? count : 1) * width);
}
static void *array_resize(void *old, size_t count, size_t width) {
    if (!width || count > SIZE_MAX / width || kolmo_alloc_fail()) return NULL;
    return realloc(old, (count ? count : 1) * width);
}

#ifndef KOLMO_BWT_GAIN_PCT
#define KOLMO_BWT_GAIN_PCT 3      /* max: least size gain (%) for BWT to replace LZMA2 on a binary block */
#endif
static void mode_codec(const kolmo_params *p, int *codec, int *level) {
    switch (p->mode) {
        case KOLMO_RT:  *codec = C_LZ4;    *level = p->level ? p->level : 1; break;
        case KOLMO_FAST:*codec = C_ZSTD;   *level = p->level ? p->level : CZ_FAST_LEVEL; break;
        case KOLMO_ECO: *codec = C_BROTLI; *level = p->level ? p->level : CZ_ECO_LEVEL; break;
        case KOLMO_HC:  *codec = C_ZSTD;   *level = p->level ? p->level : 19; break;
        case KOLMO_MAX: *codec = C_LZMA;   *level = p->level ? p->level : 9; break;     /* max: BWT for text, the better of BWT and LZMA2 for binary (level 1: BWT only) */
        default:      *codec = C_CM;     *level = p->level == 1 ? 1 : 0; break;        /* ultra: context mixing (level 1: model 1, CM only) */
    }
}

/* Minified JSON (no newline at all): a newline is inserted after every ',' that follows '}' or ']' outside strings, so
 * each record becomes a line for the JSON template transform. The decoder removes every newline again, so the split
 * is lossless whatever the input (a block that starts inside a string only splits worse). Returns the new length. */
static size_t json_split(const uint8_t *s, size_t n, uint8_t *d) {
    size_t o = 0, i = 0;
    if (n && s[0] != '[' && s[0] != '{') {             /* a block may start inside a string: sync at the first "},{" */
        while (i + 2 < n && !(s[i] == '}' && s[i + 1] == ',' && s[i + 2] == '{')) i++;
        if (i + 2 >= n) i = n;
        memcpy(d, s, i); o = i;
    }
    const size_t i0 = i;
    while (i < n) {                                     /* runs between quotes and commas are copied whole */
        size_t j = i; while (j < n && s[j] != '"' && s[j] != ',') j++;
        memcpy(d + o, s + i, j - i); o += j - i; i = j;
        if (i >= n) break;
        if (s[i] == ',') {                              /* a newline after a ',' whose last non-blank byte is } or ] */
            d[o++] = ','; uint8_t lst = 0;
            for (size_t b2 = i; b2 > i0;) { uint8_t c = s[--b2]; if (c != ' ' && c != '\t' && c != '\r') { lst = c; break; } }
            if (lst == '}' || lst == ']') d[o++] = '\n';
            i++; continue;
        }
        size_t st = ++i;                                /* a string: its closing quote has an even run of backslashes before it */
        for (;;) {
            const uint8_t *q = i < n ? memchr(s + i, '"', n - i) : NULL;
            if (!q) { i = n; break; }
            size_t qi = (size_t)(q - s), bs = 0; while (qi - bs > st && s[qi - 1 - bs] == '\\') bs++;
            i = qi + 1; if (!(bs & 1)) break;
        }
        d[o++] = '"'; memcpy(d + o, s + st, i - st); o += i - st;
    }
    return o;
}
/* minified JSON: no newline in the sample, starts with { or [, and many records (",{" after "}") */
static int json_minified(const uint8_t *s, size_t h) {
    if (memchr(s, '\n', h)) return 0;
    size_t k = 0; while (k < h && (s[k] == ' ' || s[k] == '\t' || s[k] == '\r')) k++;
    if (k == h || (s[k] != '{' && s[k] != '[')) return 0;
    size_t rec = 0; for (size_t i = k; i + 2 < h; i++) rec += s[i] == '}' && s[i + 1] == ',' && (s[i + 2] == '{' || s[i + 2] == ' ');
    return rec >= 16;
}

/* JSON text: nearly every non-blank line of the sample starts (after indentation) with { [ " } or ], and some hold a ':'.
 * Returns 2 for JSON lines (records start with { or [), 1 for other (pretty-printed) JSON, 0 otherwise. */
static int json_lines(const uint8_t *s, size_t h) {
    size_t lines = 0, js = 0, colon = 0, rec = 0, i = 0;
    while (i < h) {
        const uint8_t *e = memchr(s + i, '\n', h - i); size_t end = e ? (size_t)(e - s) : h;
        size_t k = i; while (k < end && (s[k] == ' ' || s[k] == '\t' || s[k] == '\r')) k++;
        if (k < end) { lines++; uint8_t c = s[k]; js += c == '{' || c == '[' || c == '"' || c == '}' || c == ']'; rec += (c == '{' || c == '[') && end - k > 2; colon += memchr(s + k, ':', end - k) != NULL; }
        if (!e) break;
        i = end + 1;
    }
    if (lines < 8 || js * 100 < lines * 95 || colon * 4 < lines) return 0;
    return rec * 100 >= lines * 95 ? 2 : 1;
}

/* file-level analysis: one plan for every block */
/* light: the cheap analysis of the fastest settings: no JSON, log or word trials (whose transforms are also the slow
 * ones), and binary candidates scored by zstd -1 on a 256 KiB slice instead of zstd -3 on 1 MiB */
/* integer-array candidate for the planner: width from a .npy header (head = the header), else 2, 4 and 8 tried; each
 * coded as an INT graph with zstd at the scoring level on the slice [st, st+L). Returns the best cost per byte (or a
 * huge value) and sets the width and head. */
/* float-array candidate: float32 or a 16-bit float (or a .npy float dtype) whose rotated top byte (the exponent) is far more predictable
 * than its mantissa bytes; the byte alignment (head) with the most predictable exponent wins. Scored like the INT
 * candidate; returns the cost per byte (or a huge value), sets width and head. */
static double kg_plane_h(const uint8_t *p, size_t cnt, int w, int b, int rot) {   /* order-0 entropy of plane b, bits */
    uint32_t c[256] = {0}; size_t m = cnt < 8192 ? cnt : 8192, step = cnt / m;
    for (size_t q = 0; q < m; q++) {
        const uint8_t *e = p + q * step * (size_t)w; uint64_t v = 0; for (int k = 0; k < w; k++) v |= (uint64_t)e[k] << (8 * k);
        if (rot) v = (v << 1) | (v >> (8 * w - 1));
        c[(v >> (8 * b)) & 255]++;
    }
    double hs = 0; for (int k = 0; k < 256; k++) if (c[k]) { double q = (double)c[k] / m; hs -= q * log2(q); }
    return hs;
}
/* 8-bit image candidate (IMG8 node): row length and channel step from a .npy uint8 header (shape (H, W) or (H, W, C)),
 * else from common image widths x 1, 3 or 4 channels, keeping the layout whose median-predictor residuals are much
 * smaller than a left-neighbour predictor's on a sample. Scored like the other graph candidates. */
#define KG_SCREEN ((size_t)32 << 10)   /* graph trials: bytes coded first as a screen */
#define KG_SCREEN_SLACK 1.25            /* screen passes when within this factor of plain coding (a short run codes worse) */
static double kg_img8_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, double plain, size_t *bw, unsigned *bc, size_t *bh) {
    double best = 1e300; size_t h = 0, W = 0; unsigned C = 0;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL, *sh = hl <= n ? memmem(s, hl, "'shape': (", 10) : NULL;
        if (!d || !sh || d + 13 > s + hl || d[11] != 'u' || d[12] != '1' || memmem(s, hl, "'fortran_order': True", 21)) return best;
        uint64_t dims[4]; int nd = 0; const uint8_t *q = sh + 10;
        while (q < s + hl && *q != ')' && nd < 4) { while (q < s + hl && (*q == ' ' || *q == ',')) q++; if (q >= s + hl || *q == ')') break; uint64_t v = 0; while (q < s + hl && *q >= '0' && *q <= '9') v = v * 10 + (uint64_t)(*q++ - '0'); dims[nd++] = v; }
        if (nd == 2) { W = dims[1]; C = 1; } else if (nd == 3 && dims[2] >= 1 && dims[2] <= 4) { W = dims[1] * dims[2]; C = (unsigned)dims[2]; } else return best;
        h = hl;
    } else {
        /* row length: the lag (width x channels, common widths) at which bytes repeat best, on 4096 sampled positions;
         * it must stand out against the other lags, and the median predictor must not lose to the left neighbour */
        static const uint16_t WS[] = {128, 160, 192, 200, 240, 256, 300, 320, 352, 384, 400, 416, 480, 500, 512, 576, 600, 640, 720,
                                      768, 800, 960, 1000, 1024, 1080, 1200, 1280, 1366, 1440, 1536, 1600, 1920, 2048, 2560, 3840, 4096};
        enum { NS = 4096 }; size_t maxlag = 4096 * 4; if (n < 4 * maxlag) maxlag = n / 4;
        size_t span = n - maxlag, stp = span / NS ? span / NS : 1; uint32_t bestc = UINT32_MAX, sum = 0, cnt = 0; size_t bl = 0;
        for (size_t k = 0; k < sizeof WS / sizeof WS[0]; k++) for (unsigned c = 1; c <= 4; c++) {
            size_t lag = (size_t)WS[k] * c; if (c == 2 || lag > maxlag) continue;
            uint32_t cost = 0; for (size_t q = 0, i = maxlag; q < NS && i < n; q++, i += stp) { int d = s[i] - s[i - lag]; cost += (uint32_t)(d < 0 ? -d : d); }
            sum += cost / 64; cnt++; if (cost < bestc) { bestc = cost; bl = lag; }
        }
        if (!cnt || !bl || (uint64_t)(bestc / 64) * cnt * 2 > sum) return best;     /* rows repeat: one lag at half the average cost or less */
        uint32_t lc[5] = {0};
        for (size_t q = 0, i = maxlag; q < NS && i < n; q++, i += stp) for (unsigned c = 1; c <= 4; c++) { int d = s[i] - s[i - c]; lc[c] += (uint32_t)(d < 0 ? -d : d); }
        W = bl; C = 1; for (unsigned c = 3; c <= 4; c++) if (bl % c == 0 && lc[c] < lc[C]) C = c;
        size_t a0 = (n / 2 / W) * W, m = W * 16 < 65536 ? W * 16 : (65536 / W ? 65536 / W * W : W);
        if (a0 < W || a0 + m > n) return best;
        double pc = 0, left = 0;
        for (size_t i = a0; i < a0 + m; i++) { int x = s[i], d = x - kg_med(s[i - C], s[i - W], s[i - W - C]), dl = x - s[i - C]; pc += d < 0 ? -d : d; left += dl < 0 ? -dl : dl; }
        if (pc > left) return best;                             /* the 2-D predictor must not lose to the left neighbour */
    }
    if (W < 2 || n < h + 4 * W) return best;
    if (st < h) st = h;
    size_t a = h + (st - h) / W * W, l = (n - a < L ? n - a : L) / W * W; if (l < 2 * W) l = (n - a) / W * W < 2 * W ? 0 : 2 * W;
    codec_ctx cx = {0}; uint8_t *o = malloc(l + 64);
    if (o && plain < 1e299 && l > KG_SCREEN * 2 && W * 8 <= KG_SCREEN) {   /* screen on a few rows, as for integers */
        size_t ls = KG_SCREEN / W * W, zs = kg_encode_img8(&cx, C_ZSTD, zl, s + a, ls, W, C, 0, o, l + 64);
        if (!zs || (double)zs / ls > plain * KG_SCREEN_SLACK) { free(o); codec_ctx_free(&cx); return best; }
    }
    size_t z = o && l ? kg_encode_img8(&cx, C_ZSTD, zl, s + a, l, W, C, 0, o, l + 64) : 0;
    if (z) { best = (double)z / l; *bw = W; *bc = C; *bh = h; }
    free(o); codec_ctx_free(&cx);
    return best;
}

static double kg_float_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, double plain, int *bw, size_t *bh) {
    double best = 1e300; int w = 4; size_t h = 0; int fixed = 0;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL;
        if (!d || d + 13 > s + hl || d[10] == '>' || d[11] != 'f' || (d[12] != '2' && d[12] != '4' && d[12] != '8')) return best;
        h = hl; w = d[12] - '0'; fixed = 1;
        if (n < h + 4096) return best;                       /* same shape test as raw data: predictable exponent, random mantissa */
        if (kg_plane_h(s + h, (n - h) / w, w, w - 1, 1) > 6.0 || kg_plane_h(s + h, (n - h) / w, w, w - 2, 1) < 7.0) return best;
    }
    if (n < h + 4096) return best;
    int ws[2] = {w, 0}; size_t hs[2] = {h, 0}; int nw = 1;
    if (!fixed) {   /* float32 and 16-bit floats (bf16, fp16), each at the byte alignment with the most predictable exponent */
        nw = 0;
        for (int fw = 4; fw >= 2; fw -= 2) {
            double he = 1e9; size_t ph = 0;
            for (size_t k = 0; k < (size_t)fw; k++) { double e = kg_plane_h(s + k, (n - k) / fw, fw, fw - 1, 1); if (e < he) { he = e; ph = k; } }
            double hm = kg_plane_h(s + ph, (n - ph) / fw, fw, fw - 2, 1);
            if (he <= 6.0 && hm >= 7.0) { ws[nw] = fw; hs[nw] = ph; nw++; }   /* exponent predictable, mantissa near random */
        }
        if (!nw) return best;
    }
    codec_ctx cx = {0}; uint8_t *o = malloc(L + 64);
    for (int q = 0; o && q < nw; q++) {
        int fw = ws[q]; size_t fh = hs[q], s0 = st < fh ? fh : st, L0 = L > n - s0 ? n - s0 : L;
        size_t a = fh + (s0 - fh) / fw * fw, l = (n - a < L0 ? n - a : L0) / fw * fw;
        if (l >= 4096) {                                      /* screen: the planes' order-0 entropy must already beat plain coding */
            double e = 0; for (int b = 0; b < fw; b++) e += kg_plane_h(s + a, l / fw, fw, b, 1);
            if (e / (8.0 * fw) >= plain) continue;
        }
        size_t z = l ? kg_encode_float(&cx, C_ZSTD, zl, s + a, l, fw, 0, o, L + 64) : 0;
        if (z && (double)z / l < best) { best = (double)z / l; *bw = fw; *bh = fh; }
    }
    free(o); codec_ctx_free(&cx);
    return best;
}

/* Binary64 values that were written in decimal (DEC64). A row-major .npy '<f8' table gives its row length; any other
 * data is tried as a plain array of doubles when a spread sample of 8-byte words is nearly all decimal or one repeated
 * pattern (missing values). *bcols: values per row, or 0 for a plain array. */
static double kg_dec64_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, uint32_t *bcols, size_t *bh) {
    double best = 1e300; size_t h = 0, cols = 0;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL;
        if (!d || d + 13 > s + hl || d[10] != '<' || d[11] != 'f' || d[12] != '8') return best;
        h = hl;
        if (n >= h + 4096) {                                 /* 64 values across the array: a quarter must be decimal or NaN */
            size_t words = (n - h) / 8, ok = 0;
            for (size_t q = 0; q < 64; q++) {
                uint64_t bits = get64(s + h + (size_t)((uint64_t)q * words / 64) * 8); int64_t m; int e = 0;
                while (e < KG_DEC_ESEARCH && !kg_dec_fit(bits, KG_P10[e], &m)) e++;
                ok += e < KG_DEC_ESEARCH || (bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull;
            }
            if (ok < 16) return best;
        }
        const uint8_t *sh = memmem(s, hl, "'shape': (", 10), *fo = memmem(s, hl, "'fortran_order': False", 22);
        if (sh && fo) {                                      /* row-major: a row is the product of the dimensions after the first */
            const uint8_t *q = sh + 10, *e = s + hl; uint64_t dim = 0, prod = 1; int nd = 0, ok = 1, dig = 0;
            for (; q < e && *q != ')'; q++) {
                if (*q >= '0' && *q <= '9') { dim = dim * 10 + (uint64_t)(*q - '0'); dig = 1; if (dim > (1u << 30)) ok = 0; }
                else if (*q == ',') { if (nd++ && dig) { prod *= dim; if (prod > 65536) ok = 0; } dim = 0; dig = 0; }
            }
            if (dig && nd) { prod *= dim; if (prod > 65536) ok = 0; }
            if (ok && prod >= 2 && (n - h) / 8 / prod >= 16) cols = (size_t)prod;
        }
    } else {
        /* arrays inside other data (a raw dump, pickle, Arrow, HDF5: headers, then columns at any byte alignment):
         * 64 places across the input are probed, and a quarter of them must hold decimal doubles */
        if (n < 4096) return best;
        size_t hits = 0;
        for (size_t q = 0; q < 64; q++) hits += kg_dec_probe(s + (size_t)((uint64_t)q * (n - 136) / 64)) != 0;
        if (hits < 16) return best;
    }
    if (n < h + 512) return best;
    const size_t rb = cols ? cols * 8 : 8;
    if (st < h) st = h;
    size_t a = h + (st - h) / rb * rb, l = (n - a < L ? n - a : L) / rb * rb;
    if (l < 16 * rb || l < 512) { a = h; l = (n - h < L ? n - h : L) / rb * rb; if (l < 512) return best; }
    codec_ctx cx = {0}; uint8_t *o = malloc(l + 64);
    size_t z = o ? kg_encode_dec64(&cx, C_ZSTD, zl, s + a, l, cols, 0, o, l + 64) : 0;
    if (z) { best = (double)z / l; *bcols = (uint32_t)cols; *bh = h; }
    free(o); codec_ctx_free(&cx);
    return best;
}

static double kg_num_trial(const uint8_t *s, size_t n, size_t st, size_t L, int zl, double plain, int *bw, size_t *bh) {
    size_t h = 0; int ws[3] = {2, 4, 8}, nw = 3; double best = 1e300;
    if (n > 16 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? 10 + (size_t)(s[8] | s[9] << 8) : 12 + (size_t)(s[8] | s[9] << 8 | s[10] << 16 | (size_t)s[11] << 24);
        const uint8_t *d = hl <= n ? memmem(s, hl, "'descr': '", 10) : NULL;
        if (!d || d + 13 > s + hl || d[10] == '>' || (d[11] != 'i' && d[11] != 'u' && d[11] != 'f') || (d[12] != '2' && d[12] != '4' && d[12] != '8')) return best;
        h = hl; ws[0] = d[12] - '0'; nw = 1;
    }
    if (n - h < 4096) return best;
    if (st < h) st = h;
    if (L > n - st) L = n - st;
    {   /* score only the width with the fewest estimated residual bytes per input byte, and only when that estimate
         * is near the plain coding cost (the bit-length estimate ignores repetition, so clearly worse means no gain) */
        double eb = 1e300; int bw0 = 0;
        for (int q = 0; q < nw; q++) {
            int w = ws[q], o0, s0; size_t a = h + (st - h) / w * w, c = (n - a < L ? n - a : L) / w;
            if (c < 16) continue;
            double e = kg_num_choose(s + a, c, w, &o0, &s0) / (8.0 * w); if (e < eb) { eb = e; bw0 = w; }
        }
        if (!bw0 || eb > plain) return best;            /* estimate above plain coding: no gain */
        ws[0] = bw0; nw = 1;
    }
    codec_ctx cx = {0}; uint8_t *o = malloc(L + 64);
    for (int q = 0; o && q < nw; q++) {
        int w = ws[q]; size_t a = h + (st - h) / w * w, l = (n - a < L ? n - a : L) / w * w;
        if (plain < 1e299 && l > KG_SCREEN * 2) {            /* screen on a short run: far above plain coding means no full trial */
            size_t zs = kg_encode_num(&cx, C_ZSTD, zl, s + a, KG_SCREEN / w * w, w, 0, o, L + 64);
            if (!zs || (double)zs / (KG_SCREEN / w * w) > plain * KG_SCREEN_SLACK) continue;
        }
        size_t z = l ? kg_encode_num(&cx, C_ZSTD, zl, s + a, l, w, 0, o, L + 64) : 0;
        if (z && (double)z / l < best) { best = (double)z / l; *bw = w; *bh = h; }
    }
    free(o); codec_ctx_free(&cx);
    return best;
}

#ifndef SHUF_SCREEN
#define SHUF_SCREEN 0.97
#endif
static plan_t make_plan(const uint8_t *s, size_t n, int effort, int logs, int structured, int threads, int strong, int light) {
    const int numtrial = light == 4;                      /* balanced: quick analysis of binary data, with the integer trial kept */
    if (light && light != 4) logs = 0;
    plan_t pl = {T_NONE, 0, 0, 0};
    if (n < 4096) return pl;
    const size_t TRIAL = effort ? (1u << 20) : (256u << 10);   /* text-transform trial slice (fast: 256 KiB) */
    size_t h = n < 65536 ? n : 65536, pr = 0;
    for (size_t i = 0; i < h; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13; }
    if (pr * 100 > h * 95) {                                     /* text: field transposition if lines have a fixed field count */
        if (light == 2 || light == 3) return pl;                 /* minimal analysis: text is coded as it is */
        if (logs && n >= 65536) {                                /* wide delimited table (over 256 columns): kolmo graph */
            static const uint8_t WD[4] = {';', ',', '\t', '|'};
            for (int di = 0; di < 4 && pl.t != T_KG; di++) {
                size_t lines = 0, same = 0, i = 0; uint32_t k0 = 0;
                while (i < h) {
                    const uint8_t *e = memchr(s + i, '\n', h - i); if (!e) break;
                    uint32_t f = 1; for (const uint8_t *q = s + i; q < e; q++) f += *q == WD[di];
                    if (lines == 1) k0 = f;                      /* the first line may be a header */
                    if (lines >= 1) same += f == k0;
                    lines++; i = (size_t)(e - s) + 1;
                }
                if (lines < 4 || k0 <= 256 || same * 100 < (lines - 1) * 95) continue;
                uint8_t sep = '.';
                if (WD[di] != ',') { size_t dc = 0, cc = 0; for (size_t q = 1; q + 1 < h; q++) if ((uint8_t)(s[q - 1] - '0') < 10 && (uint8_t)(s[q + 1] - '0') < 10) { dc += s[q] == '.'; cc += s[q] == ','; } if (cc > dc) sep = ','; }
                const size_t TW = effort ? (512u << 10) : TRIAL;      /* wide rows repeat their structure every line: half a MiB decides */
                size_t mm = n < TW ? n : TW; while (mm > 0 && s[mm - 1] != '\n') mm--;
                if (mm < 16384) continue;
                const uint8_t *ts = s;                           /* large inputs: trial from the middle (the start may be atypical) */
                if (n > 4 * (size_t)TRIAL) {
                    const uint8_t *m0 = memchr(s + n / 2, '\n', n / 2); size_t st = m0 ? (size_t)(m0 - s) + 1 : 0;
                    if (st && st + mm <= n) { size_t e = mm; while (e > 0 && s[st + e - 1] != '\n') e--; if (e >= 16384) { ts = s + st; mm = e; } }
                }
                codec_ctx cx = {0}; uint8_t *o = malloc(mm + 64); ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t zcap = ZSTD_compressBound(mm); uint8_t *z = malloc(zcap);
                if (o && zc && z) {
                    size_t a = kg_encode_table(&cx, C_ZSTD, 1, ts, mm, WD[di], sep, o, mm + 64), b = ZSTD_compressCCtx(zc, z, zcap, ts, mm, 1);
                    int ok = a && !ZSTD_isError(b) && a * 100 < CZ_LOG_GAIN_PCT * b;
                    if (ok && effort) {                          /* slower settings: the graph must also win with a strong codec (max, */
                        int cc = strong ? C_BSC : C_ZSTD, lv = 9;     /* ultra: BWT; hc: zstd -9 standing in for zstd -19) */
                        size_t ms = mm; if (ms > (384u << 10)) { ms = 384u << 10; while (ms > 0 && ts[ms - 1] != '\n') ms--; }   /* a shorter run decides the same and costs a third */
                        size_t a2 = ms ? kg_encode_table(&cx, cc, lv, ts, ms, WD[di], sep, o, mm + 64) : 0, b2 = ms ? codec_compress(&cx, cc, lv, ts, ms, z, zcap) : 0;
                        ok = a2 && b2 && a2 * 100 < 97 * b2;
                    }
                    if (ok) { pl.t = T_KG; pl.a = WD[di]; pl.b = sep; }
                }
                free(o); free(z); ZSTD_freeCCtx(zc); codec_ctx_free(&cx);
            }
            if (pl.t == T_KG) return pl;
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        double tx0 = prof_now(); fprintf(stderr, "text: minified=%d\n", (int)(logs && n >= 65536 && json_minified(s, h)));
#endif
        if (logs && n >= 65536 && json_minified(s, h)) {   /* minified JSON: split into records, then as JSON lines */
            size_t mm = n < TRIAL ? n : TRIAL;             /* (shorter slices were tried: the gain does not show below 256 KiB) */
            uint8_t *sp = malloc(2 * mm + 64), *o = malloc(2 * mm + 64); ZSTD_CCtx *zc = ZSTD_createCCtx();
            size_t cap = ZSTD_compressBound(2 * mm); uint8_t *z = malloc(cap);
            int shaped = 1;
            if (sp && mm >= (64u << 10)) {
                /* records must share a few key layouts for the template transform to pay: split the first 64 KiB and
                 * count distinct key sequences over up to 256 records (a hash of each record's key names) */
                size_t sn0 = json_split(s, 64u << 10, sp), i = 0; uint32_t seen[64]; int ns = 0, lines = 0;
                while (i < sn0 && lines < 256) {
                    const uint8_t *e = memchr(sp + i, '\n', sn0 - i); size_t l = e ? (size_t)(e - (sp + i)) : sn0 - i; uint32_t hk = 2166136261u; int inq = 0; size_t qs = 0;
                    for (size_t q = 0; q < l; q++) {
                        uint8_t c = sp[i + q];
                        if (c == '\\' && inq) { q++; continue; }
                        if (c == '"') { if (!inq) { inq = 1; qs = q + 1; } else { inq = 0; if (q + 1 < l && sp[i + q + 1] == ':') { for (size_t r = qs; r < q; r++) hk = (hk ^ sp[i + r]) * 16777619u; hk = (hk ^ 0xFF) * 16777619u; } } }
                    }
                    int f = 0; for (int q = 0; q < ns; q++) if (seen[q] == hk) { f = 1; break; }
                    if (!f) { if (ns < 64) seen[ns] = hk; ns++; }
                    lines++; i += l + 1;
                }
                shaped = lines >= 8 && (ns <= 8 || ns * 16 <= lines);   /* a handful of layouts, or at most one per 16 records */
            }
            if (sp && o && zc && z && shaped) {
                size_t sn = json_split(s, mm, sp); int64_t tn = log_encode_m(sp, sn, o, 2 * mm + 64, 1);
                if (tn > 0) {
                    size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 1), b = ZSTD_compressCCtx(zc, z, cap, s, mm, 1);
                    if (!ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_LOG_GAIN_PCT * b) { pl.t = T_LOG; pl.a = 2; }
                }
            }
            free(sp); free(o); free(z); ZSTD_freeCCtx(zc);
            if (pl.t == T_LOG) return pl;
        }
        int js = logs && n >= 65536 ? json_lines(s, h) : 0;
        int logc = 0; size_t loga = 0, logb = 0;  /* the JSON trial's log-token result, reused by the log trial below */
        if (js == 1 && !strong && n > (64u << 10)) {  /* pretty JSON: a 64 KiB look first; no gain there, no full trial */
            size_t m0 = 64u << 10; while (m0 > 0 && s[m0 - 1] != '\n') m0--;
            uint8_t *o = malloc(m0 + 64); ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(m0); uint8_t *z = malloc(cap);
            int gain = 1;
            if (o && zc && z && m0 >= 16384) {
                size_t b = ZSTD_compressCCtx(zc, z, cap, s, m0, 1); gain = 0;
                for (int jm = 1; jm >= 0 && !gain && !ZSTD_isError(b); jm--) {
                    int64_t tn = log_encode_m(s, m0, o, m0 + 64, jm); if (tn <= 0) continue;
                    size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 1); gain = !ZSTD_isError(a) && a * 100 < b * 100;
                }
            }
            free(o); free(z); ZSTD_freeCCtx(zc);
            if (!gain) js = 0;
        }
        if (js == 2 || (js == 1 && !strong)) {   /* JSON: values become columns (max/ultra model pretty JSON well raw) */
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            if (mm >= 16384) {                   /* JSON tokens, or log tokens when values hold free text: the smaller */
                uint8_t *o = malloc(mm + 64); ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm); uint8_t *z = malloc(cap);
                if (o && zc && z) {
                    size_t b = ZSTD_compressCCtx(zc, z, cap, s, mm, 1), best = ZSTD_isError(b) ? 0 : b * CZ_LOG_GAIN_PCT / 100;
                    for (int jm = 1; jm >= 0; jm--) {
                        int64_t tn = log_encode_m(s, mm, o, mm + 64, jm);
                        if (jm == 0) { logc = 1; loga = (size_t)-1; logb = b; }
                        if (tn <= 0) continue;
                        size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 1);
                        if (jm == 0) { loga = a; logc = 2; }
                        if (!ZSTD_isError(a) && a < best) { best = a; pl.t = T_LOG; pl.a = (uint8_t)jm; }
                    }
                }
                free(o); free(z); ZSTD_freeCCtx(zc);
                if (pl.t == T_LOG) return pl;
            }
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        fprintf(stderr, "text: json_ms=%.3f js=%d\n", (prof_now() - tx0) * 1000, js); tx0 = prof_now();
#endif
        static const struct { uint8_t d; int q; } D[] = {{',', 1}, {'\t', 0}, {';', 1}, {'|', 0}, {' ', 1}};
        int *cnt = malloc(4001 * sizeof(int)); size_t head = n < (1 << 16) ? n : (1 << 16);
        if (!cnt) return pl;
        for (int di = 0; di < 5 && pl.t == T_NONE; di++) {
            int L = tp_fieldcounts(s, (int64_t)head, D[di].d, D[di].q, cnt, 4000);
            /* Version 3 can store 255 individual fields and the complete row
             * suffix in its final lane. A literal delimiter split is reversible
             * even if later blocks contain quotes or change width. */
            if (logs && !effort && L >= 2 && L < 9 && cnt[1] > 256 && n > head) {
                size_t probe = n < (256u << 10) ? n : (256u << 10);
                L = tp_fieldcounts(s, probe, D[di].d, D[di].q, cnt, 4000);
            }
            if (logs && !effort && L >= 9 && cnt[1] > 256) {
                int r = 2; while (r < L && cnt[r] == cnt[1]) r++;
                if (r == L) {
                    free(cnt);
                    return (plan_t){.t=T_TP, .a=D[di].d, .c=255, .wide=1};
                }
            }
            if (L < 51) continue;
            int best = 0, bc = 0;
            for (int i = 1; i < L; i++) { int k = cnt[i], c = 0; if (k == best) continue; for (int j = 1; j < L; j++) c += cnt[j] == k; if (c > bc) { bc = c; best = k; } }
            if (best >= 2 && best <= 256 && bc * 100 > (L - 1) * 95) { pl.t = T_TP; pl.a = D[di].d; pl.b = D[di].q; pl.c = best - 1; }
        }
        free(cnt);
        if (pl.t == T_TP) {                       /* typed table transform, kept only if it helps on a sample */
            size_t mm = head; while (mm > 0 && s[mm - 1] != '\n') mm--; if (!mm) mm = head;
            uint8_t *o = malloc(mm + TSLACK);
            if (!o) return pl;
            int64_t tn = tbl_encode(s, mm, (uint8_t)pl.a, pl.c + 1, pl.b, o);
            ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm + TSLACK); uint8_t *z = malloc(cap);
            if (!zc || !z) { free(o); free(z); ZSTD_freeCCtx(zc); return pl; }
            size_t a = tn > 0 ? ZSTD_compressCCtx(zc, z, cap, o, tn, 1) : (size_t)-1, b = ZSTD_compressCCtx(zc, z, cap, s, mm, 1);
            if (tn > 0 && !ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_TBL_GAIN_PCT * b) {
                pl.t = T_TBL; pl.fs = malloc(sizeof(fset_t));
                if (pl.fs) fset_discover(s, n, (uint8_t)pl.a, pl.c + 1, pl.b, effort, pl.fs, threads);
            }
            free(o); free(z); ZSTD_freeCCtx(zc);
        }
        size_t logcost = 0;                                /* the template transform's coded size on the trial slice */
        if (logs && pl.t == T_NONE && n >= 65536) {        /* logs: template transform, kept if it helps on a sample */
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            if (mm >= 16384 && logc) {           /* same slice already coded by the JSON trial */
                if (logc == 2 && !ZSTD_isError(loga) && !ZSTD_isError(logb) && loga * 100 < CZ_LOG_GAIN_PCT * logb) { pl.t = T_LOG; logcost = loga; }
            } else if (mm >= 16384) {
                uint8_t *o = malloc(mm + 64);
                if (!o) return pl;
                int64_t tn = log_encode(s, mm, o, mm + 64);
                if (tn > 0) {
                    ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm); uint8_t *z = malloc(cap);
                    if (!zc || !z) { free(o); free(z); ZSTD_freeCCtx(zc); return pl; }
                    size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 1), b = ZSTD_compressCCtx(zc, z, cap, s, mm, 1);
                    if (!ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_LOG_GAIN_PCT * b) { pl.t = T_LOG; logcost = a; }
                    free(z); ZSTD_freeCCtx(zc);
                }
                free(o);
            }
        }
        /* balanced: lines of a few fixed lengths, stored column by column. Taken when it codes the slice at least as
         * small as the template transform (or, where that did not apply, clears the same bar): it encodes and decodes
         * several times faster. */
        if (logs && numtrial && n >= 65536 && (pl.t == T_NONE || (pl.t == T_LOG && pl.a == 0 && logcost))) {
            size_t mm = n < TRIAL ? n : TRIAL; while (mm > 0 && s[mm - 1] != '\n') mm--;
            uint8_t *o = mm >= 16384 ? malloc(mm + TSLACK) : NULL;
            int64_t tn = o ? lines_encode(s, mm, o, mm + TSLACK) : -1;
            if (tn > 0) {
                ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm + TSLACK); uint8_t *z = malloc(cap);
                if (zc && z) {
                    size_t a = ZSTD_compressCCtx(zc, z, cap, o, (size_t)tn, 1), b = pl.t == T_LOG ? 0 : ZSTD_compressCCtx(zc, z, cap, s, mm, 1);
                    if (!ZSTD_isError(a) && !ZSTD_isError(b) && (pl.t == T_LOG ? a <= logcost : a * 100 < CZ_LOG_GAIN_PCT * b)) { pl.t = T_LINES; pl.a = pl.b = pl.c = 0; }
                }
                free(z); ZSTD_freeCCtx(zc);
            }
            free(o);
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        fprintf(stderr, "text: table_log_ms=%.3f t=%d\n", (prof_now() - tx0) * 1000, pl.t);
#endif
        if (logs && effort && pl.t == T_NONE && n >= 65536) {   /* prose and markup (eco and above): word dictionary */
            size_t mm = n < (2u << 20) ? n : (2u << 20);
            uint8_t *o = malloc(mm + 64);
            if (!o) return pl;
            int64_t tn = word_encode(s, mm, o, mm + 64);
            if (tn > 0) {
                ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(mm); uint8_t *z = malloc(cap);
                if (!zc || !z) { free(o); free(z); ZSTD_freeCCtx(zc); return pl; }
                size_t a = ZSTD_compressCCtx(zc, z, cap, o, tn, 3), b = ZSTD_compressCCtx(zc, z, cap, s, mm, 3);
                if (!ZSTD_isError(a) && !ZSTD_isError(b) && a * 100 < CZ_WORD_GAIN_PCT * b) pl.t = T_WORD;
                free(z); ZSTD_freeCCtx(zc);
            }
            free(o);
        }
        return pl;
    }
    /* binary: numeric byte-plane shuffle, chosen by a zstd -1 trial on a 32 KB slice */
    if (light == 4) light = 1;
    size_t mid = n / 2, sl = n - mid < 32768 ? n - mid : 32768;
    const uint8_t *smp = s + mid;
    int wonly = 0;                                        /* minimal analysis: at most one word width goes to the trials */
    if (light >= 2 && sl >= 4096) {
        /* a shuffle can only pay where some byte plane is more predictable than the bytes as a whole: compare the
         * mean plane entropy per width with the plain byte entropy on the sample (microseconds; no coder runs) */
        double h1 = kg_plane_h(smp, sl, 1, 0, 0), hb = 1e9;
        for (int w = 2; w <= 8; w *= 2) { double e = 0; for (int b = 0; b < w; b++) e += kg_plane_h(smp, sl / w, w, b, 0); e /= w; if (e < hb) { hb = e; wonly = w; } }
        if (hb > (light == 3 ? 0.75 : 0.93) * h1) return pl;  /* (speed mode asks for a plane a quarter below the byte entropy) */                     /* no plane stands out: plain coding, and no coder was run */
        if (light == 3) {                                   /* speed mode: LZ4 passes over incompressible input at several GB/s;
                                                             * a shuffle that makes such data compressible costs most of that speed */
            char lz[LZ4_COMPRESSBOUND(32768)]; int lc = LZ4_compress_default((const char *)smp, lz, (int)sl, (int)sizeof lz);
            if (lc <= 0 || (size_t)lc * 100 >= sl * 98) return pl;
        }
    }
    int noshuf = 0;
    if (numtrial && sl >= 4096) {                       /* balanced: the 28 shuffle trials only where some byte plane is
                                                         * more predictable than the bytes as a whole (see above) */
        double h1 = kg_plane_h(smp, sl, 1, 0, 0), hb = 1e9;
        for (int w = 2; w <= 8; w *= 2) { double e = 0; for (int b = 0; b < w; b++) e += kg_plane_h(smp, sl / w, w, b, 0); e /= w; if (e < hb) { hb = e; wonly = w; } }   /* and only at the width whose planes are most predictable */
        noshuf = hb > SHUF_SCREEN * h1;
    }
    ZSTD_CCtx *zc = ZSTD_createCCtx(); size_t cap = ZSTD_compressBound(sl);
    uint8_t *o = malloc(cap), *t = malloc(sl);
    if (!zc || !o || !t) { free(o); free(t); ZSTD_freeCCtx(zc); return pl; }
    size_t base = noshuf ? 0 : ZSTD_compressCCtx(zc, o, cap, smp, sl, 1);
    if (!noshuf && !ZSTD_isError(base) && base * 4 >= sl && wonly >= 0) {
        double best = base * 0.95;
        for (int w = 2; w <= 8; w *= 2) for (int c = 1; c <= 8; c++) {
            if (wonly && w != wonly) continue;
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
        size_t LM = light && !numtrial ? (1u << 18) : (1u << 20), L = n / 4 < LM ? n / 4 : LM, st = (n / 2 - L / 2) & ~(size_t)7;
        const int zl = light ? 1 : 3;                      /* scoring level */
        /* balanced keeps the 1 MiB slice of the full analysis: on a 47 MB weights file the 256 KiB slice picked a layout
         * that coded 0.8% larger, and the slice costs under a millisecond at level 1 */
        size_t bc = L * 2 + 1024; uint8_t *buf = malloc(bc), *zb = malloc(ZSTD_compressBound(bc));
        if (!buf || !zb) { free(buf); free(zb); ZSTD_freeCCtx(zc); return pl; }
        size_t zcap = ZSTD_compressBound(bc);
        double plain = (double)zcost(zc, s + st, L, zb, zcap, zl) / L, best = plain * CZ_STRUCT_GAIN_PCT / 100.0; plan_t keep = pl; int chosen = 0;
        double d64c = 1e300; uint32_t d64cols = 0; size_t d64h = 0;   /* the DEC64 trial's result, kept for a marked plan */
        if (pl.t == T_SHUF) {
            size_t u = (size_t)pl.a * pl.b, rows = L / u; shuffle_fwd(s + st, rows, pl.a, pl.b, pl.c, buf);
            double c = (double)zcost(zc, buf, rows * u, zb, zcap, zl) / (rows * u);
            if (c < best) { best = c; chosen = 1; }
        }
        img_plan_t im;
        int code = 0;                                      /* balanced: machine code where the call/jump transform wins is
                                                            * not an image, a record file or a number array */
        if (numtrial && pl.t != T_SHUF) {
            size_t ej = 0; for (size_t i = 0; i + 4 < L; i++) ej += ((s[st + i] & 0xFE) == 0xE8) & ((uint8_t)(s[st + i + 4] + 1) < 2);
            if (ej * 1000 >= L) {
                memcpy(buf, s + st, L); e8e9_fwd(buf, L);
                double c = (double)zcost(zc, buf, L, zb, zcap, zl) / L;
                if (c < best) { best = c; keep = pl; keep.t = T_X86; chosen = 4; code = 1; }
            }
        }
        if (code) goto structured_done;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        double image_start = prof_now();
#endif
        if (img16_detect(s, n, &im)) {
            int64_t e = img16_encode(s + st, L, st, &im, buf);
            if (e > 0) { double c = (double)zcost(zc, buf, (size_t)e, zb, zcap, zl) / L; if (c < best) { best = c; keep = pl; keep.t = T_IMG; keep.img = im; chosen = 2; } }
        }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        double image_ms = (prof_now() - image_start) * 1000;
        double record_start = prof_now();
#endif
        /* balanced: a layout that already saves a fifth is the answer; the remaining trials are alternatives to it */
        if (numtrial && chosen == 2 && best < plain * 0.80) goto structured_done;
        rec_plan_t *R = malloc(sizeof(rec_plan_t));
        if (R && rec_detect(s, n, effort, light ? 6 : 4, R)) {   /* layouts are searched only for a clear record period (4x the median lag; quick analysis 6x) */
            size_t rs = (size_t)R->h + (st > (size_t)R->h ? (st - R->h) / R->P * R->P : 0); if (!rs) rs = R->P;
            size_t rl = L / R->P * R->P;
            if (rs + rl <= n) {
                int64_t e = rec_encode(s + rs, rl, rs, R, buf, bc);
                if (e > 0) { double c = (double)zcost(zc, buf, (size_t)e, zb, zcap, zl) / rl; if (c < best) { best = c; keep = pl; keep.t = T_REC; keep.rec = R; R = NULL; chosen = 3; } }
            }
        }
        free(R);
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
        fprintf(stderr, "structure bytes=%zu image_ms=%.3f record_ms=%.3f\n", n, image_ms, (prof_now() - record_start) * 1000);
#endif
        {   /* executables: x86 call/jump transform, tried only where call/jump opcodes with a plausible 32-bit
             * displacement (top byte 00 or FF) are at least 1 in 1000 bytes; other data has about 1 in 16000 */
            size_t ej = 0; for (size_t i = 0; i + 4 < L; i++) ej += ((s[st + i] & 0xFE) == 0xE8) & ((uint8_t)(s[st + i + 4] + 1) < 2);
            if (ej * 1000 >= L) {
            memcpy(buf, s + st, L); e8e9_fwd(buf, L);
            double c = (double)zcost(zc, buf, L, zb, zcap, zl) / L;
            if (c < best && chosen < 2 && pl.t != T_SHUF) { best = c; if (keep.rec != pl.rec) free(keep.rec);   /* a record plan loses to this one */ keep = pl; keep.t = T_X86; chosen = 4; }
            else if (c < best && chosen == 0) { best = c; if (keep.rec != pl.rec) free(keep.rec); keep = pl; keep.t = T_X86; chosen = 4; }
            }
        }
        {   /* float arrays first (FLOAT node); integer arrays (INT node) only when the data does not look like floats */
            const size_t Lg = effort || L <= (256u << 10) ? L : (256u << 10);     /* fast: graph trials on 256 KiB */
            double plg = -1;                                    /* a shorter slice is compared by its gain over plain coding of that slice */
#define KG_ADJ(C) ((C) > 1e299 || Lg == L ? (C) : (C) * plain / (plg > 0 ? plg : (plg = (double)zcost(zc, s + st, Lg, zb, zcap, zl) / Lg)))
            /* the float trial keeps the full slice: its pre-check is cheap, and zstd's plain cost on 256 KiB misleads on float data */
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double kgt0 = prof_now();
#endif
            int w = 0; size_t hh = 0; double c = kg_float_trial(s, n, st, L, zl, plain, &w, &hh), cf = c; int fw = w; size_t fh = hh;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double kgt1 = prof_now();
#endif
            const int npy = n > 16 && !memcmp(s, "\x93NUMPY", 6);   /* quick analysis: integer and image trials only for a .npy header */
            if (cf > 1e299 && (!light || npy || numtrial)) {
                w = 0; hh = 0; c = kg_num_trial(s, n, st, Lg, zl, plain, &w, &hh); c = KG_ADJ(c);
                if (c < best) { best = c; if (keep.rec != pl.rec) free(keep.rec); keep = pl; keep.t = T_KG; keep.a = 0; keep.b = w; keep.c = (int)hh; chosen = 5; }
            }
            c = cf; w = fw; hh = fh;
            if (c < best) { best = c; if (keep.rec != pl.rec) free(keep.rec); keep = pl; keep.t = T_KG; keep.a = 0; keep.b = w | 16; keep.c = (int)hh; chosen = 6; }
            { uint32_t dc = 0; size_t dh = 0; c = kg_dec64_trial(s, n, st, L, zl, &dc, &dh);     /* decimal doubles: decode is slower than the other graphs, so it must win by 7% */
              d64c = c; d64cols = dc; d64h = dh;
              if (c < best * 0.93) { best = c; if (keep.rec != pl.rec) free(keep.rec); keep = pl; keep.t = T_KG; keep.a = 0; keep.b = dc ? 64 : 65; keep.c = (int)dh; keep.kgw = dc ? dc * 8 : 8; chosen = 8; } }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            double kgt2 = prof_now();
#endif
            size_t iw = 0, ih = 0; unsigned ic = 0; const size_t Li = light && !npy && Lg > (64u << 10) ? (64u << 10) : Lg;   /* quick analysis: a 64 KiB image trial */
            c = kg_img8_trial(s, n, st, Li, zl, plain, &iw, &ic, &ih);
            if (c < 1e299 && Li != Lg) c = c * plain / ((double)zcost(zc, s + st, Li, zb, zcap, zl) / Li); else c = KG_ADJ(c);   /* 8-bit images: IMG8 */
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
            fprintf(stderr, "kg float_ms=%.3f num_ms=%.3f img8_ms=%.3f\n", (kgt1 - kgt0) * 1000, (kgt2 - kgt1) * 1000, (prof_now() - kgt2) * 1000);
#endif
            if (c < best) { best = c; if (keep.rec != pl.rec) free(keep.rec); keep = pl; keep.t = T_KG; keep.a = 0; keep.b = 32 | (int)ic; keep.c = (int)ih; keep.kgw = (uint32_t)iw; chosen = 7; }
        }
#undef KG_ADJ
structured_done:
        if (chosen >= 5 && n >= (1u << 20)) {
            /* a numeric graph was chosen on one slice from the middle. Four short probes elsewhere check that the
             * rest of the input is the same kind of data: where the graph codes a probe clearly larger than plain
             * coding (a pickle whose integer columns are followed by strings), the plan is marked, and every block is
             * then coded both ways and the smaller kept. Inputs that are one kind of data throughout pay only the probes. */
            static const int at16[4] = {1, 5, 11, 15}; const size_t E = 32u << 10;
            for (int q = 0; q < 4 && !keep.verify; q++) {
                size_t pos = (n / 16 * (size_t)at16[q]) & ~(size_t)7; if (pos + E > n) continue;
                double pe = (double)zcost(zc, s + pos, E, zb, zcap, zl) / E, c = 1e300; int w3 = 0; size_t h3 = 0, iw3 = 0; unsigned ic3 = 0; uint32_t dc3 = 0;
                if (chosen == 5) c = kg_num_trial(s, n, pos, E, zl, 1e300, &w3, &h3);
                else if (chosen == 6) c = kg_float_trial(s, n, pos, E, zl, 1e300, &w3, &h3);
                else if (chosen == 7) c = kg_img8_trial(s, n, pos, E, zl, 1e300, &iw3, &ic3, &h3);
                else c = kg_dec64_trial(s, n, pos, E, zl, &dc3, &h3);
                if (c > pe * 1.10 && pe < 0.98) keep.verify = 1;
            }
            /* a marked whole-file plan (one word size and alignment for everything) gives way to the segment plan when
             * that also beat plain coding on the slice: segments follow the arrays wherever they sit */
            if (keep.verify && chosen != 8 && d64c < plain * CZ_STRUCT_GAIN_PCT / 100.0) {
                if (keep.rec != pl.rec) free(keep.rec);
                keep = pl; keep.t = T_KG; keep.a = 0; keep.b = d64cols ? 64 : 65; keep.c = (int)d64h; keep.kgw = d64cols ? d64cols * 8 : 8; keep.verify = 1; chosen = 8;
            }
        }
        if (chosen >= 2) pl = keep;
        else if (pl.t == T_SHUF && n >= (1u << 20)) {      /* the same probes for a byte shuffle */
            static const int at16[4] = {1, 5, 11, 15}; const size_t u = (size_t)pl.a * pl.b, E = (32u << 10) / u * u;
            for (int q = 0; q < 4 && !pl.verify; q++) {
                size_t pos = n / 16 * (size_t)at16[q] / u * u; if (pos + E > n || E > bc) continue;
                double pe = (double)zcost(zc, s + pos, E, zb, zcap, zl) / E;
                shuffle_fwd(s + pos, E / u, pl.a, pl.b, pl.c, buf);
                if ((double)zcost(zc, buf, E, zb, zcap, zl) / E > pe * 1.10 && pe < 0.98) pl.verify = 1;
            }
        }
        if (0) {}
        else if (!chosen && pl.t == T_SHUF && plain * 0.97 <= best) { /* shuffle did not beat plain coding at zstd -3 */ }
        free(buf); free(zb);
    }
    if (structured && n < (1u << 18)) {                    /* small arrays: the integer graph against plain and shuffle */
        const int zl = light && !numtrial ? 1 : 3; size_t zcap = ZSTD_compressBound(n); uint8_t *zb = malloc(zcap), *buf = malloc(n);
        if (zb && buf) {
            double plain = (double)zcost(zc, s, n, zb, zcap, zl) / n, best = plain * CZ_STRUCT_GAIN_PCT / 100.0;
            if (pl.t == T_SHUF) {
                size_t u = (size_t)pl.a * pl.b, rows = n / u; shuffle_fwd(s, rows, pl.a, pl.b, pl.c, buf);
                double c = (double)zcost(zc, buf, rows * u, zb, zcap, zl) / (rows * u); if (c < best) best = c;
            }
            int w = 0; size_t hh = 0; double c = kg_num_trial(s, n, 0, n, zl, 1e300, &w, &hh);   /* small input: no pre-check */
            if (c < best) { best = c; pl.t = T_KG; pl.a = 0; pl.b = w; pl.c = (int)hh; }
            w = 0; hh = 0; c = kg_float_trial(s, n, 0, n, zl, plain, &w, &hh);
            if (c < best) { best = c; pl.t = T_KG; pl.a = 0; pl.b = w | 16; pl.c = (int)hh; }
            { uint32_t dc = 0; size_t dh = 0; c = kg_dec64_trial(s, n, 0, n, zl, &dc, &dh);
              if (c < best * 0.93) { best = c; pl.t = T_KG; pl.a = 0; pl.b = dc ? 64 : 65; pl.c = (int)dh; pl.kgw = dc ? dc * 8 : 8; } }
            size_t iw = 0, ih = 0; unsigned ic = 0; c = kg_img8_trial(s, n, 0, n, zl, 1e300, &iw, &ic, &ih);
            if (c < best) { pl.t = T_KG; pl.a = 0; pl.b = 32 | (int)ic; pl.c = (int)ih; pl.kgw = (uint32_t)iw; }
        }
        free(zb); free(buf);
    }
    ZSTD_freeCCtx(zc);
    return pl;
}

/* POSIX allows a short positional read or write, including after a signal. */
static int read_block(int fd, uint8_t *p, size_t n, off_t off) {
    while (n) {
        ssize_t count = pread(fd, p, n > SSIZE_MAX ? SSIZE_MAX : n, off);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return KOLMO_ERR_IO;
        p += count; n -= count; off += count;
    }
    return 0;
}

static int write_block(int fd, const uint8_t *p, size_t n, off_t off) {
    while (n) {
        ssize_t count = pwrite(fd, p, n > SSIZE_MAX ? SSIZE_MAX : n, off);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return KOLMO_ERR_IO;
        p += count; n -= count; off += count;
    }
    return 0;
}

/* block boundaries: newline-aligned for transposed text, element-aligned for shuffled arrays */
static uint32_t make_blocks(const uint8_t *s, size_t n, uint32_t B, plan_t pl, uint64_t **starts_out,
                            int fd, uint64_t offset, int64_t *error) {
    size_t cap = n / (B / 2 + 1) + 2; uint64_t *st = array_alloc(cap, sizeof(uint64_t), 0); uint32_t nb = 0; size_t pos = 0;
    *starts_out = NULL;
    if (!st) return 0;
    if (pl.t == T_SHUF) { uint32_t u = pl.a * pl.b; B = B / u * u; if (!B) B = u; }
    if (pl.t == T_IMG) B &= ~1u;
    if (pl.t == T_REC) { B = B / pl.rec->P * pl.rec->P; if (!B) B = pl.rec->P; }
    int kgnum = pl.t == T_KG && pl.a == 0;                     /* integer graph: width-aligned blocks after the head */
    if (kgnum) {
        uint32_t kw = pl.b & 96 ? pl.kgw : (uint32_t)(pl.b & 15);     /* IMG8, DEC64: whole rows; INT/FLOAT: whole words */
        B = B / kw * kw; if (!B) B = kw;
    }
    while (pos < n) {
        size_t end = B < n - pos ? pos + B : n;
        if (pl.t == T_REC && pos == 0) end = (size_t)pl.rec->h + B < n ? (size_t)pl.rec->h + B : n;
        if (kgnum && pos == 0) end = (size_t)pl.c + B < n ? (size_t)pl.c + B : n;
        if ((pl.t == T_TP || pl.t == T_TBL || pl.t == T_LOG || pl.t == T_LINES || (pl.t == T_KG && !kgnum) || pl.experimental_table) && end < n) {
            size_t lo = pos + B / 2, e = end;
            if (fd >= 0 && (pl.wide || pl.experimental_table)) {
                /* Avoid faulting a sparse page from the planning map at every
                 * boundary of a large file. Search the same bytes backwards. */
                uint8_t tail[4096];
                while (e > lo) {
                    size_t count = e - lo < sizeof(tail) ? e - lo : sizeof(tail);
                    if (read_block(fd, tail, count, offset + e - count)) {
                        free(st); *error = KOLMO_ERR_IO; return 0;
                    }
                    size_t at = count;
                    while (at && tail[at - 1] != '\n') at--;
                    e -= count - at;
                    if (at) break;
                }
            } else while (e > lo && s[e - 1] != '\n') e--;
            if (e > lo) end = e;
        }
        if (nb == UINT32_MAX - 1) { free(st); return 0; }
        if ((size_t)nb + 1 >= cap) {
            if (cap > SIZE_MAX / 2) { free(st); return 0; }
            uint64_t *grown = array_resize(st, cap * 2, sizeof(uint64_t));
            if (!grown) { free(st); return 0; }
            st = grown; cap *= 2;
        }
        st[nb++] = pos; pos = end;
    }
    if (n == 0) st[nb++] = 0;
    st[nb] = n;
    *starts_out = st; return nb;
}

static size_t slot_size(uint32_t B, int codec) { size_t a = codec_bound(codec, (size_t)B + TSLACK), b = codec_bound(C_BSC, (size_t)B + TSLACK); return (a > b ? a : b) + 256 * 4 + 64; }

/* Speed-tier (rt/fast) default block size: aim for ~8 balanced blocks so decode and
   compress parallelise across cores, with a 1 MiB floor (below it ratio drops faster
   than speed rises) and the 4 MiB ceiling (so large inputs are unchanged). A pure
   function of n, so output stays byte-identical across machines regardless of cores. */
static uint32_t kolmo_fast_block(size_t n) {
    uint64_t b = (uint64_t)n / 8;
    if (b < (1u << 20)) b = 1u << 20;
    if (b > (4u << 20)) b = 4u << 20;
    return (uint32_t)b;
}
/* worst case: every block stored raw (+ index entry); blocks are >= B/2 except the last */
size_t kolmo_bound(size_t n, const kolmo_params *p) {
    uint32_t B = p && p->block ? p->block : (p && p->mode >= KOLMO_MAX ? (128u << 20) : p && p->mode >= KOLMO_ECO ? (16u << 20) : kolmo_fast_block(n));
    size_t nb = n / (B / 2 > 0 ? B / 2 : 1) + 2 + 2 * (n >> 20);     /* + archive segments (>= 1 MiB each) */
    return HDR + nb * (IDX + 4) + n + 64 + 2 * IDX;
}

/* ------------------------------------------------------------------ parallel driver */
typedef struct {
    const uint8_t *src; int input_fd; uint8_t *dst; const uint64_t *starts; uint32_t nb; size_t slot; uint8_t *idx;
    int codec, level, default_level, output_fd; plan_t pl; atomic_uint next, newmodes; atomic_int err; size_t B;
    const plan_t *plans; const uint32_t *bplan; const uint64_t *rstart;   /* per-block plan (container v2) */
    const uint64_t *poff;   /* decode: payload offsets */
    uint64_t rlo, rhi; uint8_t *rdst;   /* range read */
    uint8_t **out;          /* compressed block copies */
    size_t *outlen;         /* their sizes */
    pthread_mutex_t wmu; uint32_t wnext; size_t wpos, cap; int werr;   /* blocks are written to dst in order as they finish */
    int psum;               /* checksums over stored bytes (rt) instead of decoded bytes */
    const void *dict; size_t dictlen;   /* optional zstd dictionary, read-only by workers */
} job_t;
static void append_bytes(job_t *j, const uint8_t *bytes, size_t size) {
    if (j->werr) return;
    if (size > j->cap - j->wpos) { j->werr = KOLMO_ERR_DSTSIZE; return; }
    if (j->output_fd >= 0) j->werr = write_block(j->output_fd, bytes, size, j->wpos);
    else memcpy(j->dst + j->wpos, bytes, size);
    j->wpos += size;
}

static void drain_blocks(job_t *j) {
    while (j->wnext < j->nb && j->out[j->wnext]) {
        uint32_t next = j->wnext++;
        append_bytes(j, j->out[next], j->outlen[next]);
        free(j->out[next]); j->out[next] = NULL;
    }
}

/* Serialize the next payload. Copy out-of-order results outside the lock. */
static void emit_block(job_t *j, uint32_t i, const uint8_t *header, const uint8_t *body, size_t size) {
    pthread_mutex_lock(&j->wmu);
    if (i == j->wnext) {
        if (!header) append_bytes(j, body, size);
        else { append_bytes(j, header, 4); append_bytes(j, body, size - 4); }
        j->wnext++;
        drain_blocks(j);
        pthread_mutex_unlock(&j->wmu);
        return;
    }
    pthread_mutex_unlock(&j->wmu);
    uint8_t *saved = malloc(size);
    if (!saved) { atomic_store(&j->err, KOLMO_ERR_ALLOC); return; }
    if (!header) memcpy(saved, body, size);
    else { memcpy(saved, header, 4); memcpy(saved + 4, body, size - 4); }
    pthread_mutex_lock(&j->wmu);
    j->out[i] = saved; j->outlen[i] = size;
    drain_blocks(j);
    pthread_mutex_unlock(&j->wmu);
}

static void *compress_worker(void *arg) {
    job_t *J = arg; tls_t *S = tls_begin();
    if (!S) { atomic_store(&J->err, KOLMO_ERR_ALLOC); return NULL; }
    codec_ctx *xp = &S->x;
    xp->dict = J->dict; xp->dictlen = J->dictlen;                          /* per-call dictionary (read-only) */
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->B + TSLACK + 64);
    uint8_t *slot = S->slot = grow(S->slot, &S->scap, J->slot + 4);
    uint8_t *input = J->input_fd >= 0 ? malloc(J->B ? J->B : 1) : NULL;
    if (!tbuf || !slot || (J->input_fd >= 0 && !input)) { free(input); atomic_store(&J->err, KOLMO_ERR_ALLOC); tls_end(S); return NULL; }
    for (;;) {
        uint32_t i = atomic_fetch_add(&J->next, 1); if (i >= J->nb) break;
        const plan_t pl = J->plans[J->bplan[i]]; const uint64_t rs = J->rstart[i];
        size_t len = J->starts[i + 1] - J->starts[i];
        const uint8_t *raw;
        if (input) {
            if (read_block(J->input_fd, input, len, J->starts[i])) { atomic_store(&J->err, KOLMO_ERR_IO); break; }
            raw = input;
        } else raw = J->src + J->starts[i];
        const uint8_t *in = raw; size_t meta = 0; int tflag = 0; unsigned modes = 0;
        uint8_t *sl = slot + 4;                                             /* slot: plan (4 bytes) | meta | codec data */
        slot[0] = (uint8_t)pl.t; slot[1] = (uint8_t)pl.a; slot[2] = (uint8_t)pl.b; slot[3] = (uint8_t)pl.c;
#ifdef KOLMO_FRONTIER
        size_t trial_len = 0;
        if (!J->dict && J->default_level) {
            if (pl.experimental_table)
                trial_len = frontier_table_encode(raw, len, pl.experimental_table, tbuf, len + TSLACK);
            else if (pl.lags.near)
                trial_len = frontier_lag_encode(raw, len, &pl.lags, tbuf, len + TSLACK);
        }
        if (trial_len) {
            put32(sl, (uint32_t)trial_len);
            slot[0] = T_TBL; slot[1] = (uint8_t)pl.experimental_table;
            slot[2] = slot[3] = 0; tflag = pl.experimental_table ? 10 : 11;
            meta = 4; in = tbuf; modes |= 4;
        } else
#endif
        if (pl.t == T_TP && len) {
            int k = pl.c + 1; int64_t colsz[256];
            int quoted = pl.b && (memchr(raw, '"', len) || memchr(raw, '[', len) || memchr(raw, ']', len));
            if (quoted) { transpose_fwd(raw, len, (uint8_t)pl.a, k, pl.b, tbuf, colsz); tflag = 1; }
            else if (pl.wide) { tpq_wide_fwd(raw, len, (uint8_t)pl.a, tbuf, colsz); tflag = 2; }
            else {
                uint32_t *pos = S->pos = grow(S->pos, &S->pcap, (J->B + 1) * sizeof(uint32_t));
                if (!pos) { atomic_store(&J->err, KOLMO_ERR_ALLOC); break; }
                tpq_fwd(raw, len, (uint8_t)pl.a, k, tbuf, colsz, pos); tflag = 2;
            }
            for (int j = 0; j < k; j++) put32(sl + 4 * j, (uint32_t)colsz[j]);
            meta = 4 * k; in = tbuf;
        } else if (pl.t == T_TBL && len) {
            int64_t tn = tbl2_encode(raw, len, (uint8_t)pl.a, pl.c + 1, pl.b, pl.fs, tbuf, &modes);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 4; }
            else modes = 0;
        } else if (pl.t == T_LOG && len) {
            uint8_t *sp = pl.a == 2 && !memchr(raw, '\n', len) ? malloc(2 * len + 64) : NULL;
            if (sp) {                                   /* minified JSON: records split onto lines (block flag 10) */
                size_t sn = json_split(raw, len, sp); int64_t tn = log_encode_m(sp, sn, tbuf, len + TSLACK, 1);
                if (tn > 0) { put32(sl, (uint32_t)tn); put32(sl + 4, (uint32_t)(sn - len)); meta = 8; in = tbuf; tflag = 10; modes |= 4; }
                free(sp);
            } else {
                int64_t tn = log_encode_m(raw, len, tbuf, len + TSLACK, pl.a >= 1);
                if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 5; }
            }
        } else if (pl.t == T_LINES && len) {
            int64_t tn = lines_encode(raw, len, tbuf, len + TSLACK);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 12; modes |= 16; }
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
        size_t inlen = (tflag >= 3 && tflag <= 7) || tflag >= 9 ? get32(sl) : len, cl = 0;
        int codec = J->codec, level = (pl.wide || (tflag >= 10 && pl.t == T_TBL)) && J->default_level ? 1 : J->level;
        if (pl.t == T_KG && len) {                                          /* kolmo graph: the block is a program + leaves */
            /* max, wide table: the column leaves are text, so they take BWT like any text block at max (LZMA2 on the
             * leaves was 3% smaller and ten times slower to encode) */
            int kgc = codec == C_LZMA && pl.a ? C_BSC : codec, kgl = kgc != codec ? 9 : level;
            cl = pl.a ? kg_encode_table(xp, kgc, kgl, raw, len, (uint8_t)pl.a, (uint8_t)pl.b, sl, J->slot)
                      : pl.b & 32 ? kg_encode_img8(xp, kgc, kgl, raw, len, pl.kgw, (unsigned)(pl.b & 15), rs == 0 ? (size_t)pl.c : 0, sl, J->slot)
                      : pl.b & 64 ? kg_encode_dec64(xp, kgc, kgl, raw, len, pl.b & 1 ? 0 : pl.kgw / 8, rs == 0 ? (size_t)pl.c : 0, sl, J->slot)
                      : pl.b & 16 ? kg_encode_float(xp, kgc, kgl, raw, len, pl.b & 15, rs == 0 ? (size_t)pl.c : 0, sl, J->slot)
                      : kg_encode_num(xp, kgc, kgl, raw, len, pl.b, rs == 0 ? (size_t)pl.c : 0, sl, J->slot);
            if (cl && (pl.b & 32) && !pl.a && (codec == C_LZMA || codec == C_CM)) {
                /* 8-bit images under the slow coders: block-sorted leaves are 5 to 10% smaller than LZMA2 leaves on
                 * colour photographs (the residual planes are noise-like, with no matches to find). Both are coded. */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t c2 = alt ? kg_encode_img8(xp, C_BSC, 9, raw, len, pl.kgw, (unsigned)(pl.b & 15), rs == 0 ? (size_t)pl.c : 0, alt, J->slot) : 0;
                if (c2 && c2 < cl) { memcpy(sl, alt, c2); cl = c2; }
            }
            if (cl && (pl.b & 32) && !pl.a && codec == C_CM && level != 1 && len < ((size_t)1 << 28)) {
                /* ultra, 8-bit image: context mixing with the image model (the pixel's neighbours as contexts) over
                 * the plain pixels; 12 to 20% smaller than the predictor graph on photographs */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t c2 = alt ? cm_compress_img(raw, len, pl.kgw, (unsigned)(pl.b & 15), rs == 0 ? (uint32_t)pl.c : 0, alt, J->slot) : 0;
                if (c2 && c2 < cl) { memcpy(sl, alt, c2); cl = c2; codec = C_CMI; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; goto coded; }
            }
            if (cl && len < (256u << 10)) {                                 /* small block: the plan was scored with zstd; the block's
                                                                             * own coder on the plain bytes may still be smaller */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t pc = alt ? codec_compress(xp, codec, level, raw, len, alt, J->slot) : 0;
                if (pc && pc < cl) cl = 0;
            }
            if (cl && !pl.a && len >= (256u << 10) && (pl.verify || codec == C_CM || ((pl.b & 64) && (codec == C_LZMA || codec == C_BSC)))) {
                /* decimal doubles under the slow coders: the plan was scored with zstd, and on small-integer tables
                 * block sorting or context mixing over the plain bytes can still be smaller. Marked plans (the input
                 * is not one kind of data throughout): this block may be the part the graph does not fit. Both are
                 * coded; the smaller one is kept. */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4); int bc = 0; size_t bz = cl;
                if (alt && codec != C_ZSTD && codec != C_LZ4) {
                    /* the slow coders: block sorting of a slice from the middle tells whether plain coding is within
                     * reach (a slice sorts worse than the block, so the limit is loose); otherwise the graph stands */
                    size_t sn = len < (512u << 10) ? len : (512u << 10), so = ((len - sn) / 2) & ~(size_t)63;
                    size_t cs = codec_compress(xp, C_BSC, 9, raw + so, sn, alt, J->slot);
                    /* ultra: every numeric graph block comes here, and context mixing over the plain bytes costs 20
                     * times the graph's time, so plain block sorting must already be within 2% (model weights are not) */
                    if (!cs || (double)cs / sn > (codec == C_CM ? 1.02 : 1.35) * (double)cl / len) alt = NULL;
                }
                size_t a1 = alt ? codec_compress(xp, codec, level, raw, len, alt, J->slot) : 0;
                if (a1 && a1 < bz) { memcpy(sl, alt, a1); bz = a1; bc = codec; }
                size_t a2 = alt && codec == C_LZMA ? codec_compress(xp, C_BSC, 9, raw, len, alt, J->slot) : 0;
                if (a2 && a2 < bz) { memcpy(sl, alt, a2); bz = a2; bc = C_BSC; }
                if (bc) { cl = bz; codec = bc; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; goto coded; }
            }
            if (cl) { codec = C_GRAPH; tflag = 0; meta = 0; modes |= 8; goto coded; }
            slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0;               /* did not apply: plain block */
        }
        if (codec == C_CM && tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; }                    /* ultra: CM models words itself */
        if (codec == C_LZMA && level == 1) { codec = C_BSC; if (tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; } }   /* max -l 1: BWT for every block */
        else if (codec == C_LZMA && (tflag == 10 || (tflag == 5 && pl.t == T_LOG && pl.a == 1))) codec = C_BSC;            /* max, JSON token streams (minified or one record per line): BWT won over LZMA2 by 10%; coding it first skips the LZMA2 pass */
        else if (codec == C_LZMA && (!tflag || tflag == 9) && looks_text(raw, len)) { codec = C_BSC; if (tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; } }   /* max: text to BWT */
        if (codec == C_ZSTD && level == 19 && tflag == 9) level = CZ_HC_TEXT_LEVEL;                                  /* hc text: ultra level */
#ifdef KOLMO_FRONTIER
#ifdef KOLMO_FRONTIER_PATCHED_PCM
        if (pl.experimental_pcm && !J->dict && J->default_level) {
            cl = frontier_pcm_encode(raw, len, pl.experimental_pcm, sl, J->slot);
            if (cl) {
                codec = C_PCM; tflag = 0; meta = 0; modes |= 4;
                memset(slot, 0, 4);
            }
        }
#endif
        if (pl.experimental_dna && !J->dict && J->default_level) {
            size_t packed = frontier_dna_encode(raw, len, tbuf, J->B + TSLACK);
            if (packed) {
                cl = frontier_runs_encode(tbuf, packed, sl, J->slot);
                if (cl) codec = C_DNA_RUNS;
                else {
                    cl = frontier_prefix_encode(tbuf, packed, sl, J->slot);
                    if (cl) codec = C_DNA_PREFIX;
                    else { memcpy(sl, tbuf, packed); cl = packed; codec = C_DNA; }
                }
                modes |= 4;
            }
        }
#endif
#ifdef KOLMO_FRONTIER_FAST_PCM
        if (pl.experimental_pcm && !J->dict && J->default_level && codec == C_ZSTD) level = 1;
#endif
        if (!cl) cl = codec_compress(xp, codec, level, in, inlen, sl + meta, J->slot - meta);
        if (pl.verify && tflag == 1 && pl.t == T_SHUF && cl && len >= (64u << 10)) {   /* marked plan: this block may not suit the shuffle */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            size_t a1 = alt ? codec_compress(xp, codec, level, raw, len, alt, J->slot) : 0;
            if (a1 && a1 < cl) { memcpy(sl, alt, a1); cl = a1; in = raw; inlen = len; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; }
        }
        if (codec == C_LZMA && cl) {                  /* max, binary block: BWT too, kept only when clearly smaller */
            /* BWT decodes about half as fast as LZMA2 here, so it must pay for that: at least KOLMO_BWT_GAIN_PCT smaller. */
            /* BWT is tried only where it has won in tests: not on x86 code, typed table columns or log token streams (4 to 12% larger), and only when LZMA2 already reaches
             * 35% or less (on less compressible binary data BWT was 1.5 to 8% larger) */
            uint8_t *alt = tflag == 8 || tflag == 4 || tflag == 5 || (uint64_t)cl * 100 > (uint64_t)inlen * 35 ? NULL : (S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4));
            if (alt && inlen > (8u << 20)) {                /* large blocks only: on small ones the slice costs as much as the attempt */
                /* a short slice from the middle, coded both ways (BWT against LZMA2 at its fastest preset), predicts the
                 * outcome: where BWT won on the whole block the slice ratio was at most 1.02, where it lost by 5% or
                 * more it was 1.04 or higher. Above 1.03 the full attempt is skipped. */
                size_t sn = inlen / 8 < (512u << 10) ? inlen / 8 : (512u << 10), so = ((inlen - sn) / 2) & ~(size_t)63;
                size_t sb = codec_compress(xp, C_BSC, 9, in + so, sn, alt, J->slot - meta), slz = sb ? codec_compress(xp, C_LZMA, 1, in + so, sn, alt, J->slot - meta) : 0;
                if (!sb || !slz || (uint64_t)sb * 100 > (uint64_t)slz * 103) alt = NULL;
            }
            size_t c2 = alt ? codec_compress(xp, C_BSC, 9, in, inlen, alt, J->slot - meta) : 0;
            if (c2 && (uint64_t)c2 * 100 <= (uint64_t)cl * (100 - KOLMO_BWT_GAIN_PCT)) { memcpy(sl + meta, alt, c2); cl = c2; codec = C_BSC; }
        }
        if ((codec == C_LZMA || codec == C_BSC) && J->codec == C_LZMA && cl && in != raw && len >= (64u << 10)) {
            /* max, transformed block: the transform was chosen by its zstd score, and under block sorting or LZMA2 the
             * plain bytes can be smaller (Silesia nci: template tokens 1.53 MB, plain block sorting 1.23 MB). Block
             * sorting of the plain block is tried when a slice from the middle comes within 10% of the transformed
             * block's ratio (25% for text transforms); blocks up to 2 MiB also try plain LZMA2. A plain result must be 3% smaller to be kept. */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            int try_bsc = alt != NULL, bc = 0; size_t bz = cl + meta;
            if (alt && len > (2u << 20)) {
                size_t sn = 512u << 10, so = ((len - sn) / 2) & ~(size_t)63, cs = codec_compress(xp, C_BSC, 9, raw + so, sn, alt, J->slot);
                /* a slice sorts worse than the whole block, most of all on repetitive text (nci: slice 5.2%, block 3.7%) */
                const double tol = tflag == 4 || tflag == 5 || tflag == 12 ? 1.25 : 1.10;
                if (!cs || (double)cs / sn > tol * (double)bz / len) try_bsc = 0;
            }
            size_t a1 = try_bsc ? codec_compress(xp, C_BSC, 9, raw, len, alt, J->slot) : 0;
            if (a1 && (uint64_t)a1 * 100 <= (uint64_t)bz * 97) { memcpy(sl, alt, a1); bz = a1; bc = C_BSC; }
            /* plain LZMA2 only where plain block sorting came within 25% of the transformed block */
            size_t a2 = alt && len <= (2u << 20) && a1 && (double)a1 <= 1.25 * (double)(cl + meta) ? codec_compress(xp, C_LZMA, level, raw, len, alt, J->slot) : 0;
            if (a2 && (uint64_t)a2 * 100 <= (uint64_t)(cl + meta) * 97 && a2 < bz) { memcpy(sl, alt, a2); bz = a2; bc = C_LZMA; }
            if (bc) { cl = bz; codec = bc; in = raw; inlen = len; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; }
        }
        if (codec == C_CM && level != 1) {            /* ultra: best of context mixing, LZMA2 and (text) BWT per block */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            if (alt) {
                int alts[2] = {C_LZMA, C_BSC};
                for (int k = 0; k < 2; k++) {
                    int text = !tflag && looks_text(in, inlen);
                    if (alts[k] == C_BSC && !text) continue;
                    if (alts[k] == C_LZMA && text) continue;  /* text: BWT always beat LZMA2 in tests; saves ~25% of the time */
                    if (cl && inlen > (2u << 20)) {           /* large block: try the alternative on a 1 MiB slice first */
                        size_t sn = 1u << 20, so = (inlen - sn) / 2;
                        size_t cs = codec_compress(xp, alts[k], 9, in + so, sn, alt, J->slot - meta);
                        if (!cs || (double)cs / sn > 0.97 * (double)cl / inlen) continue;   /* not within reach of CM */
                    }
                    size_t c2 = codec_compress(xp, alts[k], 9, in, inlen, alt, J->slot - meta);
#if defined(KOLMO_TRACE) || defined(KOLMO_PROF)
                    fprintf(stderr, "len %zu t%d text %d cm %zu alt%d %zu\n", (size_t)inlen, tflag, looks_text(in, inlen), cl, alts[k], c2);
#endif
                    if (c2 && (!cl || c2 < cl)) { memcpy(sl + meta, alt, c2); cl = c2; codec = alts[k]; }
                }
                if (cl && in != raw && len >= (64u << 10)) {
                    /* ultra, transformed block: the transform was chosen by its zstd score, and the plain bytes can be
                     * smaller under the strong coders (a 1 MB image array was larger at ultra than at max). Plain block
                     * sorting is quick; when it comes within 2% of the transformed block, plain context mixing and,
                     * up to 2 MiB, plain LZMA2 are tried as well. The smallest is kept. */
                    int bc = 0; size_t bz = cl + meta, ref = cl + meta;
                    size_t a1 = codec_compress(xp, C_BSC, 9, raw, len, alt, J->slot);
                    if (a1 && a1 < bz) { memcpy(sl, alt, a1); bz = a1; bc = C_BSC; }
                    if (a1 && (double)a1 <= 1.02 * (double)ref) {
                        size_t a2 = codec_compress(xp, C_CM, level, raw, len, alt, J->slot);
                        if (a2 && a2 < bz) { memcpy(sl, alt, a2); bz = a2; bc = C_CM; }
                        size_t a3 = len <= (2u << 20) ? codec_compress(xp, C_LZMA, 9, raw, len, alt, J->slot) : 0;
                        if (a3 && a3 < bz) { memcpy(sl, alt, a3); bz = a3; bc = C_LZMA; }
                    }
                    if (bc) { cl = bz; codec = bc; in = raw; inlen = len; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; }
                }
            }
        }
coded:;
        size_t tot = 4 + meta + cl;
        uint8_t *e = J->idx + (size_t)i * IDX;
        if (cl == 0 || cl + meta >= len) {                                  /* store raw */
            codec = C_STORE; tflag = 0; tot = 4 + len;
            static const uint8_t h0[4] = {T_NONE, 0, 0, 0};
            emit_block(J, i, h0, raw, tot);
        } else {
            emit_block(J, i, NULL, slot, tot);
            atomic_fetch_or_explicit(&J->newmodes, modes, memory_order_relaxed);
        }
        put32(e, (uint32_t)len); put32(e + 4, (uint32_t)tot);
        if (J->psum) put32(e + 8, codec == C_STORE ? (uint32_t)kolmo_xxh3(raw, len, 0x637a6e31u ^ (uint32_t)T_NONE)
                                                   : (uint32_t)kolmo_xxh3(slot + 4, tot - 4, 0x637a6e31u ^ get32(slot)));
        else put32(e + 8, blk_sum(3, raw, len));
        e[12] = (uint8_t)codec; e[13] = (uint8_t)tflag; e[14] = e[15] = 0;
    }
    free(input);
    tls_end(S);
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
        pthread_t th[255]; int started = 0;
        for (int t = 1; t < threads; t++) {
            if (pthread_create(&th[started], NULL, fn, J)) break;
            started++;
        }
        fn(J);
        for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
        return;
    }
    pthread_mutex_lock(&pool_mu);
    while (pool_n < threads - 1) {
        pthread_t th; pthread_attr_t at;
        if (pthread_attr_init(&at)) break;
        if (pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED)) { pthread_attr_destroy(&at); break; }
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
    if (pl->t == T_TBL) tn = tbl2_encode(s, m, (uint8_t)pl->a, pl->c + 1, pl->b, pl->fs, t, NULL);
    else if (pl->t == T_LOG) tn = log_encode_m(s, m, t, m + TSLACK, pl->a == 1);
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

#ifdef KOLMO_FRONTIER
/* A packed base costs two bits even in long gaps or repeated motifs. Check
 * several parts of the segment before bypassing the regular planner. */
static int is_simple_dna(const uint8_t *src, size_t len) {
    enum { WINDOW = 1024, MAX_WINDOWS = 12, SIXMERS = 4096 };
    size_t width = len < WINDOW ? len : WINDOW;
    size_t windows = len / width;
    if (windows > MAX_WINDOWS) windows = MAX_WINDOWS;
    size_t step = windows > 1 ? (len - width) / (windows - 1) : 0;
    unsigned lower = 0, upper = 0;
    for (size_t w = 0; w < windows; ++w) {
        const uint8_t *sample = src + w * step;
        size_t pos = 0;
        if (sample[0] == '>') {
            const uint8_t *newline = memchr(sample, '\n', width);
            if (!newline) return 0;
            pos = (size_t)(newline - sample) + 1;
        }
        uint64_t seen[SIXMERS / 64] = {0};
        unsigned kmer = 0, run = 0, known = 0, other = 0, words = 0, distinct = 0;
        for (; pos < width; ++pos) {
            uint8_t c = sample[pos];
            if (c == '\n' || c == '\r') continue;
            uint8_t base = c & 0xdf;
            if (base != 'A' && base != 'C' && base != 'T' && base != 'G') {
                ++other;
                run = 0;
                continue;
            }
            ++known;
            lower += (c >> 5) & 1;
            upper += 1 - ((c >> 5) & 1);
            kmer = ((kmer << 2) | ((c >> 1) & 3u)) & (SIXMERS - 1);
            if (++run < 6) continue;
            uint64_t bit = UINT64_C(1) << (kmer & 63);
            distinct += !(seen[kmer >> 6] & bit);
            seen[kmer >> 6] |= bit;
            ++words;
        }
        if (32 * other > known + other || (words >= 128 && distinct * 3 < words)) return 0;
    }
    /* Entirely lower-case sequence packs as efficiently as upper case. */
    unsigned minority = lower < upper ? lower : upper;
    return 64 * minority <= lower + upper;
}

/* Use the production encoder to measure the serialized block, including its
 * transform metadata and any fallback to the regular codec or raw storage. */
static size_t fast_sample_bytes(const uint8_t *src, size_t len, const plan_t *plan,
                                const kolmo_params *params, uint8_t *body) {
    /* One synchronous block writes directly to body. The ordered queue is
     * unused, and compress_worker manages its own thread-local scratch. */
    uint8_t index[IDX];
    uint64_t starts[] = {0, len}, offset = 0;
    uint32_t which = 0;
    job_t job = {
        .src = src, .input_fd = -1, .dst = body, .starts = starts, .nb = 1,
        .idx = index, .default_level = 1, .output_fd = -1, .B = len,
        .plans = plan, .bplan = &which, .rstart = &offset, .cap = len + 4,
    };
    mode_codec(params, &job.codec, &job.level);
    job.slot = slot_size((uint32_t)len, job.codec);
    if (pthread_mutex_init(&job.wmu, NULL)) return 0;
    atomic_init(&job.next, 0);
    atomic_init(&job.newmodes, 0);
    atomic_init(&job.err, 0);
    compress_worker(&job);
    pthread_mutex_destroy(&job.wmu);
    if (job.werr || atomic_load(&job.err) || job.wnext != 1) return 0;
    return job.wpos;
}

static int sampled_dna_wins(const uint8_t *src, size_t len, const plan_t *normal,
                            const kolmo_params *params) {
    /* A normal transform can depend on whole-segment record geometry. Honor
     * that decision without applying it to a slice with arbitrary boundaries. */
    if (normal->t != T_NONE || normal->experimental_table || normal->experimental_pcm ||
        normal->lags.near) return 0;
    size_t width = len / 32;
    if (width < 4096) width = len < 4096 ? len : 4096;
    if (width > (64u << 10)) width = 64u << 10;
    size_t windows = len / width;
    if (windows > 4) windows = 4;
    size_t step = windows > 1 ? (len - width) / (windows - 1) : 0;
    uint8_t *body = malloc(width + 4);
    if (!body) return 0;
    plan_t dna = {.experimental_dna = 1};
    size_t packed = 0, regular = 0;
    for (size_t w = 0; w < windows; ++w) {
        const uint8_t *s = src + w * step;
        size_t a = fast_sample_bytes(s, width, &dna, params, body);
        size_t b = fast_sample_bytes(s, width, normal, params, body);
        if (!a || !b) { free(body); return 0; }
        packed += a + IDX;
        regular += b + IDX;
    }
    free(body);
    return packed * 100 < regular * 97;
}

/* Select the wider layout against the incumbent bytes on the same complete
 * records. All buffers are bounded by the sample size, independent of input. */
static int prefer_frontier_table(const uint8_t *src, size_t n, const plan_t *pl, uint8_t delim) {
    size_t sample = n < (192u << 10) ? n : (192u << 10);
    while (sample && src[sample - 1] != '\n') --sample;
    if (sample < 8192) return 0;
    size_t cap = sample + TSLACK;
    uint8_t *tmp = malloc(cap + 64);
    size_t bound = ZSTD_compressBound(cap);
    uint8_t *z = malloc(bound);
    ZSTD_CCtx *ctx = ZSTD_createCCtx();
    int keep = 0;
    if (!tmp || !z || !ctx) goto done;
    size_t trial = frontier_table_encode(src, sample, delim, tmp, cap);
    if (!trial) goto done;
    size_t candidate = ZSTD_compressCCtx(ctx, z, bound, tmp, trial, 1);
    if (ZSTD_isError(candidate)) goto done;
    const uint8_t *previous = src;
    size_t bytes = sample;
    if (pl->t == T_TBL) {
        int64_t count = tbl2_encode(src, sample, pl->a, pl->c + 1, pl->b, pl->fs, tmp, NULL);
        if (count > 0) { previous = tmp; bytes = (size_t)count; }
    } else if (pl->t == T_TP) {
        int64_t sizes[256];
        if (pl->wide) tpq_wide_fwd(src, sample, pl->a, tmp, sizes);
        else transpose_fwd(src, sample, pl->a, pl->c + 1, pl->b, tmp, sizes);
        previous = tmp;
    } else if (pl->t != T_NONE) goto done;
    size_t reference = ZSTD_compressCCtx(ctx, z, bound, previous, bytes, pl->wide ? 1 : 3);
    if (!ZSTD_isError(reference))
        keep = 100 * (candidate + 4) < 97 * (reference + (pl->t == T_TP ? 4 * (pl->c + 1) : 4));
done:
    ZSTD_freeCCtx(ctx); free(tmp); free(z);
    return keep;
}
#endif

typedef struct {
    const uint8_t *src; const uint64_t *starts, *ends;
    plan_t *plans; const kolmo_params *params;
    uint32_t count; int threads_per_plan, allow_frontier, member_fd; atomic_uint next;
} plan_job;
static void *plan_worker(void *arg) {
    plan_job *j = arg; const kolmo_params *p = j->params;
    long page_size = j->member_fd >= 0 ? sysconf(_SC_PAGESIZE) : 0;
    for (;;) {
        uint32_t g = atomic_fetch_add(&j->next, 1); if (g >= j->count) break;
        const uint8_t *src = j->src + j->starts[g]; size_t len = j->ends[g] - j->starts[g];
        /* Keep pages touched by this archive member resident only while its
         * planner runs. Block construction retains the original file mapping. */
        void *member = MAP_FAILED;
        size_t mapped = 0;
        if (page_size > 0 && len >= (1u << 20)) {
            size_t offset = (size_t)j->starts[g], delta = offset % (size_t)page_size;
            mapped = len + delta;
            member = mmap(NULL, mapped, PROT_READ, MAP_PRIVATE, j->member_fd, (off_t)(offset - delta));
            if (member != MAP_FAILED) {
                madvise(member, mapped, MADV_RANDOM);
                src = (const uint8_t *)member + delta;
            }
        }
#ifdef KOLMO_FRONTIER
        int dna_candidate = j->allow_frontier && frontier_dna_probe(src, len);
        if (dna_candidate && is_simple_dna(src, len)) {
            j->plans[g] = (plan_t){.experimental_dna = 1};
            if (member != MAP_FAILED) munmap(member, mapped);
            continue;
        }
#endif
        /* transforms: 1 full analysis, 2 quick analysis, 3 minimal (speed first: only the byte-plane shuffle, whose
         * trial costs microseconds and whose decode is faster than the bytes it saves) */
#ifdef KOLMO_TIMING
        double mp0_ = prof_now();
#endif
        plan_t pl = p->transforms ? make_plan(src, len, p->mode >= KOLMO_ECO,
            p->mode != KOLMO_RT && p->transforms != 3, p->mode != KOLMO_RT && p->transforms != 3, j->threads_per_plan, p->mode >= KOLMO_MAX,
            p->transforms == 2 ? 1 : p->transforms == 3 ? (p->mode == KOLMO_RT ? 3 : 2) : p->mode == KOLMO_FAST ? 4 : 0) : (plan_t){0};
#ifdef KOLMO_TIMING
        fprintf(stderr, "mp n=%zu ms=%.3f t=%d\n", len, (prof_now() - mp0_) * 1000, pl.t);
#endif
#ifdef KOLMO_FRONTIER
        uint8_t delim = 0;
        if (j->allow_frontier && frontier_table_probe(src, len, &delim) && prefer_frontier_table(src, len, &pl, delim))
            pl.experimental_table = delim;
#if defined(KOLMO_FRONTIER_PATCHED_PCM) || defined(KOLMO_FRONTIER_FAST_PCM)
        else if (j->allow_frontier && pl.t == T_SHUF && pl.a == 2)
            pl.experimental_pcm = frontier_pcm_probe(src, len);
#endif
#ifdef KOLMO_FRONTIER_LAG
        else if (j->allow_frontier &&
                 (pl.t == T_IMG || pl.t == T_REC || pl.t == T_NONE) && !looks_text(src,len)) {
            frontier_lags trial = {0};
            if (frontier_lag_probe(src, len, &trial)) pl.lags = trial;
        }
#endif
        if (dna_candidate && sampled_dna_wins(src, len, &pl, p)) {
            free(pl.fs);
            free(pl.rec);
            pl = (plan_t){.experimental_dna = 1};
        }
#endif
        if (p->mode == KOLMO_RT && (pl.t == T_TP || pl.t == T_LOG)) pl.t = T_NONE;
        if (p->mode == KOLMO_ULTRA && !ultra_keep_transform(src, len, &pl)) {
            free(pl.fs); pl.fs = NULL; pl.t = T_NONE;
        }
        j->plans[g] = pl;
        if (member != MAP_FAILED) munmap(member, mapped);
    }
    zs_trim();
    return NULL;
}

static int64_t compress_impl(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *p,
                             const void *dict, size_t dictlen, uint32_t dictid) {
    kolmo_params d = {KOLMO_FAST, 0, 0, 0, 1}; if (!p) p = &d;
    if (cap < HDR + IDX) return KOLMO_ERR_DSTSIZE;
    if (n > INT64_MAX) return KOLMO_ERR_CORRUPT;
    int threads = p->threads > 0 ? p->threads : default_threads();
    uint32_t B = p->block ? p->block : (p->mode >= KOLMO_MAX ? (128u << 20) : p->mode >= KOLMO_ECO ? (16u << 20) : kolmo_fast_block(n));   /* slow modes: larger blocks; speed modes: ~8 balanced blocks */
    if (B < 4096) B = 4096;
    int codec, level; mode_codec(p, &codec, &level);
    enum { MAXSEG = 4096 };
    int64_t ret = KOLMO_ERR_ALLOC; size_t w = 0;
    uint8_t *file_index = NULL;
    job_t J = {0}; int mutex_ready = 0;
    uint32_t nseg = 0, nb = 0; size_t bcap = 2;
    plan_t *plans = NULL;
    uint64_t *starts = NULL, *rstart = NULL; uint32_t *bplan = NULL;
    uint64_t *ss = array_alloc(MAXSEG, sizeof(uint64_t), 0), *se = array_alloc(MAXSEG, sizeof(uint64_t), 0);
    void *planning_map = MAP_FAILED;
    if (!ss || !se) goto cleanup;
    if (input_fd >= 0 && n) {
        planning_map = mmap(NULL, n, PROT_READ, MAP_PRIVATE, input_fd, 0);
        if (planning_map == MAP_FAILED) { ret = KOLMO_ERR_IO; goto cleanup; }
        /* Formula discovery samples dispersed rows before the block pass. */
        madvise(planning_map, n, MADV_RANDOM);
        src = planning_map;
    }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    double pt0 = prof_now();
#endif
    nseg = segments(src, n, ss, se, MAXSEG);
    plans = array_alloc(nseg, sizeof(plan_t), 1);
    starts = array_alloc(bcap, sizeof(uint64_t), 0);
    bplan = array_alloc(bcap, sizeof(uint32_t), 0);
    rstart = array_alloc(bcap, sizeof(uint64_t), 0);
    if (!plans || !starts || !bplan || !rstart) goto cleanup;
    /* Parallelize independent members, or formula discovery within a single
     * member. Never multiply the two thread budgets. Plans remain in input order. */
    plan_job planning = {src, ss, se, plans, p, nseg, nseg > 1 ? 1 : threads,
                         p->transforms && p->mode == KOLMO_FAST && !p->level && !dict,
                         nseg > 1 && p->transforms && p->mode == KOLMO_FAST ? input_fd : -1};
    atomic_init(&planning.next, 0);
    run_pool(plan_worker, &planning, p->transforms ? threads : 1, nseg);
    if (!p->block && nseg == 1 && (plans[0].wide || plans[0].experimental_table) && B > (2u << 20)) B = 2u << 20;
    for (uint32_t g = 0; g < nseg; g++) {
        const uint8_t *s0 = src + ss[g]; size_t len = (size_t)(se[g] - ss[g]);
        plan_t pl = plans[g];
        uint32_t Bs = B;
        if (!p->block && p->mode == KOLMO_HC && pl.t == T_WORD) Bs = 128u << 20;   /* hc text: one large window */
        uint64_t *st; uint32_t m = make_blocks(s0, len, Bs, pl, &st, input_fd, ss[g], &ret);
        if (!st) goto cleanup;
        if (len == 0) m = 0;
        if (m > UINT32_MAX - nb - 1) { free(st); goto cleanup; }
        size_t needed = (size_t)nb + m + 1;
        if (needed > bcap) {
            size_t grown_cap = bcap;
            while (grown_cap < needed) {
                if (grown_cap > SIZE_MAX / 2) { free(st); goto cleanup; }
                grown_cap *= 2;
            }
            uint64_t *grown_starts = array_resize(starts, grown_cap, sizeof(uint64_t));
            if (!grown_starts) { free(st); goto cleanup; } starts = grown_starts;
            uint32_t *grown_bplan = array_resize(bplan, grown_cap, sizeof(uint32_t));
            if (!grown_bplan) { free(st); goto cleanup; } bplan = grown_bplan;
            uint64_t *grown_rstart = array_resize(rstart, grown_cap, sizeof(uint64_t));
            if (!grown_rstart) { free(st); goto cleanup; } rstart = grown_rstart;
            bcap = grown_cap;
        }
        for (uint32_t i = 0; i < m; i++) { starts[nb] = ss[g] + st[i]; rstart[nb] = st[i]; bplan[nb] = g; nb++; }
        free(st);
    }
    if (nb == 0) { starts[0] = 0; rstart[0] = 0; bplan[0] = 0; nb = 1; }
    starts[nb] = n;
    size_t maxb = 0; for (uint32_t i = 0; i < nb; i++) if (starts[i + 1] - starts[i] > maxb) maxb = starts[i + 1] - starts[i];
    if (planning_map != MAP_FAILED) {
        munmap(planning_map, n); planning_map = MAP_FAILED; src = NULL;
    }
    J.input_fd = input_fd; J.src = src; J.starts = starts; J.nb = nb; J.slot = slot_size((uint32_t)maxb, codec);
    if (output_fd >= 0) {
        if ((uint64_t)HDR + (uint64_t)nb * IDX > SIZE_MAX) goto cleanup;
        file_index = malloc(HDR + (size_t)nb * IDX);
        if (!file_index) goto cleanup;
        dst = file_index;
    }
    J.output_fd = output_fd; J.default_level = !p->level;
    J.idx = dst + HDR; J.codec = codec; J.level = level; J.B = maxb; J.plans = plans; J.bplan = bplan; J.rstart = rstart;
    J.out = array_alloc(nb, sizeof(uint8_t *), 1); J.outlen = array_alloc(nb, sizeof(size_t), 1);
    if (!J.out || !J.outlen) goto cleanup;
    J.psum = p->mode == KOLMO_RT;
    J.dict = dict; J.dictlen = dictlen;
    J.dst = dst; J.cap = cap; J.wnext = 0; J.wpos = HDR + (size_t)nb * IDX; J.werr = 0;
    if (pthread_mutex_init(&J.wmu, NULL)) goto cleanup;
    mutex_ready = 1;
    atomic_init(&J.next, 0); atomic_init(&J.newmodes, 0); atomic_init(&J.err, 0);
    ret = 0;
    if ((size_t)nb > (cap - HDR) / IDX) ret = KOLMO_ERR_DSTSIZE;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    double pt1 = prof_now();
#endif
    if (!ret) run_pool(compress_worker, &J, threads, nb);
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    fprintf(stderr, "compress bytes=%zu threads=%d plan_ms=%.3f blocks_ms=%.3f\n", n, threads, (pt1 - pt0) * 1000, (prof_now() - pt1) * 1000);
#endif
    if (!ret) ret = atomic_load(&J.err);
    if (!ret) ret = J.werr;
    if (!ret && J.wnext != nb) ret = KOLMO_ERR_ALLOC;
    w = J.wpos;
cleanup:
    if (planning_map != MAP_FAILED) munmap(planning_map, n);
    if (J.out) for (uint32_t i = 0; i < nb; i++) free(J.out[i]);
    free(J.outlen); if (mutex_ready) pthread_mutex_destroy(&J.wmu);
    if (plans) for (uint32_t g = 0; g < nseg; g++) { free(plans[g].fs); free(plans[g].rec); }
    free(J.out); free(starts); free(bplan); free(rstart); free(plans); free(ss); free(se);
    zs_trim();
    if (ret) { free(file_index); zs_trim(); return ret; }
    { int nm = atomic_load_explicit(&J.newmodes, memory_order_relaxed); memcpy(dst, "CZN1", 4); dst[4] = (nm & 16) ? 8 : (nm & 8) ? 7 : (nm & 4) ? 6 : (nm & 2) ? 5 : (nm & 1) ? 4 : 3; } dst[5] = (uint8_t)p->mode; dst[6] = dst[7] = dst[8] = dst[9] = 0; dst[10] = (uint8_t)J.psum; dst[11] = 0;
    if (dict) { dst[10] |= 2; put32(dst + 6, dictid); }   /* flag bit1 = zstd dictionary; dst[6..9] = dict id for validation */
    put64(dst + 12, n); put32(dst + 20, nb); put32(dst + 24, B);
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, (size_t)nb * IDX); put32(dst + 28, hc);
    if (output_fd >= 0) ret = write_block(output_fd, file_index, HDR + (size_t)nb * IDX, 0);
    if (file_index) { free(file_index); zs_trim(); }
    return ret ? ret : (int64_t)w;
}

int64_t kolmo_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p) {
    return compress_impl(src, -1, n, dst, -1, cap, p, NULL, 0, 0);
}

/* ------------------------------------------------------------------ decoding */
typedef struct { uint64_t orig; uint32_t nb; int ver, psum, needdict; uint32_t dictid; plan_t pl; const uint8_t *idx; uint64_t *poff; uint64_t *roff; } parsed_t;

static int parse(const uint8_t *s, size_t n, parsed_t *P, const kolmo_decode_limits *limits) {
    if (n < HDR || memcmp(s, "CZN1", 4) || s[4] < 1 || s[4] > KOLMO_MAX_VERSION) return KOLMO_ERR_CORRUPT;
    P->ver = s[4]; P->psum = P->ver >= 3 && (s[10] & 1);
    P->needdict = P->ver >= 3 && (s[10] & 2); P->dictid = P->needdict ? get32(s + 6) : 0;
    P->orig = get64(s + 12); P->nb = get32(s + 20);
    if (limits && ((limits->max_output_bytes && P->orig > limits->max_output_bytes) ||
                   (limits->max_blocks && P->nb > limits->max_blocks))) return KOLMO_ERR_LIMIT;
    P->pl = (plan_t){s[6], s[7], s[8], s[9]};
    if ((uint64_t)P->nb * IDX + HDR > n) return KOLMO_ERR_CORRUPT;
    uint32_t hc = kolmo_crc32c(0, s, 28); hc = kolmo_crc32c(hc, s + HDR, (size_t)P->nb * IDX);
    if (hc != get32(s + 28)) return KOLMO_ERR_CHECKSUM;
    P->idx = s + HDR;
    P->poff = array_alloc((size_t)P->nb + 1, sizeof(uint64_t), 0);
    P->roff = array_alloc((size_t)P->nb + 1, sizeof(uint64_t), 0);
    if (!P->poff || !P->roff) { free(P->poff); free(P->roff); return KOLMO_ERR_ALLOC; }
    uint64_t po = HDR + (uint64_t)P->nb * IDX, ro = 0;
    for (uint32_t i = 0; i < P->nb; i++) {
        P->poff[i] = po; P->roff[i] = ro;
        uint32_t cl = get32(P->idx + (size_t)i * IDX + 4), raw = get32(P->idx + (size_t)i * IDX);
        if (limits && limits->max_block_bytes && raw > limits->max_block_bytes) {
            free(P->poff); free(P->roff); return KOLMO_ERR_LIMIT;
        }
        if (po > n || cl > n - po || ro > P->orig || raw > P->orig - ro) {
            free(P->poff); free(P->roff); return KOLMO_ERR_CORRUPT;
        }
        po += cl; ro += raw;
    }
    P->poff[P->nb] = po; P->roff[P->nb] = ro;
    if (po > n || ro != P->orig) { free(P->poff); free(P->roff); return KOLMO_ERR_CORRUPT; }
    return 0;
}

typedef struct { const uint8_t *src; parsed_t *P; uint8_t *dst; uint32_t lo, hi; atomic_uint next; atomic_int err;
                 int input_fd, output_fd; size_t maxraw, maxcompressed; uint64_t clip_lo, clip_hi; const void *dict; size_t dictlen; } djob_t;

static int decode_block(const djob_t *J, uint32_t i, codec_ctx *x, tls_t *S, const uint8_t *pay, uint8_t *tbuf, uint8_t *out) {
    const parsed_t *P = J->P; const uint8_t *e = P->idx + (size_t)i * IDX;
    size_t len = get32(e), cl = get32(e + 4); int codec = e[12], tflag = e[13];
    size_t meta = 0;
    plan_t pl = P->pl;
    if (P->psum) {                                             /* rt: the checksum covers the stored bytes; checked first */
        if (cl < 4 || (uint32_t)kolmo_xxh3(pay + 4, cl - 4, 0x637a6e31u ^ get32(pay)) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
    }
    if (P->ver >= 2) {                                         /* per-block plan */
        if (cl < 4) return KOLMO_ERR_CORRUPT;
        pl.t = pay[0]; pl.a = pay[1]; pl.b = pay[2]; pl.c = pay[3]; pay += 4; cl -= 4;
        if (pl.t > T_LINES || (pl.t == T_KG && P->ver < 7) || (pl.t == T_LINES && P->ver < 8)) return KOLMO_ERR_CORRUPT;
    }
    if (codec == C_GRAPH || pl.t == T_KG) {                     /* kolmo graph block (version 7) */
        if (codec != C_GRAPH || pl.t != T_KG || tflag || P->ver < 7) return KOLMO_ERR_CORRUPT;
        int r = kg_decode(x, pay, cl, out, len); if (r) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
#ifdef KOLMO_FRONTIER
    if (pl.t == T_TBL && (tflag == 10 || tflag == 11)) {   /* experimental wide table / lag (the log plan's flag 10 is minified JSON) */
        if (P->ver < 6 || P->psum || pl.t != T_TBL || codec != C_ZSTD || cl < 4) return KOLMO_ERR_CORRUPT;
        size_t bytes = get32(pay);
        if (bytes > len + TSLACK || codec_decompress(x, codec, pay + 4, cl - 4, tbuf, bytes)) return KOLMO_ERR_CORRUPT;
        int error = tflag == 10 ? frontier_table_decode(tbuf, bytes, (uint8_t)pl.a, out, len)
                                : frontier_lag_decode(tbuf, bytes, out, len);
        if (error) return KOLMO_ERR_CORRUPT;
        return blk_sum(P->ver, out, len) == get32(e + 8) ? 0 : KOLMO_ERR_CHECKSUM;
    }
#endif
    if (tflag && pl.t == T_TP) meta = 4 * (size_t)(pl.c + 1);
    if ((tflag >= 3 && tflag <= 7) || tflag == 9) meta = 4;
    if (tflag == 10) meta = 8;
    if (tflag == 12) meta = 4;
    int maxf = pl.t == T_TP ? 2 : pl.t == T_TBL ? 4 : pl.t == T_LOG ? (P->ver >= 6 ? 10 : 5) : pl.t == T_IMG ? 6 : pl.t == T_REC ? 7 : pl.t == T_X86 ? 8 : pl.t == T_WORD ? 9 : pl.t == T_LINES ? 12 : 1;
    if (tflag > maxf || (pl.t == T_LINES && tflag && tflag != 12) || (pl.t == T_TBL && tflag && tflag < 3) || (pl.t == T_LOG && tflag && tflag != 5 && tflag != 10)
        || (pl.t == T_IMG && tflag && tflag != 6) || (pl.t == T_REC && tflag && tflag != 7) || (pl.t == T_X86 && tflag && tflag != 8) || (pl.t == T_WORD && tflag && tflag != 9)
        || (pl.t == T_NONE && tflag)) return KOLMO_ERR_CORRUPT;
    if (meta > cl) return KOLMO_ERR_CORRUPT;
    uint8_t *tgt = (tflag && tflag != 8) ? tbuf : out;
    size_t dlen = len;
    if ((tflag >= 3 && tflag <= 7) || tflag == 9 || tflag == 10 || tflag == 12) { dlen = get32(pay); if (dlen > len + TSLACK) return KOLMO_ERR_CORRUPT; }
#ifdef KOLMO_FRONTIER
    if (codec == C_PCM) {
        if (P->ver < 6 || tflag || pl.t != T_NONE || frontier_pcm_decode(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
    } else if (codec == C_DNA || codec == C_DNA_PREFIX || codec == C_DNA_RUNS) {
        if (P->ver < 6 || tflag || pl.t != T_NONE) return KOLMO_ERR_CORRUPT;
        if (codec != C_DNA) {
            if (cl < 4) return KOLMO_ERR_CORRUPT;
            size_t bytes = get32(pay);
            if (bytes > len) return KOLMO_ERR_CORRUPT;
            int error = codec == C_DNA_PREFIX ? frontier_prefix_decode(pay, cl, tbuf, bytes)
                                              : frontier_runs_decode(pay, cl, tbuf, bytes);
            if (error) return KOLMO_ERR_CORRUPT;
            pay = tbuf; cl = bytes;
        }
        if (frontier_dna_decode(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
    } else
#endif
    if (codec_decompress(x, codec, pay + meta, cl - meta, tgt, dlen)) return KOLMO_ERR_CORRUPT;
    if (tflag == 3) { if (tbl_decode(tbuf, dlen, (uint8_t)pl.a, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 4) { if (tbl2_decode(tbuf, dlen, (uint8_t)pl.a, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 5) { if (log_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 10) {                                    /* minified JSON: decode the split text, drop the newlines */
        size_t k = get32(pay + 4); if (k > len) return KOLMO_ERR_CORRUPT;
        uint8_t *sp = malloc(len + k + 64); if (!sp) return KOLMO_ERR_ALLOC;
        int bad = log_decode(tbuf, dlen, sp, len + k); size_t o = 0;
        for (size_t q = 0, e = len + k; !bad && q < e;) {     /* copy the runs between newlines */
            const uint8_t *nl = memchr(sp + q, '\n', e - q); size_t r = (nl ? (size_t)(nl - sp) : e) - q;
            if (r > len - o) { bad = 1; break; }
            memcpy(out + o, sp + q, r); o += r; q += r + (nl != NULL);
        }
        free(sp);
        if (bad || o != len) return KOLMO_ERR_CORRUPT;
    }
    else if (tflag == 12) { if (lines_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 6) { if (img16_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 7) { if (rec_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 8) e8e9_inv(out, len);
    else if (tflag == 9) { if (word_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag && pl.t == T_TP) {
        int k = pl.c + 1; int64_t colsz[256];
        if (cl < 4 * (size_t)k) return KOLMO_ERR_CORRUPT;
        for (int j = 0; j < k; j++) colsz[j] = get32(pay + 4 * j);
        if (tflag == 2) {
            int uniform = 1;
            if (k == 256) uniform = tpq_inv_cols(tbuf, len, (uint8_t)pl.a, k, colsz, out, NULL, NULL);
            if (uniform > 0) {
                uint32_t *pos = S->pos = grow(S->pos, &S->pcap, (J->maxraw + 1) * sizeof(uint32_t));
                if (!pos) return KOLMO_ERR_ALLOC;
                if (k != 256) {
                    uint32_t *rows = S->rowbuf = grow(S->rowbuf, &S->rcap, (J->maxraw + 8) * sizeof(uint32_t));
                    if (!rows) return KOLMO_ERR_ALLOC;
                    uniform = tpq_inv_cols(tbuf, len, (uint8_t)pl.a, k, colsz, out, pos, rows);
                }
                if (uniform > 0 && tpq_inv(tbuf, len, (uint8_t)pl.a, k, colsz, out, pos)) return KOLMO_ERR_CORRUPT;
            }
            if (uniform < 0) return KOLMO_ERR_CORRUPT;
        }
        else if (transpose_inv(tbuf, len, (uint8_t)pl.a, k, pl.b, colsz, out)) return KOLMO_ERR_CORRUPT;
    } else if (tflag && pl.t == T_SHUF) {
        size_t u = (size_t)pl.a * pl.b, rows = u ? len / u : 0;
        if (!u || u > 64 * 8 || (pl.a != 2 && pl.a != 4 && pl.a != 8) || pl.b < 1 || pl.b > 8) return KOLMO_ERR_CORRUPT;
        shuffle_inv(tbuf, rows, pl.a, pl.b, pl.c, out);
        memcpy(out + rows * u, tbuf + rows * u, len - rows * u);
    } else if (tflag) return KOLMO_ERR_CORRUPT;
    if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
    return 0;
}

static void *decompress_worker(void *arg) {
    djob_t *J = arg; tls_t *S = tls_begin();
    if (!S) { atomic_store(&J->err, KOLMO_ERR_ALLOC); return NULL; }
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->maxraw + TSLACK + 64);
    int buffered = J->clip_hi || J->output_fd >= 0;
    uint8_t *rbuf = buffered ? (S->rbuf = grow(S->rbuf, &S->rbcap, J->maxraw + 64)) : NULL;
    uint8_t *packed = J->input_fd >= 0 ? malloc(J->maxcompressed ? J->maxcompressed : 1) : NULL;
    if ((J->input_fd >= 0 && !packed) || !tbuf || (buffered && !rbuf)) { free(packed); atomic_store(&J->err, KOLMO_ERR_ALLOC); tls_end(S); return NULL; }
    codec_ctx *xp = &S->x;
    xp->dict = J->dict; xp->dictlen = J->dictlen;                          /* per-call dictionary (read-only) */
    for (;;) {
        uint32_t i = J->lo + atomic_fetch_add(&J->next, 1); if (i >= J->hi) break;
        if (atomic_load(&J->err)) break;
        int r;
        const uint8_t *payload;
        if (packed) {
            size_t bytes = J->P->poff[i + 1] - J->P->poff[i];
            if (read_block(J->input_fd, packed, bytes, J->P->poff[i])) { atomic_store(&J->err, KOLMO_ERR_IO); break; }
            payload = packed;
        } else payload = J->src + J->P->poff[i];
        if (!rbuf) r = decode_block(J, i, xp, S, payload, tbuf, J->dst + J->P->roff[i]);
        else {                                               /* range read: decode block, copy the overlap */
            r = decode_block(J, i, xp, S, payload, tbuf, rbuf);
            if (!r && J->output_fd >= 0) {
                r = write_block(J->output_fd, rbuf, J->P->roff[i + 1] - J->P->roff[i], J->P->roff[i]);
            } else if (!r) {
                uint64_t b0 = J->P->roff[i], b1 = J->P->roff[i + 1];
                uint64_t a = J->clip_lo > b0 ? J->clip_lo : b0, z = J->clip_hi < b1 ? J->clip_hi : b1;
                memcpy(J->dst + (a - J->clip_lo), rbuf + (a - b0), z - a);
            }
        }
        if (r) atomic_store(&J->err, r);
    }
    free(packed);
    tls_end(S);
    return NULL;
}

int64_t kolmo_original_size(const uint8_t *src, size_t n) { return n >= HDR && !memcmp(src, "CZN1", 4) ? (int64_t)get64(src + 12) : KOLMO_ERR_CORRUPT; }

static int64_t run_decode(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, int threads, uint64_t lo, uint64_t hi, int range,
                          const void *dict, size_t dictlen, uint32_t dictid, const kolmo_decode_limits *limits) {
    parsed_t P; int r = parse(src, n, &P, limits); if (r) return r;
    if (P.needdict && (!dict || dictid != P.dictid)) { free(P.poff); free(P.roff); return KOLMO_ERR_DICT; }
    djob_t J = {0}; J.src = src; J.P = &P; J.input_fd = input_fd; J.dst = dst; J.output_fd = output_fd; J.dict = dict; J.dictlen = dictlen;
    for (uint32_t i = 0; i < P.nb; i++) {
        size_t raw = P.roff[i + 1] - P.roff[i], stored = P.poff[i + 1] - P.poff[i];
        if (raw > J.maxraw) J.maxraw = raw;
        if (stored > J.maxcompressed) J.maxcompressed = stored;
    }
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

int64_t kolmo_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads) {
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, NULL, 0, 0, NULL);
}

int64_t kolmo_decompress_limited(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                               int threads, const kolmo_decode_limits *limits) {
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if (limits && limits->max_output_bytes && (uint64_t)o > limits->max_output_bytes) return KOLMO_ERR_LIMIT;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, NULL, 0, 0, limits);
}

int64_t kolmo_read_range(const uint8_t *src, size_t n, uint64_t off, size_t len, uint8_t *dst, int threads) {
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if (off > (uint64_t)o) return KOLMO_ERR_CORRUPT;
    if (len > (uint64_t)o - off) len = (size_t)((uint64_t)o - off);
    if (!len) return 0;
    return run_decode(src, -1, n, dst, -1, threads, off, off + len, 1, NULL, 0, 0, NULL);
}

const char *kolmo_strerror(int64_t c) {
    switch (c) {
        case KOLMO_ERR_CORRUPT: return "corrupt or unsupported input";
        case KOLMO_ERR_CHECKSUM: return "checksum mismatch (data corrupted)";
        case KOLMO_ERR_DSTSIZE: return "destination buffer too small";
        case KOLMO_ERR_ALLOC: return "out of memory";
        case KOLMO_ERR_CODEC: return "codec error";
        case KOLMO_ERR_DICT: return "dictionary required or mismatched (compressed with a different dictionary)";
        case KOLMO_ERR_IO: return "file I/O error";
        case KOLMO_ERR_LIMIT: return "decoding resource limit exceeded";
    }
    return c >= 0 ? "ok" : "unknown error";
}

/* ------------------------------------------------------------------ dictionary */

int64_t kolmo_train_dict(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                       uint8_t *dict, size_t dict_cap) {
    if (!samples || !sizes || !nsamples || !dict || dict_cap < 256) return KOLMO_ERR_CORRUPT;
    size_t r = ZDICT_trainFromBuffer(dict, dict_cap, samples, sizes, (unsigned)nsamples);
    if (ZDICT_isError(r)) return KOLMO_ERR_CODEC;
    return (int64_t)r;
}

int64_t kolmo_compress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                          const kolmo_params *p, const void *dict, size_t dictlen) {
    if (!dict || !dictlen) return kolmo_compress(src, n, dst, cap, p);
    uint32_t id = (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u);   /* dictionary id seed, fixed by the format */
    return compress_impl(src, -1, n, dst, -1, cap, p, dict, dictlen, id);
}

int64_t kolmo_decompress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                            const void *dict, size_t dictlen) {
    if (!dict || !dictlen) return kolmo_decompress(src, n, dst, cap, threads);
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    uint32_t id = (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u);
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, dict, dictlen, id, NULL);
}

int64_t kolmo_compress_from_fd(int fd, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                             const void *dict, size_t dictlen) {
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (!dictlen) dict = NULL;
    uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
    return compress_impl((const uint8_t *)"", fd, n, dst, -1, cap, p, dict, dictlen, id);
}

int64_t kolmo_decompress_to_fd(const uint8_t *src, size_t n, int fd, int threads,
                             const void *dict, size_t dictlen) {
    if (fd < 0) return KOLMO_ERR_IO;
    int64_t size = kolmo_original_size(src, n);
    if (size < 0) return size;
    if (!dictlen) dict = NULL;
    uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
    return run_decode(src, -1, n, NULL, fd, threads, 0, 0, 0, dict, dictlen, id, NULL);
}

int64_t kolmo_compress_file(int input_fd, int output_fd, size_t n, const kolmo_params *p,
                          const void *dict, size_t dictlen) {
    struct stat st;
    if (output_fd < 0 || fstat(input_fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (!dictlen) dict = NULL;
    uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
    return compress_impl((const uint8_t *)"", input_fd, n, NULL, output_fd, INT64_MAX, p, dict, dictlen, id);
}

int64_t kolmo_decompress_file(int input_fd, int output_fd, size_t n, int threads,
                            const void *dict, size_t dictlen) {
    uint8_t header[HDR];
    if (n < HDR) return KOLMO_ERR_CORRUPT;
    struct stat st;
    if (output_fd < 0 || fstat(input_fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (read_block(input_fd, header, HDR, 0)) return KOLMO_ERR_IO;
    int64_t size = kolmo_original_size(header, HDR);
    uint32_t nb = get32(header + 20);
    if (size < 0 || header[4] < 1 || header[4] > KOLMO_MAX_VERSION || nb > (n - HDR) / IDX) return KOLMO_ERR_CORRUPT;
    size_t index_size = HDR + (size_t)nb * IDX;
    uint8_t *index = malloc(index_size);
    if (!index) return KOLMO_ERR_ALLOC;
    memcpy(index, header, HDR);
    int64_t ret = read_block(input_fd, index + HDR, index_size - HDR, HDR);
    if (!ret) {
        if (!dictlen) dict = NULL;
        uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
        ret = run_decode(index, input_fd, n, NULL, output_fd, threads, 0, 0, 0, dict, dictlen, id, NULL);
    }
    free(index);
    zs_trim();
    return ret;
}

#include "stream.inc"
