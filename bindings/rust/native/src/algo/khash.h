/* khash64: Kolmo's own 64-bit block hash, the candidate replacement for XXH3-64 in block checksums.
   Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   Construction (written for Kolmo from the published NH and multiply-fold techniques, no xxHash code):
   - inputs up to 32 bytes: the bytes as two to four 64-bit words (overlapping loads), each pair folded through a
     64x64->128-bit multiply (high half xor low half), with the length and the seed;
   - longer inputs: eight 64-bit lanes over 64-byte stripes. Each lane adds NH(word xor key) = low32 x high32 (a 32x32->64
     product, the universal-hash step of UMAC's NH) and the raw word itself, so a word whose product is
     zero still changes the state. Every 16 stripes (1 KiB) each lane is scrambled (xorshift, multiply by an odd
     constant). The tail is the last 64 bytes of the input (overlapping), then the lanes, the length and the seed are
     folded by 128-bit multiplies and a final avalanche (xorshift-multiply).
   The C code is written so compilers vectorise the stripe loop (SSE2 / AVX2 / NEON vpmuludq/umull), giving the same
   value on every path; there is no hand-written kernel. */
#ifndef KOLMO_ALGO_KHASH_H
#define KOLMO_ALGO_KHASH_H
#include <stddef.h>
#include <stdint.h>
uint64_t kolmo_khash64(const void *p, size_t n, uint64_t seed);
#endif
