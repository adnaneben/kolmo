/* This frozen lzbench imports the two historical czn_* entry points.
 * The default-mode parameter layout is unchanged in Kolmo 1.1.0. */
#include "kolmo.h"
#include <stddef.h>

_Static_assert(sizeof(kolmo_params) == 20 && offsetof(kolmo_params, mode) == 0 &&
               offsetof(kolmo_params, level) == 4 && offsetof(kolmo_params, threads) == 8 &&
               offsetof(kolmo_params, block) == 12 && offsetof(kolmo_params, transforms) == 16,
               "The benchmark bridge expects the frozen adapter's exact parameter layout");

int64_t czn_compress(const uint8_t *input, size_t n, uint8_t *output, size_t cap,
                     const kolmo_params *p) {
    if (p->mode < KOLMO_RT || p->mode > KOLMO_ULTRA || p->level ||
        p->threads != 1 || p->block || p->transforms != 1)
        return KOLMO_ERR_CODEC;
    return kolmo_compress(input, n, output, cap, p);
}

int64_t czn_decompress(const uint8_t *input, size_t n, uint8_t *output, size_t cap, int workers) {
    return workers == 1 ? kolmo_decompress(input, n, output, cap, workers) : KOLMO_ERR_CODEC;
}
