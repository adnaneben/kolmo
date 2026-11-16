/* NEON: byte-plane split and merge with optional per-channel delta (C reference: shuffle_fwd / shuffle_inv in
 * src/kolmo.c). Rows of c channels of w-byte little-endian words; plane (b, ch) holds byte b of channel ch's words
 * (or of their differences, mod 2^(8w)). Words of 2, 4 and 8 bytes and 1 to 4 channels are handled here, 16 rows
 * per step: LD1-LD4 separate the channels, the delta is a lane shift and a subtract (the merge: an in-register
 * prefix sum), and the planes come out of UZP (go back in with ZIP). Other shapes return 0 and take the reference. */
#include "../arch.h"
#if KOLMO_NEON
#include <arm_neon.h>
#include <string.h>

#define AI static inline __attribute__((always_inline))

/* ---- generic scalar rows (the tail after the last 16-row step) */
static void shuf_rows_fwd(const uint8_t *in, int64_t r0, int64_t rows, int w, int c, int delta, uint64_t *prev, uint8_t *out) {
    uint64_t mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r = r0; r < rows; r++)
        for (int ch = 0; ch < c; ch++) {
            uint64_t v = 0; memcpy(&v, in + (r * c + ch) * w, (size_t)w);
            uint64_t d = delta ? ((v - prev[ch]) & mask) : v; prev[ch] = v;
            for (int b = 0; b < w; b++) out[((int64_t)b * c + ch) * rows + r] = (uint8_t)(d >> (8 * b));
        }
}
static void shuf_rows_inv(const uint8_t *in, int64_t r0, int64_t rows, int w, int c, int delta, uint64_t *prev, uint8_t *out) {
    uint64_t mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r = r0; r < rows; r++)
        for (int ch = 0; ch < c; ch++) {
            uint64_t t = 0;
            for (int b = 0; b < w; b++) t |= (uint64_t)in[((int64_t)b * c + ch) * rows + r] << (8 * b);
            uint64_t v = delta ? ((prev[ch] + t) & mask) : t; prev[ch] = v;
            memcpy(out + (r * c + ch) * w, &v, (size_t)w);
        }
}

/* ---- channel loads and stores: X[ch] = lanes of channel ch for consecutive rows */
#define LDC(T, S, p, c, X) do { \
    if (c == 1) X[0] = vld1q_##S(p); \
    else if (c == 2) { T##x2_t t = vld2q_##S(p); X[0] = t.val[0]; X[1] = t.val[1]; } \
    else if (c == 3) { T##x3_t t = vld3q_##S(p); X[0] = t.val[0]; X[1] = t.val[1]; X[2] = t.val[2]; } \
    else { T##x4_t t = vld4q_##S(p); X[0] = t.val[0]; X[1] = t.val[1]; X[2] = t.val[2]; X[3] = t.val[3]; } } while (0)
#define STC(T, S, p, c, X) do { \
    if (c == 1) vst1q_##S(p, X[0]); \
    else if (c == 2) vst2q_##S(p, ((T##x2_t){{X[0], X[1]}})); \
    else if (c == 3) vst3q_##S(p, ((T##x3_t){{X[0], X[1], X[2]}})); \
    else vst4q_##S(p, ((T##x4_t){{X[0], X[1], X[2], X[3]}})); } while (0)

/* ---- 16-bit words: 8 rows per vector */
AI void fwd2(const uint8_t *in, int64_t rows, const int c, int delta, uint64_t *prev, uint8_t *out, int64_t R) {
    uint16x8_t P[4];
    for (int ch = 0; ch < c; ch++) P[ch] = vdupq_n_u16((uint16_t)prev[ch]);
    for (int64_t r = 0; r < R; r += 16) {
        uint16x8_t A[4], B[4];
        LDC(uint16x8, u16, (const uint16_t *)(const void *)(in + r * 2 * c), c, A);
        LDC(uint16x8, u16, (const uint16_t *)(const void *)(in + (r + 8) * 2 * c), c, B);
        for (int ch = 0; ch < c; ch++) {
            uint16x8_t a = A[ch], b = B[ch];
            if (delta) { uint16x8_t da = vsubq_u16(a, vextq_u16(P[ch], a, 7)), db = vsubq_u16(b, vextq_u16(a, b, 7)); P[ch] = b; a = da; b = db; }
            uint8x16_t x = vreinterpretq_u8_u16(a), y = vreinterpretq_u8_u16(b);
            vst1q_u8(out + (int64_t)ch * rows + r, vuzp1q_u8(x, y));
            vst1q_u8(out + (int64_t)(c + ch) * rows + r, vuzp2q_u8(x, y));
        }
    }
    if (delta) for (int ch = 0; ch < c; ch++) prev[ch] = vgetq_lane_u16(P[ch], 7);
}
AI uint16x8_t pfx16(uint16x8_t x, uint16x8_t carry) {
    const uint16x8_t z = vdupq_n_u16(0);
    x = vaddq_u16(x, vextq_u16(z, x, 7)); x = vaddq_u16(x, vextq_u16(z, x, 6)); x = vaddq_u16(x, vextq_u16(z, x, 4));
    return vaddq_u16(x, vdupq_laneq_u16(carry, 7));
}
AI void inv2(const uint8_t *in, int64_t rows, const int c, int delta, uint64_t *prev, uint8_t *out, int64_t R) {
    uint16x8_t P[4];
    for (int ch = 0; ch < c; ch++) P[ch] = vdupq_n_u16((uint16_t)prev[ch]);
    for (int64_t r = 0; r < R; r += 16) {
        uint16x8_t A[4], B[4];
        for (int ch = 0; ch < c; ch++) {
            uint8x16_t lo = vld1q_u8(in + (int64_t)ch * rows + r), hi = vld1q_u8(in + (int64_t)(c + ch) * rows + r);
            uint16x8_t a = vreinterpretq_u16_u8(vzip1q_u8(lo, hi)), b = vreinterpretq_u16_u8(vzip2q_u8(lo, hi));
            if (delta) { a = pfx16(a, P[ch]); b = pfx16(b, a); P[ch] = b; }
            A[ch] = a; B[ch] = b;
        }
        STC(uint16x8, u16, (uint16_t *)(void *)(out + r * 2 * c), c, A);
        STC(uint16x8, u16, (uint16_t *)(void *)(out + (r + 8) * 2 * c), c, B);
    }
    if (delta) for (int ch = 0; ch < c; ch++) prev[ch] = vgetq_lane_u16(P[ch], 7);
}

/* ---- 32-bit words: 4 rows per vector */
AI void fwd4(const uint8_t *in, int64_t rows, const int c, int delta, uint64_t *prev, uint8_t *out, int64_t R) {
    uint32x4_t P[4];
    for (int ch = 0; ch < c; ch++) P[ch] = vdupq_n_u32((uint32_t)prev[ch]);
    for (int64_t r = 0; r < R; r += 16) {
        uint32x4_t V[4][4];
        for (int q = 0; q < 4; q++) { uint32x4_t X[4]; LDC(uint32x4, u32, (const uint32_t *)(const void *)(in + (r + 4 * q) * 4 * c), c, X); for (int ch = 0; ch < c; ch++) V[ch][q] = X[ch]; }
        for (int ch = 0; ch < c; ch++) {
            uint32x4_t a = V[ch][0], b = V[ch][1], d = V[ch][2], e = V[ch][3];
            if (delta) {
                uint32x4_t da = vsubq_u32(a, vextq_u32(P[ch], a, 3)), db = vsubq_u32(b, vextq_u32(a, b, 3));
                uint32x4_t dd = vsubq_u32(d, vextq_u32(b, d, 3)), de = vsubq_u32(e, vextq_u32(d, e, 3));
                P[ch] = e; a = da; b = db; d = dd; e = de;
            }
            uint8x16_t t0 = vuzp1q_u8(vreinterpretq_u8_u32(a), vreinterpretq_u8_u32(b)), t1 = vuzp2q_u8(vreinterpretq_u8_u32(a), vreinterpretq_u8_u32(b));
            uint8x16_t t2 = vuzp1q_u8(vreinterpretq_u8_u32(d), vreinterpretq_u8_u32(e)), t3 = vuzp2q_u8(vreinterpretq_u8_u32(d), vreinterpretq_u8_u32(e));
            vst1q_u8(out + (int64_t)ch * rows + r, vuzp1q_u8(t0, t2));
            vst1q_u8(out + (int64_t)(c + ch) * rows + r, vuzp1q_u8(t1, t3));
            vst1q_u8(out + (int64_t)(2 * c + ch) * rows + r, vuzp2q_u8(t0, t2));
            vst1q_u8(out + (int64_t)(3 * c + ch) * rows + r, vuzp2q_u8(t1, t3));
        }
    }
    if (delta) for (int ch = 0; ch < c; ch++) prev[ch] = vgetq_lane_u32(P[ch], 3);
}
AI uint32x4_t pfx32(uint32x4_t x, uint32x4_t carry) {
    const uint32x4_t z = vdupq_n_u32(0);
    x = vaddq_u32(x, vextq_u32(z, x, 3)); x = vaddq_u32(x, vextq_u32(z, x, 2));
    return vaddq_u32(x, vdupq_laneq_u32(carry, 3));
}
AI void inv4(const uint8_t *in, int64_t rows, const int c, int delta, uint64_t *prev, uint8_t *out, int64_t R) {
    uint32x4_t P[4];
    for (int ch = 0; ch < c; ch++) P[ch] = vdupq_n_u32((uint32_t)prev[ch]);
    for (int64_t r = 0; r < R; r += 16) {
        uint32x4_t V[4][4];
        for (int ch = 0; ch < c; ch++) {
            uint8x16_t p0 = vld1q_u8(in + (int64_t)ch * rows + r), p1 = vld1q_u8(in + (int64_t)(c + ch) * rows + r);
            uint8x16_t p2 = vld1q_u8(in + (int64_t)(2 * c + ch) * rows + r), p3 = vld1q_u8(in + (int64_t)(3 * c + ch) * rows + r);
            uint16x8_t t0 = vreinterpretq_u16_u8(vzip1q_u8(p0, p1)), t1 = vreinterpretq_u16_u8(vzip2q_u8(p0, p1));
            uint16x8_t t2 = vreinterpretq_u16_u8(vzip1q_u8(p2, p3)), t3 = vreinterpretq_u16_u8(vzip2q_u8(p2, p3));
            uint32x4_t a = vreinterpretq_u32_u16(vzip1q_u16(t0, t2)), b = vreinterpretq_u32_u16(vzip2q_u16(t0, t2));
            uint32x4_t d = vreinterpretq_u32_u16(vzip1q_u16(t1, t3)), e = vreinterpretq_u32_u16(vzip2q_u16(t1, t3));
            if (delta) { a = pfx32(a, P[ch]); b = pfx32(b, a); d = pfx32(d, b); e = pfx32(e, d); P[ch] = e; }
            V[ch][0] = a; V[ch][1] = b; V[ch][2] = d; V[ch][3] = e;
        }
        for (int q = 0; q < 4; q++) { uint32x4_t X[4]; for (int ch = 0; ch < c; ch++) X[ch] = V[ch][q]; STC(uint32x4, u32, (uint32_t *)(void *)(out + (r + 4 * q) * 4 * c), c, X); }
    }
    if (delta) for (int ch = 0; ch < c; ch++) prev[ch] = vgetq_lane_u32(P[ch], 3);
}

/* ---- 64-bit words: 2 rows per vector */
AI void fwd8(const uint8_t *in, int64_t rows, const int c, int delta, uint64_t *prev, uint8_t *out, int64_t R) {
    uint64x2_t P[4];
    for (int ch = 0; ch < c; ch++) P[ch] = vdupq_n_u64(prev[ch]);
    for (int64_t r = 0; r < R; r += 16) {
        uint64x2_t V[4][8];
        for (int q = 0; q < 8; q++) { uint64x2_t X[4]; LDC(uint64x2, u64, (const uint64_t *)(const void *)(in + (r + 2 * q) * 8 * c), c, X); for (int ch = 0; ch < c; ch++) V[ch][q] = X[ch]; }
        for (int ch = 0; ch < c; ch++) {
            uint8x16_t x[8];
            uint64x2_t pv = P[ch];
            for (int q = 0; q < 8; q++) {
                uint64x2_t v = V[ch][q];
                if (delta) { uint64x2_t d = vsubq_u64(v, vextq_u64(pv, v, 1)); pv = v; v = d; }
                x[q] = vreinterpretq_u8_u64(v);
            }
            P[ch] = pv;
            uint8x16_t s0 = vuzp1q_u8(x[0], x[1]), s1 = vuzp2q_u8(x[0], x[1]), s2 = vuzp1q_u8(x[2], x[3]), s3 = vuzp2q_u8(x[2], x[3]);
            uint8x16_t s4 = vuzp1q_u8(x[4], x[5]), s5 = vuzp2q_u8(x[4], x[5]), s6 = vuzp1q_u8(x[6], x[7]), s7 = vuzp2q_u8(x[6], x[7]);
            /* s0, s2, s4, s6: even bytes (0 2 4 6); s1, s3, s5, s7: odd bytes */
            uint8x16_t e0 = vuzp1q_u8(s0, s2), e1 = vuzp2q_u8(s0, s2), e2 = vuzp1q_u8(s4, s6), e3 = vuzp2q_u8(s4, s6);   /* bytes 0 4 / 2 6 */
            uint8x16_t o0 = vuzp1q_u8(s1, s3), o1 = vuzp2q_u8(s1, s3), o2 = vuzp1q_u8(s5, s7), o3 = vuzp2q_u8(s5, s7);   /* bytes 1 5 / 3 7 */
            vst1q_u8(out + (int64_t)(0 * c + ch) * rows + r, vuzp1q_u8(e0, e2));
            vst1q_u8(out + (int64_t)(4 * c + ch) * rows + r, vuzp2q_u8(e0, e2));
            vst1q_u8(out + (int64_t)(2 * c + ch) * rows + r, vuzp1q_u8(e1, e3));
            vst1q_u8(out + (int64_t)(6 * c + ch) * rows + r, vuzp2q_u8(e1, e3));
            vst1q_u8(out + (int64_t)(1 * c + ch) * rows + r, vuzp1q_u8(o0, o2));
            vst1q_u8(out + (int64_t)(5 * c + ch) * rows + r, vuzp2q_u8(o0, o2));
            vst1q_u8(out + (int64_t)(3 * c + ch) * rows + r, vuzp1q_u8(o1, o3));
            vst1q_u8(out + (int64_t)(7 * c + ch) * rows + r, vuzp2q_u8(o1, o3));
        }
    }
    if (delta) for (int ch = 0; ch < c; ch++) prev[ch] = vgetq_lane_u64(P[ch], 1);
}
AI void inv8(const uint8_t *in, int64_t rows, const int c, int delta, uint64_t *prev, uint8_t *out, int64_t R) {
    uint64x2_t P[4];
    for (int ch = 0; ch < c; ch++) P[ch] = vdupq_n_u64(prev[ch]);
    for (int64_t r = 0; r < R; r += 16) {
        uint64x2_t V[4][8];
        for (int ch = 0; ch < c; ch++) {
            uint8x16_t p[8];
            for (int b = 0; b < 8; b++) p[b] = vld1q_u8(in + (int64_t)(b * c + ch) * rows + r);
            uint8x16_t a0 = vzip1q_u8(p[0], p[1]), a1 = vzip2q_u8(p[0], p[1]), a2 = vzip1q_u8(p[2], p[3]), a3 = vzip2q_u8(p[2], p[3]);
            uint8x16_t a4 = vzip1q_u8(p[4], p[5]), a5 = vzip2q_u8(p[4], p[5]), a6 = vzip1q_u8(p[6], p[7]), a7 = vzip2q_u8(p[6], p[7]);
            /* a0/a1: byte pairs 01 of rows 0-7 / 8-15; a2/a3: 23; a4/a5: 45; a6/a7: 67 */
            uint16x8_t b0 = vzip1q_u16(vreinterpretq_u16_u8(a0), vreinterpretq_u16_u8(a2)), b1 = vzip2q_u16(vreinterpretq_u16_u8(a0), vreinterpretq_u16_u8(a2));
            uint16x8_t b2 = vzip1q_u16(vreinterpretq_u16_u8(a1), vreinterpretq_u16_u8(a3)), b3 = vzip2q_u16(vreinterpretq_u16_u8(a1), vreinterpretq_u16_u8(a3));
            uint16x8_t b4 = vzip1q_u16(vreinterpretq_u16_u8(a4), vreinterpretq_u16_u8(a6)), b5 = vzip2q_u16(vreinterpretq_u16_u8(a4), vreinterpretq_u16_u8(a6));
            uint16x8_t b6 = vzip1q_u16(vreinterpretq_u16_u8(a5), vreinterpretq_u16_u8(a7)), b7 = vzip2q_u16(vreinterpretq_u16_u8(a5), vreinterpretq_u16_u8(a7));
            /* b0: bytes 0-3 of rows 0-3, b1: rows 4-7, b2: rows 8-11, b3: rows 12-15; b4-b7: bytes 4-7 */
            uint32x4_t lo[4] = {vreinterpretq_u32_u16(b0), vreinterpretq_u32_u16(b1), vreinterpretq_u32_u16(b2), vreinterpretq_u32_u16(b3)};
            uint32x4_t hi[4] = {vreinterpretq_u32_u16(b4), vreinterpretq_u32_u16(b5), vreinterpretq_u32_u16(b6), vreinterpretq_u32_u16(b7)};
            uint64x2_t pv = P[ch];
            for (int q = 0; q < 4; q++) {
                uint64x2_t v0 = vreinterpretq_u64_u32(vzip1q_u32(lo[q], hi[q])), v1 = vreinterpretq_u64_u32(vzip2q_u32(lo[q], hi[q]));
                if (delta) {
                    v0 = vaddq_u64(vaddq_u64(v0, vextq_u64(vdupq_n_u64(0), v0, 1)), vdupq_laneq_u64(pv, 1));
                    v1 = vaddq_u64(vaddq_u64(v1, vextq_u64(vdupq_n_u64(0), v1, 1)), vdupq_laneq_u64(v0, 1));
                    pv = v1;
                }
                V[ch][2 * q] = v0; V[ch][2 * q + 1] = v1;
            }
            P[ch] = pv;
        }
        for (int q = 0; q < 8; q++) { uint64x2_t X[4]; for (int ch = 0; ch < c; ch++) X[ch] = V[ch][q]; STC(uint64x2, u64, (uint64_t *)(void *)(out + (r + 2 * q) * 8 * c), c, X); }
    }
    if (delta) for (int ch = 0; ch < c; ch++) prev[ch] = vgetq_lane_u64(P[ch], 1);
}

#define DISPATCH(F, c) do { switch (c) { case 1: F(in, rows, 1, delta, prev, out, R); break; case 2: F(in, rows, 2, delta, prev, out, R); break; \
                                         case 3: F(in, rows, 3, delta, prev, out, R); break; default: F(in, rows, 4, delta, prev, out, R); } } while (0)
int kolmo_shuffle_fwd_neon(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out) {
    if ((w != 2 && w != 4 && w != 8) || c < 1 || c > 4) return 0;
    uint64_t prev[4] = {0}; int64_t R = rows & ~(int64_t)15;
    if (w == 2) DISPATCH(fwd2, c); else if (w == 4) DISPATCH(fwd4, c); else DISPATCH(fwd8, c);
    shuf_rows_fwd(in, R, rows, w, c, delta, prev, out);
    return 1;
}
int kolmo_shuffle_inv_neon(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out) {
    if ((w != 2 && w != 4 && w != 8) || c < 1 || c > 4) return 0;
    uint64_t prev[4] = {0}; int64_t R = rows & ~(int64_t)15;
    if (w == 2) DISPATCH(inv2, c); else if (w == 4) DISPATCH(inv4, c); else DISPATCH(inv8, c);
    shuf_rows_inv(in, R, rows, w, c, delta, prev, out);
    return 1;
}
#undef DISPATCH
#endif
