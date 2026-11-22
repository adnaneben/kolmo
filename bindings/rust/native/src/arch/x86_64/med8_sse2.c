/* IMG8 median-predictor inverse, SSE2 (x86-64 baseline) and SSE4.1. Apache-2.0, Kolmo contributors. */
#ifndef KOLMO_NO_ASM
#include "arch/x86_64/kernels_x86.h"
static inline __m128i k_blend_sse2(__m128i a, __m128i b, __m128i m) { return _mm_or_si128(_mm_and_si128(m, b), _mm_andnot_si128(m, a)); }
#define K_TGT
#define K_SFX sse2
#define K_BLEND k_blend_sse2
#include "arch/x86_64/med8_tmpl.h"
#undef K_TGT
#undef K_SFX
#undef K_BLEND
#define K_TGT __attribute__((target("sse4.1")))
#define K_SFX sse41
#define K_BLEND _mm_blendv_epi8
#include "arch/x86_64/med8_tmpl.h"
#endif
