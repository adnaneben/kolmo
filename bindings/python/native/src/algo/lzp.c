/*-----------------------------------------------------------*/
/* Lempel Ziv Prediction, ported to C from libbsc 3.4.0       */
/*-----------------------------------------------------------*/
/*
   Copyright (c) 2009-2026 Ilya Grebnov <ilya.grebnov@gmail.com>  (libbsc, lzp/lzp.cpp)
   C port and changes: Copyright 2026 Adnane Aqartit and contributors.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   Changes from libbsc:
   - C instead of C++; the x86-64/AArch64 "large" encoder (8-byte compares, 8-byte heuristic) is the only encoder and
     runs on every CPU with portable little-endian loads, so the output is the same everywhere (libbsc fell back to a
     different encoder with 4-byte compares on other CPUs, which wrote other bytes). The four-positions-per-step loop
     of libbsc is written position by position with the same step boundaries, so the output is byte-identical.
   - The decoder checks every read and write against the input and output bounds (libbsc trusted the stream).
   - Blocks are encoded and decoded on threads; the encoder's result is replayed in order so that it equals the
     single-threaded output exactly (see kolmo_lzp_compress).
*/
#include "lzp.h"
#include "bytes.h"
#include "par.h"
#include <stdlib.h>
#include <string.h>

#define LZP_MATCH_FLAG 0xf2

int kolmo_lzp_num_blocks(int n) {
    static const int blocks[] = {128, 96, 64, 48, 32, 24, 16, 12, 8, 6, 4, 2};
    for (unsigned i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++)
        if (n >= blocks[i] * blocks[i] * 65536) return blocks[i];
    return 1;
}

static inline uint32_t lzp_hash(uint32_t ctx, uint32_t mask) { return ((ctx >> 15) ^ ctx ^ (ctx >> 3)) & mask; }
static inline uint32_t lzp_ctx(const uint8_t *p) { return (uint32_t)p[-1] | (uint32_t)p[-2] << 8 | (uint32_t)p[-3] << 16 | (uint32_t)p[-4] << 24; }

/* Encode one block. Returns its size, or KB_NOT_COMPRESSIBLE if it does not fit before out_end - 8. *peak receives
   nothing: the encoder's success for a smaller capacity c is "size < c - 8" (the output pointer only grows). */
static int lzp_encode_block(const uint8_t *input, const uint8_t *input_end, uint8_t *output, uint8_t *output_end,
                            int32_t *lookup, int hash_size, int min_len) {
    if (input_end - input - min_len < 32) return KB_NOT_COMPRESSIBLE;
    const uint32_t mask = (1u << hash_size) - 1;
    const uint8_t *input_start = input, *output_start = output;
    const uint8_t *output_eob = output_end - 8, *input_minlen_end = input_end - min_len - 32;
    const uint8_t *heuristic = input;
    uint64_t heuristic_v = input < input_minlen_end ? kb_ld64(heuristic) : 0;

    for (int i = 0; i < 4; i++) *output++ = *input++;

    while (input < input_minlen_end && output < output_eob) {
        /* libbsc steps four positions at a time and checks the loop condition only between steps: any match or
           escape ends a step. Same here. */
        for (int k = 0; k < 4; k++) {
            const uint32_t idx = lzp_hash(lzp_ctx(input), mask);
            const int32_t value = lookup[idx];
            lookup[idx] = (int32_t)(input - input_start);
            const uint8_t next = *output = *input;
            if (value > 0) {
                const uint8_t *reference = input_start + value;
                if (kb_ld64(input + min_len - 8) == kb_ld64(reference + min_len - 8) && kb_ld64(input) == kb_ld64(reference)) {
                    if (heuristic > input && heuristic_v != kb_ld64(reference + (heuristic - input))) goto not_found;
                    long len = 8;
                    for (; input + len < input_minlen_end; len += 8) {
                        uint64_t m = kb_ld64(input + len) ^ kb_ld64(reference + len);
                        if (m) { len += kb_bsf64(m) / 8; break; }
                    }
                    if (len < min_len) { heuristic = input + len; heuristic_v = kb_ld64(heuristic); goto not_found; }
                    input += len; len -= min_len;
                    *output++ = LZP_MATCH_FLAG;
                    while (len >= 254) { len -= 254; *output++ = 254; if (output >= output_eob) break; }
                    *output++ = (uint8_t)len;
                    goto next_step;
                not_found:
                    input++; output++;
                    if (next == LZP_MATCH_FLAG) *output++ = 255;
                    goto next_step;
                }
                if (next == LZP_MATCH_FLAG) { input++; output++; *output++ = 255; goto next_step; }
            }
            input++; output++;
        }
    next_step:;
    }

    uint32_t context = lzp_ctx(input);
    while (input < input_end && output < output_eob) {
        const uint32_t idx = lzp_hash(context, mask);
        const int32_t value = lookup[idx];
        lookup[idx] = (int32_t)(input - input_start);
        const uint8_t next = *output++ = *input++;
        context = context << 8 | next;
        if (next == LZP_MATCH_FLAG && value > 0) *output++ = 255;
    }
    return output >= output_eob ? KB_NOT_COMPRESSIBLE : (int)(output - output_start);
}

/* Decode one block of in[0..in_size) to out (capacity out_cap). */
static int lzp_decode_block(const uint8_t *input, const uint8_t *input_end, uint8_t *output, uint8_t *output_end,
                            int32_t *lookup, int hash_size, int min_len) {
    if (input_end - input < 4) return KB_UNEXPECTED_EOB;
    if (output_end - output < 4) return KB_DATA_CORRUPT;
    const uint32_t mask = (1u << hash_size) - 1;
    uint8_t *output_start = output;
    for (int i = 0; i < 4; i++) *output++ = *input++;
    uint32_t context = lzp_ctx(output);
    const uint64_t flags8 = 0x0101010101010101ull * LZP_MATCH_FLAG;
    while (input < input_end) {
        /* Eight literals without the flag byte: copy them and record their eight positions at once. */
        if (input_end - input >= 8 && output_end - output >= 8) {
            uint64_t v; memcpy(&v, input, 8);
            uint64_t f = v ^ flags8;
            if (((f - 0x0101010101010101ull) & ~f & 0x8080808080808080ull) == 0) {
                memcpy(output, input, 8);
                for (int k = 0; k < 8; k++) {
                    lookup[lzp_hash(context, mask)] = (int32_t)(output - output_start + k);
                    context = context << 8 | input[k];
                }
                input += 8; output += 8;
                continue;
            }
        }
        const uint32_t idx = lzp_hash(context, mask);
        const int32_t value = lookup[idx];
        lookup[idx] = (int32_t)(output - output_start);
        if (*input == LZP_MATCH_FLAG && value > 0) {
            if (++input >= input_end) return KB_DATA_CORRUPT;
            if (*input != 255) {
                long len = min_len;
                for (;;) {
                    if (input >= input_end) return KB_DATA_CORRUPT;
                    len += *input;
                    if (*input++ != 254) break;
                }
                if (len > output_end - output) return KB_DATA_CORRUPT;
                const uint8_t *reference = output_start + value;    /* value < output - output_start: set earlier */
                uint8_t *end = output + len;
                if (output - reference >= 8) {
                    while (output + 8 <= end) { memcpy(output, reference, 8); output += 8; reference += 8; }
                }
                while (output < end) *output++ = *reference++;
                context = lzp_ctx(output);
            } else {
                input++;
                if (output >= output_end) return KB_DATA_CORRUPT;
                *output++ = LZP_MATCH_FLAG; context = context << 8 | LZP_MATCH_FLAG;
            }
        } else {
            if (output >= output_end) return KB_DATA_CORRUPT;
            context = context << 8 | (*output++ = *input++);
        }
    }
    return (int)(output - output_start);
}

/* ---- block-parallel driver ---- */

typedef struct {
    const uint8_t *in; uint8_t *buf; int n, nblocks, chunk, hash_size, min_len;
    int *result; int32_t **tables;
} lzp_enc_job;

static void lzp_enc_task(void *p, int b, int worker) {
    lzp_enc_job *j = (lzp_enc_job *)p;
    int start = b * j->chunk, size = b != j->nblocks - 1 ? j->chunk : j->n - start;
    int32_t *t = j->tables[worker];
    if (!t) { j->result[b] = KB_NOT_ENOUGH_MEMORY; return; }
    memset(t, 0, ((size_t)1 << j->hash_size) * sizeof(int32_t));
    j->result[b] = lzp_encode_block(j->in + start, j->in + start + size, j->buf + start, j->buf + start + size, t, j->hash_size, j->min_len);
}

int kolmo_lzp_compress(const uint8_t *in, uint8_t *out, int n, int hash_size, int min_len, int threads) {
    if (n < 0 || hash_size < 10 || hash_size > 28 || min_len < 16 || min_len > 255) return KB_BAD_PARAMETER;
    const size_t table_size = ((size_t)1 << hash_size) * sizeof(int32_t);
    const int nblocks = kolmo_lzp_num_blocks(n);
    if (nblocks == 1) {
        int32_t *t = (int32_t *)calloc(1, table_size);
        if (!t) return KB_NOT_ENOUGH_MEMORY;
        int r = n > 2 ? lzp_encode_block(in, in + n, out + 1, out + n - 1, t, hash_size, min_len) : KB_NOT_COMPRESSIBLE;
        free(t);
        if (r >= 0) { out[0] = 1; r += 1; }
        return r;
    }
    /* Each block is encoded on its own (fresh table) into a scratch buffer with its full size as capacity. libbsc
       encodes them in order with capacity min(size, n - written): the encoder's output pointer only grows, so that run
       succeeds exactly when the unlimited run's size is below that capacity - 8, and then writes the same bytes. */
    const int chunk = n / nblocks;
    if (threads > nblocks) threads = nblocks;
    if (threads < 1) threads = 1;
    int result[256]; int32_t *tables[256];
    uint8_t *buf = (uint8_t *)malloc((size_t)n);
    int ok = buf != NULL;
    for (int t = 0; t < threads; t++) { tables[t] = ok ? (int32_t *)malloc(table_size) : NULL; ok &= tables[t] != NULL; }
    if (!ok) { for (int t = 0; t < threads; t++) free(tables[t]); free(buf); return KB_NOT_ENOUGH_MEMORY; }
    lzp_enc_job j = {in, buf, n, nblocks, chunk, hash_size, min_len, result, tables};
    kolmo_par_for(threads, nblocks, lzp_enc_task, &j);
    for (int t = 0; t < threads; t++) free(tables[t]);

    int out_ptr = 1 + 8 * nblocks;
    out[0] = (uint8_t)nblocks;
    for (int b = 0; b < nblocks; b++) {
        int start = b * chunk, size = b != nblocks - 1 ? chunk : n - start;
        int cap = size; if (cap > n - out_ptr) cap = n - out_ptr;
        int r = result[b];
        if (r == KB_NOT_ENOUGH_MEMORY) { free(buf); return r; }
        if (r >= 0 && r < cap - 8) memcpy(out + out_ptr, buf + start, (size_t)r);
        else {
            if (out_ptr + size >= n) { out_ptr = KB_NOT_COMPRESSIBLE; break; }
            r = size; memcpy(out + out_ptr, in + start, (size_t)size);
        }
        kb_st32(out + 1 + 8 * b, (uint32_t)size);
        kb_st32(out + 1 + 8 * b + 4, (uint32_t)r);
        out_ptr += r;
    }
    free(buf);
    return out_ptr;
}

typedef struct {
    const uint8_t *in; uint8_t *out; int nblocks, hash_size, min_len;
    const int *in_off, *in_size, *out_off, *out_size; int *result; int32_t **tables;
} lzp_dec_job;

static void lzp_dec_task(void *p, int b, int worker) {
    lzp_dec_job *j = (lzp_dec_job *)p;
    int r;
    if (j->in_size[b] != j->out_size[b]) {
        int32_t *t = j->tables[worker];
        if (!t) { j->result[b] = KB_NOT_ENOUGH_MEMORY; return; }
        memset(t, 0, ((size_t)1 << j->hash_size) * sizeof(int32_t));
        r = lzp_decode_block(j->in + j->in_off[b], j->in + j->in_off[b] + j->in_size[b], j->out + j->out_off[b],
                             j->out + j->out_off[b] + j->out_size[b], t, j->hash_size, j->min_len);
    } else {
        memcpy(j->out + j->out_off[b], j->in + j->in_off[b], (size_t)j->in_size[b]); r = j->in_size[b];
    }
    if (r >= 0 && r != j->out_size[b]) r = KB_DATA_CORRUPT;
    j->result[b] = r;
}

int kolmo_lzp_decompress(const uint8_t *in, int in_size, uint8_t *out, int out_cap, int hash_size, int min_len, int threads) {
    if (in_size < 1 || hash_size < 10 || hash_size > 28 || min_len < 4 || min_len > 255) return KB_DATA_CORRUPT;
    const size_t table_size = ((size_t)1 << hash_size) * sizeof(int32_t);
    int nblocks = in[0];
    if (nblocks == 1) {
        int32_t *t = (int32_t *)calloc(1, table_size);
        if (!t) return KB_NOT_ENOUGH_MEMORY;
        int r = lzp_decode_block(in + 1, in + in_size, out, out + out_cap, t, hash_size, min_len);
        free(t);
        return r;
    }
    if (nblocks == 0 || in_size < 1 + 8 * nblocks) return KB_DATA_CORRUPT;
    int in_off[256], insz[256], out_off[256], outsz[256], result[256];
    long ip = 1 + 8 * nblocks, op = 0;
    for (int b = 0; b < nblocks; b++) {
        int32_t o = (int32_t)kb_ld32(in + 1 + 8 * b), c = (int32_t)kb_ld32(in + 1 + 8 * b + 4);
        if (o < 0 || c < 0 || ip + c > in_size || op + o > out_cap) return KB_DATA_CORRUPT;
        in_off[b] = (int)ip; insz[b] = c; out_off[b] = (int)op; outsz[b] = o;
        ip += c; op += o;
    }
    if (threads > nblocks) threads = nblocks;
    if (threads < 1) threads = 1;
    int32_t *tables[256]; int ok = 1;
    for (int t = 0; t < threads; t++) { tables[t] = ok ? (int32_t *)malloc(table_size) : NULL; ok &= tables[t] != NULL; }
    if (!ok) { for (int t = 0; t < threads; t++) free(tables[t]); return KB_NOT_ENOUGH_MEMORY; }
    lzp_dec_job j = {in, out, nblocks, hash_size, min_len, in_off, insz, out_off, outsz, result, tables};
    kolmo_par_for(threads, nblocks, lzp_dec_task, &j);
    for (int t = 0; t < threads; t++) free(tables[t]);
    for (int b = 0; b < nblocks; b++) if (result[b] < 0) return result[b];
    return (int)op;
}
