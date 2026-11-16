/* AVX2 decimal formatters: 4 values per step for |v| < 1e8 and S < 8. Reference: fmt_dec_swar (algo/decimal.c);
 * dispatch: arch/decimal.c. Compiled with a target attribute and chosen by CPUID on baseline x86-64 builds. */
#if defined(KOLMO_FMT_AVX2)
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
#if defined(KOLMO_FMT_AVX2)
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
#if defined(KOLMO_FMT_AVX2)
KOLMO_TARGET_AVX2 static int fmt_slots_avx2(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0;
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
    return i;
}
#endif
#if defined(KOLMO_FMT_AVX2)
KOLMO_TARGET_AVX2 static int fmt_run_avx2(uint8_t **op, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0; uint8_t *o = *op;
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
    *op = o; return i;
}
#endif
