/* Byte-plane split and merge, 16 words per step (included by planes_*.c with K_TGT and K_SFX set).
 * A group of w vectors holds 16 words: as planes (vector b = byte b of words 0..15) or as words (vector o = words
 * 16o/w .. ). With the vector index and the byte position written as one binary number, one round of unpacks
 * (v[i] with v[i + w/2]) rotates that number left by one bit; merging rotates by log2(w), splitting by 4.
 * Delta (w = 2): the wrapping differences are summed back with an in-register prefix sum, carried across steps. */
#include <immintrin.h>
#include "algo/planes.h"

#define K_CAT2(a, b) a##b
#define K_CAT(a, b) K_CAT2(a, b)
#define K_FN(n) K_CAT(n, K_SFX)

static K_TGT inline __attribute__((always_inline)) void K_FN(pl_round)(__m128i *v, unsigned n) {
    __m128i t[8]; unsigned h = n / 2;
    for (unsigned i = 0; i < h; i++) { t[2 * i] = _mm_unpacklo_epi8(v[i], v[i + h]); t[2 * i + 1] = _mm_unpackhi_epi8(v[i], v[i + h]); }
    for (unsigned i = 0; i < n; i++) v[i] = t[i];
}
static K_TGT inline __m128i K_FN(pl_psum16)(__m128i x, __m128i carry) {   /* prefix sum of 8 u16 lanes plus carry (lane 7 of the last) */
    x = _mm_add_epi16(x, _mm_slli_si128(x, 2)); x = _mm_add_epi16(x, _mm_slli_si128(x, 4)); x = _mm_add_epi16(x, _mm_slli_si128(x, 8));
    return _mm_add_epi16(x, _mm_unpackhi_epi64(_mm_shufflehi_epi16(carry, 0xFF), _mm_shufflehi_epi16(carry, 0xFF)));
}

/* one instance per word width (w and delta are constants after inlining, so the vectors stay in registers) */
static K_TGT inline __attribute__((always_inline)) size_t K_FN(pl_merge_w)(const uint8_t *in, size_t rows, const unsigned w, const int delta, uint8_t *out) {
    const unsigned lg = w == 2 ? 1 : w == 4 ? 2 : 3;
    size_t r = 0; __m128i carry = _mm_setzero_si128();
    for (; r + 16 <= rows; r += 16) {
        __m128i v[8];
        for (unsigned b = 0; b < w; b++) v[b] = _mm_loadu_si128((const __m128i *)(in + (size_t)b * rows + r));
        for (unsigned k = 0; k < lg; k++) K_FN(pl_round)(v, w);
        if (delta) { v[0] = carry = K_FN(pl_psum16)(v[0], carry); v[1] = carry = K_FN(pl_psum16)(v[1], carry); }
        for (unsigned o = 0; o < w; o++) _mm_storeu_si128((__m128i *)(out + r * w + 16 * (size_t)o), v[o]);
    }
    return r;
}
static K_TGT inline __attribute__((always_inline)) size_t K_FN(pl_split_w)(const uint8_t *in, size_t rows, const unsigned w, const int delta, uint8_t *out) {
    size_t r = 0; __m128i prev = _mm_setzero_si128();
    for (; r + 16 <= rows; r += 16) {
        __m128i v[8];
        for (unsigned o = 0; o < w; o++) v[o] = _mm_loadu_si128((const __m128i *)(in + r * w + 16 * (size_t)o));
        if (delta) for (unsigned o = 0; o < 2; o++) {
            __m128i x = v[o]; v[o] = _mm_sub_epi16(x, _mm_or_si128(_mm_slli_si128(x, 2), _mm_srli_si128(prev, 14))); prev = x;
        }
        for (unsigned k = 0; k < 4; k++) K_FN(pl_round)(v, w);
        for (unsigned b = 0; b < w; b++) _mm_storeu_si128((__m128i *)(out + (size_t)b * rows + r), v[b]);
    }
    return r;
}

K_TGT void K_FN(kolmo_planes_merge_)(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out) {
    size_t r;
    if (rows < 32) r = 0;
    else if (w == 2) r = delta ? K_FN(pl_merge_w)(in, rows, 2, 1, out) : K_FN(pl_merge_w)(in, rows, 2, 0, out);
    else if (w == 4 && !delta) r = K_FN(pl_merge_w)(in, rows, 4, 0, out);
    else if (w == 8 && !delta) r = K_FN(pl_merge_w)(in, rows, 8, 0, out);
    else r = 0;
    if (!r) { kolmo_planes_merge_c(in, rows, w, delta, out); return; }
    uint64_t pv = (uint64_t)out[r * 2 - 2] | (uint64_t)out[r * 2 - 1] << 8;
    for (; r < rows; r++) {                                           /* tail: the reference's arithmetic */
        uint64_t d = 0; for (unsigned b = 0; b < w; b++) d |= (uint64_t)in[(size_t)b * rows + r] << (8 * b);
        uint64_t v = delta ? (pv + d) & 0xffff : d; pv = v;
        for (unsigned b = 0; b < w; b++) out[r * w + b] = (uint8_t)(v >> (8 * b));
    }
}

K_TGT void K_FN(kolmo_planes_split_)(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out) {
    size_t r;
    if (rows < 32) r = 0;
    else if (w == 2) r = delta ? K_FN(pl_split_w)(in, rows, 2, 1, out) : K_FN(pl_split_w)(in, rows, 2, 0, out);
    else if (w == 4 && !delta) r = K_FN(pl_split_w)(in, rows, 4, 0, out);
    else if (w == 8 && !delta) r = K_FN(pl_split_w)(in, rows, 8, 0, out);
    else r = 0;
    if (!r) { kolmo_planes_split_c(in, rows, w, delta, out); return; }
    uint64_t pv = (uint64_t)in[r * 2 - 2] | (uint64_t)in[r * 2 - 1] << 8;
    for (; r < rows; r++) {
        uint64_t v = 0; for (unsigned b = 0; b < w; b++) v |= (uint64_t)in[r * w + b] << (8 * b);
        uint64_t d = delta ? (v - pv) & 0xffff : v; pv = v;
        for (unsigned b = 0; b < w; b++) out[(size_t)b * rows + r] = (uint8_t)(d >> (8 * b));
    }
}
#undef K_FN
#undef K_CAT
#undef K_CAT2
