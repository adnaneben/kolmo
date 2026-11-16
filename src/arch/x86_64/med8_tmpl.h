/* IMG8 median-predictor inverse, 16 rows at a time (included by med8_*.c with K_TGT, K_SFX and K_BLEND set).
 *
 * The scalar inverse is one serial chain per channel along a row: each byte needs its left neighbour first. Rows,
 * though, form a wavefront: byte x of row y needs only bytes x and x - C of row y - 1. So 16 rows run in the 16 byte
 * lanes of a vector, lane j skewed C bytes behind lane j - 1. Lane 0 holds the finished row above the group; lanes
 * 1..15 decode 15 new rows. At each step the left neighbours are the result of C steps before, the upper neighbours
 * that result shifted up one lane, the upper-left ones the result of 2C steps before, shifted. Rows go in and out
 * through 16 x 16 byte transposes, 16 steps at a time; the corners of the skewed band are decoded by the scalar code.
 * The arithmetic is the reference's, lane by lane, so the bytes are the same. */
#include <immintrin.h>
#include "algo/predict.h"

#define K_CAT2(a, b) a##b
#define K_CAT(a, b) K_CAT2(a, b)
#define K_FN(n) K_CAT(n, K_SFX)

static K_TGT inline void K_FN(med8_tr16)(__m128i v[16]) {           /* 16 x 16 byte transpose: four perfect shuffles */
    for (int s = 0; s < 4; s++) {
        __m128i w[16];
        for (int i = 0; i < 8; i++) { w[2 * i] = _mm_unpacklo_epi8(v[i], v[i + 8]); w[2 * i + 1] = _mm_unpackhi_epi8(v[i], v[i + 8]); }
        for (int i = 0; i < 16; i++) v[i] = w[i];
    }
}
static K_TGT inline __m128i K_FN(med8_unzz)(__m128i z) {
    __m128i h = _mm_and_si128(_mm_srli_epi16(z, 1), _mm_set1_epi8(0x7f));
    return _mm_xor_si128(h, _mm_sub_epi8(_mm_setzero_si128(), _mm_and_si128(z, _mm_set1_epi8(1))));
}
static K_TGT inline __m128i K_FN(med8_pred)(__m128i a, __m128i b, __m128i c) {
    __m128i mn = _mm_min_epu8(a, b), mx = _mm_max_epu8(a, b), g = _mm_sub_epi8(_mm_add_epi8(a, b), c);
    __m128i lo = _mm_cmpeq_epi8(_mm_max_epu8(c, mx), c);              /* c >= max(a, b): min(a, b) */
    __m128i hi = _mm_cmpeq_epi8(_mm_min_epu8(c, mn), c);              /* c <= min(a, b): max(a, b) */
    return K_BLEND(K_BLEND(g, mx, hi), mn, lo);                        /* else a + b - c, which lies between them */
}

static K_TGT void K_FN(med8_rows)(const uint8_t *r, size_t W, unsigned C, uint8_t *p, size_t y) {
    /* rows y .. y + 14 (byte offsets y * W); row y - 1 is finished */
    const size_t S0 = 16 * (size_t)C;
    uint8_t *P[16]; const uint8_t *Rr[16];
    for (int j = 0; j < 16; j++) { P[j] = p + (y - 1 + (size_t)j) * W; Rr[j] = r + (y - 1 + (size_t)j) * W; }
    for (int j = 1; j < 16; j++) {                                      /* head corner: row j up to column S0 - jC */
        uint8_t *q = P[j]; const uint8_t *u = P[j - 1], *o = Rr[j]; size_t xe = S0 - (size_t)j * C, x;
        for (x = 0; x < C; x++) q[x] = (uint8_t)(u[x] + kolmo_unzz8(o[x]));
        for (; x < xe; x++) q[x] = (uint8_t)(kolmo_med8(q[x - C], u[x], u[x - C]) + kolmo_unzz8(o[x]));
    }
    __m128i R[16 + 8];                                                  /* results of steps s - 2C .. s + 15 */
    const int c2 = 2 * (int)C;
    for (int k = 0; k < c2; k++) {
        size_t s = S0 - (size_t)c2 + (size_t)k; uint8_t t[16];
        for (int j = 0; j < 16; j++) t[j] = s >= (size_t)j * C ? P[j][s - (size_t)j * C] : 0;
        R[k] = _mm_loadu_si128((const __m128i *)t);
    }
    const __m128i not0 = _mm_setr_epi8(0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    size_t s = S0;
    for (; s + 16 <= W; s += 16) {
        __m128i T[16];
        T[0] = _mm_loadu_si128((const __m128i *)(P[0] + s));
        for (int j = 1; j < 16; j++) T[j] = K_FN(med8_unzz)(_mm_loadu_si128((const __m128i *)(Rr[j] + s - (size_t)j * C)));
        K_FN(med8_tr16)(T);
        for (int t = 0; t < 16; t++) {
            __m128i a = R[c2 + t - (int)C], c = _mm_slli_si128(R[t], 1), b = _mm_slli_si128(a, 1);
            R[c2 + t] = _mm_add_epi8(_mm_and_si128(K_FN(med8_pred)(a, b, c), not0), T[t]);
        }
        __m128i O[16];
        for (int t = 0; t < 16; t++) O[t] = R[c2 + t];
        K_FN(med8_tr16)(O);
        for (int j = 1; j < 16; j++) _mm_storeu_si128((__m128i *)(P[j] + s - (size_t)j * C), O[j]);
        for (int k = 0; k < c2; k++) R[k] = R[16 + k];
    }
    for (int j = 1; j < 16; j++) {                                      /* tail corner */
        uint8_t *q = P[j]; const uint8_t *u = P[j - 1], *o = Rr[j];
        for (size_t x = s - (size_t)j * C; x < W; x++) q[x] = (uint8_t)(kolmo_med8(q[x - C], u[x], u[x - C]) + kolmo_unzz8(o[x]));
    }
}

K_TGT void K_FN(kolmo_med8_inv_)(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p) {
    if (C < 1 || C > 4 || W < 16 * (size_t)C + 16 || m < 17 * W) { kolmo_med8_inv_c(r, m, W, C, p); return; }
    size_t x = 0;
    for (; x < C; x++) p[x] = kolmo_unzz8(r[x]);
    for (; x < W; x++) p[x] = (uint8_t)(p[x - C] + kolmo_unzz8(r[x]));
    size_t H = m / W, y = 1;
    for (; y + 15 <= H; y += 15) K_FN(med8_rows)(r, W, C, p, y);
    for (; y < H; y++) {
        uint8_t *q = p + y * W; const uint8_t *u = q - W, *o = r + y * W;
        for (x = 0; x < C; x++) q[x] = (uint8_t)(u[x] + kolmo_unzz8(o[x]));
        for (; x < W; x++) q[x] = (uint8_t)(kolmo_med8(q[x - C], u[x], u[x - C]) + kolmo_unzz8(o[x]));
    }
}
#undef K_FN
#undef K_CAT
#undef K_CAT2
