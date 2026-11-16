/* IMG16 forward prediction and stride search, SSE2 (x86-64 baseline) and AVX2. Apache-2.0, Kolmo contributors. */
#ifndef KOLMO_NO_ASM
#include "arch/x86_64/kernels_x86.h"
#include "algo/predict.h"

/* SSE2 */
#define K_V __m128i
#define K_TGT
#define K_SFX sse2
#define K_ZERO _mm_setzero_si128()
#define K_LOAD(p) _mm_loadu_si128((const __m128i *)(p))
#define K_STORE8(p, v) _mm_storeu_si128((__m128i *)(p), v)
#define K_OR _mm_or_si128
#define K_AND _mm_and_si128
#define K_XOR _mm_xor_si128
#define K_ADD16 _mm_add_epi16
#define K_SUB16 _mm_sub_epi16
#define K_ADD32 _mm_add_epi32
#define K_SUBSU16 _mm_subs_epu16
#define K_UNPLO16 _mm_unpacklo_epi16
#define K_UNPHI16 _mm_unpackhi_epi16
#define K_SRLI16 _mm_srli_epi16
#define K_SRAI16 _mm_srai_epi16
#define K_CMPEQ16 _mm_cmpeq_epi16
#define K_SET16(v) _mm_set1_epi16((short)(v))
#define K_PACKUS _mm_packus_epi16
static inline __m128i k2_minu16(__m128i a, __m128i b) { const __m128i f = _mm_set1_epi16((short)0x8000); return _mm_xor_si128(_mm_min_epi16(_mm_xor_si128(a, f), _mm_xor_si128(b, f)), f); }
static inline __m128i k2_maxu16(__m128i a, __m128i b) { const __m128i f = _mm_set1_epi16((short)0x8000); return _mm_xor_si128(_mm_max_epi16(_mm_xor_si128(a, f), _mm_xor_si128(b, f)), f); }
static inline __m128i k2_blend(__m128i a, __m128i b, __m128i m) { return _mm_or_si128(_mm_and_si128(m, b), _mm_andnot_si128(m, a)); }
static inline uint64_t k2_hsum32(__m128i a) { uint32_t t[4]; _mm_storeu_si128((__m128i *)t, a); return (uint64_t)t[0] + t[1] + t[2] + t[3]; }
#define K_MINU16 k2_minu16
#define K_MAXU16 k2_maxu16
#define K_BLEND k2_blend
#define K_HSUM32 k2_hsum32
#include "arch/x86_64/img16_tmpl.h"
#undef K_V
#undef K_TGT
#undef K_SFX
#undef K_ZERO
#undef K_LOAD
#undef K_STORE8
#undef K_OR
#undef K_AND
#undef K_XOR
#undef K_ADD16
#undef K_SUB16
#undef K_ADD32
#undef K_SUBSU16
#undef K_UNPLO16
#undef K_UNPHI16
#undef K_SRLI16
#undef K_SRAI16
#undef K_CMPEQ16
#undef K_SET16
#undef K_PACKUS
#undef K_MINU16
#undef K_MAXU16
#undef K_BLEND
#undef K_HSUM32

/* AVX2: 16 lanes; packus works within 128-bit halves, so the packed bytes are put back in order with vpermq */
#define K_V __m256i
#define K_TGT __attribute__((target("avx2")))
#define K_SFX avx2
#define K_ZERO _mm256_setzero_si256()
#define K_LOAD(p) _mm256_loadu_si256((const __m256i *)(p))
#define K_STORE8(p, v) _mm256_storeu_si256((__m256i *)(p), _mm256_permute4x64_epi64(v, 0xD8))
#define K_OR _mm256_or_si256
#define K_AND _mm256_and_si256
#define K_XOR _mm256_xor_si256
#define K_ADD16 _mm256_add_epi16
#define K_SUB16 _mm256_sub_epi16
#define K_ADD32 _mm256_add_epi32
#define K_SUBSU16 _mm256_subs_epu16
#define K_UNPLO16 _mm256_unpacklo_epi16
#define K_UNPHI16 _mm256_unpackhi_epi16
#define K_SRLI16 _mm256_srli_epi16
#define K_SRAI16 _mm256_srai_epi16
#define K_CMPEQ16 _mm256_cmpeq_epi16
#define K_SET16(v) _mm256_set1_epi16((short)(v))
#define K_PACKUS _mm256_packus_epi16
#define K_MINU16 _mm256_min_epu16
#define K_MAXU16 _mm256_max_epu16
#define K_BLEND(a, b, m) _mm256_blendv_epi8(a, b, m)
static __attribute__((target("avx2"))) inline uint64_t k8_hsum32(__m256i a) {
    __m128i s = _mm_add_epi32(_mm256_castsi256_si128(a), _mm256_extracti128_si256(a, 1));
    uint32_t t[4]; _mm_storeu_si128((__m128i *)t, s); return (uint64_t)t[0] + t[1] + t[2] + t[3];
}
#define K_HSUM32 k8_hsum32
#include "arch/x86_64/img16_tmpl.h"
#endif
