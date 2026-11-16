/* NEON: image prediction kernels (C references in src/kgraph.inc and src/sci.inc).
 *
 * kolmo_img8_back_neon - inverse of the IMG8 node (kg_img8_back): byte x of row y is the median predictor
 *   MED(left, above, above-left) plus a residual, so every byte waits for its left neighbour and the scalar loop is a
 *   chain of compares. Row y needs only row y - 1 (columns x and x - C) and its own earlier bytes, so 16 rows run as a
 *   wavefront: lane l holds row y0 + l, one column behind lane l - 1. A 16 x 16 tile of residuals is read along the
 *   skew and transposed, 16 steps of byte arithmetic decode one column of 16 rows each (min, max and saturating
 *   add/subtract give the median exactly), and the tile is transposed back. The ragged ends of each band of 16 rows,
 *   row 0 and the rows after the last band run the reference code.
 * kolmo_img8_fwd_neon  - forward IMG8 residuals (kg_img8_fwd), 16 bytes per step.
 * kolmo_img16_fwd_neon - forward IMG16 residuals (img16_fwd) from sample W + 1 on, 16 samples per step.
 * kolmo_sad_u16_neon   - sum of |x[i * step] - y[i * step]| over 16-bit samples, step 1 or 4 (img16_detect).
 *
 * Median identity: for bytes L, U, UL, clamp(L + U - UL, min(L, U), max(L, U)) is the reference's median; the sum is
 * formed with saturation, (L +sat (U -sat UL)) -sat (UL -sat U), which clamps it to [0, 255] first and so does not
 * change the result. */
#include "../arch.h"
#if KOLMO_NEON
#include <arm_neon.h>

#define AI static inline __attribute__((always_inline))

static inline uint8_t ib_med(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b, g = a + b - c; g = g < mn ? mn : g; return (uint8_t)(g > mx ? mx : g);
}
static inline uint8_t ib_unzz(uint8_t z) { return (uint8_t)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }
static inline uint8_t ib_zz(uint8_t d) { int8_t v = (int8_t)d; return (uint8_t)(((unsigned)v << 1) ^ (unsigned)(v >> 7)); }
/* the reference for columns [x0, x1) of row y >= 1 */
static inline void ib_row(const uint8_t *r, size_t W, unsigned C, uint8_t *p, size_t y, size_t x0, size_t x1) {
    uint8_t *q = p + y * W; const uint8_t *u = q - W, *o = r + y * W;
    for (size_t x = x0; x < x1; x++) q[x] = (uint8_t)((x < C ? u[x] : ib_med(q[x - C], u[x], u[x - C])) + ib_unzz(o[x]));
}

/* in-place 16 x 16 byte transpose: x[r] lane c -> x[c] lane r */
AI void tr16(uint8x16_t *x) {
    uint8x16_t a[16]; uint16x8_t b[16]; uint32x4_t c[16];
    for (int r = 0; r < 16; r += 2) { a[r] = vtrn1q_u8(x[r], x[r + 1]); a[r + 1] = vtrn2q_u8(x[r], x[r + 1]); }
    for (int r = 0; r < 16; r += 4) {
        uint16x8_t a0 = vreinterpretq_u16_u8(a[r]), a1 = vreinterpretq_u16_u8(a[r + 1]), a2 = vreinterpretq_u16_u8(a[r + 2]), a3 = vreinterpretq_u16_u8(a[r + 3]);
        b[r] = vtrn1q_u16(a0, a2); b[r + 2] = vtrn2q_u16(a0, a2); b[r + 1] = vtrn1q_u16(a1, a3); b[r + 3] = vtrn2q_u16(a1, a3);
    }
    for (int r = 0; r < 16; r += 8)
        for (int o = 0; o < 4; o++) {
            uint32x4_t u = vreinterpretq_u32_u16(b[r + o]), v = vreinterpretq_u32_u16(b[r + 4 + o]);
            c[r + o] = vtrn1q_u32(u, v); c[r + 4 + o] = vtrn2q_u32(u, v);
        }
    for (int j = 0; j < 8; j++) {
        uint64x2_t u = vreinterpretq_u64_u32(c[j]), v = vreinterpretq_u64_u32(c[8 + j]);
        x[j] = vreinterpretq_u8_u64(vtrn1q_u64(u, v)); x[8 + j] = vreinterpretq_u8_u64(vtrn2q_u64(u, v));
    }
}
AI uint8x16_t med16(uint8x16_t L, uint8x16_t U, uint8x16_t UL) {
    uint8x16_t g = vqsubq_u8(vqaddq_u8(L, vqsubq_u8(U, UL)), vqsubq_u8(UL, U));
    return vminq_u8(vmaxq_u8(g, vminq_u8(L, U)), vmaxq_u8(L, U));
}

/* rows y0 .. y0 + 15 (y0 >= 1), W >= 64 */
AI void img8_band(const uint8_t *r, size_t W, const unsigned C, uint8_t *p, size_t y0) {
    const size_t S0 = 16 + C;                     /* first vector step: lane l is at column S0 - l >= C + 1 */
    for (size_t l = 0; l < 16; l++) ib_row(r, W, C, p, y0 + l, 0, S0 - l);
    uint8x16_t T[21];                            /* T[5 + j] = step s0 + j; T[0..4]: the five steps before */
    for (int k = 0; k < 5; k++) {
        uint8_t g[16]; size_t t = S0 - 5 + (size_t)k;
        for (size_t l = 0; l < 16; l++) g[l] = t >= l ? p[(y0 + l) * W + t - l] : 0;   /* (steps before S0 - 1 - C are not read) */
        T[k] = vld1q_u8(g);
    }
    const uint8_t *pa = p + (y0 - 1) * W;
    size_t s0 = S0;
    for (; s0 + 16 <= W; s0 += 16) {
        uint8x16_t R[16];
        for (int l = 0; l < 16; l++) R[l] = vld1q_u8(r + (y0 + (size_t)l) * W + s0 - (size_t)l);
        tr16(R);                                  /* R[j] lane l: residual of row l at column s0 + j - l */
        uint8x16_t Ar = vextq_u8(vld1q_u8(pa + s0), vld1q_u8(pa + s0), 1);         /* lane 15: the row above, column s0 + j */
        uint8x16_t ALr = vextq_u8(vld1q_u8(pa + s0 - C), vld1q_u8(pa + s0 - C), 1);
#pragma clang loop unroll(full)
        for (int j = 0; j < 16; j++) {
            uint8x16_t U = vextq_u8(Ar, T[4 + j], 15), UL = vextq_u8(ALr, T[4 + j - C], 15), L = T[5 + j - C];
            uint8x16_t z = R[j], res = veorq_u8(vshrq_n_u8(z, 1), vtstq_u8(z, vdupq_n_u8(1)));
            T[5 + j] = vaddq_u8(med16(L, U, UL), res);
            Ar = vextq_u8(Ar, Ar, 1); ALr = vextq_u8(ALr, ALr, 1);
        }
        uint8x16_t X[16];
        for (int j = 0; j < 16; j++) X[j] = T[5 + j];
        tr16(X);                                  /* X[l]: row l, columns s0 - l .. s0 - l + 15 */
        for (int l = 0; l < 16; l++) vst1q_u8(p + (y0 + (size_t)l) * W + s0 - (size_t)l, X[l]);
        for (int k = 0; k < 5; k++) T[k] = T[16 + k];
    }
    for (size_t l = 0; l < 16; l++) ib_row(r, W, C, p, y0 + l, s0 - l, W);
}
int kolmo_img8_back_neon(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p) {
    if (W < 64 || C < 1 || C > 4) return 0;
    size_t x = 0, H = m / W, y = 1;
    for (; x < C && x < m; x++) p[x] = ib_unzz(r[x]);
    for (; x < W && x < m; x++) p[x] = (uint8_t)(p[x - C] + ib_unzz(r[x]));
    for (; y + 16 <= H; y += 16)
        switch (C) { case 1: img8_band(r, W, 1, p, y); break; case 2: img8_band(r, W, 2, p, y); break;
                     case 3: img8_band(r, W, 3, p, y); break; default: img8_band(r, W, 4, p, y); }
    for (; y < H; y++) ib_row(r, W, C, p, y, 0, W);
    return 1;
}

/* forward: residual bytes of rows y >= 1, columns x >= C */
int kolmo_img8_fwd_neon(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r) {
    if (W < 32 || C < 1 || C > 4) return 0;
    size_t x = 0;
    for (; x < C && x < m; x++) r[x] = ib_zz(p[x]);
    for (; x < W && x < m; x++) r[x] = ib_zz((uint8_t)(p[x] - p[x - C]));
    for (size_t y = W; y + W <= m; y += W) {
        const uint8_t *q = p + y, *u = q - W; uint8_t *o = r + y;
        for (x = 0; x < C; x++) o[x] = ib_zz((uint8_t)(q[x] - u[x]));
        for (; x + 16 <= W; x += 16) {
            uint8x16_t d = vsubq_u8(vld1q_u8(q + x), med16(vld1q_u8(q + x - C), vld1q_u8(u + x), vld1q_u8(u + x - C)));
            vst1q_u8(o + x, veorq_u8(vshlq_n_u8(d, 1), vreinterpretq_u8_s8(vshrq_n_s8(vreinterpretq_s8_u8(d), 7))));
        }
        for (; x < W; x++) o[x] = ib_zz((uint8_t)(q[x] - ib_med(q[x - C], u[x], u[x - C])));
    }
    return 1;
}

/* forward IMG16 from sample i (> W); returns where it stopped (the reference finishes) */
int64_t kolmo_img16_fwd_neon(const uint16_t *a, int64_t i, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi) {
    for (; i + 16 <= n; i += 16) {
        uint16x8_t z[2];
        for (int h = 0; h < 2; h++) {
            const uint16_t *s = a + i + 8 * h;
            uint16x8_t x = vld1q_u16(s), L = vld1q_u16(s - 1), pr;
            if (mode == 0) pr = L;
            else if (mode == 1) pr = vhaddq_u16(L, vld1q_u16(s - W));
            else {
                uint16x8_t U = vld1q_u16(s - W), UL = vld1q_u16(s - W - 1);
                uint16x8_t g = vqsubq_u16(vqaddq_u16(L, vqsubq_u16(U, UL)), vqsubq_u16(UL, U));
                pr = vminq_u16(vmaxq_u16(g, vminq_u16(L, U)), vmaxq_u16(L, U));
            }
            uint16x8_t d = vsubq_u16(x, pr);
            z[h] = veorq_u16(vshlq_n_u16(d, 1), vreinterpretq_u16_s16(vshrq_n_s16(vreinterpretq_s16_u16(d), 15)));
        }
        vst1q_u8(lo + i, vuzp1q_u8(vreinterpretq_u8_u16(z[0]), vreinterpretq_u8_u16(z[1])));
        vst1q_u8(hi + i, vuzp2q_u8(vreinterpretq_u8_u16(z[0]), vreinterpretq_u8_u16(z[1])));
    }
    return i;
}

uint64_t kolmo_sad_u16_neon(const uint16_t *x, const uint16_t *y, int64_t n, int step) {
    uint64_t s = 0; int64_t i = 0, nv = step == 1 ? n : n - 1;   /* step 4: loads reach 3 samples past the last one used */
    while (i + 16 <= nv) {                       /* lanes take at most 2^15 pairs of 16-bit differences */
        uint32x4_t a0 = vdupq_n_u32(0), a1 = vdupq_n_u32(0);
        int64_t lim = nv - i < (1 << 15) ? nv - i : (1 << 15);
        int64_t e = i + (lim & ~(int64_t)15);
        if (step == 1)
            for (; i < e; i += 16) {
                a0 = vpadalq_u16(a0, vabdq_u16(vld1q_u16(x + i), vld1q_u16(y + i)));
                a1 = vpadalq_u16(a1, vabdq_u16(vld1q_u16(x + i + 8), vld1q_u16(y + i + 8)));
            }
        else
            for (; i < e; i += 16) {                   /* 16 counted samples are 64 positions */
                a0 = vpadalq_u16(a0, vabdq_u16(vld4q_u16(x + 4 * i).val[0], vld4q_u16(y + 4 * i).val[0]));
                a1 = vpadalq_u16(a1, vabdq_u16(vld4q_u16(x + 4 * i + 32).val[0], vld4q_u16(y + 4 * i + 32).val[0]));
            }
        s += vaddlvq_u32(a0) + vaddlvq_u32(a1);
    }
    for (; i < n; i++) { int d = (int)x[i * step] - (int)y[i * step]; s += (uint64_t)(d < 0 ? -d : d); }
    return s;
}
#endif
