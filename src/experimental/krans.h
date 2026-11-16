/* KRANS: static interleaved rANS for byte streams (research library, not used by the kolmo binary or library).
 *
 * Order-0 rANS with 8, 16 or 32 interleaved 32-bit states and 16-bit renormalisation, and order-1 rANS (context =
 * previous byte) with the same state layout split into contiguous segments. Every block carries its own frequency
 * tables (static coding); nothing adapts while coding. See KRANS.md for the frame layout and measurements.
 *
 * The method is Jarek Duda's asymmetric numeral systems (rANS variant); the interleaving and the reciprocal
 * encoder follow Fabian Giesen's published descriptions; the order-1 segment layout follows the description of
 * htscodecs' rANS-Nx16. All code here was written for this project. */
#ifndef KRANS_H
#define KRANS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { KRANS_DEC_AUTO = 0, KRANS_DEC_SCALAR = 1, KRANS_DEC_AVX2 = 2, KRANS_DEC_NEON = 3 };

typedef struct {
    uint64_t blocks, stored, rle, o0, o1;   /* blocks by kind */
    uint64_t table_bytes;                   /* frequency-table bytes, all blocks */
    uint64_t o1_tables;                     /* order-1 tables written (own contexts + shared) */
    uint64_t prec_hist[16];                 /* blocks by table precision (bits) */
} krans_stats;

typedef struct {
    int order;          /* 0: order 0 only; 1: order 1 whenever the block is not stored or one byte; 2: choose per block (default) */
    int ways;           /* interleaved states: 8, 16 or 32 (default 32) */
    int prec;           /* table total 2^prec: 0 = choose per block (12 or 14 for order 0, 10 or 12 for order 1), or 10..14 */
    uint32_t block;     /* block size in bytes, 1 KiB .. 16 MiB (default 256 KiB) */
    int threads;        /* encoder threads over independent blocks (1 default) */
    krans_stats *stats; /* optional: filled by krans_compress */
} krans_params;

void krans_default_params(krans_params *p);
size_t krans_bound(size_t n, uint32_t block);
/* Returns the frame size, or 0 on error (bad parameters, capacity below krans_bound, out of memory). */
size_t krans_compress(const void *src, size_t n, void *dst, size_t cap, const krans_params *p);
/* Original size recorded in a frame; 0 on success. */
int krans_frame_size(const void *src, size_t cn, uint64_t *n);
/* Decodes a whole frame into dst (n must equal the recorded size). 0 on success, -1 on any malformed input. */
int krans_decompress(const void *src, size_t cn, void *dst, size_t n, int threads);

/* Decoder selection (process-wide). Returns 0 when the decoder is available on this machine and selected. */
int krans_set_decoder(int which);
int krans_decoder_available(int which);
int krans_decoder_active(void);       /* the decoder auto or the forced choice resolves to */
const char *krans_decoder_name(int which);

#ifdef __cplusplus
}
#endif
#endif
