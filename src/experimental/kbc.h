/* Independent byte-context experiment; established arithmetic-coding mathematics.
 * Separate format and API: does not modify KRC1 or normal Kolmo. Apache-2.0. */
#ifndef KBC_H
#define KBC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KBC_BLOCK_BYTES 262144
size_t kbc_bound(size_t bytes);
int64_t kbc_original_size(const uint8_t *src, size_t n);
int64_t kbc_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads);
int64_t kbc_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads);
#ifdef __cplusplus
}
#endif
#endif
