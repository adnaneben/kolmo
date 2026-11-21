/* NEON: inverse of the DEC64 node for 8-byte values (C reference: KG_DEC_VALUE in kg_dec64_inv, src/kgraph.inc).
 * A value with tag 0 is rebuilt from its residual z (np byte planes): v = unzigzag(z) [+ the previous v when the
 * column is second-order], m = v [+ the previous m when it is first-order], and the 8 output bytes are m (integer
 * columns) or the bits of (double)m / 10^e. 16 values per step while all 16 tags are 0: planes zipped into 64-bit
 * lanes, the running sums as prefix sums of 2 lanes with a carry, the conversion and the division as FCVT/FDIV on
 * 2 lanes (IEEE division rounds the same in either unit). Values with tag 1 (the column's missing-value bits) and
 * tag 2 (raw bits from the exception stream, in order) do not move the running values: their lanes enter the sums
 * as 0, so each carry is the last tag-0 value as in the reference, and their output is written afterwards in stream
 * order. Dictionary columns and 4-byte columns stay with the reference. */
#include "../arch.h"
#if KOLMO_NEON
#include <arm_neon.h>
#include <string.h>

#define AI static inline __attribute__((always_inline))
AI uint64x2_t not64(uint64x2_t x) { return vreinterpretq_u64_u32(vmvnq_u32(vreinterpretq_u32_u64(x))); }
AI uint64x2_t pfx2(uint64x2_t x, uint64x2_t carry) {
    return vaddq_u64(vaddq_u64(x, vextq_u64(vdupq_n_u64(0), x, 1)), vdupq_laneq_u64(carry, 1));
}
static const uint64_t d64_tm[4][2] = {{0, 0}, {~0ull, 0}, {0, ~0ull}, {~0ull, ~0ull}};
static const uint8_t d64_bw[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
AI size_t d64_body(const uint8_t *tag, const uint8_t *const *pl, int np, size_t q, size_t n, double P, const int isint,
                   const int o1, const int o2, uint64_t *pprev, uint64_t *ppd, uint8_t *w, size_t step, uint64_t miss,
                   const uint8_t **pxp) {
    uint64x2_t cprev = vdupq_n_u64(*pprev), cpd = vdupq_n_u64(*ppd);
    const float64x2_t vP = vdupq_n_f64(P); const uint64x2_t one = vdupq_n_u64(1);
    const uint8_t *xp = *pxp; size_t k = 0;
    for (; k + 16 <= n; k += 16) {
        uint8x16_t tg = vld1q_u8(tag + q + k), tw = vandq_u8(vtstq_u8(tg, tg), vld1q_u8(d64_bw));
        unsigned tmask = vaddv_u8(vget_low_u8(tw)) | (unsigned)vaddv_u8(vget_high_u8(tw)) << 8;
        uint8x16_t p[8];
        for (int b = 0; b < 8; b++) p[b] = b < np ? vld1q_u8(pl[b] + q + k) : vdupq_n_u8(0);
        uint8x16_t a0 = vzip1q_u8(p[0], p[1]), a1 = vzip2q_u8(p[0], p[1]), a2 = vzip1q_u8(p[2], p[3]), a3 = vzip2q_u8(p[2], p[3]);
        uint8x16_t a4 = vzip1q_u8(p[4], p[5]), a5 = vzip2q_u8(p[4], p[5]), a6 = vzip1q_u8(p[6], p[7]), a7 = vzip2q_u8(p[6], p[7]);
        uint16x8_t b0 = vzip1q_u16(vreinterpretq_u16_u8(a0), vreinterpretq_u16_u8(a2)), b1 = vzip2q_u16(vreinterpretq_u16_u8(a0), vreinterpretq_u16_u8(a2));
        uint16x8_t b2 = vzip1q_u16(vreinterpretq_u16_u8(a1), vreinterpretq_u16_u8(a3)), b3 = vzip2q_u16(vreinterpretq_u16_u8(a1), vreinterpretq_u16_u8(a3));
        uint16x8_t b4 = vzip1q_u16(vreinterpretq_u16_u8(a4), vreinterpretq_u16_u8(a6)), b5 = vzip2q_u16(vreinterpretq_u16_u8(a4), vreinterpretq_u16_u8(a6));
        uint16x8_t b6 = vzip1q_u16(vreinterpretq_u16_u8(a5), vreinterpretq_u16_u8(a7)), b7 = vzip2q_u16(vreinterpretq_u16_u8(a5), vreinterpretq_u16_u8(a7));
        uint32x4_t lo[4] = {vreinterpretq_u32_u16(b0), vreinterpretq_u32_u16(b1), vreinterpretq_u32_u16(b2), vreinterpretq_u32_u16(b3)};
        uint32x4_t hi[4] = {vreinterpretq_u32_u16(b4), vreinterpretq_u32_u16(b5), vreinterpretq_u32_u16(b6), vreinterpretq_u32_u16(b7)};
        for (int j = 0; j < 8; j++) {
            uint64x2_t z = vreinterpretq_u64_u32(j & 1 ? vzip2q_u32(lo[j >> 1], hi[j >> 1]) : vzip1q_u32(lo[j >> 1], hi[j >> 1]));
            uint64x2_t v = veorq_u64(vshrq_n_u64(z, 1), vtstq_u64(z, one)), keep = vdupq_n_u64(~0ull);
            if (tmask) { keep = not64(vld1q_u64(d64_tm[tmask >> (2 * j) & 3])); v = vandq_u64(v, keep); }
            if (o2) v = pfx2(v, cpd);
            cpd = v;
            uint64x2_t m = o1 ? pfx2(vandq_u64(v, keep), cprev) : v;
            cprev = m;
            uint64x2_t bits = isint ? m : vreinterpretq_u64_f64(vdivq_f64(vcvtq_f64_s64(vreinterpretq_s64_u64(m)), vP));
            uint8_t *o = w + (k + 2 * (size_t)j) * step;
            if (step == 8) vst1q_u64((uint64_t *)(void *)o, bits);
            else { vst1q_lane_u64((uint64_t *)(void *)o, bits, 0); vst1q_lane_u64((uint64_t *)(void *)(o + step), bits, 1); }
        }
        while (tmask) {                              /* tagged values, in stream order */
            int b = __builtin_ctz(tmask); tmask &= tmask - 1;
            uint64_t bits; if (tag[q + k + (size_t)b] == 1) bits = miss; else { memcpy(&bits, xp, 8); xp += 8; }
            memcpy(w + (k + (size_t)b) * step, &bits, 8);
        }
    }
    if (k) { *pprev = vgetq_lane_u64(cprev, 1); *ppd = vgetq_lane_u64(cpd, 1); *pxp = xp; }
    return k;
}
size_t kolmo_dec64_run_neon(const uint8_t *tag, const uint8_t *const *pl, int np, size_t q, size_t n, double P, int isint,
                            uint64_t dmask, uint64_t d2mask, uint64_t *prev, uint64_t *pd, uint8_t *w, size_t step,
                            uint64_t miss, const uint8_t **xp) {
    if (np < 1 || np > 8 || n < 16) return 0;
#define d64_body(...) d64_body(__VA_ARGS__, miss, xp)
    switch ((isint ? 4 : 0) | (dmask ? 2 : 0) | (d2mask ? 1 : 0)) {
        case 0: return d64_body(tag, pl, np, q, n, P, 0, 0, 0, prev, pd, w, step);
        case 1: return d64_body(tag, pl, np, q, n, P, 0, 0, 1, prev, pd, w, step);
        case 2: return d64_body(tag, pl, np, q, n, P, 0, 1, 0, prev, pd, w, step);
        case 3: return d64_body(tag, pl, np, q, n, P, 0, 1, 1, prev, pd, w, step);
        case 4: return d64_body(tag, pl, np, q, n, P, 1, 0, 0, prev, pd, w, step);
        case 5: return d64_body(tag, pl, np, q, n, P, 1, 0, 1, prev, pd, w, step);
        case 6: return d64_body(tag, pl, np, q, n, P, 1, 1, 0, prev, pd, w, step);
        default: return d64_body(tag, pl, np, q, n, P, 1, 1, 1, prev, pd, w, step);
    }
#undef d64_body
}
#endif
