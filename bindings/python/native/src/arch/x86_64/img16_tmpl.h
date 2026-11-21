/* IMG16 forward prediction and the stride search's sums of absolute differences (included by img16_*.c).
 * K_V is the vector type (__m128i or __m256i) and the K_* macros its operations; unsigned 16-bit order comes from
 * min/max_epu16 (SSE4.1, AVX2) or from signed order after flipping the top bit (SSE2). Same values as
 * src/algo/predict.c: the median, average and zigzag are the reference's, lane by lane, and integer sums do not depend
 * on their order. */
#define K_CAT2(a, b) a##b
#define K_CAT(a, b) K_CAT2(a, b)
#define K_FN(n) K_CAT(n, K_SFX)
#define K_N ((int)(sizeof(K_V) / 2))                         /* 16-bit lanes */

K_TGT uint64_t K_FN(kolmo_sad16_)(const uint16_t *x, const uint16_t *y, size_t n) {
    size_t i = 0; uint64_t s = 0;
    while (i + K_N <= n) {
        K_V acc = K_ZERO; size_t e = n - i > 32768 ? i + 32768 : n;  /* 32-bit lanes: at most 2 x 65535 per step */
        for (; i + K_N <= e; i += K_N) {
            K_V a = K_LOAD(x + i), b = K_LOAD(y + i), d = K_OR(K_SUBSU16(a, b), K_SUBSU16(b, a));
            acc = K_ADD32(acc, K_ADD32(K_UNPLO16(d, K_ZERO), K_UNPHI16(d, K_ZERO)));
        }
        s += K_HSUM32(acc);
    }
    for (; i < n; i++) s += (uint64_t)(x[i] > y[i] ? x[i] - y[i] : y[i] - x[i]);
    return s;
}

K_TGT void K_FN(kolmo_img16_fwd_)(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi) {
    int64_t h = n < W + 1 ? n : W + 1;
    kolmo_img16_fwd_c(a, h, W, mode, lo, hi);                       /* the first row and one sample: edge rules */
    int64_t i = h;
    for (; i + 2 * K_N <= n; i += 2 * K_N) {
        K_V z[2];
        for (int q = 0; q < 2; q++) {
            int64_t j = i + q * K_N;
            K_V c = K_LOAD(a + j), L = K_LOAD(a + j - 1), p;
            if (mode == 0) p = L;
            else {
                K_V U = K_LOAD(a + j - W);
                if (mode == 1) p = K_ADD16(K_AND(L, U), K_SRLI16(K_XOR(L, U), 1));
                else {
                    K_V UL = K_LOAD(a + j - W - 1), mn = K_MINU16(L, U), mx = K_MAXU16(L, U), g = K_SUB16(K_ADD16(L, U), UL);
                    K_V lo_ = K_CMPEQ16(K_MAXU16(UL, mx), UL), hi_ = K_CMPEQ16(K_MINU16(UL, mn), UL);
                    p = K_BLEND(K_BLEND(g, mx, hi_), mn, lo_);
                }
            }
            K_V r = K_SUB16(c, p);
            z[q] = K_XOR(K_ADD16(r, r), K_SRAI16(r, 15));
        }
        const K_V m8 = K_SET16(0xff);
        K_STORE8(lo + i, K_PACKUS(K_AND(z[0], m8), K_AND(z[1], m8)));
        K_STORE8(hi + i, K_PACKUS(K_SRLI16(z[0], 8), K_SRLI16(z[1], 8)));
    }
    if (i < n) {                                                    /* tail: the reference on the remaining samples */
        for (; i < n; i++) {
            int L = a[i - 1], U = a[i - W], UL = a[i - W - 1], p;
            if (mode == 0) p = L; else if (mode == 1) p = (L + U) >> 1;
            else { int mx = L > U ? L : U, mn = L < U ? L : U; p = UL >= mx ? mn : UL <= mn ? mx : L + U - UL; }
            uint16_t r = (uint16_t)(a[i] - p), zz = (uint16_t)((uint16_t)(r << 1) ^ (uint16_t)-(uint16_t)(r >> 15));
            lo[i] = (uint8_t)zz; hi[i] = (uint8_t)(zz >> 8);
        }
    }
}
#undef K_N
#undef K_FN
#undef K_CAT
#undef K_CAT2
