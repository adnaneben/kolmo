/* Image predictors: portable C references. Apache-2.0, Kolmo contributors. */
#include "algo/predict.h"

void kolmo_med8_inv_c(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p) {
    size_t x = 0;
    for (; x < C && x < m; x++) p[x] = kolmo_unzz8(r[x]);
    for (; x < W && x < m; x++) p[x] = (uint8_t)(p[x - C] + kolmo_unzz8(r[x]));
    for (size_t y = W; y + W <= m; y += W) {
        uint8_t *q = p + y; const uint8_t *u = q - W, *o = r + y;
        for (x = 0; x < C; x++) q[x] = (uint8_t)(u[x] + kolmo_unzz8(o[x]));
        for (; x < W; x++) q[x] = (uint8_t)(kolmo_med8(q[x - C], u[x], u[x - C]) + kolmo_unzz8(o[x]));
    }
}

void kolmo_med8_fwd_c(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r) {
    size_t x = 0;
    for (; x < C && x < m; x++) r[x] = kolmo_zz8(p[x]);
    for (; x < W && x < m; x++) r[x] = kolmo_zz8((uint8_t)(p[x] - p[x - C]));
    for (size_t y = W; y + W <= m; y += W) {
        const uint8_t *q = p + y, *u = q - W; uint8_t *o = r + y;
        for (x = 0; x < C; x++) o[x] = kolmo_zz8((uint8_t)(q[x] - u[x]));
        for (; x < W; x++) o[x] = kolmo_zz8((uint8_t)(q[x] - kolmo_med8(q[x - C], u[x], u[x - C])));
    }
}

static inline int pred16(const uint16_t *x, int64_t i, int64_t W, int mode) {
    int L = i >= 1 ? x[i - 1] : 0, U = i >= W ? x[i - W] : 0, UL = i >= W + 1 ? x[i - W - 1] : 0;
    if (mode == 0) return L;
    if (mode == 1) return (L + U) >> 1;
    int mx = L > U ? L : U, mn = L < U ? L : U;
    if (UL >= mx) return mn;
    if (UL <= mn) return mx;
    return L + U - UL;
}
void kolmo_img16_fwd_c(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi) {
    for (int64_t i = 0; i < n; i++) {
        uint16_t r = (uint16_t)(a[i] - pred16(a, i, W, mode)); uint16_t z = (uint16_t)((uint16_t)(r << 1) ^ (uint16_t)-(uint16_t)(r >> 15));
        lo[i] = (uint8_t)z; hi[i] = (uint8_t)(z >> 8);
    }
}
uint64_t kolmo_sad16_c(const uint16_t *x, const uint16_t *y, size_t n) {
    uint64_t s = 0;
    for (size_t i = 0; i < n; i++) s += (uint64_t)(x[i] > y[i] ? x[i] - y[i] : y[i] - x[i]);
    return s;
}
