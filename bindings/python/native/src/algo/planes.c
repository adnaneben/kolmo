/* Byte planes: portable C references. Apache-2.0, Kolmo contributors. */
#include "algo/planes.h"
#include <string.h>

/* rc2's loop (shuffle_fwd/inv with one channel): words through a 4096-entry buffer, then one plane at a time. The
 * SIMD kernels cover w = 2, 4, 8 without delta and w = 2 with delta; the other shapes run this. */
#define PL_SB 4096
static inline uint64_t ldw(const uint8_t *p, unsigned w) { uint64_t v = 0; memcpy(&v, p, w); return v; }   /* little endian hosts */
static inline uint64_t wmask(unsigned w) { return w == 8 ? ~0ull : (1ull << (8 * w)) - 1; }

void kolmo_planes_split_c(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out) {
    uint64_t pv = 0, mask = wmask(w), tmp[PL_SB];
    for (size_t r0 = 0; r0 < rows; r0 += PL_SB) {
        size_t nb = rows - r0 < PL_SB ? rows - r0 : PL_SB; const uint8_t *p = in + r0 * w;
        for (size_t r = 0; r < nb; r++, p += w) { uint64_t v = ldw(p, w); tmp[r] = delta ? (v - pv) & mask : v; pv = v; }
        for (unsigned b = 0; b < w; b++) { uint8_t *o = out + (size_t)b * rows + r0; for (size_t r = 0; r < nb; r++) o[r] = (uint8_t)(tmp[r] >> (8 * b)); }
    }
}
void kolmo_planes_merge_c(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out) {
    uint64_t pv = 0, mask = wmask(w), tmp[PL_SB];
    for (size_t r0 = 0; r0 < rows; r0 += PL_SB) {
        size_t nb = rows - r0 < PL_SB ? rows - r0 : PL_SB;
        memset(tmp, 0, nb * 8);
        for (unsigned b = 0; b < w; b++) { const uint8_t *q = in + (size_t)b * rows + r0; for (size_t r = 0; r < nb; r++) tmp[r] |= (uint64_t)q[r] << (8 * b); }
        uint8_t *p = out + r0 * w;
        for (size_t r = 0; r < nb; r++, p += w) { uint64_t v = delta ? (pv + tmp[r]) & mask : tmp[r]; memcpy(p, &v, w); pv = v; }
    }
}
