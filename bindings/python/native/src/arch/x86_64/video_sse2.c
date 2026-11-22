/* Video strips (kvid): SSE2 kernels. Apache-2.0, Kolmo contributors.
 * Encode: the three predictors' residuals and their sums of |residual| in one pass over 16 samples at a time, then
 * the residuals of the chosen one (row 0 and column 0 by the reference's code). Predictor 2 takes the median of signed
 * frame differences: biased by 0x80 they compare as unsigned bytes, so it is the unsigned median of the biased values.
 * Decode: predictor 1 is a vector add; predictors 0 and 2 are the median inverse, run 16 rows at a time by the
 * wavefront kernel (med8_rows), predictor 2 on the biased differences. Same bytes as src/algo/video.c. */
#ifndef KOLMO_NO_ASM
#include "arch/x86_64/kernels_x86.h"
#include "algo/video.h"
#include <stdlib.h>
#include <string.h>

static inline __m128i blend(__m128i a, __m128i b, __m128i m) { return _mm_or_si128(_mm_and_si128(m, b), _mm_andnot_si128(m, a)); }
static inline __m128i med(__m128i a, __m128i b, __m128i c) {           /* as med8_pred in med8_tmpl.h */
    __m128i mn = _mm_min_epu8(a, b), mx = _mm_max_epu8(a, b), g = _mm_sub_epi8(_mm_add_epi8(a, b), c);
    __m128i lo = _mm_cmpeq_epi8(_mm_max_epu8(c, mx), c), hi = _mm_cmpeq_epi8(_mm_min_epu8(c, mn), c);
    return blend(blend(g, mx, hi), mn, lo);
}
static inline __m128i absb(__m128i r) { __m128i s = _mm_cmpgt_epi8(_mm_setzero_si128(), r); return _mm_sub_epi8(_mm_xor_si128(r, s), s); }
static inline __m128i zzb(__m128i r) { return _mm_xor_si128(_mm_add_epi8(r, r), _mm_cmpgt_epi8(_mm_setzero_si128(), r)); }
static inline __m128i ld(const uint8_t *p) { return _mm_loadu_si128((const __m128i *)p); }

static inline int med_s(int a, int b, int c) { int mx = a > b ? a : b, mn = a < b ? a : b; return c >= mx ? mn : c <= mn ? mx : a + b - c; }
static inline int pred_s(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t x, size_t y) {   /* the reference's predict() */
    size_t i = y * w + x;
    if (k == 1) return prev[i];
    int a, b, c;
    if (k == 0) { a = x ? cur[i - 1] : (y ? cur[i - w] : 0); b = y ? cur[i - w] : a; c = (x && y) ? cur[i - w - 1] : b; return med_s(a, b, c); }
#define D(j) ((int)(int8_t)(uint8_t)(cur[j] - prev[j]))
    a = x ? D(i - 1) : (y ? D(i - w) : 0); b = y ? D(i - w) : a; c = (x && y) ? D(i - w - 1) : b;
#undef D
    return prev[i] + med_s(a, b, c);
}
static inline uint8_t zz_s(int r) { int8_t s = (int8_t)(uint8_t)r; return (uint8_t)(((uint8_t)s << 1) ^ (uint8_t)(s >> 7)); }
static inline int unzz_s(uint8_t z) { return (int)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }

/* residuals of the three predictors for samples x .. x + 15 of row y >= 1, x >= 1 */
static inline void res3(const uint8_t *cur, const uint8_t *prev, size_t w, size_t i, __m128i r[3]) {
    const __m128i bias = _mm_set1_epi8((char)0x80);
    __m128i c = ld(cur + i), L = ld(cur + i - 1), U = ld(cur + i - w), UL = ld(cur + i - w - 1);
    r[0] = _mm_sub_epi8(c, med(L, U, UL));
    if (!prev) return;
    __m128i p = ld(prev + i);
    r[1] = _mm_sub_epi8(c, p);
    __m128i dL = _mm_xor_si128(_mm_sub_epi8(L, ld(prev + i - 1)), bias), dU = _mm_xor_si128(_mm_sub_epi8(U, ld(prev + i - w)), bias);
    __m128i dUL = _mm_xor_si128(_mm_sub_epi8(UL, ld(prev + i - w - 1)), bias);
    r[2] = _mm_sub_epi8(c, _mm_add_epi8(p, _mm_xor_si128(med(dL, dU, dUL), bias)));
}

int kolmo_vid_strip_enc_sse2(const uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, uint8_t *res) {
    if (w < 17) return kolmo_vid_strip_enc_c(cur, prev, w, y0, y1, res);
    const int nk = prev ? 3 : 1; uint64_t cost[3] = {0, 0, 0};
    const __m128i z = _mm_setzero_si128();
    for (size_t y = y0; y < y1; y++) {
        size_t x = 0;
        if (y) {
            __m128i acc[3] = {z, z, z};
            for (x = 1; x + 16 <= w; x += 16) {
                __m128i r[3]; res3(cur, prev, w, y * w + x, r);
                for (int k = 0; k < nk; k++) acc[k] = _mm_add_epi64(acc[k], _mm_sad_epu8(absb(r[k]), z));
            }
            for (int k = 0; k < nk; k++) cost[k] += (uint64_t)_mm_cvtsi128_si64(acc[k]) + (uint64_t)_mm_cvtsi128_si64(_mm_unpackhi_epi64(acc[k], acc[k]));
            for (int k = 0; k < nk; k++) { int8_t r = (int8_t)(uint8_t)(cur[y * w] - pred_s(k, cur, prev, w, 0, y)); cost[k] += (uint64_t)(r < 0 ? -r : r); }
        }
        for (; x < w; x++) for (int k = 0; k < nk; k++) {
            if (y && x == 0) continue;
            int8_t r = (int8_t)(uint8_t)(cur[y * w + x] - pred_s(k, cur, prev, w, x, y)); cost[k] += (uint64_t)(r < 0 ? -r : r);
        }
    }
    int best = 0; for (int k = 1; k < nk; k++) if (cost[k] < cost[best]) best = k;
    for (size_t y = y0; y < y1; y++) {
        size_t x = 0;
        if (y) {
            res[y * w] = zz_s(cur[y * w] - pred_s(best, cur, prev, w, 0, y));
            for (x = 1; x + 16 <= w; x += 16) {
                size_t i = y * w + x; __m128i r[3];
                if (best == 1) r[1] = _mm_sub_epi8(ld(cur + i), ld(prev + i)); else res3(cur, best ? prev : NULL, w, i, r);
                _mm_storeu_si128((__m128i *)(res + i), zzb(r[best]));
            }
        }
        for (; x < w; x++) res[y * w + x] = zz_s(cur[y * w + x] - pred_s(best, cur, prev, w, x, y));
    }
    return best;
}

typedef void (*rows_fn)(const uint8_t *r, size_t W, unsigned C, uint8_t *p, size_t nrows);
static inline __attribute__((always_inline)) void dec_body(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res, rows_fn med8_rows) {
    if (k == 1) {
        size_t i = y0 * w, e = y1 * w;
        const __m128i one = _mm_set1_epi8(1), m7 = _mm_set1_epi8(0x7f);
        for (; i + 16 <= e; i += 16) {
            __m128i zv = ld(res + i), u = _mm_xor_si128(_mm_and_si128(_mm_srli_epi16(zv, 1), m7), _mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128(zv, one)));
            _mm_storeu_si128((__m128i *)(cur + i), _mm_add_epi8(ld(prev + i), u));
        }
        for (; i < e; i++) cur[i] = (uint8_t)(prev[i] + unzz_s(res[i]));
        return;
    }
    if (y0 == y1) return;
    size_t y = y0;
    if (y == 0) { for (size_t x = 0; x < w; x++) cur[x] = (uint8_t)(pred_s(k, cur, prev, w, x, 0) + unzz_s(res[x])); y = 1; }
    if (y >= y1) return;
    if (k == 0) { med8_rows(res + y * w, w, 1, cur + y * w, y1 - y); return; }
    /* k == 2: the median inverse on e = (cur - prev) ^ 0x80, row y - 1 first */
    size_t rows = y1 - y + 1; uint8_t *e = malloc(rows * w);
    if (!e) { kolmo_vid_strip_dec_c(cur, prev, w, y, y1, k, res); return; }
    const __m128i bias = _mm_set1_epi8((char)0x80);
    size_t i = 0, a = (y - 1) * w;
    for (; i + 16 <= w; i += 16) _mm_storeu_si128((__m128i *)(e + i), _mm_xor_si128(_mm_sub_epi8(ld(cur + a + i), ld(prev + a + i)), bias));
    for (; i < w; i++) e[i] = (uint8_t)((cur[a + i] - prev[a + i]) ^ 0x80);
    med8_rows(res + y * w, w, 1, e + w, rows - 1);
    for (i = w; i + 16 <= rows * w; i += 16) _mm_storeu_si128((__m128i *)(cur + a + i), _mm_add_epi8(ld(prev + a + i), _mm_xor_si128(ld(e + i), bias)));
    for (; i < rows * w; i++) cur[a + i] = (uint8_t)(prev[a + i] + (e[i] ^ 0x80));
    free(e);
}
void kolmo_vid_strip_dec_sse2(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res) {
    dec_body(cur, prev, w, y0, y1, k, res, kolmo_med8_rows_sse2);
}
void kolmo_vid_strip_dec_sse41(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res) {
    dec_body(cur, prev, w, y0, y1, k, res, kolmo_med8_rows_sse41);
}
#endif
