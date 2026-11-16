/* Context-mixing mixer arithmetic (src/cm.inc): portable C references of the src/arch kernels. */
#ifndef KOLMO_ALGO_MIXER_H
#define KOLMO_ALGO_MIXER_H
#include <stdint.h>
/* s[k] = sum_i w_k[i] * x[i] over i < n, exactly (64-bit), for the weight sets w1, w2 and, unless w3 is NULL, w3 and w4 */
void kolmo_cm_dot_c(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]);
/* w_k[i] += (x[i] * e[k]) >> shift for i < n (32-bit products, arithmetic shift), same sets as kolmo_cm_dot_c */
void kolmo_cm_train_c(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift);
#endif
