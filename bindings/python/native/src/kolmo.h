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
#define KOLMO_ERR_PROFILE   (-9)   /* trained profile required, mismatched, or damaged */
#define KOLMO_ERR_TARGET    (-10)  /* --target expression or calibration table invalid */
#define KOLMO_ERR_QUERY     (-11)  /* a query names no such column, or its pattern does not compile */

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

/* --- Trained profiles (kolmo train) ---------------------------------------------------------------
 * A profile is learnt once from a sample of a dataset (files of one source: monthly CSV exports, rotated logs, JSON
 * messages of one API) and then used for every later file of that dataset. It holds the setting to use (mode, level,
 * analysis), the plans the analysis chose for each kind of file in the sample (pinned: matching files skip the
 * analysis), a zstd dictionary and a raw dictionary for Brotli and LZ4, and reserved room for context-mixing priors.
 * The serialized form (a .kpf file) is versioned and checksummed: SPEC.md, "Trained profiles (KPF1)".
 *
 * Frames written with a profile record its 32-bit id; decoding needs the same profile and fails with
 * KOLMO_ERR_PROFILE without it or with another one. A frame of a single block of at most 1 MiB is written in the
 * compact form ("CZC1", 16-17 bytes of header instead of 52), which is what makes small messages pay off.
 * A loaded profile is read-only and may be used by any number of concurrent calls. */
typedef struct kolmo_profile kolmo_profile;

typedef struct {
    kolmo_params params;   /* setting the profile is trained for (and records as its default) */
    size_t dict_size;      /* 0 = 110 KiB; dictionary budget for zstd (the raw dictionary is its content part) */
    int no_dict;           /* 1 = no dictionaries: plans and setting only */
    const char *target;    /* NULL, or a --target expression: the setting is chosen among the candidate settings by
                              measuring them on a held-out fifth of the samples (see kolmo_compress_target) */
    int quick;             /* 1 = the zstd --train dictionary trainer only (no COVER search, which can take a minute
                              on tens of MB of samples) */
} kolmo_train_params;

/* Train: samples laid end to end in `samples`, each length in `sizes`. Writes the serialized profile to dst (at most
 * cap bytes; kolmo_profile_bound gives enough) and returns its length, or a negative error. */
size_t  kolmo_profile_bound(const kolmo_train_params *tp);
int64_t kolmo_profile_train(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                            const kolmo_train_params *tp, uint8_t *dst, size_t cap);
/* Load (validate magic, version, CRC32C, id, every section) a serialized profile. 0 or KOLMO_ERR_PROFILE. */
int     kolmo_profile_load(const void *buf, size_t n, kolmo_profile **out);
void    kolmo_profile_free(kolmo_profile *prof);
uint32_t kolmo_profile_id(const kolmo_profile *prof);
/* The setting recorded in the profile (what kolmo -P uses unless -m or -l is given). */
void    kolmo_profile_params(const kolmo_profile *prof, kolmo_params *p);
/* Human-readable summary (training sample, sections, pinned plans). Returns the length written (NUL-terminated). */
size_t  kolmo_profile_describe(const kolmo_profile *prof, char *out, size_t cap);
/* As kolmo_compress / kolmo_decompress with a profile. p = NULL uses the profile's setting. The decoder also reads
 * frames written without a profile. */
int64_t kolmo_compress_profile(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                               const kolmo_profile *prof);
int64_t kolmo_decompress_profile(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                                 const kolmo_profile *prof);
/* 1 and *id when the frame was written with a profile, 0 when not, or a negative error. */
int     kolmo_frame_profile_id(const uint8_t *src, size_t n, uint32_t *id);

/* --- Targets (--target) ---------------------------------------------------------------------------
 * A target tells the planner what "best" means, and the planner picks, per chunk of the input (16 MiB by default,
 * cut where the analysis would cut blocks), the setting that is cheapest under it. Settings considered: speed (rt),
 * balanced without analysis (fast -n), balanced, eco, hc, ratio -l 1 (BWT), ratio, -x -l 1, -x.
 *   "size"                         smallest output, whatever the time
 *   "decode>=1GB/s", "encode>=50MB/s"   speed floors (one thread, predicted for the chunk from the calibration
 *                                  table); smallest output that meets them. Several terms are joined with ','.
 *   "cost:<formula>"               minimise a formula of size (bytes), n (input bytes), enc and dec (seconds, one
 *                                  thread, predicted from the calibration table): + - * / ( ) and numbers. Presets:
 *                                  cost:cloud[=months] (benchmark/TRADEOFFS.md model 3: S3 storage $0.023/GB-month,
 *                                  vCPU $0.054/h, 3.3 reads), cost:link=<rate> (model 2: enc + transfer + dec,
 *                                  e.g. link=1Gbit). The formula must not decrease when size grows.
 * Sizes are predicted from trial compressions of the chunk (the whole chunk up to 256 KiB, three 64 KiB slices above);
 * times (a fixed cost per call plus bytes over a rate, per setting and class of data) come from a calibration table
 * (kolmo_calibrate), or a built-in table measured on the reference machine.
 * The output is an ordinary frame; any kolmo decodes it. */
typedef struct kolmo_target kolmo_target;
int     kolmo_target_parse(const char *spec, kolmo_target **out);        /* 0 or KOLMO_ERR_TARGET */
/* Replace the built-in speed table with a calibration table (the text kolmo_calibrate writes). 0 or KOLMO_ERR_TARGET. */
int     kolmo_target_set_calibration(kolmo_target *t, const char *table, size_t n);
void    kolmo_target_free(kolmo_target *t);
/* Per-chunk report (optional): which setting each chunk got. */
typedef struct { uint32_t chunks; uint32_t per_setting[16]; double predicted_cost, predicted_enc_s, predicted_dec_s;
                 uint64_t predicted_size; } kolmo_target_report;
const char *kolmo_target_setting_name(int setting);                      /* 0..8, NULL past the end */
/* p: threads and, if non-zero, the chunk size (block); other fields ignored. prof may be NULL. */
int64_t kolmo_compress_target(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                              const kolmo_target *t, const kolmo_profile *prof, kolmo_target_report *report);
/* Measure encode and decode speed of every setting on built-in synthetic data of each class (or, with files, on
 * them) on one thread, and write the calibration table (text). Returns its length, or a negative error. */
int64_t kolmo_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                        char *out, size_t cap);

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
