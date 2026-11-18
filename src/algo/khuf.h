/* Kolmo Huffman (khuf): Kolmo's own static Huffman coder, the graph leaves' Huffman coder (codec 27, and codec 30 under
   zstd; the default since wave 4) and the entropy stage of the native LZ (codec 29). Copyright 2026 Adnane Aqartit and
   contributors, Apache-2.0.

   A block of n <= KHUF_BLOCK_MAX bytes is coded with optimal length-limited canonical codes (package-merge; at most 12
   bits from KH_LEN12_MIN bytes, 11 below), most significant bit first.
   Block layout: table | S - 1 stream lengths (LEB128) | S streams, S = 4 when n >= KH_MULTI_MIN, else 1. Stream q
   holds symbols [q * ceil(n/S), min(n, (q+1) * ceil(n/S))); its codes are written one after another, the first in the
   top bits of its first byte, and its last byte is padded with zero bits.
   Table: byte 0 = longest code length L (bits 0-3, 1..12) | mode << 4, then by mode:
     0 (sparse): count - 1 u8, then count pairs (symbol, length);
     1 (nibbles): last symbol u8, then the lengths of symbols 0..last, two per byte, low nibble first;
     2 (coded): last symbol u8, then a bit stream, least significant bit first: for v = 0..L the length (3 bits, 0-7)
       of v's code in a canonical code of the values 0..L, then the lengths of symbols 0..last in that code (codes
       bit-reversed, as written least significant bit first). That code is complete, or one value of length 1.
   Codes are canonical: shorter codes first, equal lengths by symbol; the lengths must make a complete code (Kraft sum
   1) of at least two symbols. The encoder writes the smallest table mode.

   The chunked framing (kolmo_khuf_compress_chunks) is the leaf format of codec 27: chunks of up to KHUF_BLOCK_MAX
   bytes, each mode u8 (0 stored, 1 one repeated byte, 2 Huffman) | raw length uv | mode 0: the bytes, mode 1: the
   byte, mode 2: coded length uv and the block.

   Wave 4 changed the block format (was: lengths by a greedy limit of 11 bits, least significant bit first, a two-mode
   table); rc3 codec 27 and 29 frames written before it do not decode. */
#ifndef KOLMO_ALGO_KHUF_H
#define KOLMO_ALGO_KHUF_H
#include <stddef.h>
#include <stdint.h>

#define KHUF_MAXLEN 12
#define KHUF_STREAMS 4                  /* streams of a block of at least KH_MULTI_MIN bytes */
#define KH_MULTI_MIN 1024               /* smaller blocks: one stream */
#define KH_LEN12_MIN (16u << 10)        /* the encoder allows 12-bit codes from this block size, 11 bits below */
#define KHUF_BLOCK_MAX (128u << 10)

/* One block. Returns the coded size, or 0 when the block should not be Huffman-coded (fewer than two distinct bytes,
   or not smaller than n). cap must be at least n + 600 for the coder to try. */
size_t kolmo_khuf_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap);
/* Decodes exactly n bytes from in[0..cn); 0 on success, -1 when the block is corrupt. Every read is bounds-checked. */
int kolmo_khuf_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);

/* Chunked framing (codec 27). Bound: n + n / KHUF_BLOCK_MAX * 8 + 16. Returns the size or 0 (cap too small). */
size_t kolmo_khuf_bound(size_t n);
size_t kolmo_khuf_compress_chunks(const uint8_t *in, size_t n, uint8_t *out, size_t cap);
int kolmo_khuf_decompress_chunks(const uint8_t *in, size_t cn, uint8_t *out, size_t n);

/* Code lengths for a histogram (at most maxlen bits; 0 for absent symbols), as the encoder builds them. Returns the
   number of present symbols. Exposed for the native LZ's cost estimates. */
int kolmo_khuf_lengths(const uint32_t *cnt, int nsym, int maxlen, uint8_t *len);

/* Internal: the decoder's main loop over four streams (src/algo/khuf.c), for the assembly kernels and their test. */
typedef struct { uint64_t b[4], c[4]; const uint8_t *p[4]; uint8_t *o[4]; } kolmo_khuf_st4;
void kolmo_khuf_dec4_c(kolmo_khuf_st4 *st, size_t R, const uint64_t *dm, int W);
#endif
