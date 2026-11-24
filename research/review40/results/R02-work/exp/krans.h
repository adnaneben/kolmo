/* Kolmo context rANS (krans): Kolmo's own static rANS coder with static context modelling, the entropy coder of
   graph leaf codec 33. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0.

   The method is Jarek Duda's asymmetric numeral systems (range variant, "Asymmetric numeral systems: entropy coding
   combining speed of Huffman coding with compression rate of arithmetic coding", arXiv:1311.2540); interleaving
   several states in one word stream follows Fabian Giesen, "Interleaved entropy coders", arXiv:1402.3392; contexts
   mapped onto a smaller set of shared tables is the idea of the context maps of Brotli and JPEG XL. All code here
   was written for Kolmo.

   CODER. 32-bit states in [2^16, 2^32), totals 2^12, renormalisation by 16-bit little-endian words (at most one per
   symbol in either direction). A chunk of m bytes is cut into S = 1, 2, 4, 8, 16 or 32 contiguous segments of
   floor(m / S) bytes (the last one takes the m mod S bytes left over); segment k is coded by state k, and the
   decoder takes byte i of every segment in turn (i = 0, 1, ...; segment 0 first), then the left-over bytes with
   state S - 1. A byte's context only looks at earlier bytes of its own segment, so the S chains are independent.
   The encoder starts every state at 2^16 and codes backwards; the decoder must end with every state at 2^16 and the
   word stream used up exactly, which is the integrity check of a chunk.

   CONTEXTS. Byte j of a chunk (position i in its segment) is coded under
       context = (QE[e] * NB + QB[prev]) * P + phase
     phase = j mod P, P = 1..8 (channel of an interleaved image, byte of a word);
     e     = sum of MAG[byte at i - lag] over the 0..4 lags of the header (lags that reach before the segment start
             count 0), quantised by QE into NE bins: eq 0: none (NE 1, no lags); eq 1: 17 bins with edges
             1 2 4 6 8 11 14 18 23 29 37 47 60 80 110 160; eq 2: 9 bins with edges 1 3 6 11 18 29 47 80;
     prev  = the byte at i - 1 (0 at the segment start), quantised by QB into NB bins: bq 0: none (NB 1); bq 1: the
             byte itself (NB 256, order 1); bq 2: 6 bins of MAG[prev] with edges 1 3 6 12 25; bq 3: prev >> 4 (NB 16).
             With the header's pf bit the term is 0 in phase 0 (the previous byte belongs to another pixel).
     MAG   = mk 0: (b + 1) >> 1 (zigzag residuals); mk 1: min(b, 256 - b) (two's complement); mk 2: b.
   A bin index is the number of edges <= the value. N = NE * NB * P <= 4096 contexts; a context map sends each to one
   of T <= 255 frequency tables (the encoder merges contexts whose tables do not pay for themselves).

   LEAF (codec 33) = chunks until the raw length is reached; every chunk is independent. Chunk:
     kind u8: 0 stored: m uv, m bytes | 1 fill: m uv, the byte | 2 coded:
       m uv (1 .. 2^24)
       b0: bits 0-2 log2 S (0..5), bits 3-5 P - 1, bits 6-7 mantissa bits MB of the tables (0..3 -> 3, 4, 6, 12)
       b1: bits 0-1 eq, bits 2-3 bq, bits 4-5 mk, bit 6 pf (only with bq != 0 and P > 1), bit 7 zero
       if eq != 0: lag count u8 (1..4), each lag uv (1 .. 2^24)
       T - 1 u8 (T <= N, T <= 1 + m / 16)
       a bit stream, least significant bit first, zero-padded to a whole byte:
         context map: N entries of ceil(log2 T) bits;
         T tables: lo 8 bits, hi 8 bits (first and last present symbol), then for each symbol lo..hi its class by a
           prefix code against the previous class pc (0 at lo): 0 same, 10 pc - 1, 110 pc + 1, 1110 pc - 2,
           11110 pc + 2, 11111 + 4 bits: the class itself (0..12), or 14 for the one symbol of the table that takes
           what the others leave of 2^12 (pc is not changed by it). Class 0 is an absent symbol; class e >= 1 is a
           frequency in [2^(e-1), 2^e) and is followed by min(e - 1, MB) bits: the bits below its top bit, the rest 0.
       z uv, then z bytes: S states (4 bytes each, little-endian, segment 0 first), then the 16-bit words.
   uv is LEB128. A decoder refuses anything else (reserved values, a table whose coded frequencies leave nothing for
   the class-14 symbol, lo or hi absent, a context map entry >= T, non-zero padding, a short or long word stream). */
#ifndef KOLMO_ALGO_KRANS_H
#define KOLMO_ALGO_KRANS_H
#include <stddef.h>
#include <stdint.h>

#define KRANS_CHUNK_MAX (1u << 24)
#define KRANS_NCTX_MAX 4096

/* Encoder choices. Zero-initialise for the defaults. */
typedef struct {
    uint32_t stride;     /* row length in bytes of an image-like leaf (0: unknown): enables the neighbour contexts */
    uint8_t step;        /* bytes per pixel (channel step, 1..8) when stride is set; 0 = 1 */
    uint8_t effort;      /* 0/1: order 0, then the image contexts (stride set) or order 1; 2: more context sets tried */
    uint32_t chunk;      /* chunk size in bytes (0: chosen from the leaf) */
    /* tests and experiments: force one context set on every coded chunk (force = 1) */
    uint8_t force, ls, P, mb, eq, bq, mk, pf, nlag;
    uint32_t lag[4];
    uint8_t max_tables;  /* 0 = 64 */
} kolmo_krans_params;

size_t kolmo_krans_bound(size_t n);
/* Returns the coded size, or 0 when cap is too small (cap >= kolmo_krans_bound(n) always suffices) or memory ran out. */
size_t kolmo_krans_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap, const kolmo_krans_params *p);
/* Decodes exactly n bytes from in[0..cn); 0 on success, -1 when the payload is not a valid leaf. Every read and
   write is bounds-checked. Working memory: at most 255 tables of 5120 bytes, allocated as the chunks ask. */
int kolmo_krans_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
#endif
