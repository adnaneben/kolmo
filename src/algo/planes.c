/* Byte planes: portable C references. Apache-2.0, Kolmo contributors. */
#include "algo/planes.h"
#include <string.h>

static inline uint64_t ldw(const uint8_t *p, unsigned w) { uint64_t v = 0; for (unsigned b = 0; b < w; b++) v |= (uint64_t)p[b] << (8 * b); return v; }
static inline uint64_t wmask(unsigned w) { return w == 8 ? ~0ull : (1ull << (8 * w)) - 1; }

void kolmo_planes_split_c(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out) {
    uint64_t pv = 0, mask = wmask(w);
    for (size_t r = 0; r < rows; r++) {
        uint64_t v = ldw(in + r * w, w), d = delta ? (v - pv) & mask : v; pv = v;
        for (unsigned b = 0; b < w; b++) out[(size_t)b * rows + r] = (uint8_t)(d >> (8 * b));
    }
}
void kolmo_planes_merge_c(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out) {
    uint64_t pv = 0, mask = wmask(w);
    for (size_t r = 0; r < rows; r++) {
        uint64_t d = 0; for (unsigned b = 0; b < w; b++) d |= (uint64_t)in[(size_t)b * rows + r] << (8 * b);
        uint64_t v = delta ? (pv + d) & mask : d; pv = v;
        for (unsigned b = 0; b < w; b++) out[r * w + b] = (uint8_t)(v >> (8 * b));
    }
}
