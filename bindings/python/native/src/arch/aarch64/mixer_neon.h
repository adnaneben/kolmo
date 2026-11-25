/* AArch64 baseline NEON for the 16-bit model-4 mixer. Included by arch/cpu.c.
 * Apache-2.0, Kolmo contributors. */
#ifndef KOLMO_MIXER_NEON_H
#define KOLMO_MIXER_NEON_H
#include <arm_neon.h>

static inline int32x4_t cm_dot16_pairs(int16x8_t x, int16x8_t w) {
    int32x4_t lo = vmull_s16(vget_low_s16(x), vget_low_s16(w));
    int32x4_t hi = vmull_high_s16(x, w);
    return vpaddq_s32(lo, hi);
}

static void kolmo_cm_dot16_neon(const int16_t *x, int n, const int16_t *w1, const int16_t *w2,
                                const int16_t *w3, const int16_t *w4, int32_t s[4]) {
    int32x4_t a1 = vdupq_n_s32(0), a2 = a1, a3 = a1, a4 = a1;
    for (int i = 0; i < n; i += 8) {
        int16x8_t in = vld1q_s16(x + i);
        /* Shift after adding the two products in each pair, as in the scalar model. */
        a1 = vsraq_n_s32(a1, cm_dot16_pairs(in, vld1q_s16(w1 + i)), 8);
        a2 = vsraq_n_s32(a2, cm_dot16_pairs(in, vld1q_s16(w2 + i)), 8);
        a3 = vsraq_n_s32(a3, cm_dot16_pairs(in, vld1q_s16(w3 + i)), 8);
        a4 = vsraq_n_s32(a4, cm_dot16_pairs(in, vld1q_s16(w4 + i)), 8);
    }
    int32x4_t p12 = vpaddq_s32(a1, a2), p34 = vpaddq_s32(a3, a4);
    vst1q_s32(s, vpaddq_s32(p12, p34));
}

static inline void cm_train16_update(int16_t *w, int16x8_t x, int16x8_t e) {
    /* The model keeps |e| < 32768, so the rounded product fits before the saturating add. */
    int16x8_t delta = vqrdmulhq_s16(x, e);
    vst1q_s16(w, vqaddq_s16(vld1q_s16(w), delta));
}

static void kolmo_cm_train16_neon(const int16_t *x, int n, int16_t *w1, int16_t *w2,
                                  int16_t *w3, int16_t *w4, const int e[4]) {
    const int16x8_t e1 = vdupq_n_s16((int16_t)e[0]), e2 = vdupq_n_s16((int16_t)e[1]);
    const int16x8_t e3 = vdupq_n_s16((int16_t)e[2]), e4 = vdupq_n_s16((int16_t)e[3]);
    for (int i = 0; i < n; i += 8) {
        int16x8_t in = vld1q_s16(x + i);
        cm_train16_update(w1 + i, in, e1);
        cm_train16_update(w2 + i, in, e2);
        cm_train16_update(w3 + i, in, e3);
        cm_train16_update(w4 + i, in, e4);
    }
}
#endif
