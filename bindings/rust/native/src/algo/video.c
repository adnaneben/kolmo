/* Video strips: portable C references (the prediction of src/experimental/kvid.c). Apache-2.0, Kolmo contributors. */
#include "algo/video.h"

static inline int med(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b;
    return c >= mx ? mn : c <= mn ? mx : a + b - c;
}
static inline int predict(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t x, size_t y) {
    size_t i = y * w + x;
    if (k == 1) return prev[i];
    int a, b, c;
    if (k == 0) {
        a = x ? cur[i - 1] : (y ? cur[i - w] : 0); b = y ? cur[i - w] : a; c = (x && y) ? cur[i - w - 1] : b;
        return med(a, b, c);
    }
#define D(j) ((int)(int8_t)(uint8_t)(cur[j] - prev[j]))
    a = x ? D(i - 1) : (y ? D(i - w) : 0); b = y ? D(i - w) : a; c = (x && y) ? D(i - w - 1) : b;
#undef D
    return prev[i] + med(a, b, c);
}
static inline uint8_t zz(int r) { int8_t s = (int8_t)(uint8_t)r; return (uint8_t)(((uint8_t)s << 1) ^ (uint8_t)(s >> 7)); }
static inline int unzz(uint8_t z) { return (int)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }

int kolmo_vid_strip_enc_c(const uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, uint8_t *res) {
    int best = 0; uint64_t bc = UINT64_MAX;
    for (int k = 0; k < (prev ? 3 : 1); k++) {
        uint64_t c = 0;
        for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++) {
            int8_t r = (int8_t)(uint8_t)(cur[y * w + x] - predict(k, cur, prev, w, x, y)); c += (uint64_t)(r < 0 ? -r : r);
        }
        if (c < bc) { bc = c; best = k; }
    }
    for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++) res[y * w + x] = zz(cur[y * w + x] - predict(best, cur, prev, w, x, y));
    return best;
}
void kolmo_vid_strip_dec_c(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res) {
    for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++)
        cur[y * w + x] = (uint8_t)(predict(k, cur, prev, w, x, y) + unzz(res[y * w + x]));
}
