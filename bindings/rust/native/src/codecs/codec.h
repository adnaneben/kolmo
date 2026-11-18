/* Block codecs: identifiers, the per-thread codec context and the codec table (struct kolmo_codec). Each codec
 * file defines one table (or two for one family, as LZ4 and LZ4-HC) and codecs/codec.c lists them by identifier.
 * Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md). */
#define KOLMO_CODEC_REGISTRY 1
enum { C_STORE = 0, C_LZ4 = 1, C_LZ4HC = 2, C_ZSTD = 3, C_BROTLI = 4, C_BSC5 = 5, C_LZMA = 6, C_CM = 7, C_DNA = 8, C_DNA_PREFIX = 9, C_PCM = 10, C_DNA_RUNS = 11,
       C_GRAPH = 12, C_HUF = 13, C_RANS = 14, C_NPLANE = 15, C_FQ = 16, C_CMI = 17, C_BWT = 18,
       C_AUD = 19, C_VID = 20, C_PLANES = 21, C_HZ = 22, C_ZRUN = 23, C_KLZMA = 24, C_CMP = 25, C_GR6 = 26,
       C_KHUF = 27, C_KLZ = 28, C_KLZH = 29 };   /* 24, 27-29: Kolmo's own coders, opt-in (KOLMO_NATIVE, codecs/native.c; version 11) */   /* 18: Kolmo block sorting (BWT in C); 19-23: container version 11 (format 2): 19 audio (kaud), 20 video (kvid), 21 per-plane leaf, 22 Huffman then zstd leaf, 23 zero-run leaf (21-23: graph leaves only), 25 context mixing over parts (columns or halves, coded in parallel), 26 GR6+ line-structured text (ultra blocks only) */   /* 8-11: experimental frontier codecs (opt-in build); 12: kolmo graph block; 13: Huffman only, 14: rANS order 0, 15: native plane-context leaf (graph leaves only; version 9); 16: FASTQ family coder (blocks only; ultra; version 10; src/kfam.inc); 17: context mixing with the image model (ultra; carries its own row length, channels and head); 5: libbsc block sorting (read only since rc3); 18: Kolmo block sorting (codecs/bwt.c; version 11) */

typedef struct { ZSTD_CCtx *zc; ZSTD_DCtx *zd; void *lz4hc_state; const void *dict; size_t dictlen;
                 const struct kolmo_profile *prof;   /* trained profile: its dictionaries for zstd, Brotli and LZ4 (src/plan/profile.c) */
                 int native_leaf;        /* encoder: graph integer residual leaves may use C_NPLANE (opt-in, balanced) */
                 int used_native_leaf;   /* encoder: set when a C_NPLANE leaf was written (the frame needs version 9) */
                 int allow_nplane;       /* decoder: C_NPLANE leaves are valid (container version 9 or later) */
                 int aligned_leaf;       /* encoder: graph integer residual leaves also try plane-aligned Huffman / rANS chunks
                                          * (opt-in: --native-leaf, --typed-huf; existing leaf ids, any version-7 decoder) */
                 int threads;            /* threads the block may use inside a codec (BWT, codec 25 parts); never changes the output */
                 int used_bwt;           /* encoder: set when codec 18 was written (the frame needs version 11) */
                 int used_f2;            /* encoder: a format-2 leaf (codecs 21-23) was written (the frame needs version 11) */
                 int no_f2;              /* encoder: do not try codecs 21-23 (planner scores, the --typed-huf route, WAV sample blocks) */
                 int allow_f2;           /* decoder: codecs 21-23 are valid graph leaves (container version 11 or later) */ } codec_ctx;

/* One block codec. Missing functions keep the behaviour of the rc2 dispatch: bound NULL -> n, compress NULL -> 0
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
static const kolmo_codec codec_store, codec_lz4, codec_lz4hc, codec_zstd, codec_brotli, codec_bwt, codec_bwt5, codec_lzma, codec_cm,
    codec_graph, codec_huf, codec_rans, codec_nplane, codec_fastq, codec_cm_image,
    codec_audio, codec_video, codec_planes, codec_hz, codec_zrun, codec_gr6, codec_cm_par, codec_klzma, codec_khuf, codec_klz, codec_klzh;
