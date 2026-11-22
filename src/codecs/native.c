/* Kolmo's own coders: codec tables for 24 (KLZMA), 27 (khuf leaf), 28 (KLZ), 29 (KLZH) and 32 (KLZH2), and the
 * mapping from the encoder roles (codecs/codec.h) to them. Every encoder path codes with these coders and writes
 * their ids; there is no other choice and no environment switch (container version 12).
 *   speed (R_SPEED) -> KLZ level 1, R_SPEEDHC -> KLZ level 3
 *   balanced and hc (R_LZH) -> KLZH2, levels on zstd's scale: 0 and below (zstd's --fast levels) -> 0, the fast parse; 1-2 -> 1, 3-5 -> 3, 6-9 -> 5, 10-15 -> 6, 16-18 -> 8,
 *     19 and up -> 9
 *   eco (R_ECO) -> KLZH2 level 7
 *   ratio's LZ coder (R_LZMA) -> KLZMA (level 1 -> 1, 2-5 -> 4, 6-9 -> 8; >= 100: lc/lp/pb digits, algo/klzma.h)
 *   Huffman leaves (R_HUF) -> khuf
 * The algorithms are in src/algo/khuf.c, klz.c, klzma.c (compiled on their own); formats in their headers.
 * Part of the single translation unit built from src/kolmo.c. */
#include "../algo/khuf.h"
#include "../algo/klz.h"
#include "../algo/klzma.h"

/* the codec the encoder codes with, and writes, for role (or id) c */
static inline int codec_native_id(int c) {
    switch (c) {
        case R_SPEED: case R_SPEEDHC: return C_KLZ;
        case R_LZH: case R_ECO: return C_KLZH2;
        case R_HUF: return C_KHUF;
        case R_LZMA: return C_KLZMA;
    }
    return c;
}
static inline int codec_is_native(int c) { return c == C_KLZMA || c == C_KHUF || c == C_KLZ || c == C_KLZH || c == C_KLZH2; }
/* the id written for role c (the context is kept for the callers; nothing is noted on it) */
#define codec_wire(x, c) ((void)(x), (uint8_t)codec_native_id(c))

/* role level -> native level */
static int native_level(int c, int level) {
    switch (c) {
        case R_SPEED: return 1;
        case R_SPEEDHC: return 3;
        case R_LZH: return level <= 0 ? 0 : level <= 2 ? 1 : level <= 5 ? 3 : level <= 9 ? 5 : level <= 15 ? 6 : level <= 18 ? 8 : 9;   /* hc's 19 -> 9 */
        case R_ECO: return 7;
        case R_LZMA: return level >= 100 ? level : level <= 1 ? 1 : level <= 5 ? 4 : 8;   /* the parse level; >= 100: lc/lp/pb */
    }
    return level;
}

static size_t klzma_c_bound(size_t n) { return kolmo_klzma_bound(n); }
static size_t klzma_c_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; if (level >= 100) level = level * 10 + 8;   /* lc/lp/pb digits, then parse level 8 (algo/klzma.h) */
    else if (level > 9) level = 9;
    return kolmo_klzma_compress(in, n, out, cap, level);
}
static int klzma_c_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return kolmo_klzma_decompress(in, cn, out, n); }
static size_t khuf_c_bound(size_t n) { return kolmo_khuf_bound(n); }
static size_t khuf_c_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) { (void)x; (void)level; return kolmo_khuf_compress_chunks(in, n, out, cap); }
static int khuf_c_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return kolmo_khuf_decompress_chunks(in, cn, out, n); }
static size_t klz_c_bound(size_t n) { return kolmo_klz_bound(n); }
static size_t klz_c_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) { (void)x; return kolmo_klz_compress(in, n, out, cap, level); }
static int klz_c_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return kolmo_klz_decompress(in, cn, out, n); }
static size_t klzh_c_bound(size_t n) { return kolmo_klzh_bound(n); }
static size_t klzh_c_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) { (void)x; return kolmo_klzh_compress(in, n, out, cap, level); }
static int klzh_c_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return kolmo_klzh_decompress(in, cn, out, n); }
static const kolmo_codec codec_klzma = { C_KLZMA, "klzma", klzma_c_bound, klzma_c_compress, klzma_c_decompress, NULL };
static const kolmo_codec codec_khuf = { C_KHUF, "khuf", khuf_c_bound, khuf_c_compress, khuf_c_decompress, NULL };
static const kolmo_codec codec_klz = { C_KLZ, "klz", klz_c_bound, klz_c_compress, klz_c_decompress, NULL };
static const kolmo_codec codec_klzh = { C_KLZH, "klzh", klzh_c_bound, klzh_c_compress, klzh_c_decompress, NULL };
static size_t klzh2_c_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) { (void)x; return kolmo_klzh2_compress(in, n, out, cap, level); }
static int klzh2_c_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return kolmo_klzh2_decompress(in, cn, out, n); }
static const kolmo_codec codec_klzh2 = { C_KLZH2, "klzh2", klzh_c_bound, klzh2_c_compress, klzh2_c_decompress, NULL };
