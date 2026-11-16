/* Block-sorting codec: LZP + BWT + QLFC.
   Copyright 2026 Adnane Aqartit and contributors. Apache-2.0.
   The codec 5 block format and its glue (bsc_compress, bsc_decompress, bsc_block_info, the BWT wrapper) are ported
   from libbsc 3.4.0, libbsc/libbsc.cpp and bwt/bwt.cpp: Copyright (c) 2009-2026 Ilya Grebnov
   <ilya.grebnov@gmail.com>, Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).

   Codec 5 block (all integers little-endian int32):
     0 block size (header included)   4 data size   8 mode   12 primary index   16 adler32(data)
     20 adler32(payload)              24 adler32(header bytes 0..23)
     payload: QLFC stream | sampled BWT indexes (4 * k bytes) | k (1 byte)
   mode = sorter (1 = BWT) | coder << 5 | lzp_min << 8 | lzp_hash << 16; mode 0 is a stored block. */
#include "bwt.h"
#include "../algo/bytes.h"
#include "../algo/lzp.h"
#include "../algo/qlfc.h"
#include "../algo/sais/libsais.h"
#include <stdlib.h>
#include <string.h>

/* ---- adler32 (zlib's definition; libbsc's SIMD version computes the same value) ---- */

uint32_t kolmo_adler32(const uint8_t *p, size_t n) {
    uint32_t a = 1, b = 0;
    while (n) {
        size_t k = n < 5552 ? n : 5552;            /* largest k with 255 k (k + 1) / 2 + (k + 1)(65520) < 2^32 */
        n -= k;
        while (k >= 16) {
            for (int i = 0; i < 16; i++) { a += p[i]; b += a; }
            p += 16; k -= 16;
        }
        while (k--) { a += *p++; b += a; }
        a %= 65521u; b %= 65521u;
    }
    return b << 16 | a;
}

/* ---- BWT wrappers (libbsc bwt.cpp) ---- */

static int bwt_mod(int n) {            /* sampling interval - 1 for the auxiliary indexes: largest power of two <= n/8, - 1 */
    int mod = n / 8;
    mod |= mod >> 1; mod |= mod >> 2; mod |= mod >> 4; mod |= mod >> 8; mod |= mod >> 16; mod >>= 1;
    return mod;
}

static int bwt_encode(uint8_t *T, int n, uint8_t *num_indexes, int32_t *indexes) {
    int32_t *A = (int32_t *)malloc((size_t)n * sizeof(int32_t) + 4);
    if (!A) return KB_NOT_ENOUGH_MEMORY;
    int32_t I[256];
    int mod = bwt_mod(n);
    int index = libsais_bwt_aux(T, T, A, n, 0, NULL, mod + 1, I);
    if (index == 0) {
        num_indexes[0] = (uint8_t)((n - 1) / (mod + 1));
        index = I[0]; for (int t = 0; t < num_indexes[0]; ++t) indexes[t] = I[t + 1] - 1;
    }
    free(A);
    if (index == -1) return KB_BAD_PARAMETER;
    if (index == -2) return KB_NOT_ENOUGH_MEMORY;
    return index;
}

static int bwt_decode(uint8_t *T, int n, int index, uint8_t num_indexes, const int32_t *indexes) {
    if (!T || n < 0 || index <= 0 || index > n) return KB_BAD_PARAMETER;
    if (n <= 1) return KB_OK;
    int32_t *P = (int32_t *)malloc(((size_t)n + 1) * sizeof(int32_t));
    if (!P) return KB_NOT_ENOUGH_MEMORY;
    int mod = bwt_mod(n), r;
    if (num_indexes == (uint8_t)((n - 1) / (mod + 1)) && indexes) {
        int32_t I[256]; I[0] = index;
        for (int t = 0; t < num_indexes; ++t) I[t + 1] = indexes[t] + 1;
        r = libsais_unbwt_aux(T, T, P, n, NULL, mod + 1, I);
    } else r = libsais_unbwt(T, T, P, n, NULL, index);
    free(P);
    if (r == -1) return KB_BAD_PARAMETER;
    if (r == -2) return KB_NOT_ENOUGH_MEMORY;
    return KB_OK;
}

/* ---- codec 5: libbsc block ---- */

static int bsc_mode(int lzp_hash, int lzp_min, int coder) {
    if (coder != KOLMO_QLFC_STATIC && coder != KOLMO_QLFC_ADAPTIVE) return -1;
    int mode = 1 | coder << 5;
    if (lzp_min != 0 || lzp_hash != 0) {
        if (lzp_min < 4 || lzp_min > 255 || lzp_hash < 10 || lzp_hash > 28) return -1;
        mode += lzp_min << 8; mode += lzp_hash << 16;
    }
    return mode;
}

static int bsc_store(const uint8_t *in, uint8_t *out, int n) {
    uint32_t a = kolmo_adler32(in, (size_t)n);
    memmove(out + KOLMO_BSC_HEADER_SIZE, in, (size_t)n);
    kb_st32(out + 0, (uint32_t)(n + KOLMO_BSC_HEADER_SIZE)); kb_st32(out + 4, (uint32_t)n);
    kb_st32(out + 8, 0); kb_st32(out + 12, 0);
    kb_st32(out + 16, a); kb_st32(out + 20, a);
    kb_st32(out + 24, kolmo_adler32(out, 24));
    return n + KOLMO_BSC_HEADER_SIZE;
}

int kolmo_bsc_compress(const uint8_t *in, uint8_t *out, int n, int lzp_hash, int lzp_min, int coder, int threads) {
    int mode = bsc_mode(lzp_hash, lzp_min, coder);
    if (mode < 0 || in == out) return KB_BAD_PARAMETER;
    if (n < 0 || n > 1073741824) return KB_BAD_PARAMETER;
    if (n <= KOLMO_BSC_HEADER_SIZE) return bsc_store(in, out, n);
    int lz_size = 0;
    if (mode != (mode & 0xff)) {
        lz_size = lzp_min >= 16 ? kolmo_lzp_compress(in, out, n, lzp_hash, lzp_min, threads) : KB_NOT_SUPPORTED;
        if (lz_size == KB_NOT_ENOUGH_MEMORY) return lz_size;
        if (lz_size < 0) mode &= 0xff;
    }
    if (mode == (mode & 0xff)) { lz_size = n; memcpy(out, in, (size_t)n); }

    int32_t indexes[256]; uint8_t num_indexes = 0;
    int index = bwt_encode(out, lz_size, &num_indexes, indexes);
    if (n < 64 * 1024) num_indexes = 0;
    if (index < KB_OK) return index;

    uint8_t *buffer = (uint8_t *)malloc((size_t)lz_size + 4096);
    if (!buffer) return KB_NOT_ENOUGH_MEMORY;
    int result = kolmo_qlfc_compress(out, buffer, lz_size, coder, threads);
    if (result >= KB_OK && result + 1 + 4 * num_indexes < n) memcpy(out + KOLMO_BSC_HEADER_SIZE, buffer, (size_t)result);
    free(buffer);
    if (result == KB_NOT_ENOUGH_MEMORY) return result;
    if (result < KB_OK || result + 1 + 4 * num_indexes >= n) return bsc_store(in, out, n);
    for (int t = 0; t < num_indexes; t++) kb_st32(out + KOLMO_BSC_HEADER_SIZE + result + 4 * t, (uint32_t)indexes[t]);
    out[KOLMO_BSC_HEADER_SIZE + result + 4 * num_indexes] = num_indexes;
    result += 1 + 4 * num_indexes;
    kb_st32(out + 0, (uint32_t)(result + KOLMO_BSC_HEADER_SIZE)); kb_st32(out + 4, (uint32_t)n);
    kb_st32(out + 8, (uint32_t)mode); kb_st32(out + 12, (uint32_t)index);
    kb_st32(out + 16, kolmo_adler32(in, (size_t)n));
    kb_st32(out + 20, kolmo_adler32(out + KOLMO_BSC_HEADER_SIZE, (size_t)result));
    kb_st32(out + 24, kolmo_adler32(out, 24));
    return result + KOLMO_BSC_HEADER_SIZE;
}

int kolmo_bsc_block_info(const uint8_t *hdr, int hdr_size, int *block_size, int *data_size) {
    if (hdr_size < KOLMO_BSC_HEADER_SIZE) return KB_UNEXPECTED_EOB;
    if (kb_ld32(hdr + 24) != kolmo_adler32(hdr, 24)) return KB_DATA_CORRUPT;
    int bs = (int)kb_ld32(hdr + 0), ds = (int)kb_ld32(hdr + 4), mode = (int)kb_ld32(hdr + 8), index = (int)kb_ld32(hdr + 12);
    int lzp_hash = (mode >> 16) & 0xff, lzp_min = (mode >> 8) & 0xff, coder = (mode >> 5) & 0x7, sorter = mode & 0x1f;
    int test_mode = 0;
    if (sorter == 1) test_mode = 1; else if (sorter > 0) return KB_DATA_CORRUPT;
    if (coder == 1 || coder == 2 || coder == 3) test_mode += coder << 5; else if (coder > 0) return KB_DATA_CORRUPT;
    if (lzp_min != 0 || lzp_hash != 0) {
        if (lzp_min < 4 || lzp_min > 255 || lzp_hash < 10 || lzp_hash > 28) return KB_DATA_CORRUPT;
        test_mode += lzp_min << 8; test_mode += lzp_hash << 16;
    }
    if (test_mode != mode) return KB_DATA_CORRUPT;
    if (ds < 0 || bs < KOLMO_BSC_HEADER_SIZE || bs > KOLMO_BSC_HEADER_SIZE + ds) return KB_DATA_CORRUPT;
    if (index < 0 || index > ds) return KB_DATA_CORRUPT;
    if (block_size) *block_size = bs;
    if (data_size) *data_size = ds;
    return KB_OK;
}

int kolmo_bsc_decompress(const uint8_t *in, int in_size, uint8_t *out, int out_size, int threads) {
    int block_size = 0, data_size = 0;
    int info = kolmo_bsc_block_info(in, in_size, &block_size, &data_size);
    if (info != KB_OK) return info;
    if (in == out) return KB_BAD_PARAMETER;
    if (in_size < block_size || out_size < data_size) return KB_UNEXPECTED_EOB;
    if (kb_ld32(in + 20) != kolmo_adler32(in + KOLMO_BSC_HEADER_SIZE, (size_t)(block_size - KOLMO_BSC_HEADER_SIZE))) return KB_DATA_CORRUPT;
    int mode = (int)kb_ld32(in + 8);
    if (mode == 0) {
        if (block_size != KOLMO_BSC_HEADER_SIZE + data_size) return KB_DATA_CORRUPT;
        memcpy(out, in + KOLMO_BSC_HEADER_SIZE, (size_t)data_size); return KB_OK;
    }
    int index = (int)kb_ld32(in + 12);
    uint32_t adler_data = kb_ld32(in + 16);
    if (block_size < KOLMO_BSC_HEADER_SIZE + 1) return KB_DATA_CORRUPT;
    uint8_t num_indexes = in[block_size - 1];
    int coded = block_size - KOLMO_BSC_HEADER_SIZE - 1 - 4 * num_indexes;
    if (coded < 1) return KB_DATA_CORRUPT;
    int32_t indexes[256];
    for (int t = 0; t < num_indexes; t++) indexes[t] = (int32_t)kb_ld32(in + KOLMO_BSC_HEADER_SIZE + coded + 4 * t);
    int lzp_hash = (mode >> 16) & 0xff, lzp_min = (mode >> 8) & 0xff, coder = (mode >> 5) & 0x7;
    if (coder != KOLMO_QLFC_STATIC && coder != KOLMO_QLFC_ADAPTIVE) return KB_NOT_SUPPORTED;

    int lz_size = kolmo_qlfc_decompress(in + KOLMO_BSC_HEADER_SIZE, coded, out, data_size, coder, threads);
    if (lz_size < KB_OK) return lz_size;
    int r = bwt_decode(out, lz_size, index, num_indexes, indexes);
    if (r < KB_OK) return r;
    if (mode != (mode & 0xff)) {
        uint8_t *buffer = (uint8_t *)malloc((size_t)lz_size + 1);
        if (!buffer) return KB_NOT_ENOUGH_MEMORY;
        memcpy(buffer, out, (size_t)lz_size);
        r = kolmo_lzp_decompress(buffer, lz_size, out, data_size, lzp_hash, lzp_min, threads);
        free(buffer);
        if (r < KB_OK) return r;
        if (r != data_size) return KB_DATA_CORRUPT;
    } else if (lz_size != data_size) return KB_DATA_CORRUPT;
    return adler_data == kolmo_adler32(out, (size_t)data_size) ? KB_OK : KB_DATA_CORRUPT;
}
