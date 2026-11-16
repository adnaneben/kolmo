/* QLFC (quantized local frequency coding) entropy coder for BWT output, ported to C from libbsc.
   Copyright (c) 2009-2026 Ilya Grebnov (libbsc), Apache-2.0. C port and changes: Copyright 2026 Adnane Aqartit and
   contributors, Apache-2.0. */
#ifndef KOLMO_ALGO_QLFC_H
#define KOLMO_ALGO_QLFC_H
#include <stdint.h>

enum { KOLMO_QLFC_STATIC = 1, KOLMO_QLFC_ADAPTIVE = 2 };

/* libbsc's bsc_coder_compress: in[0..n) (BWT output) to out (capacity n + 64; the result is below n): out[0] = number of coder blocks, then per
   block {raw size, coded size} (little-endian int32), then the blocks (a block that does not shrink is stored).
   Returns the size or KB_NOT_COMPRESSIBLE. Blocks are coded on up to `threads` threads; the bytes do not depend on
   it and equal libbsc's single-threaded output. */
int kolmo_qlfc_compress(const uint8_t *in, uint8_t *out, int n, int coder, int threads);

/* Decode in[0..in_size) into out (capacity out_cap). Returns the decoded size or a negative KB_ error code. */
int kolmo_qlfc_decompress(const uint8_t *in, int in_size, uint8_t *out, int out_cap, int coder, int threads);

int kolmo_qlfc_num_blocks(int n);
#endif
