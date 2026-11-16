/* Block-sorting codec: LZP + BWT + QLFC. Codec 5 is libbsc's block format (rc2 frames, byte-identical to libbsc 3.4.0
   with Kolmo's parameters), written by Kolmo's own C code. Copyright 2026 Adnane Aqartit and contributors. Apache-2.0.
   Parts ported from libbsc, Copyright (c) 2009-2026 Ilya Grebnov, Apache-2.0. */
#ifndef KOLMO_CODECS_BWT_H
#define KOLMO_CODECS_BWT_H
#include <stddef.h>
#include <stdint.h>

#define KOLMO_BSC_HEADER_SIZE 28

/* libbsc bsc_compress (input != output): returns the block size (<= n + 28) or a negative KB_ error code. out must
   hold n + 28 bytes. lzp_hash/lzp_min 0 disables LZP; coder is KOLMO_QLFC_STATIC or KOLMO_QLFC_ADAPTIVE.
   The output does not depend on threads. */
int kolmo_bsc_compress(const uint8_t *in, uint8_t *out, int n, int lzp_hash, int lzp_min, int coder, int threads);
/* libbsc bsc_block_info. */
int kolmo_bsc_block_info(const uint8_t *hdr, int hdr_size, int *block_size, int *data_size);
/* libbsc bsc_decompress (input != output): KB_OK or a negative error code. */
int kolmo_bsc_decompress(const uint8_t *in, int in_size, uint8_t *out, int out_size, int threads);

uint32_t kolmo_adler32(const uint8_t *p, size_t n);
#endif
