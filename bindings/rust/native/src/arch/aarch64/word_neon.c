/* NEON: word transform decoder (C reference: the loop of word_decode in src/word.inc).
 *
 * The reference finds each special byte (a code prefix, or the capital marker in front of one) with a fresh scan, so
 * the address of the next token waits for the load and compare of the bytes after the previous one. Here 64 input
 * bytes are classified at once (table lookups give two bit masks: special bytes and capital markers), and tokens are
 * taken from the masks with count-trailing-zeros: the chain from one token to the next is a few integer operations.
 * Literal runs move in 16-byte pieces and words in three 16-byte pieces from their 64-byte slots; bytes written past
 * the token are overwritten by the next one, as in the reference. Output is the same; the test is tests/test_arch.c. */
#include "../arch.h"
#if KOLMO_NEON
#include <arm_neon.h>

static inline uint8x16_t wd_class(uint8x16_t v, const uint8x16x4_t *T, int low) {
    if (low) return vqtbl2q_u8((uint8x16x2_t){{T[0].val[0], T[0].val[1]}}, v);       /* every special byte is < 32 */
    uint8x16_t r = vqtbl4q_u8(T[0], v);
    r = vorrq_u8(r, vqtbl4q_u8(T[1], vsubq_u8(v, vdupq_n_u8(64))));
    r = vorrq_u8(r, vqtbl4q_u8(T[2], vsubq_u8(v, vdupq_n_u8(128))));
    return vorrq_u8(r, vqtbl4q_u8(T[3], vsubq_u8(v, vdupq_n_u8(192))));
}
/* bit i of each half = byte i of a..d (S) and e..h (C) is nonzero */
static inline uint64x2_t wd_masks(uint8x16_t a, uint8x16_t b, uint8x16_t c, uint8x16_t d, uint8x16_t e, uint8x16_t f,
                                  uint8x16_t g, uint8x16_t h) {
    const uint8x16_t w = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    uint8x16_t s0 = vpaddq_u8(vandq_u8(a, w), vandq_u8(b, w)), s1 = vpaddq_u8(vandq_u8(c, w), vandq_u8(d, w));
    uint8x16_t s2 = vpaddq_u8(vandq_u8(e, w), vandq_u8(f, w)), s3 = vpaddq_u8(vandq_u8(g, w), vandq_u8(h, w));
    return vreinterpretq_u64_u8(vpaddq_u8(vpaddq_u8(s0, s1), vpaddq_u8(s2, s3)));
}

void kolmo_wd_dec_neon(const uint8_t **pp, const uint8_t *end, uint8_t **po, uint8_t *oe, const uint8_t *arena,
                       const uint8_t *wl, uint32_t nw, const int16_t *pidx, uint8_t capb, const uint8_t *special) {
    const uint8_t *p = *pp; uint8_t *o = *po;
    uint8_t tab[256]; int low = 1;
    for (int b = 0; b < 256; b++) { tab[b] = special[b] ? 0xFF : 0; if (b >= 32 && special[b]) low = 0; }
    uint8x16x4_t T[4];
    for (int q = 0; q < 4; q++) T[q] = vld1q_u8_x4(tab + 64 * q);
    const uint8x16_t vc = vdupq_n_u8(capb);
    const uint8x16_t capclr[2] = {vdupq_n_u8(0xFF), {0xDF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    /* per 64-byte window at most 64 literal bytes plus 32 words of at most 48 bytes (each written as 48) */
    while (end - p >= 80 && oe - o >= 2048) {
        uint8x16_t v0 = vld1q_u8(p), v1 = vld1q_u8(p + 16), v2 = vld1q_u8(p + 32), v3 = vld1q_u8(p + 48);
        uint64x2_t mk = low ? wd_masks(wd_class(v0, T, 1), wd_class(v1, T, 1), wd_class(v2, T, 1), wd_class(v3, T, 1),
                                       vceqq_u8(v0, vc), vceqq_u8(v1, vc), vceqq_u8(v2, vc), vceqq_u8(v3, vc))
                            : wd_masks(wd_class(v0, T, 0), wd_class(v1, T, 0), wd_class(v2, T, 0), wd_class(v3, T, 0),
                                       vceqq_u8(v0, vc), vceqq_u8(v1, vc), vceqq_u8(v2, vc), vceqq_u8(v3, vc));
        const uint64_t S = vgetq_lane_u64(mk, 0), C = vgetq_lane_u64(mk, 1), P = S & ~C;
        uint64_t rest = S; unsigned c = 0;
        for (;;) {
            if (!rest) {                                         /* no token left in the window: the rest is literal */
                for (unsigned k = c; k < 64; k += 16) vst1q_u8(o + (k - c), vld1q_u8(p + k));
                o += 64 - c; p += 64;
                break;
            }
            unsigned t = (unsigned)__builtin_ctzll(rest);
            for (unsigned k = c; k < t; k += 16) vst1q_u8(o + (k - c), vld1q_u8(p + k));
            o += t - c;
            unsigned cf = (unsigned)(C >> t) & 1, e = t + cf + 2;
            if (e > 64) { p += t; break; }                       /* the token crosses the window: start one at it */
            if (cf && !((P >> (t + 1)) & 1)) { *pp = p + t; *po = o; return; }   /* not a prefix: the C loop rejects it */
            uint32_t code = (uint32_t)pidx[p[t + cf]] << 8 | p[t + cf + 1];
            if (code >= nw) { *pp = p + t; *po = o; return; }
            const uint8_t *src = arena + (size_t)code * 64;
            vst1q_u8(o, vandq_u8(vld1q_u8(src), capclr[cf]));
            vst1q_u8(o + 16, vld1q_u8(src + 16));
            vst1q_u8(o + 32, vld1q_u8(src + 32));
            o += wl[code];
            c = e;
            if (c >= 64) { p += 64; break; }
            rest = S & (~0ull << c);
        }
    }
    *pp = p; *po = o;
}
#endif
