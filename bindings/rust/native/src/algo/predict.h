/* Image predictors: the portable C references of the prediction kernels (the specification of src/arch). */
#ifndef KOLMO_ALGO_PREDICT_H
#define KOLMO_ALGO_PREDICT_H
#include <stddef.h>
#include <stdint.h>

/* LOCO-I median predictor of a (left), b (up) and c (up-left): the median of a, b and a + b - c */
static inline uint8_t kolmo_med8(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b, g = a + b - c; g = g < mn ? mn : g; return (uint8_t)(g > mx ? mx : g);
}
static inline uint8_t kolmo_unzz8(uint8_t z) { return (uint8_t)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }

/* Inverse of the IMG8 node (kgraph): r holds zigzagged residuals of m bytes in rows of W bytes with channel step C.
 * Row 0 is predicted from the left (the first C bytes from 0), later rows from above for the first C bytes and from
 * the median predictor after that. Only whole rows are reconstructed: a trailing partial row of r is ignored. */
void kolmo_med8_inv_c(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);

/* 16-bit images (IMG16, src/sci.inc). Samples a[0..n), row stride W (>= 1); predictor 0 = left, 1 = (left + up) / 2,
 * 2 = median of left, up and left + up - up-left (absent neighbours are 0). The residual a - p (mod 2^16) is zigzagged
 * and split into a low and a high byte plane. */
void kolmo_img16_fwd_c(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi);
/* sum of |x[i] - y[i]| over i < n (the stride search of the IMG16 detector) */
uint64_t kolmo_sad16_c(const uint16_t *x, const uint16_t *y, size_t n);
#endif
