/* AVX2 integer inverse kernel (16 values per step), chosen by CPUID in fi_vec4 (transforms/graph.c). See
 * arch/x86_64/int_inv_sse2.c for the contract. */
#if defined(FI_VEC4)
#if defined(KOLMO_FMT_AVX2)
#define FI_PFX8(x) do { x = _mm256_add_epi32(x, _mm256_slli_si256(x, 4)); x = _mm256_add_epi32(x, _mm256_slli_si256(x, 8)); \
                        __m256i t_ = _mm256_shuffle_epi32(x, 0xFF); x = _mm256_add_epi32(x, _mm256_permute2x128_si256(t_, t_, 0x08)); } while (0)
KOLMO_TARGET_AVX2 static size_t fi_vec4_avx2(const uint8_t *r, size_t m, int ow, int ord, uint32_t v1, uint32_t v2, uint8_t *o) {
    const __m256i b7 = _mm256_set1_epi32(7), one = _mm256_set1_epi32(1), zr = _mm256_setzero_si256();
    __m256i c1 = _mm256_set1_epi32((int)v1), ce = _mm256_set1_epi32((int)(v1 - v2));
    size_t i = 0;
    for (; i + 16 <= m; i += 16) {
        __m256i z16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(r + i)));
        if (ow == 2) z16 = _mm256_or_si256(z16, _mm256_slli_epi16(_mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)(r + m + i))), 8));
        __m256i za = _mm256_cvtepu16_epi32(_mm256_castsi256_si128(z16)), zb = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(z16, 1));
        __m256i da = _mm256_xor_si256(_mm256_srli_epi32(za, 1), _mm256_sub_epi32(zr, _mm256_and_si256(za, one)));
        __m256i db = _mm256_xor_si256(_mm256_srli_epi32(zb, 1), _mm256_sub_epi32(zr, _mm256_and_si256(zb, one)));
        if (ord == 2) {
            FI_PFX8(da); da = _mm256_add_epi32(da, ce); ce = _mm256_permutevar8x32_epi32(da, b7);
            FI_PFX8(db); db = _mm256_add_epi32(db, ce); ce = _mm256_permutevar8x32_epi32(db, b7);
        }
        FI_PFX8(da); da = _mm256_add_epi32(da, c1); c1 = _mm256_permutevar8x32_epi32(da, b7);
        FI_PFX8(db); db = _mm256_add_epi32(db, c1); c1 = _mm256_permutevar8x32_epi32(db, b7);
        _mm256_storeu_si256((__m256i *)(o + 4 * i), da); _mm256_storeu_si256((__m256i *)(o + 4 * i + 32), db);
    }
    return i;
}
#endif
#endif
