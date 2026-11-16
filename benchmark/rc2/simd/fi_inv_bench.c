/* Microbenchmark for the integer inverse (src/kgraph.inc FI_INV) at W=4: scalar (the shipped macro, copied),
 * SSE2 intrinsics (baseline x86-64) and AVX2 intrinsics (target attribute). Checks byte identity, then times each
 * on prefaulted buffers (fastest of 15). usage: fi_inv_bench [n] */
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int64_t kg_unzz(uint64_t u) { return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }
#define FI_ST(o, v, W) do { if ((W) == 8) { uint64_t t_ = (v); memcpy(o, &t_, 8); } else if ((W) == 4) { uint32_t t_ = (uint32_t)(v); memcpy(o, &t_, 4); } \
                            else { uint16_t t_ = (uint16_t)(v); memcpy(o, &t_, 2); } } while (0)
#define FI_FIX(v, W, SG) ((W) == 8 ? (v) : (SG) ? (uint64_t)(int64_t)((W) == 4 ? (int64_t)(int32_t)(uint32_t)(v) : (int64_t)(int16_t)(uint16_t)(v)) \
                                              : ((v) & (((uint64_t)1 << (8 * (W))) - 1)))
#define FI_Z(r, i, m, OW) ((OW) == 1 ? (uint64_t)(r)[i] : (uint64_t)(r)[i] | (uint64_t)(r)[(m) + (i)] << 8)
#define FI_INV(W, OW, SG)                                                                                             \
static void fi_inv_##W##_##OW##_##SG(const uint8_t *r, size_t m, int ord, uint64_t g, uint64_t bias, uint64_t v1, uint64_t v2, uint8_t *o) { \
    if (ord == 0) { if (g == 1) for (size_t i = 0; i < m; i++, o += W) { uint64_t v = bias + FI_Z(r, i, m, OW); FI_ST(o, FI_FIX(v, W, SG), W); } \
                    else for (size_t i = 0; i < m; i++, o += W) { uint64_t v = bias + FI_Z(r, i, m, OW) * g; FI_ST(o, FI_FIX(v, W, SG), W); } } \
    else if (ord == 1) { for (size_t i = 0; i < m; i++, o += W) { uint64_t d = (uint64_t)kg_unzz(FI_Z(r, i, m, OW)); if (g != 1) d *= g; \
                                                                   uint64_t v = FI_FIX(v1 + d, W, SG); FI_ST(o, v, W); v1 = v; } } \
    else { for (size_t i = 0; i < m; i++, o += W) { uint64_t d = (uint64_t)kg_unzz(FI_Z(r, i, m, OW)); if (g != 1) d *= g;      \
                                                     uint64_t v = FI_FIX(2 * v1 - v2 + d, W, SG); FI_ST(o, v, W); v2 = v1; v1 = v; } } \
}
FI_INV(4, 2, 0)
FI_INV(4, 1, 0)

/* SSE2: 8 values per step. 32-bit lanes are exact for W = 4 (only the low 32 bits of each value are stored, and the
 * recurrence is arithmetic mod 2^64, so mod 2^32 agrees). g != 1 or ord 0 go to the scalar loop. */
static inline __m128i pfx4(__m128i x) { x = _mm_add_epi32(x, _mm_slli_si128(x, 4)); return _mm_add_epi32(x, _mm_slli_si128(x, 8)); }
static inline __m128i unzz32(__m128i z) { return _mm_xor_si128(_mm_srli_epi32(z, 1), _mm_sub_epi32(_mm_setzero_si128(), _mm_and_si128(z, _mm_set1_epi32(1)))); }
static size_t fi_inv4_sse2(const uint8_t *r, size_t m, int ow, int ord, uint64_t v1, uint64_t v2, uint8_t *o) {
    __m128i c1 = _mm_set1_epi32((int)(uint32_t)v1), ce = _mm_set1_epi32((int)(uint32_t)(v1 - v2)), zr = _mm_setzero_si128();
    size_t i = 0;
    for (; i + 8 <= m; i += 8) {
        __m128i lo = _mm_loadl_epi64((const __m128i *)(r + i));
        __m128i z16 = ow == 2 ? _mm_unpacklo_epi8(lo, _mm_loadl_epi64((const __m128i *)(r + m + i))) : _mm_unpacklo_epi8(lo, zr);
        __m128i da = unzz32(_mm_unpacklo_epi16(z16, zr)), db = unzz32(_mm_unpackhi_epi16(z16, zr));
        if (ord == 2) {                                  /* first differences e = prefix(d) + e_prev, then values */
            da = _mm_add_epi32(pfx4(da), ce); ce = _mm_shuffle_epi32(da, 0xFF);
            db = _mm_add_epi32(pfx4(db), ce); ce = _mm_shuffle_epi32(db, 0xFF);
        }
        __m128i va = _mm_add_epi32(pfx4(da), c1); c1 = _mm_shuffle_epi32(va, 0xFF);
        __m128i vb = _mm_add_epi32(pfx4(db), c1); c1 = _mm_shuffle_epi32(vb, 0xFF);
        _mm_storeu_si128((__m128i *)(o + 4 * i), va); _mm_storeu_si128((__m128i *)(o + 4 * i + 16), vb);
    }
    return i;
}
__attribute__((target("avx2"))) static size_t fi_inv4_avx2(const uint8_t *r, size_t m, int ow, int ord, uint64_t v1, uint64_t v2, uint8_t *o) {
    __m256i c1 = _mm256_set1_epi32((int)(uint32_t)v1), ce = _mm256_set1_epi32((int)(uint32_t)(v1 - v2)), b7 = _mm256_set1_epi32(7);
    size_t i = 0;
#define PFX8(x) do { x = _mm256_add_epi32(x, _mm256_slli_si256(x, 4)); x = _mm256_add_epi32(x, _mm256_slli_si256(x, 8)); \
                     x = _mm256_add_epi32(x, _mm256_permute2x128_si256(_mm256_shuffle_epi32(x, 0xFF), _mm256_shuffle_epi32(x, 0xFF), 0x08)); } while (0)
    for (; i + 16 <= m; i += 16) {
        __m128i lo = _mm_loadu_si128((const __m128i *)(r + i)), hi = ow == 2 ? _mm_loadu_si128((const __m128i *)(r + m + i)) : _mm_setzero_si128();
        __m256i z16 = _mm256_cvtepu8_epi16(lo);
        if (ow == 2) z16 = _mm256_or_si256(z16, _mm256_slli_epi16(_mm256_cvtepu8_epi16(hi), 8));
        __m256i za = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(z16)), zb = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(z16, 1));
        __m256i one = _mm256_set1_epi32(1);
        __m256i da = _mm256_xor_si256(_mm256_srli_epi32(za, 1), _mm256_sub_epi32(_mm256_setzero_si256(), _mm256_and_si256(za, one)));
        __m256i db = _mm256_xor_si256(_mm256_srli_epi32(zb, 1), _mm256_sub_epi32(_mm256_setzero_si256(), _mm256_and_si256(zb, one)));
        if (ord == 2) {
            PFX8(da); da = _mm256_add_epi32(da, ce); ce = _mm256_permutevar8x32_epi32(da, b7);
            PFX8(db); db = _mm256_add_epi32(db, ce); ce = _mm256_permutevar8x32_epi32(db, b7);
        }
        PFX8(da); da = _mm256_add_epi32(da, c1); c1 = _mm256_permutevar8x32_epi32(da, b7);
        PFX8(db); db = _mm256_add_epi32(db, c1); c1 = _mm256_permutevar8x32_epi32(db, b7);
        _mm256_storeu_si256((__m256i *)(o + 4 * i), da); _mm256_storeu_si256((__m256i *)(o + 4 * i + 32), db);
    }
    return i;
}
typedef size_t (*vec_fn)(const uint8_t *, size_t, int, int, uint64_t, uint64_t, uint8_t *);
static void run_vec(vec_fn f, const uint8_t *r, size_t m, int ow, int ord, uint64_t v1, uint64_t v2, uint8_t *o) {
    size_t i = f(r, m, ow, ord, v1, v2, o);
    /* scalar tail: rebuild v1, v2 from the stored output, then the shipped loop on planes offset by i */
    if (i) { uint32_t a, b = 0; memcpy(&a, o + 4 * (i - 1), 4); if (i >= 2) memcpy(&b, o + 4 * (i - 2), 4); else b = (uint32_t)v1; v1 = a; v2 = b; }
    for (; i < m; i++) { uint64_t z = ow == 2 ? (uint64_t)r[i] | (uint64_t)r[m + i] << 8 : r[i]; uint64_t d = (uint64_t)kg_unzz(z);
        uint64_t v = (ord == 1 ? v1 + d : 2 * v1 - v2 + d) & 0xffffffffu; memcpy(o + 4 * i, &v, 4); v2 = v1; v1 = v; }
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    size_t m = argc > 1 ? strtoull(argv[1], 0, 10) : (1u << 24) - 1;
    uint8_t *r = malloc(2 * m), *o0 = malloc(4 * m + 64), *o1 = malloc(4 * m + 64), *o2 = malloc(4 * m + 64);
    uint64_t x = 88172645463325252ull;
    for (size_t i = 0; i < 2 * m; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; r[i] = (uint8_t)x; }
    memset(o0, 0, 4 * m + 64); memset(o1, 0, 4 * m + 64); memset(o2, 0, 4 * m + 64);
    __builtin_cpu_init(); int avx2 = __builtin_cpu_supports("avx2");
    printf("m = %zu values, W = 4\n| ord | planes | scalar ns/value | SSE2 ns/value | AVX2 ns/value | identical |\n|---:|---:|---:|---:|---:|---|\n", m);
    for (int ord = 1; ord <= 2; ord++) for (int ow = 1; ow <= 2; ow++) {
        double best[3] = {1e9, 1e9, 1e9}; uint64_t v1 = 123456789, v2 = 123450000;
        for (int rep = 0; rep < 15; rep++) {
            double t = now(); (ow == 2 ? fi_inv_4_2_0 : fi_inv_4_1_0)(r, m, ord, 1, 0, v1, v2, o0); t = now() - t; if (t < best[0]) best[0] = t;
            t = now(); run_vec(fi_inv4_sse2, r, m, ow, ord, v1, v2, o1); t = now() - t; if (t < best[1]) best[1] = t;
            if (avx2) { t = now(); run_vec(fi_inv4_avx2, r, m, ow, ord, v1, v2, o2); t = now() - t; if (t < best[2]) best[2] = t; }
        }
        int same = !memcmp(o0, o1, 4 * m) && (!avx2 || !memcmp(o0, o2, 4 * m));
        printf("| %d | %d | %.3f | %.3f | %.3f | %s |\n", ord, ow, best[0] * 1e9 / m, best[1] * 1e9 / m, avx2 ? best[2] * 1e9 / m : 0, same ? "yes" : "NO");
    }
    return 0;
}
