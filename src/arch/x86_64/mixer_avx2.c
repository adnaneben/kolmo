/* Context-mixing mixer: dot products and weight updates, AVX2 (8 lanes) and SSE4.1 (4 lanes). Same sums as the C
 * reference (src/algo/mixer.c): the products are exact 64-bit (vpmuldq on even and odd lanes), integer sums do not
 * depend on their order, and the update is the same 32-bit product and arithmetic shift per weight.
 * Apache-2.0, Kolmo contributors. */
#ifndef KOLMO_NO_ASM
#include "arch/x86_64/kernels_x86.h"
#include "algo/mixer.h"

#define TGT __attribute__((target("avx2")))
static TGT inline __m256i mask8(int r) {                 /* lanes < r */
    return _mm256_cmpgt_epi32(_mm256_set1_epi32(r), _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
}
static TGT inline __m256i dot8(__m256i acc, __m256i w, __m256i x) {
    acc = _mm256_add_epi64(acc, _mm256_mul_epi32(w, x));
    return _mm256_add_epi64(acc, _mm256_mul_epi32(_mm256_srli_epi64(w, 32), _mm256_srli_epi64(x, 32)));
}
static TGT inline int64_t hsum64(__m256i a) {
    __m128i s = _mm_add_epi64(_mm256_castsi256_si128(a), _mm256_extracti128_si256(a, 1));
    return _mm_cvtsi128_si64(s) + _mm_extract_epi64(s, 1);
}
TGT void kolmo_cm_dot_avx2(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]) {
    __m256i a1 = _mm256_setzero_si256(), a2 = a1, a3 = a1, a4 = a1; int i = 0;
    if (!w3) {
        for (; i + 8 <= n; i += 8) { __m256i xv = _mm256_loadu_si256((const __m256i *)(x + i));
            a1 = dot8(a1, _mm256_loadu_si256((const __m256i *)(w1 + i)), xv); a2 = dot8(a2, _mm256_loadu_si256((const __m256i *)(w2 + i)), xv); }
        if (i < n) { __m256i m = mask8(n - i), xv = _mm256_maskload_epi32(x + i, m);
            a1 = dot8(a1, _mm256_maskload_epi32(w1 + i, m), xv); a2 = dot8(a2, _mm256_maskload_epi32(w2 + i, m), xv); }
        s[0] = hsum64(a1); s[1] = hsum64(a2); s[2] = s[3] = 0; return;
    }
    for (; i + 8 <= n; i += 8) { __m256i xv = _mm256_loadu_si256((const __m256i *)(x + i));
        a1 = dot8(a1, _mm256_loadu_si256((const __m256i *)(w1 + i)), xv); a2 = dot8(a2, _mm256_loadu_si256((const __m256i *)(w2 + i)), xv);
        a3 = dot8(a3, _mm256_loadu_si256((const __m256i *)(w3 + i)), xv); a4 = dot8(a4, _mm256_loadu_si256((const __m256i *)(w4 + i)), xv); }
    if (i < n) { __m256i m = mask8(n - i), xv = _mm256_maskload_epi32(x + i, m);
        a1 = dot8(a1, _mm256_maskload_epi32(w1 + i, m), xv); a2 = dot8(a2, _mm256_maskload_epi32(w2 + i, m), xv);
        a3 = dot8(a3, _mm256_maskload_epi32(w3 + i, m), xv); a4 = dot8(a4, _mm256_maskload_epi32(w4 + i, m), xv); }
    s[0] = hsum64(a1); s[1] = hsum64(a2); s[2] = hsum64(a3); s[3] = hsum64(a4);
}
static TGT inline void upd8(int32_t *w, __m256i xv, __m256i ev, __m128i sh) {
    _mm256_storeu_si256((__m256i *)w, _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)w), _mm256_sra_epi32(_mm256_mullo_epi32(xv, ev), sh)));
}
static TGT inline void upd8m(int32_t *w, __m256i xv, __m256i ev, __m128i sh, __m256i m) {
    _mm256_maskstore_epi32(w, m, _mm256_add_epi32(_mm256_maskload_epi32(w, m), _mm256_sra_epi32(_mm256_mullo_epi32(xv, ev), sh)));
}
TGT void kolmo_cm_train_avx2(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift) {
    const __m128i sh = _mm_cvtsi32_si128(shift);
    const __m256i e1 = _mm256_set1_epi32(e[0]), e2 = _mm256_set1_epi32(e[1]), e3 = _mm256_set1_epi32(e[2]), e4 = _mm256_set1_epi32(e[3]);
    int i = 0;
    for (; i + 8 <= n; i += 8) { __m256i xv = _mm256_loadu_si256((const __m256i *)(x + i));
        upd8(w1 + i, xv, e1, sh); upd8(w2 + i, xv, e2, sh); if (w3) { upd8(w3 + i, xv, e3, sh); upd8(w4 + i, xv, e4, sh); } }
    if (i < n) { __m256i m = mask8(n - i), xv = _mm256_maskload_epi32(x + i, m);
        upd8m(w1 + i, xv, e1, sh, m); upd8m(w2 + i, xv, e2, sh, m); if (w3) { upd8m(w3 + i, xv, e3, sh, m); upd8m(w4 + i, xv, e4, sh, m); } }
}
#undef TGT

#define TGT __attribute__((target("sse4.1")))
static TGT inline __m128i dot4(__m128i acc, __m128i w, __m128i x) {
    acc = _mm_add_epi64(acc, _mm_mul_epi32(w, x));
    return _mm_add_epi64(acc, _mm_mul_epi32(_mm_srli_epi64(w, 32), _mm_srli_epi64(x, 32)));
}
static TGT inline int64_t hsum2(__m128i a) { return _mm_cvtsi128_si64(a) + _mm_extract_epi64(a, 1); }
TGT void kolmo_cm_dot_sse41(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]) {
    __m128i a1 = _mm_setzero_si128(), a2 = a1, a3 = a1, a4 = a1; int i = 0; const int q = w3 ? 4 : 2;
    for (; i + 4 <= n; i += 4) { __m128i xv = _mm_loadu_si128((const __m128i *)(x + i));
        a1 = dot4(a1, _mm_loadu_si128((const __m128i *)(w1 + i)), xv); a2 = dot4(a2, _mm_loadu_si128((const __m128i *)(w2 + i)), xv);
        if (q == 4) { a3 = dot4(a3, _mm_loadu_si128((const __m128i *)(w3 + i)), xv); a4 = dot4(a4, _mm_loadu_si128((const __m128i *)(w4 + i)), xv); } }
    int64_t s1 = hsum2(a1), s2 = hsum2(a2), s3 = hsum2(a3), s4 = hsum2(a4);
    for (; i < n; i++) { s1 += (int64_t)w1[i] * x[i]; s2 += (int64_t)w2[i] * x[i]; if (q == 4) { s3 += (int64_t)w3[i] * x[i]; s4 += (int64_t)w4[i] * x[i]; } }
    s[0] = s1; s[1] = s2; s[2] = s3; s[3] = s4;
}
static TGT inline void upd4(int32_t *w, __m128i xv, __m128i ev, __m128i sh) {
    _mm_storeu_si128((__m128i *)w, _mm_add_epi32(_mm_loadu_si128((const __m128i *)w), _mm_sra_epi32(_mm_mullo_epi32(xv, ev), sh)));
}
TGT void kolmo_cm_train_sse41(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift) {
    const __m128i sh = _mm_cvtsi32_si128(shift);
    const __m128i e1 = _mm_set1_epi32(e[0]), e2 = _mm_set1_epi32(e[1]), e3 = _mm_set1_epi32(e[2]), e4 = _mm_set1_epi32(e[3]);
    int i = 0;
    for (; i + 4 <= n; i += 4) { __m128i xv = _mm_loadu_si128((const __m128i *)(x + i));
        upd4(w1 + i, xv, e1, sh); upd4(w2 + i, xv, e2, sh); if (w3) { upd4(w3 + i, xv, e3, sh); upd4(w4 + i, xv, e4, sh); } }
    for (; i < n; i++) { w1[i] += (x[i] * e[0]) >> shift; w2[i] += (x[i] * e[1]) >> shift; if (w3) { w3[i] += (x[i] * e[2]) >> shift; w4[i] += (x[i] * e[3]) >> shift; } }
}
#endif
