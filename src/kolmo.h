/* kolmo — native core of the kolmo compressor.
 *
 * Block-parallel, seekable, checksummed container. Each block is transformed (field transposition for delimited
 * text, byte-plane shuffle with delta for numeric arrays) and coded independently, so compression and
 * decompression scale across cores and any byte range can be read by decoding only the blocks it touches.
 *
 * Licence: Apache-2.0 (vendored zstd and LZ4 keep their own BSD licences).
 */
#ifndef KOLMO_H
#define KOLMO_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { KOLMO_RT = 0, KOLMO_FAST = 1, KOLMO_ECO = 2, KOLMO_HC = 3, KOLMO_MAX = 4, KOLMO_ULTRA = 5 } kolmo_mode;

typedef struct {
    kolmo_mode mode;      /* rt: LZ4 | fast: zstd 3 | eco: brotli 8 | hc: zstd 19 | max: BWT (text) or LZMA2 | ultra: context mixing (archive tier) */
    int level;          /* 0 = mode default; otherwise codec level (max: LZMA2 preset, except 1 = BWT for every block;
                           ultra: 1 = context-mixing model 1 only, faster; 2 = maximum ratio: as the default, and
                           integer/table graph leaves may also use context mixing when smaller, so those blocks
                           decode up to ~100x slower; any decoder reads the output) */
    int threads;        /* 0 = all cores */
    uint32_t block;     /* bytes per block, 0 = adaptive rt/fast, 16 MiB eco/hc (larger for text), 128 MiB max/ultra */
    int transforms;     /* 1 = analyse and transform (default), 0 = codec only, 2 = quick analysis: only the cheap
                           structure transforms (images, records, executables, tables), for the fastest settings */
} kolmo_params;

#define KOLMO_ERR_CORRUPT   (-1)
#define KOLMO_ERR_CHECKSUM  (-2)
#define KOLMO_ERR_DSTSIZE   (-3)
#define KOLMO_ERR_ALLOC     (-4)
#define KOLMO_ERR_CODEC     (-5)
#define KOLMO_ERR_DICT      (-6)   /* input was compressed with a dictionary; a matching one must be supplied to decode */
#define KOLMO_ERR_LIMIT     (-7)   /* a caller-specified decoding limit was exceeded */

#define KOLMO_ERR_IO        (-8)   /* seekable file read/write failed */

/* Zero fields mean no limit. These bound format sizes, not total codec heap
 * memory; a context-mixing block can require much more memory than its bytes. */
typedef struct {
    uint64_t max_output_bytes;
    uint32_t max_block_bytes;
    uint32_t max_blocks;
} kolmo_decode_limits;

size_t  kolmo_bound(size_t n, const kolmo_params *p);
int64_t kolmo_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p);
int64_t kolmo_original_size(const uint8_t *src, size_t n);
int64_t kolmo_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads);
int64_t kolmo_decompress_limited(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                               int threads, const kolmo_decode_limits *limits);
/* random access: bytes [off, off+len) of the original, decoding only the blocks that overlap */
int64_t kolmo_read_range(const uint8_t *src, size_t n, uint64_t off, size_t len, uint8_t *dst, int threads);
/* streaming / large inputs, bounded memory: file descriptors in and out (CZNS framed segments).
 * seg_mib = 0 uses the default segment size. Both return total bytes written, or a negative error. */
int64_t kolmo_stream_compress(int in_fd, int out_fd, const kolmo_params *p, uint32_t seg_mib);
int64_t kolmo_stream_decompress(int in_fd, int out_fd, int threads);
/* One-shot CZN1 file I/O. Input must remain a stable regular file of at least n
 * bytes. Supply an empty seekable output fd without O_APPEND. Workers use
 * offsets starting at zero, without changing the descriptor position. Discard
 * partial output on error. Optional dictionary semantics match the buffer API. */
int64_t kolmo_compress_from_fd(int fd, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                             const void *dict, size_t dictlen);
int64_t kolmo_decompress_to_fd(const uint8_t *src, size_t n, int fd, int threads,
                             const void *dict, size_t dictlen);
/* CZN1 regular file to file. As above, use a separate empty output. */
int64_t kolmo_compress_file(int input_fd, int output_fd, size_t n, const kolmo_params *p,
                          const void *dict, size_t dictlen);
int64_t kolmo_decompress_file(int input_fd, int output_fd, size_t n, int threads,
                            const void *dict, size_t dictlen);
const char *kolmo_strerror(int64_t code);
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len);

/* --- Dictionary support (small files) ------------------------------------------------------------
 * For many small, similar inputs (records, log fragments, JSON blobs), a dictionary trained on a
 * representative sample makes each file compress far better. Train once, then compress/decompress
 * with the same dictionary. The dictionary is applied to the zstd-coded blocks (fast/hc modes).
 *
 * kolmo_train_dict: build a dictionary of up to dict_cap bytes from `nsamples` concatenated samples
 *   (samples laid end-to-end in `samples`, each length in `sizes`). Returns the dictionary length
 *   written to `dict`, or a negative error. A few hundred samples and dict_cap ~100 KB are typical.
 * kolmo_compress_dict / kolmo_decompress_dict: as kolmo_compress / kolmo_decompress, with a dictionary.
 *   A file written with a dictionary records its id; decoding without the matching dictionary fails
 *   with KOLMO_ERR_DICT rather than returning wrong data. Independent calls may run concurrently,
 *   including calls with different dictionaries. Inputs and dictionaries must remain immutable
 *   until each call completes, and destination buffers must not overlap. */
int64_t kolmo_train_dict(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                       uint8_t *dict, size_t dict_cap);
int64_t kolmo_compress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                          const kolmo_params *p, const void *dict, size_t dictlen);
int64_t kolmo_decompress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                            const void *dict, size_t dictlen);

#ifdef __cplusplus
}
#endif
#endif
