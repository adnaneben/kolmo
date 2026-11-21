#ifndef KBX_H
#define KBX_H
#include "krc.h"
#ifdef __cplusplus
extern "C" {
#endif
#define KBX_BLOCK_BYTES 262144
/* profile 0: byte-identical KBC1; 1: verified longer contexts; 2: choose smallest
 * baseline/longer payload. New profiles write KBX3; decoder reads both. */
size_t kbx_bound(size_t n);
int64_t kbx_original_size(const uint8_t *src,size_t n);
int64_t kbx_compress(const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads,int profile);
int64_t kbx_decompress(const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads);
#ifdef __cplusplus
}
#endif
#endif
