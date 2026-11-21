/* Kolmo's own coders in place of the libraries (rc3 wave 2, native lane): codec tables for 24 (KLZMA), 27 (khuf
 * leaf), 28 (KLZ) and 29 (KLZH), and the opt-in that substitutes them for the library coders.
 *
 * Opt-in: KOLMO_NATIVE=lz,huf,lzma (or "all"), read once per process. Without it nothing changes: no default output
 * uses these ids. With it, the encoder codes with Kolmo's coder wherever it would have called the library coder
 * (same planner, same trials, same levels mapped as below), and writes the new id in the block index or the leaf
 * header (codec_wire); the frame is then version 11. Dictionaries and trained profiles keep the library coders.
 *   lz:   LZ4 -> KLZ (28), LZ4-HC -> KLZ level 3, zstd -> KLZH2 (32), Brotli -> KLZH2 level 7. KLZH2 is KLZH (29) with
 *         rANS-coded sequence codes where they gain 3% (night 2, 2026-11-21); KLZH frames stay readable
 *   huf:  zstd's HUF in the graph leaves (codec 13) -> khuf (27)
 *   lzma: raw LZMA2 (codec 6) -> KLZMA (24)
 *   strong: zstd and Brotli -> KLZMA (the optimal parse and range coder, at a slower decode): the candidate for `hc`
 *           and `eco` (zstd levels 19 and 22 -> KLZMA 6, levels 10-18 -> 5, lower levels and Brotli -> 3). The id
 *           written cannot depend on the level (ids are written where the level is not known), so strong maps every
 *           zstd call of the process: use it for `hc` and `eco` runs. Measurements: benchmark/rc3w2/native/RESULTS.md
 * The algorithms are in src/algo/khuf.c, klz.c, klzma.c (compiled on their own); formats in their headers.
 * Part of the single translation unit built from src/kolmo.c. */
#include "../algo/khuf.h"
#include "../algo/klz.h"
#include "../algo/klzma.h"

enum { KN_LZ = 1, KN_HUF = 2, KN_LZMA = 4, KN_STRONG = 8, KN_KLZ = 16 };
/* Default when KOLMO_NATIVE is unset (0.0.1 prototype, night 2): Kolmo's own coder where it is at parity or better:
 * KLZ for LZ4 (speed: 4.4% smaller than lz4 -1 on the holdout at the same decode speed), KLZMA for LZMA2 (ratio:
 * same size, decode at parity), khuf for zstd's HUF leaves (+0.16%). zstd and Brotli stay library coders.
 * KOLMO_NATIVE=none (or empty) restores the library coders everywhere. */
#ifndef KOLMO_NATIVE_DEFAULT
#define KOLMO_NATIVE_DEFAULT "klz,lzma,huf"
#endif
static int kolmo_native_parse(const char *e) {
    int r = 0;
    for (const char *s = e; s && *s;) {   /* comma-separated words */
        size_t k = strcspn(s, ",");
        if (k == 3 && !strncmp(s, "all", 3)) r |= KN_LZ | KN_HUF | KN_LZMA;
        else if (k == 2 && !strncmp(s, "lz", 2)) r |= KN_LZ;
        else if (k == 3 && !strncmp(s, "klz", 3)) r |= KN_KLZ;   /* LZ4 -> KLZ only */
        else if (k == 3 && !strncmp(s, "huf", 3)) r |= KN_HUF;
        else if (k == 4 && !strncmp(s, "lzma", 4)) r |= KN_LZMA;
        else if (k == 6 && !strncmp(s, "strong", 6)) r |= KN_STRONG;
        s += k; if (*s) s++;
    }
    return r;
}
static int kolmo_native_flags(void) {
    static int f = -1; int r = __atomic_load_n(&f, __ATOMIC_RELAXED);
    if (r < 0) {
        const char *e = getenv("KOLMO_NATIVE");
        r = kolmo_native_parse(e ? e : KOLMO_NATIVE_DEFAULT);
        __atomic_store_n(&f, r, __ATOMIC_RELAXED);
    }
    return r;
}
/* Per-job flags (0.0.1 prototype, night 2): with KOLMO_NATIVE unset, hc also codes zstd with KLZH2 (level 19 -> 9:
 * +0.11% vs zstd on the screening corpus, the same encode time), while balanced keeps zstd (+0.68% native). */
static int kolmo_native_job_flags(int mode) { return !getenv("KOLMO_NATIVE") && mode == KOLMO_HC ? 1 + (kolmo_native_flags() | KN_LZ) : 0; }
static inline int native_flags_of(const codec_ctx *x) { return x->natf ? x->natf - 1 : kolmo_native_flags(); }
/* the codec the encoder actually codes with (and writes) for library codec c */
static inline int codec_native_id(const codec_ctx *x, int c) {
    int f = native_flags_of(x);
    if (!f || x->dict || x->prof) return c;
    if ((f & KN_STRONG) && (c == C_ZSTD || c == C_BROTLI)) return C_KLZMA;
    if ((f & (KN_LZ | KN_KLZ)) && (c == C_LZ4 || c == C_LZ4HC)) return C_KLZ;
    if ((f & KN_LZ) && (c == C_ZSTD || c == C_BROTLI)) return C_KLZH2;
    if ((f & KN_HUF) && c == C_HUF) return C_KHUF;
    if ((f & KN_LZMA) && c == C_LZMA) return C_KLZMA;
    return c;
}
static inline int codec_is_native(int c) { return c == C_KLZMA || c == C_KHUF || c == C_KLZ || c == C_KLZH || c == C_KLZH2; }
/* the id written for library codec c; notes on the context when it is a native id, so that the frame is version 11
 * only when a native id was actually written (KOLMO_NATIVE alone used to make every frame version 11) */
static inline uint8_t codec_wire_note(codec_ctx *x, int c) {
    int w = codec_native_id(x, c); if (codec_is_native(w)) x->used_native = 1; return (uint8_t)w;
}
#define codec_wire(x, c) codec_wire_note((x), (c))

/* library level -> native level */
static int native_level(int c, int level) {
    switch (c) {
        case C_LZ4: return 1;
        case C_LZ4HC: return 3;
        case C_ZSTD: if (kolmo_native_flags() & KN_STRONG) return level >= 19 ? 6 : level >= 10 ? 5 : 3;
                     return level <= 2 ? 1 : level <= 5 ? 3 : level <= 9 ? 5 : level <= 15 ? 6 : level <= 18 ? 8 : 9;   /* zstd -19 -> KLZH 9: the same encode time as zstd -19 (5.6 vs 6.0 MB/s), 1% smaller than KLZH 8 (night 2) */
        case C_BROTLI: return kolmo_native_flags() & KN_STRONG ? 3 : 7;
        case C_LZMA: return level >= 100 ? level : level <= 1 ? 1 : level <= 5 ? 4 : 8;   /* the parse level; >= 100: lc/lp/pb */
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
