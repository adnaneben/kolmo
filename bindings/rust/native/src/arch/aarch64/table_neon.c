/* NEON: table decoder helpers (C references: the scalar loops of row_nonempty, compact_rows and the validation pass
 * of tbl2_decode in src/tbl2.inc).
 * kolmo_row_nonempty_neon  - bit j set when field j of a row has a nonzero length (n <= 64): a real 64-bit mask from
 *                            byte weights and pairwise adds, where the earlier NEON code walked a nibble mask bit by bit.
 * kolmo_compact_rows_neon  - the indices t < R with kd[t] == 1, in order: 8 at a time from a 256-entry left-pack table
 *                            (needs 8 bytes of room past the last index); a block of 16 numbers is a stored ramp.
 * kolmo_kind_count_neon    - counts of kind bytes equal to 1 and 2, and whether any is above 2. */
#include "../arch.h"
#if KOLMO_NEON
#include <arm_neon.h>
#include <pthread.h>

static const uint8_t tb_w[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
static inline uint64_t tb_mask64(uint8x16_t a, uint8x16_t b, uint8x16_t c, uint8x16_t d) {   /* 0/FF bytes -> bits */
    const uint8x16_t w = vld1q_u8(tb_w);
    uint8x16_t s0 = vpaddq_u8(vandq_u8(a, w), vandq_u8(b, w)), s1 = vpaddq_u8(vandq_u8(c, w), vandq_u8(d, w));
    s0 = vpaddq_u8(s0, s1); s0 = vpaddq_u8(s0, s0);
    return vgetq_lane_u64(vreinterpretq_u64_u8(s0), 0);
}
static inline uint8x16_t tb_nz16(const uint16_t *p) {
    uint16x8_t a = vld1q_u16(p), b = vld1q_u16(p + 8);
    return vcombine_u8(vmovn_u16(vtstq_u16(a, a)), vmovn_u16(vtstq_u16(b, b)));
}
uint64_t kolmo_row_nonempty_neon(const uint16_t *rl, int n) {
    uint64_t m = 0; int j = 0;
    if (n == 64) return tb_mask64(tb_nz16(rl), tb_nz16(rl + 16), tb_nz16(rl + 32), tb_nz16(rl + 48));
    const uint8x16_t z = vdupq_n_u8(0);
    for (; j + 16 <= n; j += 16) m |= (tb_mask64(tb_nz16(rl + j), z, z, z) & 0xFFFF) << j;
    for (; j < n; j++) m |= (uint64_t)(rl[j] != 0) << j;
    return m;
}

static uint8_t tb_pack[256][8];
static pthread_once_t tb_once = PTHREAD_ONCE_INIT;
static void tb_init(void) {
    for (int m = 0; m < 256; m++) { int q = 0; for (int i = 0; i < 8; i++) if (m >> i & 1) tb_pack[m][q++] = (uint8_t)i; for (; q < 8; q++) tb_pack[m][q] = 0; }
}
int kolmo_compact_rows_neon(const uint8_t *kd, int R, uint8_t *rows) {
    pthread_once(&tb_once, tb_init);
    static const uint8_t ramp[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const uint8x16_t one = vdupq_n_u8(1), rp = vld1q_u8(ramp), z = vdupq_n_u8(0);
    int q = 0, t = 0;
    for (; t + 16 <= R; t += 16) {
        uint8x16_t e = vceqq_u8(vld1q_u8(kd + t), one);
        if (vminvq_u8(e)) { vst1q_u8(rows + q, vaddq_u8(rp, vdupq_n_u8((uint8_t)t))); q += 16; continue; }
        uint64_t m = tb_mask64(e, z, z, z);
        unsigned lo = (unsigned)(m & 255), hi = (unsigned)(m >> 8 & 255);
        vst1_u8(rows + q, vadd_u8(vld1_u8(tb_pack[lo]), vdup_n_u8((uint8_t)t))); q += __builtin_popcount(lo);
        vst1_u8(rows + q, vadd_u8(vld1_u8(tb_pack[hi]), vdup_n_u8((uint8_t)(t + 8)))); q += __builtin_popcount(hi);
    }
    for (; t < R; t++) { rows[q] = (uint8_t)t; q += kd[t] == 1; }
    return q;
}

int kolmo_kind_count_neon(const uint8_t *kind, uint32_t cnt, uint32_t *pn1, uint32_t *pn2) {
    uint64_t n1 = 0, n2 = 0; uint32_t i = 0; uint8x16_t bad = vdupq_n_u8(0);
    const uint8x16_t one = vdupq_n_u8(1), two = vdupq_n_u8(2);
    while (i + 16 <= cnt) {
        uint8x16_t a1 = vdupq_n_u8(0), a2 = vdupq_n_u8(0);
        uint32_t e = cnt - i < 255 * 16 ? i + ((cnt - i) & ~15u) : i + 255 * 16;   /* byte counters: 255 steps at most */
        for (; i < e; i += 16) {
            uint8x16_t k = vld1q_u8(kind + i);
            a1 = vsubq_u8(a1, vceqq_u8(k, one)); a2 = vsubq_u8(a2, vceqq_u8(k, two)); bad = vmaxq_u8(bad, k);
        }
        n1 += vaddlvq_u8(a1); n2 += vaddlvq_u8(a2);
    }
    uint32_t b = vmaxvq_u8(bad) > 2;
    for (; i < cnt; i++) { uint8_t k = kind[i]; n1 += k == 1; n2 += k == 2; b |= k > 2; }
    *pn1 = (uint32_t)n1; *pn2 = (uint32_t)n2;
    return (int)b;
}
#endif
