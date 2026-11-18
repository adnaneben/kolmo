/* KLZ: Kolmo's own LZ77 coders. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   Two formats share one match finder (hash table, or hash chains with lazy evaluation), written for Kolmo:

   KLZ (codec 28, the LZ4 role in `speed`): byte-aligned, no entropy stage, decode limited by memory copies. Format
     of wave 4 (the rc3 wave-2 format, one interleaved stream with 3-byte offsets, is not read any more):
       header: sequence count S, literal bytes L, extension bytes X (three uv, possibly non-minimal: the encoder
       writes them at a width fixed by the block size), then four streams:
       literals (L bytes) | tokens (S bytes) | offsets (S x 2 bytes, little-endian, 1 .. 65535) | extensions (X)
     token = literal length (bits 0-3; 15: + extension) | match length - 4 (bits 4-7; 15: + extension); an extension
     is one byte below 255, or 255 then uv (value - 255). Sequence k: its literals from the literal stream, then a
     match. The literals left after the last sequence end the block. Separate streams let the decoder find each
     token, offset and literal run without the previous sequence's lengths (the loop carries only pointer adds).

   KLZH (codec 29, the zstd role in `balanced`): the same parse with repeat offsets, coded as five streams:
     sequence count uv | literals | literal-length codes | match-length codes | offset codes (each a khuf chunk stream
     with its length uv in front, khuf.h) | extra bits (the rest; least significant bit first).
     Length v (literal length, match length - 4): v < 16 is code v; otherwise h = floor(log2 v), code
     16 + 2 (h - 4) + the bit below the top, h - 1 extra bits. Offset code: 0, 1, 2 the three most recent offsets
     (move to front), else 3 + h for offset o with h = floor(log2 o) and h extra bits. Literals after the last
     sequence are those left in the literal stream.

   Levels: KLZ 1 (hash table of 6-byte keys, skipping), 2 (larger table, two positions per match), 3 (hash chains, lazy).
           KLZH 1-3 two hash tables (8- and 5-byte keys), greedy; 4-9 hash chains of growing depth, lazy evaluation from 5,
           two-step lazy from 8. */
#ifndef KOLMO_ALGO_KLZ_H
#define KOLMO_ALGO_KLZ_H
#include <stddef.h>
#include <stdint.h>

#define KLZ_WIN 0xFFFFu                /* KLZ: 16-bit offsets */
#define KLZH_WINDOW ((1u << 24) - 1)
size_t kolmo_klz_bound(size_t n);
/* Return the coded size, or 0 when it does not fit in cap (kolmo_klz_bound(n) always suffices). */
size_t kolmo_klz_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level);
/* 0 when in[0..cn) decodes to exactly n bytes, else -1. Every read and write is bounds-checked. */
int kolmo_klz_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);

size_t kolmo_klzh_bound(size_t n);
/* Return the coded size, or 0 when not smaller than n or cap too small. */
size_t kolmo_klzh_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int level);
int kolmo_klzh_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
#endif
