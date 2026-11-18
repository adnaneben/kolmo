/* Context-mixing mixer arithmetic (src/cm.inc): portable C references of the src/arch kernels. */
#ifndef KOLMO_ALGO_MIXER_H
#define KOLMO_ALGO_MIXER_H
#include <stdint.h>
/* s[k] = sum_i w_k[i] * x[i] over i < n, exactly (64-bit), for the weight sets w1, w2 and, unless w3 is NULL, w3 and w4 */
void kolmo_cm_dot_c(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]);
/* w_k[i] += (x[i] * e[k]) >> shift for i < n (32-bit products, arithmetic shift), same sets as kolmo_cm_dot_c */
void kolmo_cm_train_c(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift);
/* codec 7 model 4, the 16-bit mixer: s[k] = the sum over the pairs j < n / 2 of (x[2j] w_k[2j] + x[2j+1] w_k[2j+1]) >> 8
 * (arithmetic shift of each pair's exact sum), for the four weight sets; n a multiple of 16, no x[i] = -32768 */
void kolmo_cm_dot16_c(const int16_t *x, int n, const int16_t *w1, const int16_t *w2, const int16_t *w3, const int16_t *w4, int32_t s[4]);
/* w_k[i] = clamp(w_k[i] + ((x[i] e[k] + 2^14) >> 15), -32768, 32767) for i < n (n a multiple of 16; |e[k]| < 32768) */
void kolmo_cm_train16_c(const int16_t *x, int n, int16_t *w1, int16_t *w2, int16_t *w3, int16_t *w4, const int e[4]);
/* out[i] = the context hash (cm_hmix of codecs/cm.c) of h[i] + add, for i < n */
void kolmo_cm_hash_c(const uint32_t *h, int n, uint32_t add, uint32_t *out);
#endif
