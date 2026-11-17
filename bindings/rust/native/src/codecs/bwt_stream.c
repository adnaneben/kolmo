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
#include "bwt_stream.h"
#include "../algo/bytes.h"
#include "../algo/lzp.h"
#include "../algo/qlfc.h"
#include "../algo/sais/sais.h"
#include "../algo/qlfc.h"
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

static int bwt_encode(uint8_t *T, int n, uint8_t *num_indexes, int32_t *indexes, int threads) {
    int32_t *A = (int32_t *)malloc((size_t)n * sizeof(int32_t) + 4);
    if (!A) return KB_NOT_ENOUGH_MEMORY;
    int32_t I[256];
    int mod = bwt_mod(n);
    int index = kolmo_sais_bwt_aux(T, T, A, n, mod + 1, I, threads);
    if (index == 0) {
        num_indexes[0] = (uint8_t)((n - 1) / (mod + 1));
        index = I[0]; for (int t = 0; t < num_indexes[0]; ++t) indexes[t] = I[t + 1] - 1;
    }
    free(A);
    if (index == -1) return KB_BAD_PARAMETER;
    if (index == -2) return KB_NOT_ENOUGH_MEMORY;
    return index;
}

static int bwt_decode(uint8_t *T, int n, int index, uint8_t num_indexes, const int32_t *indexes, int threads) {
    if (!T || n < 0 || index <= 0 || index > n) return KB_BAD_PARAMETER;
    if (n <= 1) return KB_OK;
    int32_t *P = (int32_t *)malloc(((size_t)n + 1) * sizeof(int32_t));
    if (!P) return KB_NOT_ENOUGH_MEMORY;
    int mod = bwt_mod(n), r;
    if (num_indexes == (uint8_t)((n - 1) / (mod + 1)) && indexes) {
        int32_t I[256]; I[0] = index;
        for (int t = 0; t < num_indexes; ++t) I[t + 1] = indexes[t] + 1;
        r = kolmo_sais_unbwt_aux(T, T, P, n, mod + 1, I, threads);
    } else r = kolmo_sais_unbwt(T, T, P, n, index, threads);
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
    int index = bwt_encode(out, lz_size, &num_indexes, indexes, threads);
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
    int r = bwt_decode(out, lz_size, index, num_indexes, indexes, threads);
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

/* ---- codec 18: Kolmo's block-sorting stream ----
   payload: flags u8 | [lzp_hash u8 | lzp_min u8] | primary index u32 | log2 of the index sampling interval u8 |
            k u8 | k x sampled index u32 | QLFC stream (kolmo_qlfc_compress layout)
   flags: bits 0-1 coder (1 static, 2 adaptive QLFC), bit 2 LZP. All integers little-endian. The raw size comes from
   the container, whose block checksum covers the decoded bytes: there are no adler32 fields, no 28-byte header and no
   stored fallback (the container stores a block that does not shrink). Every stage decoder is bounds-checked. */

int kolmo_bwt_compress(const uint8_t *in, uint8_t *out, size_t cap, int n, const kolmo_bwt_params *p, int threads) {
    if (n < 2 || !p || (p->coder != KOLMO_QLFC_STATIC && p->coder != KOLMO_QLFC_ADAPTIVE)) return KB_BAD_PARAMETER;
    if (p->lzp_min && (p->lzp_min < 16 || p->lzp_min > 255 || p->lzp_hash < 10 || p->lzp_hash > 28)) return KB_BAD_PARAMETER;
    uint8_t *work = (uint8_t *)malloc((size_t)n + 64);
    if (!work) return KB_NOT_ENOUGH_MEMORY;
    int lz = KB_NOT_COMPRESSIBLE;
    if (p->lzp_min && n >= p->lzp_min + 64) lz = kolmo_lzp_compress(in, work, n, p->lzp_hash, p->lzp_min, threads);
    if (lz == KB_NOT_ENOUGH_MEMORY) { free(work); return lz; }
    int use_lzp = lz > 0;
    if (!use_lzp) { memcpy(work, in, (size_t)n); lz = n; }

    int rlog = p->index_log2 > 0 ? p->index_log2 : 0;
    if (!rlog) { int mod = bwt_mod(lz); rlog = 0; while ((2 << rlog) <= mod + 1) rlog++; if (rlog < 1) rlog = 1; }
    else if (rlog < 1) rlog = 1;
    while (rlog < 30 && (((int64_t)lz - 1) >> rlog) > 255) rlog++;        /* at most 255 sampled indexes */
    int32_t I[256]; int32_t r = (int32_t)1 << rlog;
    int32_t *A = (int32_t *)malloc((size_t)lz * sizeof(int32_t) + 4);
    if (!A) { free(work); return KB_NOT_ENOUGH_MEMORY; }
    int rc = kolmo_sais_bwt_aux(work, work, A, lz, r, I, threads);
    free(A);
    if (rc != 0) { free(work); return rc == -2 ? KB_NOT_ENOUGH_MEMORY : KB_BAD_PARAMETER; }
    int k = (lz - 1) / r;

    size_t head = 1 + (use_lzp ? 2 : 0) + 4 + 1 + 1 + 4 * (size_t)k;
    if (cap < head + 64) { free(work); return KB_NOT_COMPRESSIBLE; }
    uint8_t *q = out;
    *q++ = (uint8_t)(p->coder | (use_lzp ? 4 : 0));
    if (use_lzp) { *q++ = (uint8_t)p->lzp_hash; *q++ = (uint8_t)p->lzp_min; }
    kb_st32(q, (uint32_t)I[0]); q += 4;
    *q++ = (uint8_t)rlog; *q++ = (uint8_t)k;
    for (int t = 0; t < k; t++) { kb_st32(q, (uint32_t)I[t + 1]); q += 4; }
    /* the QLFC coder needs lz + 64 bytes of room; code into scratch when the caller's buffer is tighter */
    int res;
    if (cap - head >= (size_t)lz + 64) res = kolmo_qlfc_compress_min(work, q, lz, p->coder, p->min_block, threads);
    else {
        uint8_t *tmp = (uint8_t *)malloc((size_t)lz + 64);
        if (!tmp) { free(work); return KB_NOT_ENOUGH_MEMORY; }
        res = kolmo_qlfc_compress_min(work, tmp, lz, p->coder, p->min_block, threads);
        if (res > 0 && (size_t)res <= cap - head) memcpy(q, tmp, (size_t)res); else if (res > 0) res = KB_NOT_COMPRESSIBLE;
        free(tmp);
    }
    free(work);
    if (res < 0) return res;
    return (int)(head + (size_t)res);
}

int kolmo_bwt_decompress(const uint8_t *in, size_t in_size, uint8_t *out, int n, int threads) {
    if (n < 2 || in_size < 7) return KB_DATA_CORRUPT;
    const uint8_t *q = in, *end = in + in_size;
    int flags = *q++, coder = flags & 3, use_lzp = (flags >> 2) & 1, lzp_hash = 0, lzp_min = 0;
    if (flags >> 3 || (coder != KOLMO_QLFC_STATIC && coder != KOLMO_QLFC_ADAPTIVE)) return KB_DATA_CORRUPT;
    if (use_lzp) { lzp_hash = *q++; lzp_min = *q++; if (lzp_hash < 10 || lzp_hash > 28 || lzp_min < 16) return KB_DATA_CORRUPT; }
    if (end - q < 6) return KB_DATA_CORRUPT;
    int32_t I[256]; I[0] = (int32_t)kb_ld32(q); q += 4;
    int rlog = *q++, k = *q++;
    if (rlog < 1 || rlog > 30 || end - q < 4 * k + 1) return KB_DATA_CORRUPT;
    for (int t = 0; t < k; t++) { I[t + 1] = (int32_t)kb_ld32(q); q += 4; }
    uint8_t *lzbuf = use_lzp ? (uint8_t *)malloc((size_t)n + 64) : out;
    if (!lzbuf) return KB_NOT_ENOUGH_MEMORY;
    int lz = kolmo_qlfc_decompress(q, (int)(end - q), lzbuf, n, coder, threads);
    int r = KB_OK;
    if (lz < 0) r = lz;
    else if ((!use_lzp && lz != n) || lz < 2) r = KB_DATA_CORRUPT;
    else if ((lz - 1) >> rlog != k) r = KB_DATA_CORRUPT;
    else {
        int32_t *P = (int32_t *)malloc(((size_t)lz + 1) * sizeof(int32_t));
        if (!P) r = KB_NOT_ENOUGH_MEMORY;
        else {
            if (kolmo_sais_unbwt_aux(lzbuf, lzbuf, P, lz, (int32_t)1 << rlog, I, threads) != 0) r = KB_DATA_CORRUPT;
            free(P);
        }
    }
    if (r == KB_OK && use_lzp) {
        int d = kolmo_lzp_decompress(lzbuf, lz, out, n, lzp_hash, lzp_min, threads);
        if (d != n) r = d < 0 ? d : KB_DATA_CORRUPT;
    }
    if (use_lzp) free(lzbuf);
    return r;
}
