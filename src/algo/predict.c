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
