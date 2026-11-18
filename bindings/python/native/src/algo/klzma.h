/* KLZMA: Kolmo's own LZMA-class coder (codec 24, the LZMA2 role in `ratio` and the strong-parse path for `hc`).
   Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   An adaptive binary range coder (11-bit probabilities, shift-5 adaptation, carry propagated through a cached byte)
   over an LZ77 parse with repeat distances, written for Kolmo from the published descriptions of LZMA-class coding:
   - symbols: literal, match (length, distance), repeat match of one of the four last distances, short repeat (one
     byte at the last distance); a 12-state history of the last symbol kinds selects the probabilities;
   - literals: an 8-bit binary tree per context (pos low bits, top bits of the previous byte); after a match, coded
     against the byte at the last distance while the bits agree (three tree sets per context);
   - lengths 2..273: a choice bit, then 3 bits (2-9, per position state), 3 bits (10-17, per position state) or 8 bits;
   - distances: a 6-bit slot per length state (lengths 2, 3, 4, 5+), then reverse-tree extra bits for slots 4-13, or
     direct bits and a 4-bit reverse-tree low part.
   The parse is a price-based optimal parse over windows of up to 4 KiB positions (forward dynamic programming with
   the state and the repeat distances carried per node, integer bit prices, prices of lengths and distances refreshed
   every 128 coded lengths or matches), with hash chains (2-, 3- and 4-byte heads) as the match finder.
   Not bit-compatible with LZMA: the matched-literal trees, the price tables and the payload are Kolmo's own.

   Payload: properties u8 (lc + 9 lp + 45 pb, lc + lp <= 4, pb <= 4) | range-coded stream (5 flush bytes).
   Levels 1-9 set the match finder (hash chains at 1-2, binary trees from 3), its depth and the nice length
   (6: depth 48, nice 96; 9: depth 152, nice 273). */
#ifndef KOLMO_ALGO_KLZMA_H
#define KOLMO_ALGO_KLZMA_H
#include <stddef.h>
#include <stdint.h>
size_t kolmo_klzma_bound(size_t n);
/* level 1..9 with lc 3, lp 0, pb 2; or level >= 1000: 10 (100 (lc + 1) + 10 lp + pb) + parse level (codec 6's
   lc/lp/pb digits, then the level).
   Returns the payload size, or 0 when not smaller than n + 16 or out of memory. */
size_t kolmo_klzma_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level);
/* 0 when in[0..cn) decodes to exactly n bytes, else -1 (bounds-checked; reads past the end count as corrupt). */
int kolmo_klzma_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
#endif
