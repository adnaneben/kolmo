#include "kolmo.h"

/* Research entry point for frozen, complete CZN1 v12 frames held in linear memory. */
int64_t restore_frame(const uint8_t *src, size_t size, uint8_t *dst, size_t cap) {
    return kolmo_decompress(src, size, dst, cap, 1);
}
