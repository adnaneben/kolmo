/* czn — native core of the cz compressor.
 *
 * Block-parallel, seekable, checksummed container. Each block is transformed (field transposition for delimited
 * text, byte-plane shuffle with delta for numeric arrays) and coded independently, so compression and
 * decompression scale across cores and any byte range can be read by decoding only the blocks it touches.
 *
 * Licence: Apache-2.0 (vendored zstd and LZ4 keep their own BSD licences).
 */
#ifndef CZN_H
#define CZN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { CZN_RT = 0, CZN_FAST = 1, CZN_ECO = 2, CZN_HC = 3, CZN_MAX = 4, CZN_ULTRA = 5 } czn_mode;

typedef struct {
    czn_mode mode;      /* rt: LZ4 | fast: zstd 3 | eco: brotli 8 | hc: zstd 19 | max: BWT (text) or LZMA2 | ultra: context mixing (archive tier) */
    int level;          /* 0 = mode default; otherwise codec level */
    int threads;        /* 0 = all cores */
    uint32_t block;     /* bytes per block, 0 = default (4 MiB for rt/fast, 16 MiB for eco/hc, 128 MiB for max and ultra) */
    int transforms;     /* 1 = analyse and transform (default), 0 = codec only */
} czn_params;

#define CZN_ERR_CORRUPT   (-1)
#define CZN_ERR_CHECKSUM  (-2)
#define CZN_ERR_DSTSIZE   (-3)
#define CZN_ERR_ALLOC     (-4)
#define CZN_ERR_CODEC     (-5)

size_t  czn_bound(size_t n, const czn_params *p);
int64_t czn_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const czn_params *p);
int64_t czn_original_size(const uint8_t *src, size_t n);
int64_t czn_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads);
/* random access: bytes [off, off+len) of the original, decoding only the blocks that overlap */
int64_t czn_read_range(const uint8_t *src, size_t n, uint64_t off, size_t len, uint8_t *dst, int threads);
/* streaming / large inputs, bounded memory: file descriptors in and out (CZNS framed segments).
 * seg_mib = 0 uses the default segment size. Both return total bytes written, or a negative error. */
int64_t czn_stream_compress(int in_fd, int out_fd, const czn_params *p, uint32_t seg_mib);
int64_t czn_stream_decompress(int in_fd, int out_fd, int threads);
const char *czn_strerror(int64_t code);
uint32_t czn_crc32c(uint32_t crc, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif
