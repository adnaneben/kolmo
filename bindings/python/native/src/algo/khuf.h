/* Kolmo Huffman (khuf): Kolmo's own static Huffman coder, the replacement for zstd's HUF in the graph leaves (codec 27)
   and the entropy stage of the native LZ (codec 29). Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   A block of up to KHUF_BLOCK_MAX bytes is coded with canonical, length-limited codes (at most KHUF_MAXLEN bits), the
   bits least significant first, in four interleaved streams (four independent decode chains per loop iteration).
   Block layout: table | three stream lengths (LEB128) | four streams. Table: byte k = number of coded symbols - 1;
   if k < 48: k + 1 pairs (symbol, length) [sparse]; else: last symbol s, then the lengths of symbols 0..s as nibbles,
   two per byte, low nibble first [dense]. Stream q holds symbols [q * ceil(n/4), min(n, (q+1) * ceil(n/4))).

   The chunked framing (kolmo_khuf_compress_chunks) is the leaf format of codec 27: chunks of up to KHUF_BLOCK_MAX
   bytes, each mode u8 (0 stored, 1 one repeated byte, 2 Huffman) | raw length uv | mode 0: the bytes, mode 1: the
   byte, mode 2: coded length uv and the block. */
#ifndef KOLMO_ALGO_KHUF_H
#define KOLMO_ALGO_KHUF_H
#include <stddef.h>
#include <stdint.h>

#define KHUF_MAXLEN 11
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
int kolmo_khuf_lengths(const uint32_t cnt[256], int nsym, int maxlen, uint8_t len[256]);
#endif
