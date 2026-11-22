/* Block codecs: identifiers, the per-thread codec context and the codec table (struct kolmo_codec). Each codec
 * file defines one table and codecs/codec.c lists them by identifier.
 * Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md). */
#define KOLMO_CODEC_REGISTRY 1
/* Wire ids: the codec byte of the block index and of graph leaves. Every coder is Kolmo's own (container version 12).
 *   0 stored; 5 block sorting in libbsc 3.4.0's block format (Kolmo's C port; read, and written by tests only); 7 context
 *   mixing; 8-11 experimental frontier codecs (opt-in build); 12 kolmo graph block; 14 rANS order 0; 15 native
 *   plane-context leaf (graph leaves only); 16 FASTQ family coder (blocks only; ultra); 17 context mixing with the
 *   image model; 18 Kolmo block sorting (codecs/bwt.c); 19 audio (kaud); 20 video (kvid); 21 per-plane leaf and
 *   23 zero-run leaf (graph leaves only); 24 KLZMA; 25 context mixing over parts; 26 GR6+ (ultra blocks only);
 *   27 khuf; 28 KLZ; 29 KLZH; 31 kimg (8-bit images); 32 KLZH2 (codecs/native.c, algo/k*.h).
 * Ids 1-4, 6, 13 and 22 named library codecs (LZ4, LZ4-HC, zstd, Brotli, LZMA2, zstd's HUF, Huffman then zstd) in
 * earlier versions; they have no table here, so a frame naming them fails to decode (codec_get returns NULL).
 * Encoder roles (R_*): the planner, the block coder and the graph leaves choose a role and a level on the scale of the
 * coder the role was written for; codec_compress codes it with Kolmo's coder (codec_native_id, native_level) and the
 * id written is always the native one (codec_wire). Roles reuse the retired ids 1-4, 6 and 13, so a role id that
 * reached a frame by mistake could never be decoded as something else. 30 is held for khuf v2 (not merged). */
enum { C_STORE = 0, R_SPEED = 1, R_SPEEDHC = 2, R_LZH = 3, R_ECO = 4, C_BSC5 = 5, R_LZMA = 6, C_CM = 7, C_DNA = 8, C_DNA_PREFIX = 9, C_PCM = 10, C_DNA_RUNS = 11,
       C_GRAPH = 12, R_HUF = 13, C_RANS = 14, C_NPLANE = 15, C_FQ = 16, C_CMI = 17, C_BWT = 18,
       C_AUD = 19, C_VID = 20, C_PLANES = 21, C_ZRUN = 23, C_KLZMA = 24, C_CMP = 25, C_GR6 = 26,
       C_KHUF = 27, C_KLZ = 28, C_KLZH = 29, C_KIMG = 31, C_KLZH2 = 32 };
/* R_SPEED: KLZ level 1 (speed); R_SPEEDHC: KLZ level 3; R_LZH: KLZH2, levels on zstd's scale (balanced 3, hc 19 -> 9;
 * 0 and below, zstd's --fast levels -> KLZH2 level 0, the fast parse);
 * R_ECO: KLZH2 level 7 (eco); R_LZMA: KLZMA (ratio's LZ coder; levels 1-9 and the lc/lp/pb form >= 100); R_HUF: khuf. */

typedef struct { int native_leaf;        /* encoder: graph integer residual leaves may use C_NPLANE (opt-in, balanced) */
                 int used_native_leaf;   /* encoder: set when a C_NPLANE leaf was written */
                 int allow_nplane;       /* decoder: C_NPLANE leaves are valid (graph blocks) */
                 int aligned_leaf;       /* encoder: graph integer residual leaves also try plane-aligned Huffman / rANS chunks
                                          * (opt-in: --native-leaf, --typed-huf; existing leaf ids) */
                 int threads;            /* threads the block may use inside a codec (BWT, codec 25 parts); never changes the output */
                 int used_bwt;           /* encoder: set when codec 18 was written */
                 int used_f2;            /* encoder: a format-2 leaf (codecs 21, 23) was written */
                 int used_cmt;           /* encoder: a context-mixing leaf with learning settings was written */
                 int no_f2;              /* encoder: do not try codecs 21 and 23 (planner scores, the --typed-huf route, WAV sample blocks) */
                 int allow_f2;           /* decoder: codecs 21 and 23 are valid graph leaves (graph blocks) */ } codec_ctx;

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
static const kolmo_codec codec_store, codec_bwt, codec_bwt5, codec_cm,
    codec_graph, codec_rans, codec_nplane, codec_fastq, codec_cm_image,
    codec_audio, codec_video, codec_planes, codec_zrun, codec_gr6, codec_cm_par, codec_klzma, codec_khuf, codec_klz, codec_klzh, codec_kimg, codec_klzh2;
