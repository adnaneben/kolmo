/* Video strips (kvid, the Y4M node): temporal and spatial prediction of one plane, 16 rows at a time.
 * Portable C references of the src/arch kernels. */
#ifndef KOLMO_ALGO_VIDEO_H
#define KOLMO_ALGO_VIDEO_H
#include <stddef.h>
#include <stdint.h>
/* Predictors of sample (x, y) of a plane of width w: 0 = median (LOCO-I) of left, up and up-left in this frame;
 * 1 = the same sample of the previous frame; 2 = previous frame plus the median of the left, up and up-left
 * frame differences. Edges: on row 0 left = up = up-left = the left sample (0 at x = 0); at x = 0, the up sample.
 * enc: picks for rows [y0, y1) the predictor with the smallest sum of |residual| (the first on ties; only 0 without
 * a previous frame), writes the zigzagged residuals to res (plane layout) and returns the predictor.
 * dec: rebuilds rows [y0, y1) of cur from res with predictor k; rows above y0 must be decoded. */
int kolmo_vid_strip_enc_c(const uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, uint8_t *res);
void kolmo_vid_strip_dec_c(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res);
#endif
