/* IMG8 forward transform (median predictor residuals, zigzagged): SSE2 (16 bytes) and AVX2 (32 bytes per step). Each
 * byte depends only on the input, so the rows vectorize directly; the median is med8_tmpl.h's, the first row and the
 * first C bytes of each row are the reference's. Same bytes as kolmo_med8_fwd_c. Apache-2.0, Kolmo contributors. */
#ifndef KOLMO_NO_ASM
#include "arch/x86_64/kernels_x86.h"
#include "algo/predict.h"

static inline __m128i blend2(__m128i a, __m128i b, __m128i m) { return _mm_or_si128(_mm_and_si128(m, b), _mm_andnot_si128(m, a)); }
static inline __m128i zz2(__m128i r) { return _mm_xor_si128(_mm_add_epi8(r, r), _mm_cmpgt_epi8(_mm_setzero_si128(), r)); }
void kolmo_med8_fwd_sse2(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r) {
    if (m < 2 * W || W < C + 16) { kolmo_med8_fwd_c(p, m, W, C, r); return; }
    kolmo_med8_fwd_c(p, W, W, C, r);                                         /* row 0 */
    for (size_t y = W; y + W <= m; y += W) {
        const uint8_t *q = p + y, *u = q - W; uint8_t *o = r + y; size_t x;
        for (x = 0; x < C; x++) o[x] = kolmo_zz8((uint8_t)(q[x] - u[x]));
        for (; x + 16 <= W; x += 16) {
            __m128i a = _mm_loadu_si128((const __m128i *)(q + x - C)), b = _mm_loadu_si128((const __m128i *)(u + x)), c = _mm_loadu_si128((const __m128i *)(u + x - C));
            __m128i mn = _mm_min_epu8(a, b), mx = _mm_max_epu8(a, b), g = _mm_sub_epi8(_mm_add_epi8(a, b), c);
            __m128i lo = _mm_cmpeq_epi8(_mm_max_epu8(c, mx), c), hi = _mm_cmpeq_epi8(_mm_min_epu8(c, mn), c);
            __m128i pr = blend2(blend2(g, mx, hi), mn, lo);
            _mm_storeu_si128((__m128i *)(o + x), zz2(_mm_sub_epi8(_mm_loadu_si128((const __m128i *)(q + x)), pr)));
        }
        for (; x < W; x++) o[x] = kolmo_zz8((uint8_t)(q[x] - kolmo_med8(q[x - C], u[x], u[x - C])));
    }
}

#define TGT __attribute__((target("avx2")))
TGT void kolmo_med8_fwd_avx2(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r) {
    if (m < 2 * W || W < C + 32) { kolmo_med8_fwd_sse2(p, m, W, C, r); return; }
    kolmo_med8_fwd_c(p, W, W, C, r);
    const __m256i z = _mm256_setzero_si256();
    for (size_t y = W; y + W <= m; y += W) {
        const uint8_t *q = p + y, *u = q - W; uint8_t *o = r + y; size_t x;
        for (x = 0; x < C; x++) o[x] = kolmo_zz8((uint8_t)(q[x] - u[x]));
        for (; x + 32 <= W; x += 32) {
            __m256i a = _mm256_loadu_si256((const __m256i *)(q + x - C)), b = _mm256_loadu_si256((const __m256i *)(u + x)), c = _mm256_loadu_si256((const __m256i *)(u + x - C));
            __m256i mn = _mm256_min_epu8(a, b), mx = _mm256_max_epu8(a, b), g = _mm256_sub_epi8(_mm256_add_epi8(a, b), c);
            __m256i lo = _mm256_cmpeq_epi8(_mm256_max_epu8(c, mx), c), hi = _mm256_cmpeq_epi8(_mm256_min_epu8(c, mn), c);
            __m256i pr = _mm256_blendv_epi8(_mm256_blendv_epi8(g, mx, hi), mn, lo);
            __m256i d = _mm256_sub_epi8(_mm256_loadu_si256((const __m256i *)(q + x)), pr);
            _mm256_storeu_si256((__m256i *)(o + x), _mm256_xor_si256(_mm256_add_epi8(d, d), _mm256_cmpgt_epi8(z, d)));
        }
        for (; x < W; x++) o[x] = kolmo_zz8((uint8_t)(q[x] - kolmo_med8(q[x - C], u[x], u[x - C])));
    }
}
#endif
