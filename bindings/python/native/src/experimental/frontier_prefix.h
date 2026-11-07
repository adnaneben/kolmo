#ifndef KOLMO_FRONTIER_PREFIX_H
#define KOLMO_FRONTIER_PREFIX_H
#include <stdint.h>
#include <stddef.h>
/* Optional byte entropy coding of the packed DNA payload. */
size_t frontier_prefix_encode(const uint8_t *, size_t, uint8_t *, size_t);
int frontier_prefix_decode(const uint8_t *, size_t, uint8_t *, size_t);
#endif
