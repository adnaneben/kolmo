/* KRANS: Kolmo's own static order-0 rANS for small-alphabet byte streams (the sequence-code streams of KLZH2).
   Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   Huffman codes cost at least one bit per symbol, so a stream where one symbol has probability p > 1/2 (the
   literal-length codes of an LZ parse: most sequences have no literals) loses up to 1 + log2(p) bits per symbol.
   Range-ANS codes it at its entropy.

   Payload: top symbol u8 (m) | frequencies of symbols 0..m, each a uv (0 = absent; they sum to 4096) | eight 32-bit
   states (little-endian) | byte lengths of word streams 0-6, each a uv | word streams 0-7 (stream 7: the rest).
   Coding: eight interleaved rANS states (symbol i uses state i mod 8), 12-bit precision, lower bound 2^16, renormalised
   16 bits at a time (high byte first) from the state's own word stream, so the eight decode chains are independent. The encoder runs backwards from the last symbol; every state ends at 2^16,
   which the decoder checks.
   A stream with one symbol is the table alone (the frequency 4096 codes nothing). */
#ifndef KOLMO_ALGO_KRANS_H
#define KOLMO_ALGO_KRANS_H
#include <stddef.h>
#include <stdint.h>
size_t kolmo_krans_bound(size_t n);
/* Return the payload size, or 0 when n is 0 or cap is too small. */
size_t kolmo_krans_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap);
/* The same, but returns 0 at once when the size estimated from the symbol counts is not below limit. */
size_t kolmo_krans_compress_lim(const uint8_t *in, size_t n, uint8_t *out, size_t cap, size_t limit);
/* 0 when in[0..cn) decodes to exactly n symbols and uses all of the payload, else -1 (every read is bounds-checked). */
int kolmo_krans_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
#endif
