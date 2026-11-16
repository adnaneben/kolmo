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
