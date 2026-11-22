/* KLZ: Kolmo's own LZ77 coders. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   Two formats share one match finder (hash table, or hash chains with lazy evaluation), written for Kolmo:

   KLZ (codec 28, the LZ4 role in `speed`): byte-aligned sequences, no entropy stage, decode limited by memory copies.
     sequence: token u8 = literal length (bits 0-2; 7: extended) | match length - 4 (bits 3-6; 15: extended) |
     wide (bit 7: 3-byte offset), extension bytes of the literal length (255 = continue), the literals, then unless the
     output is complete: offset (2 bytes, or 3 when wide, little-endian, 1 .. 2^24 - 1), extension bytes of the match
     length. The output ends after the literals of the sequence that fills it (its match fields are 0).

   KLZH (codec 29, the zstd role in `balanced`): the same parse with repeat offsets, coded as five streams:
     sequence count uv | literals | literal-length codes | match-length codes | offset codes (each a khuf chunk stream
     with its length uv in front, khuf.h) | extra bits (the rest; least significant bit first).
     Length v (literal length, match length - 4): v < 16 is code v; otherwise h = floor(log2 v), code
     16 + 2 (h - 4) + the bit below the top, h - 1 extra bits. Offset code: 0, 1, 2 the three most recent offsets
     (move to front), else 3 + h for offset o with h = floor(log2 o) and h extra bits. Literals after the last
     sequence are those left in the literal stream.

   Levels: KLZ 1 (hash table, skipping), 2 (larger table, every position), 3 (hash chains, lazy).
           KLZH 1-3 two hash tables (8- and 5-byte keys), greedy; 4 hash chains; 5-6 hash chains on 5-byte keys, the three
           repeat offsets tried at every position, two-step lazy evaluation; 7-9 a price-based optimal parse over a binary-
           tree match finder (encoder choices only: every level writes the same format). */
#ifndef KOLMO_ALGO_KLZ_H
#define KOLMO_ALGO_KLZ_H
#include <stddef.h>
#include <stdint.h>

#define KLZ_WINDOW ((1u << 24) - 1)
size_t kolmo_klz_bound(size_t n);
/* Return the coded size, or 0 when it does not fit in cap (kolmo_klz_bound(n) always suffices). */
size_t kolmo_klz_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level);
/* 0 when in[0..cn) decodes to exactly n bytes, else -1. Every read and write is bounds-checked. */
int kolmo_klz_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);

size_t kolmo_klzh_bound(size_t n);
/* Return the coded size, or 0 when not smaller than n or cap too small. */
size_t kolmo_klzh_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level);
int kolmo_klzh_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);

/* KLZH2 (codec 32, unreleased): KLZH's layout with three differences.
   - Each of the four streams is preceded by a mode byte: 0 = khuf chunk stream (as KLZH), 1 = KRANS (krans.h; the code
     streams only). The encoder takes rANS where it is smaller: the literal-length codes of an LZ parse are mostly 0,
     where Huffman's one-bit floor loses ~14%.
   - Matches are at least 3 bytes: the match-length code codes v = match length - 3 (KLZH: - 4); the sequence count is
     at most n / 3 + 1.
   - Offsets go up to 2^30 - 1: offset codes 3 + h for h up to 29 (codes 0..32; KLZH: up to 2^24 - 1, codes 0..26).
   Encoder: level 0 (fast; levels below 0 are level 0) is the fast parse: one 6-byte hash table, greedy, the sequences
   written straight into the streams, rANS tried on the literal-length codes only (any KLZH2 decoder reads it: the
   format is the same at every level). Levels 8-9 add 3-byte matches at offsets below 2^14, and levels 7-9 long-distance candidates past the
   binary tree's window (32-byte keys) when the block is longer than that window. Every level cuts the khuf chunks of
   each stream where the input crosses a multiple of 256 KiB, so each table covers 256 KiB of input (KLZH's decoder
   already reads chunks of any size up to KHUF_BLOCK_MAX). */
size_t kolmo_klzh2_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level);
int kolmo_klzh2_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
#endif
