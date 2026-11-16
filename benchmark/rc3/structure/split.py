#!/usr/bin/env python3
"""Split rc2's src/kolmo.c and src/*.inc into the rc3 tree (docs/LAYOUT.md). Run from the repository root on a tree
where src/ is still the rc2 layout. Line numbers refer to the rc2 files and are checked against their content."""
import os, re, subprocess, sys

def rd(p):
    return open(p).read().split("\n")

K = rd("src/kolmo.c")          # K[0] is line 1
used = [0] * (len(K) + 1)

def L(a, b, f=K, mark=True):
    """lines a..b (1-based, inclusive) of f"""
    if mark and f is K:
        for i in range(a, b + 1):
            if used[i]: sys.exit(f"line {i} used twice")
            used[i] = 1
    return "\n".join(f[a - 1:b])

def expect(i, s, f=K):
    if s not in f[i - 1]:
        sys.exit(f"line {i}: expected {s!r}, got {f[i - 1]!r}")

files = {}
def put(path, *parts):
    files[path] = "\n".join(p for p in parts if p is not None).rstrip("\n") + "\n"

# ------------------------------------------------------------------ sanity anchors
for i, s in [(2, "#define _GNU_SOURCE"), (3, '#include "kolmo.h"'), (51, "libbsc/libbsc.h"), (53, "runtime ISA dispatch"),
             (89, "#endif"), (91, "CRC32C"), (141, "#endif"), (143, "transforms (shared"), (416, "#define TSLACK"),
             (431, "has_quotes"), (434, "table transform (v0.3)"), (441, "P10[19]"), (443, "1000000000000000000LL"),
             (445, "field_end_q"), (454, "}"), (456, "canonical decimal"), (591, "}"), (593, "A run of n values"),
             (597, "#if defined(KOLMO_FMT_AVX2)"), (608, "#endif"), (609, "Same values, each written"), (611, "#define FMT_STRIDE"),
             (612, "#if defined(KOLMO_FMT_AVX2)"), (630, "#endif"), (631, "__ARM_NEON"), (641, "#endif"),
             (642, "#if defined(KOLMO_FMT_AVX2)"), (697, "#endif"), (698, "static void fmt_slots"), (700, "__ARM_NEON"),
             (701, "if (S < 8)"), (731, "    }"), (732, "#endif"), (737, "}"), (738, "#if defined(KOLMO_FMT_AVX2)"),
             (785, "#endif"), (786, "static uint8_t *fmt_run"), (793, "}"), (795, "Per-thread reuse"), (834, "}"),
             (836, "encode one column"), (1063, "tbl_decode_pub"), (1065, "tunable settings"), (1086, "#endif"),
             (1087, "#define malloc"), (1090, "#define free"), (1091, "tbl2.inc"), (1092, "log.inc"), (1093, "Byte histogram"),
             (1100, "}"), (1101, "sci.inc"), (1102, "word.inc"), (1103, "#undef malloc"), (1106, "#undef free"),
             (1107, "cm.inc"), (1109, "tp_fieldcounts"), (1113, "}"), (1115, "ldw"), (1154, "}"), (1156, "codecs */"),
             (1157, "enum { C_STORE"), (1158, "C_GRAPH = 12"), (1159, "kfam.inc"), (1160, "enum { T_NONE"),
             (1161, "x86 call/jump"), (1216, "}"), (1218, "bsc_once"), (1219, "bsc_init_once"), (1220, "looks_text"),
             (1229, "}"), (1230, "codec_bound"), (1245, "}"), (1247, "typedef struct { ZSTD_CCtx"), (1252, "} codec_ctx;"),
             (1254, "kgu_get"), (1258, "}"), (1259, "Huffman-only"), (1262, "common/huf.h"), (1299, "}"),
             (1301, "Static order-0 rANS"), (1377, "rans_compress_all("), (1378, "Plane-aligned leaves"), (1390, "}"),
             (1391, "r64_rd"), (1431, "}"), (1433, "codec_compress"), (1478, "}"), (1480, "np_decompress"),
             (1481, "codec_decompress"), (1518, "}"), (1520, "nplane.inc"), (1521, "kgraph.inc"), (1523, "codec_ctx_free"),
             (1525, "per-thread state"), (1555, "}"), (1557, "container layout"), (1573, "get64"), (1574, "lines.inc"),
             (1576, "typedef struct { int t, a, b, c;"), (1580, "} plan_t;"), (1582, "default_threads"),
             (1584, "Container allocations"), (1603, "}"), (1605, "#ifndef KOLMO_BWT_GAIN_PCT"), (1617, "}"),
             (1619, "Minified JSON"), (1650, "}"), (1651, "minified JSON: no newline"), (1673, "}"),
             (1675, "file-level analysis"), (2321, "}"), (2323, "POSIX allows"), (2342, "}"), (2344, "block boundaries"),
             (2406, "}"), (2407, "worst case"), (2412, "}"), (2414, "parallel driver"), (2763, "}"),
             (2765, "persistent worker pool"), (2818, "}"), (2820, "ultra: the context-mixing"), (2836, "}"),
             (2838, "tar archives"), (2858, "}"), (2860, "#ifdef KOLMO_FRONTIER"), (2995, "#endif"),
             (2997, "typedef struct {"), (3070, "}"), (3072, "Opt-in typed encoder"), (3073, "typed_int_encoder.inc"),
             (3075, "compress_impl"), (3213, "}"), (3215, "int64_t kolmo_compress("), (3217, "}"), (3219, "decoding */"),
             (3412, "}"), (3414, "kolmo_original_size"), (3416, "run_decode"), (3439, "}"), (3441, "kolmo_decompress("),
             (3557, "}"), (3559, "stream.inc")]:
    expect(i, s)

# blank / separator lines that carry no code
for i in [1, 2, 52, 90, 142, 432, 433, 444, 455, 592, 794, 1064, 1087, 1088, 1089, 1090, 1091, 1092, 1101, 1102, 1103,
          1104, 1105, 1106, 1107, 1108, 1114, 1155, 1159, 1217, 1246, 1253, 1300, 1432, 1479, 1519, 1520, 1521, 1522,
          1524, 835, 1156, 1556, 1604, 1618, 2837, 1574, 1575, 1581, 1583, 1674, 2322, 2343, 2413, 2764, 2819, 2859, 2996, 3071, 3072, 3073, 3074, 3214,
          3218, 3413, 3415, 3440, 3558, 3559]:
    if i not in (1, 2, 1156, 1087, 1088, 1089, 1090, 1091, 1092, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1159, 1520, 1521, 1574, 3072, 3073, 3559) and K[i - 1].strip():
        sys.exit(f"line {i} is not blank: {K[i - 1]!r}")
    used[i] = 1

HDR = "/* {}  Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md). */\n"

# ------------------------------------------------------------------ core/common.h
cm = L(3, 51).replace('#include "kolmo.h"', '#include "../kolmo.h"').replace('#include "experimental/', '#include "../experimental/')
put("src/core/common.h", "/* System, vendored-codec and public headers every part of the library uses, the container version, and the\n"
    " * timing helper of the profiling builds. Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md). */",
    cm)

# ------------------------------------------------------------------ arch/cpu.h (+ integer-inverse host switches)
G = rd("src/kgraph.inc")
for i, s in [(250, "__BYTE_ORDER__"), (259, "#endif"), (287, "W = 4, g = 1"), (292, "cannot resume"),
             (293, "#if defined(__SSE2__) && FI_USE"), (294, "#define FI_VEC4 1"), (313, "}"), (314, "#if defined(KOLMO_FMT_AVX2)"),
             (337, "#endif"), (338, "static void fi_vec4"), (350, "}"), (351, "#endif")]:
    expect(i, s, G)
put("src/arch/cpu.h", "/* CPU feature detection and the target attributes of the dispatched kernels (x86-64: CPUID at first use). */",
    L(53, 89),
    "",
    "/* Host byte order for the integer-inverse loops (transforms/graph.c and the arch/ kernels): the loops store whole",
    " * words, so they are used on little-endian hosts only. -DKOLMO_FORCE_GENERIC_INT_INV sends every program to the",
    " * reference inverse (kg_int_inv). */",
    L(250, 259, G))

# ------------------------------------------------------------------ CRC32C
put("src/arch/x86_64/crc32c_sse42.c",
    "/* CRC32C with the SSE4.2 crc32 instruction (8 bytes per step). Reference and dispatch: arch/crc32c.c. */",
    L(92, 103))
put("src/arch/aarch64/crc32c_arm.c",
    "/* CRC32C with the Armv8 CRC32 extension (8 bytes per step). Reference and dispatch: arch/crc32c.c. */",
    "#if defined(__ARM_FEATURE_CRC32)",
    "#include <arm_acle.h>",
    "static uint32_t crc32c_arm(uint32_t crc, const void *buf, size_t len) {",
    L(109, 112),
    "}",
    "#endif")
expect(104, "#if defined(__SSE4_2__)"); expect(106, "#elif defined(__ARM_FEATURE_CRC32)"); expect(107, "arm_acle.h")
expect(108, "uint32_t kolmo_crc32c"); expect(113, "}"); expect(114, "#else")
for i in (106, 107, 108, 109, 110, 111, 112, 113): used[i] = 1
put("src/arch/crc32c.c",
    "/* CRC32C (Castagnoli) of the container header and index: the portable reference (slicing by 8) and the choice of",
    " * kernel. x86-64: SSE4.2 when the build targets it, else chosen at run time by CPUID (arch/x86_64/crc32c_sse42.c).",
    " * Arm: the CRC32 extension when the build targets it (arch/aarch64/crc32c_arm.c). Same value on every path. */",
    L(104, 105),
    "#elif defined(__ARM_FEATURE_CRC32)",
    "uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) { return crc32c_arm(crc, buf, len); }",
    L(114, 141))
used[91] = 1

# ------------------------------------------------------------------ integer inverse kernels (from kgraph.inc)
put("src/arch/x86_64/int_inv_sse2.c",
    L(287, 292, G).replace("/* W = 4", "/* Integer inverse kernels for transforms/graph.c (fi_vec4 there is the dispatch and the scalar tail; the\n"
                                        " * reference is kg_int_inv). W = 4"),
    L(293, 313, G),
    "#endif")
put("src/arch/x86_64/int_inv_avx2.c",
    "/* AVX2 integer inverse kernel (16 values per step), chosen by CPUID in fi_vec4 (transforms/graph.c). See\n"
    " * arch/x86_64/int_inv_sse2.c for the contract. */",
    "#if defined(FI_VEC4)",
    L(314, 337, G),
    "#endif")

# ------------------------------------------------------------------ transforms: transpose + shared helpers
put("src/transforms/transpose.c",
    "/* Column transpose of delimited text (T_TP): fields of a row become columns, quote-aware. */",
    L(143, 415),
    "",
    L(1109, 1113))
put("src/transforms/common.h",
    "/* Definitions shared by the structure transforms: transform ids, room a transform may need past the block, and",
    " * the byte scanners. */",
    L(416, 431),
    "",
    L(1160, 1160))

# ------------------------------------------------------------------ decimal parse/format (algo) + kernels (arch)
put("src/algo/decimal.c",
    "/* Canonical decimals: parse to (mantissa, scale) and format back, byte for byte. The portable references of the",
    " * decimal kernels in arch/ (arch/decimal.c dispatches). Used by the table, log and graph transforms. */",
    L(441, 443),
    "",
    L(456, 591),
    L(609, 611))
put("src/arch/x86_64/decimal_avx2.c",
    "/* AVX2 decimal formatters: 4 values per step for |v| < 1e8 and S < 8. Reference: fmt_dec_swar (algo/decimal.c);",
    " * dispatch: arch/decimal.c. Compiled with a target attribute and chosen by CPUID on baseline x86-64 builds. */",
    L(597, 608),
    L(612, 630),
    L(642, 697),
    L(738, 785))
neon_body = L(701, 731)
put("src/arch/aarch64/decimal_neon.c",
    "/* NEON decimal formatter for fixed slots: 2 values per step for |v| < 1e8 and S < 8. Reference: fmt_dec_swar",
    " * (algo/decimal.c); dispatch: arch/decimal.c. Returns the number of values written. */",
    L(631, 641),
    "#if defined(__ARM_NEON) && defined(__aarch64__)",
    "static int fmt_slots_neon(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {",
    "    int i = 0;",
    neon_body,
    "    return i;",
    "}",
    "#endif")
used[700] = used[732] = 1
expect(699, "int i = 0;"); expect(733, "#if defined(KOLMO_FMT_AVX2)"); expect(736, "for (; i < n; i++)")
put("src/arch/decimal.c",
    "/* Dispatch of the decimal formatters (arch/x86_64/decimal_avx2.c, arch/aarch64/decimal_neon.c); the scalar loop is",
    " * the reference (fmt_dec_swar, algo/decimal.c). */",
    L(593, 596),
    L(698, 699),
    "#if defined(__ARM_NEON) && defined(__aarch64__)",
    "    i = fmt_slots_neon(o, v, ds, n, S, lv);",
    "#endif",
    L(733, 737),
    L(786, 793))

# ------------------------------------------------------------------ core/memory.c
put("src/core/memory.c",
    "/* Memory: the per-thread scratch cache of the transforms, the output buffer type, and checked array allocation.",
    " * Budgets and limits: docs/MEMORY-MODEL.md. */",
    L(795, 834),
    "",
    L(1584, 1603))

put("src/transforms/table_v03.c",
    "/* Column table transform of v0.3 (tflag 3). */",
    L(434, 440),
    L(445, 454),
    "",
    L(836, 1063))
put("src/plan/tunables.h", L(1065, 1086))
put("src/algo/hist.c", L(1093, 1100))
put("src/transforms/shuffle.c", "/* Byte-plane shuffle of fixed-width arrays (T_SHUF), with optional delta. */", L(1115, 1154))
put("src/transforms/x86.c", L(1161, 1216))
put("src/plan/detect.c",
    "/* Content detection used by the planner and by the codecs (text, JSON). */",
    L(1220, 1229),
    "",
    L(1651, 1673))
put("src/algo/varint.h", "/* LEB128 unsigned varint reader of the leaf formats. */", L(1254, 1258))
put("src/transforms/json.c", L(1619, 1650))

# ------------------------------------------------------------------ codecs
for i in range(1230, 1246): used[i] = 1          # codec_bound -> codec tables
for i in range(1433, 1479): used[i] = 1          # codec_compress -> codec tables
for i in range(1481, 1519): used[i] = 1          # codec_decompress -> codec tables
put("src/codecs/codec.h",
    "/* Block codecs: identifiers, the per-thread codec context and the codec table (struct kolmo_codec). Each codec",
    " * file defines one table (or two for one family, as LZ4 and LZ4-HC) and codecs/codec.c lists them by identifier.",
    " * Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md). */",
    "#define KOLMO_CODEC_REGISTRY 1",
    L(1157, 1158),
    "",
    L(1247, 1252),
    "",
    """/* One block codec. Missing functions keep the behaviour of the rc2 dispatch: bound NULL -> n, compress NULL -> 0
 * (not written by codec_compress), decompress NULL -> -1 (refused by codec_decompress). mem_needed: bytes of working
 * memory for n input bytes at a level (NULL: not yet modelled; docs/MEMORY-MODEL.md). */
typedef struct kolmo_codec {
    int id;
    const char *name;
    size_t (*bound)(size_t n);
    size_t (*compress)(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap);
    int (*decompress)(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n);
    size_t (*mem_needed)(size_t n, int level);
} kolmo_codec;

/* tentative definitions: each codec file defines its table, codecs/codec.c lists them */
static const kolmo_codec codec_store, codec_lz4, codec_lz4hc, codec_zstd, codec_brotli, codec_bwt, codec_lzma, codec_cm,
    codec_graph, codec_huf, codec_rans, codec_nplane, codec_fastq, codec_cm_image;""")

C = {}   # the switch bodies, moved verbatim into the codec files
put("src/codecs/store.c", """/* Codec 0: stored bytes. */
static int store_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; if (cn != n) return -1; memcpy(out, in, n); return 0;
}
static const kolmo_codec codec_store = { C_STORE, "store", NULL, NULL, store_decompress, NULL };""")
put("src/codecs/lz4.c", """/* Codecs 1 and 2: LZ4 (rt) and LZ4-HC; one decoder for both. vendor/lz4. */
static size_t lz4_bound(size_t n) { return (size_t)LZ4_compressBound((int)n); }
static size_t lz4_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; int r = LZ4_compress_fast((const char *)in, (char *)out, (int)n, (int)cap, level > 0 ? level : 1); return r > 0 ? (size_t)r : 0;
}
static size_t lz4hc_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (!x->lz4hc_state) x->lz4hc_state = malloc(LZ4_sizeofStateHC());
    if (!x->lz4hc_state) return 0;
    int r = LZ4_compress_HC_extStateHC(x->lz4hc_state, (const char *)in, (char *)out, (int)n, (int)cap, level);
    return r > 0 ? (size_t)r : 0;
}
static int lz4_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; int r = LZ4_decompress_safe((const char *)in, (char *)out, (int)cn, (int)n); return r == (int)n ? 0 : -1;
}
static const kolmo_codec codec_lz4 = { C_LZ4, "lz4", lz4_bound, lz4_compress, lz4_decompress, NULL };
static const kolmo_codec codec_lz4hc = { C_LZ4HC, "lz4hc", lz4_bound, lz4hc_compress, lz4_decompress, NULL };""")
put("src/codecs/zstd.c", """/* Codec 3: Zstandard, with the optional dictionary of the context. vendor/zstd. */
static size_t zstd_bound(size_t n) { return ZSTD_compressBound(n); }
static size_t zstd_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (!x->zc) x->zc = ZSTD_createCCtx();
    if (!x->zc) return 0;
    size_t r = x->dict ? ZSTD_compress_usingDict(x->zc, out, cap, in, n, x->dict, x->dictlen, level)
                       : ZSTD_compressCCtx(x->zc, out, cap, in, n, level);
    return ZSTD_isError(r) ? 0 : r;
}
static int zstd_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (!x->zd) x->zd = ZSTD_createDCtx();
    if (!x->zd) return -1;
    size_t r = x->dict ? ZSTD_decompress_usingDict(x->zd, out, n, in, cn, x->dict, x->dictlen)
                       : ZSTD_decompressDCtx(x->zd, out, n, in, cn);
    return (!ZSTD_isError(r) && r == n) ? 0 : -1;
}
static const kolmo_codec codec_zstd = { C_ZSTD, "zstd", zstd_bound, zstd_compress, zstd_decompress, NULL };""")
put("src/codecs/brotli.c", """/* Codec 4: Brotli (eco), window 2^24. vendor/brotli. */
static size_t brotli_bound(size_t n) { return BrotliEncoderMaxCompressedSize(n) ? BrotliEncoderMaxCompressedSize(n) : n + 1024; }
static size_t brotli_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; size_t o = cap;
    int ok = BrotliEncoderCompress(level, 24, BROTLI_MODE_GENERIC, n, in, &o, out);
    return ok ? o : 0;
}
static int brotli_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; size_t o = n; int r = BrotliDecoderDecompress(cn, in, &o, out); return (r == BROTLI_DECODER_RESULT_SUCCESS && o == n) ? 0 : -1;
}
static const kolmo_codec codec_brotli = { C_BROTLI, "brotli", brotli_bound, brotli_compress, brotli_decompress, NULL };""")
put("src/codecs/bwt.c", "/* Codec 5: BWT pipeline (LZP + BWT + QLFC) of libbsc, vendor/libbsc. */",
    L(1218, 1219),
    """static size_t bwt_bound(size_t n) { return n + LIBBSC_HEADER_SIZE + 64; }
static size_t bwt_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; (void)level;               /* payload: XXH32 of the bsc stream | bsc stream (checked before decoding) */
    pthread_once(&bsc_init_once, bsc_once);
    if (n > 0x7FFFFFF0 || cap < n + LIBBSC_HEADER_SIZE + 4) return 0;
    int r = bsc_compress(in, out + 4, (int)n, LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, LIBBSC_BLOCKSORTER_BWT,
                         LIBBSC_CODER_QLFC_STATIC, LIBBSC_FEATURE_FASTMODE);
    if (r <= 0) return 0;
    uint32_t h = (uint32_t)XXH64(out + 4, (size_t)r, 0x62736331); memcpy(out, &h, 4);
    return (size_t)r + 4;
}
static int bwt_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x;
    pthread_once(&bsc_init_once, bsc_once);
    if (cn < 4 + LIBBSC_HEADER_SIZE || n > 0x7FFFFFF0 || cn > 0x7FFFFFF0) return -1;
    uint32_t h; memcpy(&h, in, 4); if (h != (uint32_t)XXH64(in + 4, cn - 4, 0x62736331)) return -1;
    int bs, ds; if (bsc_block_info(in + 4, LIBBSC_HEADER_SIZE, &bs, &ds, LIBBSC_FEATURE_FASTMODE) != LIBBSC_NO_ERROR) return -1;
    if ((size_t)bs != cn - 4 || (size_t)ds != n) return -1;
    return bsc_decompress(in + 4, (int)(cn - 4), out, (int)n, LIBBSC_FEATURE_FASTMODE) == LIBBSC_NO_ERROR ? 0 : -1;
}
static const kolmo_codec codec_bwt = { C_BSC, "bwt", bwt_bound, bwt_compress, bwt_decompress, NULL };""")
put("src/codecs/lzma.c", """/* Codec 6: raw LZMA2 (max), liblzma. Payload: dict u32 | lc lp pb | raw LZMA2. Built without liblzma
 * (-DKOLMO_NO_LZMA): never written, refused when read. */
#ifndef KOLMO_NO_LZMA
static size_t lzma_bound(size_t n) { return lzma_stream_buffer_bound(n) + 64; }
static size_t lzma_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x;
    lzma_options_lzma opt; if (lzma_lzma_preset(&opt, 9 | LZMA_PRESET_EXTREME)) return 0;
    uint32_t d = 4096; while (d < n && d < (64u << 20)) d <<= 1; opt.dict_size = d;
    if (level >= 100) { opt.lc = (level / 100) % 10 - 1; opt.lp = (level / 10) % 10; opt.pb = level % 10; }
    lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
    if (cap < 8) return 0;
    memcpy(out, &d, 4); out[4] = (uint8_t)opt.lc; out[5] = (uint8_t)opt.lp; out[6] = (uint8_t)opt.pb; out[7] = 0;
    size_t pos = 8;
    if (lzma_raw_buffer_encode(f, NULL, in, n, out, &pos, cap) != LZMA_OK) return 0;
    return pos;
}
static int lzma_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x;
    if (cn < 8) return -1;
    uint32_t d; memcpy(&d, in, 4);
    if (d < 4096 || d > (64u << 20) || in[4] > 4 || in[5] > 4 || in[6] > 4 || in[4] + in[5] > 4) return -1;
    lzma_options_lzma opt; lzma_lzma_preset(&opt, 6); opt.dict_size = d; opt.lc = in[4]; opt.lp = in[5]; opt.pb = in[6];
    lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
    size_t ip = 8, op = 0;
    if (lzma_raw_buffer_decode(f, NULL, in, &ip, cn, out, &op, n) != LZMA_OK) return -1;
    return (ip == cn && op == n) ? 0 : -1;
}
static const kolmo_codec codec_lzma = { C_LZMA, "lzma", lzma_bound, lzma_compress, lzma_decompress, NULL };
#else
static size_t lzma_bound(size_t n) { return n + n / 8 + 64; }
static const kolmo_codec codec_lzma = { C_LZMA, "lzma", lzma_bound, NULL, NULL, NULL };   /* block stored or another codec used */
#endif""")
put("src/codecs/huf.c", L(1259, 1299),
    """static size_t huf_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; (void)level; return huf_compress_all(in, n, out, cap);
}
static int huf_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return huf_decompress_all(in, cn, out, n); }
static const kolmo_codec codec_huf = { C_HUF, "huf", NULL, huf_codec_compress, huf_codec_decompress, NULL };""")
put("src/codecs/rans.c", L(1301, 1377), L(1391, 1431),
    """static size_t rans_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; (void)level; return rans_compress_all(in, n, out, cap);
}
static int rans_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return rans_decompress_all(in, cn, out, n); }
static const kolmo_codec codec_rans = { C_RANS, "rans", NULL, rans_codec_compress, rans_codec_decompress, NULL };""")
put("src/codecs/graph_leaf.c", L(1378, 1390))
put("src/codecs/codec.c", """/* The codec registry: every block codec by identifier, and the three calls the container and the graph leaves use.
 * Identifiers 8-11 are the opt-in research codecs of the frontier build (coded at block level in core/), 12 the graph
 * block (transforms/graph.c) and 16 the FASTQ family coder (codecs/fastq.c): their tables carry a name only. */
static const kolmo_codec *const kolmo_codecs[] = {
    [C_STORE] = &codec_store, [C_LZ4] = &codec_lz4, [C_LZ4HC] = &codec_lz4hc, [C_ZSTD] = &codec_zstd, [C_BROTLI] = &codec_brotli,
    [C_BSC] = &codec_bwt, [C_LZMA] = &codec_lzma, [C_CM] = &codec_cm, [C_GRAPH] = &codec_graph, [C_HUF] = &codec_huf,
    [C_RANS] = &codec_rans, [C_NPLANE] = &codec_nplane, [C_FQ] = &codec_fastq, [C_CMI] = &codec_cm_image,
};
static const kolmo_codec codec_graph = { C_GRAPH, "graph", NULL, NULL, NULL, NULL };
static inline const kolmo_codec *codec_get(int codec) {
    return codec >= 0 && (size_t)codec < sizeof kolmo_codecs / sizeof kolmo_codecs[0] ? kolmo_codecs[codec] : NULL;
}
static size_t codec_bound(int codec, size_t n) {
    const kolmo_codec *c = codec_get(codec);
    return c && c->bound ? c->bound(n) : n;
}
static size_t codec_compress(codec_ctx *x, int codec, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    const kolmo_codec *c = codec_get(codec);
    return c && c->compress ? c->compress(x, level, in, n, out, cap) : 0;
}
static int codec_decompress(codec_ctx *x, int codec, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    const kolmo_codec *c = codec_get(codec);
    return c && c->decompress ? c->decompress(x, in, cn, out, n) : -1;
}""", L(1523, 1523), "", L(1480, 1480))

# ------------------------------------------------------------------ core
put("src/core/pool.c",
    "/* Threads: per-thread state kept across calls, the persistent worker pool, and the default thread count. */",
    L(1525, 1555), "", L(1582, 1582), "", L(2765, 2818))
put("src/core/container.h", L(1557, 1573))
put("src/plan/plan.h", "/* The plan of a block: transform and its parameters, chosen by plan/planner.c. */", L(1576, 1580))
put("src/plan/planner.c", "/* The planner: codec per mode, the candidate trials, and the file-level analysis (make_plan). */",
    L(1605, 1617), "", L(1675, 2321))
put("src/core/io.c", L(2323, 2342))
put("src/core/blocks.c", "/* Block splitting, slot sizes, and the block encoder run by the pool (compress_worker). */",
    L(2344, 2406), "", L(2414, 2763))
put("src/transforms/tar.c", L(2838, 2858))
put("src/plan/frontier.c", "/* Opt-in research planner hooks of the frontier build (-DKOLMO_FRONTIER). */", L(2860, 2995))
put("src/plan/parallel.c", "/* Planning of segments (archive members) on the pool, and ultra's check of the text transforms. */",
    L(2820, 2836), "", L(2997, 3070))
put("src/core/compress.c", "/* The encoder: plan, split, code the blocks on the pool, write the container. */", L(3075, 3213))
put("src/core/decode.c", "/* The decoder: header and index parsing, block decoding on the pool. */", L(3219, 3412), "", L(3416, 3439))
put("src/api/api.c", "/* Public entry points (kolmo.h): parameter checks and the calls into core/. */",
    L(2407, 2412), "", L(3215, 3217), "", L(3414, 3414), "", L(3441, 3557))

missing = [i for i in range(1, len(K)) if not used[i] and K[i - 1].strip()]
if missing:
    sys.exit("uncovered lines: " + ", ".join(map(str, missing[:50])))

# ------------------------------------------------------------------ the .inc files (git mv, then edits)
moves = [("src/tbl2.inc", "src/transforms/table.c"), ("src/log.inc", "src/transforms/log.c"), ("src/sci.inc", "src/transforms/sci.c"),
         ("src/word.inc", "src/transforms/word.c"), ("src/cm.inc", "src/codecs/cm.c"), ("src/kfam.inc", "src/codecs/fastq.c"),
         ("src/nplane.inc", "src/codecs/nplane.c"), ("src/kgraph.inc", "src/transforms/graph.c"),
         ("src/kg_int_fwd.inc", "src/transforms/graph_fwd.c"), ("src/lines.inc", "src/transforms/lines.c"),
         ("src/typed_int_encoder.inc", "src/transforms/graph_typed.c"), ("src/stream.inc", "src/core/stream.c"),
         ("src/cli.c", "src/cli/cli.c")]
for a, b in moves:
    os.makedirs(os.path.dirname(b), exist_ok=True)
    subprocess.check_call(["git", "mv", a, b])

# graph.c: host switches -> arch/cpu.h, vector kernels -> arch/x86_64/int_inv_*.c, fi_vec4 stays
g = G[:249] + G[259:286] + ["#if defined(FI_VEC4)"] + G[337:]
g = "\n".join(g).replace('#include "kg_int_fwd.inc"', '#include "graph_fwd.c"')
open("src/transforms/graph.c", "w").write(g)

# cm.c: the image model's codec shell (codec 17) -> codecs/cm_image.c; table entry for codec 7
CM = rd("src/codecs/cm.c")
expect(718, "Image model (block codec 17)", CM); expect(762, "", CM)
put("src/codecs/cm_image.c", "\n".join(CM[717:]).rstrip("\n"),
    """#ifdef KOLMO_CODEC_REGISTRY
static int cm_image_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return cm_decompress_img(in, cn, out, n); }
static const kolmo_codec codec_cm_image = { C_CMI, "cm_image", NULL, NULL, cm_image_decompress, NULL };   /* written at block level */
#endif""")
open("src/codecs/cm.c", "w").write("\n".join(CM[:717]).rstrip("\n") + "\n" + """
/* Codec 7 table (kolmo builds only: tests/test_cm_compact.c includes this file on its own). Model 3 where it gains: text. */
#ifdef KOLMO_CODEC_REGISTRY
static size_t cm_bound(size_t n) { return n + n / 16 + 64; }
static size_t cm_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; return cm_compress_m(in, n, out, cap, level == 1 ? 1 : looks_text(in, n) ? 3 : 2);
}
static int cm_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return cm_decompress(in, cn, out, n); }
static const kolmo_codec codec_cm = { C_CM, "cm", cm_bound, cm_codec_compress, cm_codec_decompress, NULL };
#endif
""")
# nplane.c: table entry (decoded only in container version 9 or later)
open("src/codecs/nplane.c", "a").write("""
static size_t nplane_bound(size_t n) { return n + 2 + 8 + 16; }
static int nplane_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    return x->allow_nplane ? np_decompress(x, in, cn, out, n) : -1;
}
static const kolmo_codec codec_nplane = { C_NPLANE, "nplane", nplane_bound, NULL, nplane_decompress, NULL };   /* written by the graph leaves */
""")
open("src/codecs/fastq.c", "a").write("""
#ifdef KOLMO_CODEC_REGISTRY
static const kolmo_codec codec_fastq = { C_FQ, "fastq", NULL, NULL, NULL, NULL };   /* coded at block level (core/blocks.c, core/decode.c) */
#endif
""")

for p, t in files.items():
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w").write(t)

UNITY = """/* kolmo — native core of the kolmo compressor. See kolmo.h and docs/LAYOUT.md.
 *
 * The library is one translation unit: this file includes the parts of src/ below in dependency order, so the
 * compiler sees all of it at once (inlining across parts, as before the split) and the internal functions stay
 * static (tests include this file to reach them). Assembly kernels (src/arch/<isa>/*.S) and arch/xxh3.c are
 * compiled on their own. Add a new part here and to the layer list in docs/LAYOUT.md. */
#define _GNU_SOURCE
#include "core/common.h"

/* arch: CPU features, checksums and the vector kernels with their portable references */
#include "arch/cpu.h"
#include "arch/x86_64/crc32c_sse42.c"
#include "arch/aarch64/crc32c_arm.c"
#include "arch/crc32c.c"
#include "arch/x86_64/int_inv_sse2.c"
#include "arch/x86_64/int_inv_avx2.c"
#include "transforms/transpose.c"
#include "transforms/common.h"
#include "algo/decimal.c"
#include "arch/x86_64/decimal_avx2.c"
#include "arch/aarch64/decimal_neon.c"
#include "arch/decimal.c"

#include "core/memory.c"
#include "transforms/table_v03.c"
#include "plan/tunables.h"
#include "plan/detect.c"
#include "codecs/codec.h"

/* These transforms take their scratch memory from the per-thread cache (core/memory.c). */
#define malloc(n) zs_malloc(n)
#define calloc(a, b) zs_calloc(a, b)
#define realloc(p, n) zs_realloc(p, n)
#define free(p) zs_free(p)
#include "transforms/table.c"
#include "transforms/log.c"
#include "algo/hist.c"
#include "transforms/sci.c"
#include "transforms/word.c"
#undef malloc
#undef calloc
#undef realloc
#undef free

/* codecs */
#include "codecs/cm.c"
#include "codecs/cm_image.c"
#include "transforms/shuffle.c"
#include "codecs/fastq.c"
#include "transforms/x86.c"
#include "codecs/store.c"
#include "codecs/lz4.c"
#include "codecs/zstd.c"
#include "codecs/brotli.c"
#include "codecs/bwt.c"
#include "codecs/lzma.c"
#include "algo/varint.h"
#include "codecs/huf.c"
#include "codecs/rans.c"
#include "codecs/graph_leaf.c"
#include "codecs/codec.c"
#include "codecs/nplane.c"
#include "transforms/graph.c"

/* core, plan, api */
#include "core/pool.c"
#include "core/container.h"
#include "transforms/lines.c"
#include "plan/plan.h"
#include "transforms/json.c"
#include "plan/planner.c"
#include "core/io.c"
#include "core/blocks.c"
#include "transforms/tar.c"
#include "plan/frontier.c"
#include "plan/parallel.c"
/* Opt-in typed encoder route ported from the RC1 integration (PR #10). It writes ordinary version 7 graph frames. */
#include "transforms/graph_typed.c"
#include "core/compress.c"
#include "core/decode.c"
#include "api/api.c"
#include "core/stream.c"
"""
open("src/kolmo.c", "w").write(UNITY)
print("ok:", len(files), "new files,", len(moves), "moves")
