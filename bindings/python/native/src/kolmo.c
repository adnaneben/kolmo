/* kolmo — native core of the kolmo compressor. See kolmo.h and docs/LAYOUT.md.
 *
 * The library is one translation unit: this file includes the parts of src/ below in dependency order, so the
 * compiler sees all of it at once (inlining across parts, as before the split) and the internal functions stay
 * static (tests include this file to reach them). Assembly kernels (src/arch/<isa>/NAME.S) and arch/xxh3.c are
 * compiled on their own. Add a new part here and to the layer list in docs/LAYOUT.md. */
#define _GNU_SOURCE
#include "core/common.h"
#ifdef KOLMO_GPU
#include "core/hetero.h"            /* the GPU worker (make GPU=1, macOS): compiled on its own, off unless asked for */
#endif

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
/* NEON kernels of the simd-arm lane (src/arch/arch.h: KOLMO_NEON, kolmo_force_c); each has its C reference in the
 * part that calls it, and tests/test_arch.c compares the two */
#include "arch/arch.h"
int kolmo_force_c;
#if KOLMO_NEON
#include "arch/aarch64/word_neon.c"
#include "arch/aarch64/shuffle_neon.c"
#include "arch/aarch64/image_neon.c"
#include "arch/aarch64/intinv_neon.c"
#include "arch/aarch64/table_neon.c"
#include "arch/aarch64/dec64_neon.c"
#endif
#include "arch/x86_64/word_avx2.c"      /* x86 AVX2 word decoder (decmap lane), chosen by CPUID; same contract as the NEON one */

#include "core/memory.c"
#include "transforms/table_v03.c"
#include "plan/tunables.h"
#include "plan/detect.c"
#include "plan/meta.c"              /* structure read from the input's own headers (docs/METADATA.md) */
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
#include "codecs/gr6.c"
#include "codecs/cm_par.c"
#include "transforms/shuffle.c"
#include "codecs/fastq.c"
#include "transforms/x86.c"
#include "transforms/arm64.c"         /* ARM64 branch transform (plan type 11, tflag 14; plan/meta.c chooses it) */
#include "codecs/store.c"
#include "codecs/lz4.c"
#include "codecs/zstd.c"
#include "codecs/brotli.c"
#include "codecs/bwt.c"
#include "codecs/lzma.c"
#include "algo/varint.h"
#include "codecs/huf.c"
#include "codecs/rans.c"
#include "codecs/native.c"            /* Kolmo's own LZ, Huffman and LZMA-class coders (codecs 24, 27-29), opt-in */
#include "codecs/graph_leaf.c"
#include "codecs/codec.c"
/* format 2 (container version 11): audio and video blocks, per-plane / Huffman-then-zstd / zero-run graph leaves */
#include "codecs/audio.c"
#include "codecs/video.c"
#include "codecs/hufzstd_leaf.c"
#include "codecs/planes_leaf.c"
#include "codecs/zrun_leaf.c"
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
#include "transforms/table_cols.c"   /* column-split table blocks (flag 13), the column reader of IDX1 queries */
#include "core/memneed.c"           /* working-set estimates: frame declaration, reservations (docs/MEMORY-MODEL.md) */
#include "core/trials.c"
#include "core/blocks.c"
#include "plan/frontier.c"
#include "plan/parallel.c"
/* Opt-in typed encoder route ported from the RC1 integration (PR #10). It writes ordinary version 7 graph frames. */
#include "transforms/graph_typed.c"
#include "core/compress.c"
#include "core/decode.c"
#include "api/api.c"
#include "core/stream.c"

/* rc3 features (features-a lane): trained profiles (kolmo train, -P) and --target, with the public entry points */
#include "plan/profile.c"
#include "plan/target.c"
#include "api/features.c"
/* container extensions (docs: SPEC.md): KEXT trailer, IDX1 queryable archives, DDP1 deduplicating archives */
#include "core/extension.c"
#include "core/index.c"
#include "core/dedup.c"
