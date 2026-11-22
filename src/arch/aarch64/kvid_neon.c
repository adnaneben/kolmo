/* NEON: kvid's per-strip prediction (C reference: predict, encode_plane and decode_plane in src/experimental/kvid.c;
 * kvid is a research tool, built on its own by experimental.mk, and this file with it).
 * kolmo_kvid_row_neon - row y >= 1, columns 1 .. w - 1 of predictor k (0: median of left, above, above-left; 1: the
 *                       previous frame; 2: the previous frame plus the median of the frame differences): the sum of
 *                       |residual| (the strip's cost), or the zigzagged residuals when res is given. 16 samples per
 *                       step; the encoder reads only the input, so the samples are independent.
 * kolmo_kvid_add_neon - temporal strips of the decoder: sample = previous frame + residual.
 * The intra strips of the decoder are the IMG8 median inverse with one channel (kolmo_med_band16_neon, image_neon.c).
 * Medians of signed differences are taken on bytes biased by 128, an order-preserving map under which a + b - c moves
 * by the same 128, so the unsigned median of image_neon.c applies. */
#if defined(__aarch64__) && defined(__ARM_NEON) && !defined(KOLMO_NO_ASM)
#include <arm_neon.h>
#include <stddef.h>
#include <stdint.h>

static inline uint8x16_t kv_med(uint8x16_t L, uint8x16_t U, uint8x16_t UL) {
    uint8x16_t g = vqsubq_u8(vqaddq_u8(L, vqsubq_u8(U, UL)), vqsubq_u8(UL, U));
    return vminq_u8(vmaxq_u8(g, vminq_u8(L, U)), vmaxq_u8(L, U));
}
static inline int kv_med1(int a, int b, int c) { int mx = a > b ? a : b, mn = a < b ? a : b; return c >= mx ? mn : c <= mn ? mx : a + b - c; }
static inline int kv_pred(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t i) {   /* x >= 1, y >= 1 */
    if (k == 0) return kv_med1(cur[i - 1], cur[i - w], cur[i - w - 1]);
    if (k == 1) return prev[i];
#define D(j) ((int)(int8_t)(uint8_t)(cur[j] - prev[j]))
    return prev[i] + kv_med1(D(i - 1), D(i - w), D(i - w - 1));
#undef D
}
uint64_t kolmo_kvid_row_neon(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t y, uint8_t *res) {
    const uint8_t *q = cur + y * w, *u = q - w; const uint8_t *pq = prev ? prev + y * w : NULL, *pu = pq ? pq - w : NULL;
    const uint8x16_t b80 = vdupq_n_u8(0x80);
    uint64_t c = 0; size_t x = 1;
    while (x + 16 <= w) {
        uint16x8_t acc = vdupq_n_u16(0);
        size_t e = x + 16 * 128 < w ? x + 16 * 128 : w;           /* u16 lanes: 128 steps of at most 2 x 128 */
        for (; x + 16 <= e; x += 16) {
            uint8x16_t s = vld1q_u8(q + x), pr;
            if (k == 0) pr = kv_med(vld1q_u8(q + x - 1), vld1q_u8(u + x), vld1q_u8(u + x - 1));
            else if (k == 1) pr = vld1q_u8(pq + x);
            else {
                uint8x16_t dl = veorq_u8(vsubq_u8(vld1q_u8(q + x - 1), vld1q_u8(pq + x - 1)), b80);
                uint8x16_t du = veorq_u8(vsubq_u8(vld1q_u8(u + x), vld1q_u8(pu + x)), b80);
                uint8x16_t dul = veorq_u8(vsubq_u8(vld1q_u8(u + x - 1), vld1q_u8(pu + x - 1)), b80);
                pr = vaddq_u8(vld1q_u8(pq + x), veorq_u8(kv_med(dl, du, dul), b80));
            }
            uint8x16_t r = vsubq_u8(s, pr);
            if (res) vst1q_u8(res + y * w + x, veorq_u8(vshlq_n_u8(r, 1), vreinterpretq_u8_s8(vshrq_n_s8(vreinterpretq_s8_u8(r), 7))));
            else acc = vpadalq_u8(acc, vreinterpretq_u8_s8(vabsq_s8(vreinterpretq_s8_u8(r))));   /* |-128| reads as 128 */
        }
        c += vaddlvq_u16(acc);
    }
    for (; x < w; x++) {
        size_t i = y * w + x; int8_t r = (int8_t)(uint8_t)(cur[i] - kv_pred(k, cur, prev, w, i));
        if (res) res[i] = (uint8_t)(((uint8_t)r << 1) ^ (uint8_t)(r >> 7)); else c += (uint64_t)(r < 0 ? -r : r);
    }
    return c;
}
void kolmo_kvid_add_neon(uint8_t *cur, const uint8_t *prev, const uint8_t *res, size_t n) {
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        uint8x16_t z = vld1q_u8(res + i);
        vst1q_u8(cur + i, vaddq_u8(vld1q_u8(prev + i), veorq_u8(vshrq_n_u8(z, 1), vtstq_u8(z, vdupq_n_u8(1)))));
    }
    for (; i < n; i++) { uint8_t z = res[i]; cur[i] = (uint8_t)(prev[i] + ((z >> 1) ^ (uint8_t)-(int)(z & 1))); }
}
#endif
