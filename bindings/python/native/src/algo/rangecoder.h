/*-----------------------------------------------------------*/
/* Binary range coder with 16-bit output, from libbsc 3.4.0   */
/*-----------------------------------------------------------*/
/*
   Copyright (c) 2009-2026 Ilya Grebnov <ilya.grebnov@gmail.com>  (libbsc, coder/common/rangecoder.h)
   C port and changes: Copyright 2026 Adnane Aqartit and contributors.

   Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
   the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
   an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
   specific language governing permissions and limitations under the License.

   Used by the QLFC coder. Probabilities are 12-bit probabilities of a 0 bit. Output words are little-endian on every
   host. Changes from libbsc: C; the decoder has an end pointer and reads zeros past it instead of reading out of
   bounds (a valid stream never reaches it, so decoding is unchanged); the encoder records the highest position its
   overflow check has seen, which lets block-parallel encoders reproduce the serial result (see qlfc.c).
*/
#ifndef KOLMO_ALGO_RANGECODER_H
#define KOLMO_ALGO_RANGECODER_H
#include <stdint.h>
#include "bytes.h"

typedef struct {
    uint64_t low;                  /* libbsc's union {low32, carry}: low32 = (uint32_t)low, carry = low >> 32 */
    uint32_t code, ffnum, cache, range;
    const uint8_t *in, *in_end;
    uint8_t *out, *out_start;
    long out_eob;                  /* encoder: output size - 16 */
} kolmo_rc;

static inline void rc_out16(kolmo_rc *c, uint32_t s) { kb_st16(c->out, (uint16_t)s); c->out += 2; }
static inline uint32_t rc_in16(kolmo_rc *c) {
    if (c->in + 2 <= c->in_end) { uint32_t v = kb_ld16(c->in); c->in += 2; return v; }
    if (c->in < c->in_end) { uint32_t v = *c->in; c->in = c->in_end; return v; }
    return 0;
}

static __attribute__((noinline)) uint32_t rc_shift_low_slow(kolmo_rc *c) {
    uint32_t low32 = (uint32_t)c->low, carry = (uint32_t)(c->low >> 32);
    if (low32 < 0xffff0000u || carry) {
        rc_out16(c, c->cache + carry);
        if (c->ffnum) { uint32_t s = carry - 1; do { rc_out16(c, s); } while (--c->ffnum); }
        c->cache = low32 >> 16; carry = 0;
    } else c->ffnum++;
    low32 <<= 16;
    c->low = (uint64_t)carry << 32 | low32;
    return c->range << 16;
}
static inline uint32_t rc_shift_low(kolmo_rc *c) {
    uint32_t low32 = (uint32_t)c->low;
    if (!c->ffnum && low32 < 0xffff0000u) {
        rc_out16(c, c->cache + (uint32_t)(c->low >> 32));
        c->cache = low32 >> 16; c->low = (uint32_t)(low32 << 16);
        return c->range << 16;
    }
    return rc_shift_low_slow(c);
}
/* Encoder overflow check of libbsc (CheckEOB): true when the pending output reaches the end - 16 bytes. */
static inline long rc_pending(const kolmo_rc *c) { return (long)(c->out - c->out_start) + 2 * (long)c->ffnum; }
static inline int rc_check_eob(const kolmo_rc *c) { return rc_pending(c) >= c->out_eob; }

static inline void rc_init_encoder(kolmo_rc *c, uint8_t *output, int output_size) {
    c->out_start = c->out = output; c->out_eob = (long)output_size - 16;
    c->low = 0; c->ffnum = 0; c->cache = 0; c->range = 0xffffffffu;
}
static inline int rc_finish_encoder(kolmo_rc *c) {
    if (c->range < 0x10000) rc_shift_low(c);
    rc_shift_low(c); rc_shift_low(c); rc_shift_low(c);
    return (int)(c->out - c->out_start);
}
static inline void rc_encode_bit0(kolmo_rc *c, int p) {
    if (c->range < 0x10000) c->range = rc_shift_low(c);
    c->range = (c->range >> 12) * (uint32_t)p;
}
static inline void rc_encode_bit1(kolmo_rc *c, int p) {
    if (c->range < 0x10000) c->range = rc_shift_low(c);
    uint32_t r = (c->range >> 12) * (uint32_t)p;
    c->low += r; c->range -= r;
}
static inline void rc_encode_bit(kolmo_rc *c, unsigned bit, int p) {   /* bit is 0 or 1 */
    if (c->range < 0x10000) c->range = rc_shift_low(c);
    uint32_t r = (c->range >> 12) * (uint32_t)p, m = ~bit + 1u;
    c->low = c->low + (m & r);
    c->range = r + (m & (c->range - r - r));
}
static inline void rc_encode_flat(kolmo_rc *c, unsigned bit) { if (bit) rc_encode_bit1(c, 2048); else rc_encode_bit0(c, 2048); }
static inline void rc_encode_word(kolmo_rc *c, uint32_t w) { for (int b = 31; b >= 0; --b) rc_encode_flat(c, w & (1u << b)); }

static inline void rc_init_decoder(kolmo_rc *c, const uint8_t *input, const uint8_t *input_end) {
    c->in = input; c->in_end = input_end; c->code = 0; c->range = 0xffffffffu;
    c->code = c->code << 16 | rc_in16(c);
    c->code = c->code << 16 | rc_in16(c);
    c->code = c->code << 16 | rc_in16(c);
}
static inline int rc_peek_bit(kolmo_rc *c, int p) {
    if (c->range < 0x10000) { c->range <<= 16; c->code = c->code << 16 | rc_in16(c); }
    return c->code >= (c->range >> 12) * (uint32_t)p;
}
static inline int rc_decode_bit(kolmo_rc *c, int p) {
    if (c->range < 0x10000) { c->range <<= 16; c->code = c->code << 16 | rc_in16(c); }
    uint32_t r = (c->range >> 12) * (uint32_t)p;
    int bit = c->code >= r;
    c->range = bit ? c->range - r : r;
    c->code = bit ? c->code - r : c->code;
    return bit;
}
static inline void rc_decode_bit0(kolmo_rc *c, int p) { c->range = (c->range >> 12) * (uint32_t)p; }
static inline void rc_decode_bit1(kolmo_rc *c, int p) { uint32_t r = (c->range >> 12) * (uint32_t)p; c->code -= r; c->range -= r; }
static inline uint32_t rc_decode_word(kolmo_rc *c) { uint32_t w = 0; for (int b = 31; b >= 0; --b) w += w + (uint32_t)rc_decode_bit(c, 2048); return w; }
#endif
