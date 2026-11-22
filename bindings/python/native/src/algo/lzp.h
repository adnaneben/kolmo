/* LZP (Lempel-Ziv prediction) preprocessor, ported to C from libbsc.
   Copyright (c) 2009-2026 Ilya Grebnov (libbsc), Apache-2.0. C port and changes: Copyright 2026 Adnane Aqartit and
   contributors, Apache-2.0. */
#ifndef KOLMO_ALGO_LZP_H
#define KOLMO_ALGO_LZP_H
#include <stdint.h>

/* Compress in[0..n) to out (capacity n): out[0] = number of LZP blocks, then per block {raw size, coded size} (two
   little-endian int32), then the blocks. Returns the size, or KB_NOT_COMPRESSIBLE when the result would not be smaller
   than n. hash_size 10..28, min_len 16..255. The output does not depend on `threads` and equals libbsc's
   bsc_lzp_compress (single-threaded build) for every hash_size <= 17 and min_len > 16, on any CPU. */
int kolmo_lzp_compress(const uint8_t *in, uint8_t *out, int n, int hash_size, int min_len, int threads);

/* Decompress in[0..in_size) to out (capacity out_cap). Returns the decoded size or a negative KB_ error. Every read and
   write is bounds-checked: a corrupt stream returns KB_DATA_CORRUPT. */
int kolmo_lzp_decompress(const uint8_t *in, int in_size, uint8_t *out, int out_cap, int hash_size, int min_len, int threads);

/* Number of independent LZP blocks for an input of n bytes (libbsc's break points). */
int kolmo_lzp_num_blocks(int n);
#endif
