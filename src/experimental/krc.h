/* Experimental independent residual codec. Apache-2.0, Kolmo contributors.
 * Standalone format; never selected by the existing Kolmo compressor. */
#ifndef KRC_H
#define KRC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KRC_TILE_WORDS 256
#define KRC_CANDIDATES 20
#define KRC_ERR_CORRUPT (-1)
#define KRC_ERR_CAPACITY (-2)
#define KRC_ERR_ALLOC (-3)
#define KRC_ERR_PLAN (-4)
typedef struct { uint32_t mode, stride, width, exceptions; } krc_plan;
size_t krc_plan_count(size_t bytes);
size_t krc_bound(size_t bytes); /* 0 if the input cannot be represented */
int krc_plan_cpu(const uint8_t *src, size_t n, krc_plan *plans, size_t count, int threads);
int64_t krc_encode(const uint8_t *src, size_t n, const krc_plan *plans, size_t count, uint8_t *dst, size_t cap);
int64_t krc_encode_threads(const uint8_t *src,size_t n,const krc_plan *plans,size_t count,uint8_t *dst,size_t cap,int threads);
int64_t krc_decode_threads(const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads);
int64_t krc_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads);
int64_t krc_original_size(const uint8_t *src,size_t n); /* validates header, not payload */
int64_t krc_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
#ifdef __cplusplus
}
#endif
#endif
