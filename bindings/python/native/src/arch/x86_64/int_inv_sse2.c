/* Integer inverse kernels for transforms/graph.c (fi_vec4 there is the dispatch and the scalar tail; the
 * reference is kg_int_inv). W = 4, g = 1, order 1 or 2, residuals one byte wide or in two byte planes (the common case of 32-bit arrays):
 * zig-zag decode and the order-1/2 recurrence as in-register prefix sums, 8 (SSE2, baseline x86-64) or 16 (AVX2, by
 * CPUID) values per step. 32-bit lanes give the same bytes: only the low 32 bits of each value are stored and the
 * recurrence is arithmetic mod 2^64, which agrees mod 2^32. Returns the number of values written; the caller's scalar
 * loop is not used for the rest, fi_vec4 finishes it itself (planes are addressed from the start, so the shipped loop
 * cannot resume at an offset). Measured in benchmark/rc2/SIMD-ASM.md. */
#if defined(KOLMO_USE_SSE2) && FI_USE
#include <emmintrin.h>
#define FI_VEC4 1
static inline __m128i fi_pfx4(__m128i x) { x = _mm_add_epi32(x, _mm_slli_si128(x, 4)); return _mm_add_epi32(x, _mm_slli_si128(x, 8)); }
static inline __m128i fi_unzz4(__m128i z) { return _mm_xor_si128(_mm_srli_epi32(z, 1), _mm_sub_epi32(_mm_setzero_si128(), _mm_and_si128(z, _mm_set1_epi32(1)))); }
static size_t fi_vec4_sse2(const uint8_t *r, size_t m, int ow, int ord, uint32_t v1, uint32_t v2, uint8_t *o) {
    __m128i c1 = _mm_set1_epi32((int)v1), ce = _mm_set1_epi32((int)(v1 - v2)), zr = _mm_setzero_si128();
    size_t i = 0;
    for (; i + 8 <= m; i += 8) {
        __m128i lo = _mm_loadl_epi64((const __m128i *)(r + i));
        __m128i z16 = _mm_unpacklo_epi8(lo, ow == 2 ? _mm_loadl_epi64((const __m128i *)(r + m + i)) : zr);
        __m128i da = fi_unzz4(_mm_unpacklo_epi16(z16, zr)), db = fi_unzz4(_mm_unpackhi_epi16(z16, zr));
        if (ord == 2) {                                   /* first differences: prefix(d) + the previous one */
            da = _mm_add_epi32(fi_pfx4(da), ce); ce = _mm_shuffle_epi32(da, 0xFF);
            db = _mm_add_epi32(fi_pfx4(db), ce); ce = _mm_shuffle_epi32(db, 0xFF);
        }
        __m128i va = _mm_add_epi32(fi_pfx4(da), c1); c1 = _mm_shuffle_epi32(va, 0xFF);
        __m128i vb = _mm_add_epi32(fi_pfx4(db), c1); c1 = _mm_shuffle_epi32(vb, 0xFF);
        _mm_storeu_si128((__m128i *)(o + 4 * i), va); _mm_storeu_si128((__m128i *)(o + 4 * i + 16), vb);
    }
    return i;
}
#endif
