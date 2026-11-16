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
    int level;          /* 0 = mode default; otherwise codec level (max: 1 = BWT for every block, 10 = LZMA2 also on
                           decimal-array planes, smaller and about 25 times slower to encode on such files;
                           ultra: 1 = context-mixing model 1 only, faster; 2 = maximum ratio: as the default, and
                           integer/table graph leaves may also use context mixing when smaller, so those blocks
                           decode up to ~100x slower; any decoder reads the output) */
    int threads;        /* 0 = all cores */
    uint32_t block;     /* bytes per block, 0 = adaptive rt/fast, 16 MiB eco/hc (larger for text), 128 MiB max/ultra */
    int transforms;     /* 1 = analyse and transform (default), 0 = codec only, 2 = quick analysis: only the cheap
                           structure transforms (images, records, executables, tables), for the fastest settings.
                           May be OR-ed with KOLMO_T_NATIVE_LEAF or KOLMO_T_TYPED_HUF (below). */
} kolmo_params;

/* Opt-in bit for kolmo_params.transforms (balanced / KOLMO_FAST only; ignored elsewhere): integer-series residuals in
 * graph blocks may use Kolmo's native plane-context leaf coder (Huffman/rANS with byte-plane and context modelling)
 * when it is smaller than the default leaf. Typically 5-12% smaller on sensor, ECG and numeric-column data; such files
 * decode about 1.5-2.6x slower than default balanced (measured: 2.3x over 180 MB of ECG and numeric columns, ~580 MB/s
 * on Apple silicon), since those leaves are entropy coded with context models instead of zstd. Files that use it are
 * written as container version 9 and need a kolmo release that reads version 9; files that do not are unchanged. */
#define KOLMO_T_NATIVE_LEAF 16

/* Opt-in bit for kolmo_params.transforms, ported from the RC1 integration (PR #10): a file declared as a little-endian
 * integer NPY array or a PCM16 WAV whose data chunk ends the file is coded type first, as one integer graph node with
 * static Huffman residual planes, in an existing version 7 frame that every reader of version 7 decodes. It applies
 * only to KOLMO_FAST at the default level with analysis on, exactly one thread requested, the buffer API, no
 * dictionary, and inputs of at most 16 MiB that fit one block. A file of those types that a sampled zstd estimate
 * predicts would not gain is compressed with the plain codec; other inputs are planned as usual. KOLMO_T_NATIVE_LEAF
 * takes precedence when both bits are set. Not chosen by default.
 * It is a speed option, not a ratio option. Measured on this branch (Apple silicon, one thread, in memory, 12 inputs):
 * arrays and clips of 0.06-2.6 MB compress 4-13x faster than default balanced; single-channel 16-bit series (ECG) are
 * also about 7% smaller and decode faster; but 7 of the 12 inputs come out larger than default balanced, by 14% to
 * 170%, and interleaved two-channel data and one audio clip come out larger than plain zstd level 3: the route has no
 * match-finding stage, predicts interleaved channels or columns as one series, and its guard estimates from a single
 * 16 KiB sample. On 40 mono 16-bit audio clips (sizes only) the total is 5.9% smaller than default balanced: smaller
 * on 22 clips, the same on 6, larger on 12 (one of them by 170%), and larger than zstd level 3 on 6. Compare sizes
 * before relying on it. */
#define KOLMO_T_TYPED_HUF 32

/* Opt-in bit for kolmo_params.transforms (every mode but rt), set by kolmo_compress_indexed: blocks of text end at line
 * ends, and a delimited table block (table transform) codes each column as a separate stream (block flag 13), so a
 * column query (kolmo_query_run, below) decodes only that column's streams. Measured cost: +2% (dense numeric CSV) to
 * +10% (wide sparse CSV) at balanced, about 0% at hc and ratio on dense data. */
#define KOLMO_T_COLUMNS 64

#define KOLMO_ERR_CORRUPT   (-1)
#define KOLMO_ERR_CHECKSUM  (-2)
#define KOLMO_ERR_DSTSIZE   (-3)
#define KOLMO_ERR_ALLOC     (-4)
#define KOLMO_ERR_CODEC     (-5)
#define KOLMO_ERR_DICT      (-6)   /* input was compressed with a dictionary; a matching one must be supplied to decode */
#define KOLMO_ERR_LIMIT     (-7)   /* a caller-specified decoding limit was exceeded */

#define KOLMO_ERR_IO        (-8)   /* seekable file read/write failed */
#define KOLMO_ERR_QUERY     (-9)   /* a query names no such column, or its pattern does not compile */

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
/* as kolmo_stream_decompress, with decoding limits per frame and max_output_bytes for the whole stream (NULL: none) */
int64_t kolmo_stream_decompress_limited(int in_fd, int out_fd, int threads, const kolmo_decode_limits *limits);
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

/* --- Queryable archives (container extension IDX1; src/core/index.c, SPEC.md "Column and row index") -------------
 * kolmo_compress_indexed writes an ordinary CZN1 frame whose text blocks end at line ends, whose delimited tables are
 * column-split (KOLMO_T_COLUMNS), followed by an IDX1 section: lines per block, the kind of text (table, JSON lines,
 * other lines), the column names of a table's first line, and optionally a trigram filter per block. Every reader
 * decodes the file as usual (kolmo_decompress ignores the extension). kolmo_query reads lines, one column, or the lines
 * matching a pattern, decoding only the blocks (and for column-split tables only the column streams) it needs.
 * Rows are physical lines, numbered from 0 (the CLI numbers them from 1). */
typedef struct {
    int kind;                 /* KOLMO_IDX_AUTO (detect), KOLMO_IDX_LINES, KOLMO_IDX_TABLE, KOLMO_IDX_JSON */
    uint8_t delim;            /* tables: the delimiter, 0 = detect (',', '\t', ';', '|') */
    int filter;               /* 1 (default when the struct is NULL): a filter per block for kolmo_query patterns (trigrams
                                 and whole tokens);
                                 0: rows and columns only (the index is then a few bytes per block) */
    int token_bits;           /* filter bits per whole token (1-16, 0 = 4): a one-token query passes a block that lacks it
                                 with probability about 2^-token_bits; the filter grows about linearly with it */
} kolmo_index_params;
#define KOLMO_IDX_AUTO  0
#define KOLMO_IDX_LINES 1
#define KOLMO_IDX_TABLE 2
#define KOLMO_IDX_JSON  3
/* the capacity kolmo_compress_indexed may need */
size_t  kolmo_indexed_bound(size_t n, const kolmo_params *p);
/* as kolmo_compress, plus the index. p->block 0 = 4 MiB blocks (finer blocks skip more and cost some ratio). */
int64_t kolmo_compress_indexed(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                               const kolmo_index_params *ip);

typedef struct {
    uint64_t row_first, row_count; /* rows [first, first + count); count 0 = to the last row */
    const char *column;            /* NULL: whole lines. A table's column name (first line) or JSON key, or "#N" for
                                      field N (from 1): of the table, or the N-th whitespace-separated field of a line */
    const char *pattern;           /* NULL: every row in range. Otherwise rows whose line matches (POSIX extended regex,
                                      or a fixed string with KOLMO_Q_FIXED); the column, if any, is printed for them */
    int flags;                     /* KOLMO_Q_* */
    int threads;                   /* 0 = all cores */
} kolmo_query;
#define KOLMO_Q_FIXED   1   /* pattern is a fixed string */
#define KOLMO_Q_ICASE   2   /* ASCII case-insensitive */
#define KOLMO_Q_LINENO  4   /* prefix each output line with its row number from 1 and ':' */
#define KOLMO_Q_COUNT   8   /* output nothing; the return value counts the rows selected */
#define KOLMO_Q_NOINDEX 16  /* ignore any index: decode every block in order (also how files without IDX1 are read) */
#define KOLMO_Q_WORD    32  /* the pattern matches whole words only (no letter, digit or '_' on either side); with a
                               fixed string, the filter can then rule blocks out by whole tokens (IDs, hosts, names) */
typedef struct {
    uint32_t blocks, blocks_decoded;      /* blocks in the file, blocks decoded whole */
    uint32_t blocks_columnar;             /* column-split blocks read through their column streams only */
    uint32_t blocks_skipped_filter;       /* blocks the trigram filter ruled out */
    uint64_t bytes_total;                 /* decoded size of the file */
    uint64_t bytes_decoded;               /* bytes produced by the block and stream decoders */
    uint64_t stored_read;                 /* stored (compressed) bytes those decoders read */
    uint64_t rows_out;                    /* rows written (or counted) */
    uint64_t index_bytes;                 /* size of the IDX1 section, 0 without one */
} kolmo_query_stats;
/* output sink: return 0 to continue, nonzero to stop (kolmo_query then returns KOLMO_ERR_IO) */
typedef int (*kolmo_sink)(void *ctx, const uint8_t *p, size_t n);
/* Returns the rows written (or counted), or a negative error: KOLMO_ERR_CORRUPT or KOLMO_ERR_CHECKSUM for a damaged
 * frame or index (an index that disagrees with the blocks it describes is an error, never a wrong answer). */
int64_t kolmo_query_run(const uint8_t *src, size_t n, const kolmo_query *q, kolmo_sink out, void *ctx,
                        kolmo_query_stats *stats);
/* kind, delimiter and column names of an indexed file: names written to `names` separated by '\n' (up to cap bytes);
 * returns the number of rows, or 0 for a file without IDX1 (then *kind = 0), or a negative error */
int64_t kolmo_index_info(const uint8_t *src, size_t n, int *kind, uint8_t *delim, char *names, size_t cap);

/* --- Deduplicating archives (container extension DDP1; src/core/dedup.c, SPEC.md "Chunk store") ------------------
 * Many files in one frame: each file is cut into content-defined chunks (Gear hash, FastCDC normalized chunking;
 * text cuts move to the next line end), every distinct chunk is stored once in a chunk store, and the store is
 * compressed as one ordinary frame, so the structure transforms still apply to the unique data. The DDP1 section
 * holds the file table and each file's list of chunks. Any file is extracted by decoding only the blocks its chunks
 * lie in. */
typedef struct {
    uint32_t avg_chunk;       /* target chunk size, power of two from 256 B to 4 MiB; 0 = 4 KiB */
    int line_cuts;            /* 1 (default when NULL): in text files, cuts move to the next '\n' (at most 4 KiB on) */
} kolmo_archive_params;
typedef struct { const char *name; const uint8_t *data; size_t size; uint32_t mode; int64_t mtime; } kolmo_archive_file;
typedef struct { uint64_t files, input_bytes, chunks, unique_chunks, unique_bytes, store_frame, table_bytes; } kolmo_archive_stats;
/* a bound for the capacity kolmo_archive_create needs */
size_t  kolmo_archive_bound(const kolmo_archive_file *files, size_t nfiles, const kolmo_params *p);
int64_t kolmo_archive_create(const kolmo_archive_file *files, size_t nfiles, uint8_t *dst, size_t cap,
                             const kolmo_params *p, const kolmo_archive_params *ap, kolmo_archive_stats *stats);
/* number of files in the archive, or a negative error */
int64_t kolmo_archive_count(const uint8_t *src, size_t n);
/* file i: its name (NUL-terminated, at most namecap bytes including the NUL), size, mode and mtime; 0 or an error */
int     kolmo_archive_stat(const uint8_t *src, size_t n, uint64_t i, char *name, size_t namecap, uint64_t *size,
                           uint32_t *mode, int64_t *mtime);
/* file i into dst (capacity at least its size); returns its size or a negative error. Each chunk is checked. */
int64_t kolmo_archive_extract(const uint8_t *src, size_t n, uint64_t i, uint8_t *dst, size_t cap, int threads);
/* every file: the chunk store is decoded once (memory: the store's size), then fn is called for each file in order with
 * its bytes; fn returns 0 to continue. Returns the number of files, or a negative error (KOLMO_ERR_IO if fn stopped). */
typedef int (*kolmo_archive_fn)(void *ctx, const char *name, const uint8_t *data, uint64_t size, uint32_t mode, int64_t mtime);
int64_t kolmo_archive_extract_all(const uint8_t *src, size_t n, kolmo_archive_fn fn, void *ctx, int threads);

#ifdef __cplusplus
}
#endif
#endif
