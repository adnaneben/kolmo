/* Byte planes: split fixed-width words into one plane per byte and merge them back (the T_SHUF transform with one
 * channel), optionally on wrapping differences of consecutive words. Portable C references of src/arch kernels. */
#ifndef KOLMO_ALGO_PLANES_H
#define KOLMO_ALGO_PLANES_H
#include <stddef.h>
#include <stdint.h>
/* split: in holds rows words of w bytes (w = 1, 2, 4 or 8), little endian; out[b * rows + r] = byte b of word r, or of
 * (word r - word r-1) mod 2^(8w) when delta (word -1 = 0). merge is the inverse. */
void kolmo_planes_split_c(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out);
void kolmo_planes_merge_c(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out);
#endif
