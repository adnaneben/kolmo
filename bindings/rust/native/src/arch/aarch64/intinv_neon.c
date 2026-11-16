/* NEON: inverse of the integer graph node for 32-bit elements (C reference: fi_inv_4_* and kg_int_inv in
 * src/kgraph.inc). The residual z of each value comes in ow byte planes (1, 2, 4 or 8); order 0 writes bias + z * g,
 * orders 1 and 2 add unzigzag(z) * g once or twice through running sums. Only the low 32 bits of each value are
 * stored and the reference's arithmetic is mod 2^64, so 32-bit lanes give the same bytes; the low 32 bits of
 * unzigzag(z) need bit 32 of z, which is bit 0 of plane 4. 16 values per step: the planes are zipped into 32-bit
 * lanes and the sums are in-register prefix sums with a carry lane (the rc2 SSE2/AVX2 loop, extended to every
 * residual width, order and factor). */
#include "../arch.h"
#if KOLMO_NEON
#include <arm_neon.h>
#include <string.h>

#define AI static inline __attribute__((always_inline))
AI uint32x4_t pfx4(uint32x4_t x) {
    const uint32x4_t z = vdupq_n_u32(0);
    x = vaddq_u32(x, vextq_u32(z, x, 3)); return vaddq_u32(x, vextq_u32(z, x, 2));
}

AI size_t ii_body(const uint8_t *r, size_t m, const int ow, const int ord, const int one, uint32_t G, uint32_t B,
                  uint32_t a, uint32_t b, uint8_t *o) {
    uint32x4_t c1 = vdupq_n_u32(a), ce = vdupq_n_u32(a - b);
    const uint32x4_t vB = vdupq_n_u32(B), one32 = vdupq_n_u32(1);
    size_t i = 0;
    for (; i + 16 <= m; i += 16) {
        uint8x16_t p0 = vld1q_u8(r + i), p1 = vdupq_n_u8(0), p2 = vdupq_n_u8(0), p3 = vdupq_n_u8(0), p4 = vdupq_n_u8(0);
        if (ow >= 2) p1 = vld1q_u8(r + m + i);
        if (ow >= 4) { p2 = vld1q_u8(r + 2 * m + i); p3 = vld1q_u8(r + 3 * m + i); }
        if (ow == 8 && ord) p4 = vandq_u8(vld1q_u8(r + 4 * m + i), vdupq_n_u8(1));
        uint16x8_t t0 = vreinterpretq_u16_u8(vzip1q_u8(p0, p1)), t1 = vreinterpretq_u16_u8(vzip2q_u8(p0, p1));
        uint16x8_t t2 = vreinterpretq_u16_u8(vzip1q_u8(p2, p3)), t3 = vreinterpretq_u16_u8(vzip2q_u8(p2, p3));
        uint32x4_t z[4] = {vreinterpretq_u32_u16(vzip1q_u16(t0, t2)), vreinterpretq_u32_u16(vzip2q_u16(t0, t2)),
                           vreinterpretq_u32_u16(vzip1q_u16(t1, t3)), vreinterpretq_u32_u16(vzip2q_u16(t1, t3))};
        uint32x4_t v[4];
        if (ord == 0) {
            for (int k = 0; k < 4; k++) v[k] = one ? vaddq_u32(vB, z[k]) : vmlaq_n_u32(vB, z[k], G);
        } else {
            uint32x4_t hb[4] = {vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0)};   /* bit 32 of z at bit 31 */
            if (ow == 8) {
                uint16x8_t h0 = vmovl_u8(vget_low_u8(p4)), h1 = vmovl_high_u8(p4);
                hb[0] = vshlq_n_u32(vmovl_u16(vget_low_u16(h0)), 31); hb[1] = vshlq_n_u32(vmovl_high_u16(h0), 31);
                hb[2] = vshlq_n_u32(vmovl_u16(vget_low_u16(h1)), 31); hb[3] = vshlq_n_u32(vmovl_high_u16(h1), 31);
            }
            for (int k = 0; k < 4; k++) {
                uint32x4_t d = veorq_u32(vorrq_u32(vshrq_n_u32(z[k], 1), hb[k]), vtstq_u32(z[k], one32));
                if (!one) d = vmulq_n_u32(d, G);
                if (ord == 2) { d = vaddq_u32(pfx4(d), ce); ce = vdupq_laneq_u32(d, 3); }
                v[k] = vaddq_u32(pfx4(d), c1); c1 = vdupq_laneq_u32(v[k], 3);
            }
        }
        for (int k = 0; k < 4; k++) vst1q_u32((uint32_t *)(void *)(o + 4 * i + 16 * k), v[k]);
    }
    return i;
}
#define II(OW, ORD, ONE) case (OW) * 8 + (ORD) * 2 + (ONE): i = ii_body(r, m, OW, ORD, ONE, G, B, a, b, o); break
int kolmo_int_inv4_neon(const uint8_t *r, size_t m, int ow, int ord, uint64_t g, uint64_t bias, uint64_t v1, uint64_t v2, uint8_t *o) {
    if ((ow != 1 && ow != 2 && ow != 4 && ow != 8) || ord < 0 || ord > 2) return 0;
    uint32_t G = (uint32_t)g, B = (uint32_t)bias, a = (uint32_t)v1, b = (uint32_t)v2; int one = G == 1; size_t i = 0;
    switch (ow * 8 + ord * 2 + one) {
        II(1, 0, 0); II(1, 0, 1); II(1, 1, 0); II(1, 1, 1); II(1, 2, 0); II(1, 2, 1);
        II(2, 0, 0); II(2, 0, 1); II(2, 1, 0); II(2, 1, 1); II(2, 2, 0); II(2, 2, 1);
        II(4, 0, 0); II(4, 0, 1); II(4, 1, 0); II(4, 1, 1); II(4, 2, 0); II(4, 2, 1);
        II(8, 0, 0); II(8, 0, 1); II(8, 1, 0); II(8, 1, 1); II(8, 2, 0); II(8, 2, 1);
    }
    if (i >= 1) memcpy(&a, o + 4 * (i - 1), 4);
    if (i >= 2) memcpy(&b, o + 4 * (i - 2), 4); else if (i == 1) b = (uint32_t)v1;
    for (; i < m; i++) {                         /* the rest as the reference, mod 2^32 */
        uint64_t z = 0; for (int q = 0; q < ow; q++) z |= (uint64_t)r[(size_t)q * m + i] << (8 * q);
        uint32_t v;
        if (ord == 0) v = B + (uint32_t)z * G;
        else {
            uint32_t d = (uint32_t)((z >> 1) ^ (0 - (z & 1))) * G;
            v = ord == 1 ? a + d : 2 * a - b + d;
        }
        memcpy(o + 4 * i, &v, 4); b = a; a = v;
    }
    return 1;
}
#undef II
#endif
