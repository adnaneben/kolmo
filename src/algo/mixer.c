/* Context-mixing mixer arithmetic: portable C references. Apache-2.0, Kolmo contributors. */
#include "algo/mixer.h"
#include <stddef.h>

void kolmo_cm_dot_c(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]) {
    int64_t s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    if (!w3) { for (int i = 0; i < n; i++) { s1 += (int64_t)w1[i] * x[i]; s2 += (int64_t)w2[i] * x[i]; } }
    else for (int i = 0; i < n; i++) { s1 += (int64_t)w1[i] * x[i]; s2 += (int64_t)w2[i] * x[i]; s3 += (int64_t)w3[i] * x[i]; s4 += (int64_t)w4[i] * x[i]; }
    s[0] = s1; s[1] = s2; s[2] = s3; s[3] = s4;
}
void kolmo_cm_train_c(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift) {
    const int e1 = e[0], e2 = e[1], e3 = e[2], e4 = e[3];
    if (!w3) { for (int i = 0; i < n; i++) { w1[i] += (x[i] * e1) >> shift; w2[i] += (x[i] * e2) >> shift; } }
    else for (int i = 0; i < n; i++) { w1[i] += (x[i] * e1) >> shift; w2[i] += (x[i] * e2) >> shift; w3[i] += (x[i] * e3) >> shift; w4[i] += (x[i] * e4) >> shift; }
}

void kolmo_cm_hash_c(const uint32_t *h, int n, uint32_t add, uint32_t *out) {
    for (int i = 0; i < n; i++) {
        uint32_t a = h[i] + add;
        a ^= a >> 15; a *= 0x2C1B3C6Du; a ^= a >> 12; a *= 0x297A2D39u; a ^= a >> 15;
        out[i] = a;
    }
}

void kolmo_cm_dot16_c(const int16_t *x, int n, const int16_t *w1, const int16_t *w2, const int16_t *w3, const int16_t *w4, int32_t s[4]) {
    const int16_t *w[4] = {w1, w2, w3, w4};
    for (int k = 0; k < 4; k++) {
        int32_t t = 0;
        for (int i = 0; i < n; i += 2) t += ((int32_t)x[i] * w[k][i] + (int32_t)x[i + 1] * w[k][i + 1]) >> 8;
        s[k] = t;
    }
}
void kolmo_cm_train16_c(const int16_t *x, int n, int16_t *w1, int16_t *w2, int16_t *w3, int16_t *w4, const int e[4]) {
    int16_t *w[4] = {w1, w2, w3, w4};
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < n; i++) {
            int32_t v = w[k][i] + (((int32_t)x[i] * e[k] + 16384) >> 15);
            w[k][i] = (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
        }
}
