// lzbench wrapper for kolmo that loads the library at run time. Used on macOS; see setup_macos.sh.
//
// Why at run time: the shared library exports only kolmo_*, so kolmo's vendored zstd, lz4, brotli, libbsc and
// liblzma never mix with the copies lzbench builds for its own entries, and a second build of the same source can
// be measured by the same lzbench binary. Two codecs are registered:
//   kolmo      the release build
//   kolmo_ctl  the control build (-DKOLMO_FORCE_GENERIC_INT_INV: the generic integer inverse in the decoder)
// The library is loaded by the init hook, which lzbench calls before it starts the clock.
//
// Fairness: kolmo runs with threads = 1 and its default block size, as `kolmo -T1` does.
//
// Levels 0..12 are the ladder of kolmo_lzbench.cpp; 13..17 are settings added for the 1.2.0 candidate:
//   0  rt              transforms + LZ4
//   1  fast -l 1 -q    quick analysis + zstd 1
//   2  fast -l 2 -q    quick analysis + zstd 2
//   3  fast -q         quick analysis + zstd 3
//   4  fast            transforms + zstd 3 (the default, `-m balanced`)
//   5  hc -l 9         transforms + zstd 9
//   6  hc -l 12        transforms + zstd 12
//   7  max -l 1        transforms + BWT for every block
//   8  eco             transforms + brotli 8
//   9  hc              transforms + zstd 19 (22 for text)
//  10  max             transforms + BWT (text) or LZMA2 (binary) per block (`-m ratio`)
//  11  ultra -l 1      transforms + context mixing, model 1
//  12  ultra           transforms + context mixing (`-x`)
//  13  ultra -l 2      as 12, with context mixing on graph leaves (`-x -l 2`)
//  14  fast --native-leaf   as 4, with plane-context leaves for integer series
//  15  fast --typed-huf     as 4, integer NPY and PCM16 WAV coded type first
//  16  speed           minimal analysis + LZ4 (`-m speed`)
//  17  fast -l 1 -Q    minimal analysis + zstd 1
#ifndef BENCH_REMOVE_KOLMO
#include "../../bench/codecs.h"
#include "kolmo.h"
#include "kolmo_paths.h"   // KOLMO_LIB_PATH, KOLMO_LIB_CTL_PATH: written by setup_macos.sh
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int64_t (*kolmo_c_fn)(const uint8_t *, size_t, uint8_t *, size_t, const kolmo_params *);
typedef int64_t (*kolmo_d_fn)(const uint8_t *, size_t, uint8_t *, size_t, int);

static struct { void *handle; kolmo_c_fn c; kolmo_d_fn d; } kolmo_lib[2];
static const char *const kolmo_lib_env[2] = {"KOLMO_LZBENCH_LIB", "KOLMO_LZBENCH_LIB_CTL"};
static const char *const kolmo_lib_path[2] = {KOLMO_LIB_PATH, KOLMO_LIB_CTL_PATH};

static void kolmo_load(int k)
{
    if (kolmo_lib[k].handle) return;
    const char *p = getenv(kolmo_lib_env[k]);
    if (!p || !*p) p = kolmo_lib_path[k];
    void *h = dlopen(p, RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "kolmo: cannot load %s: %s\n", p, dlerror()); exit(1); }
    kolmo_lib[k].c = (kolmo_c_fn)dlsym(h, "kolmo_compress");
    kolmo_lib[k].d = (kolmo_d_fn)dlsym(h, "kolmo_decompress");
    if (!kolmo_lib[k].c || !kolmo_lib[k].d) { fprintf(stderr, "kolmo: %s lacks kolmo_compress or kolmo_decompress\n", p); exit(1); }
    kolmo_lib[k].handle = h;
    if (getenv("KOLMO_LZBENCH_VERBOSE")) {   // which image serves this codec: lets a run prove the two builds stay apart
        Dl_info info;
        memset(&info, 0, sizeof info);
        dladdr((void *)kolmo_lib[k].d, &info);
        fprintf(stderr, "kolmo: %s loaded from %s (kolmo_decompress at %p)\n", k ? "kolmo_ctl" : "kolmo",
                info.dli_fname ? info.dli_fname : "?", (void *)kolmo_lib[k].d);
    }
}

#define KOLMO_LEVELS 18
static const struct { kolmo_mode mode; int level, transforms; } kolmo_ladder[KOLMO_LEVELS] = {
    {KOLMO_RT, 0, 1}, {KOLMO_FAST, 1, 2}, {KOLMO_FAST, 2, 2}, {KOLMO_FAST, 0, 2}, {KOLMO_FAST, 0, 1}, {KOLMO_HC, 9, 1},
    {KOLMO_HC, 12, 1}, {KOLMO_MAX, 1, 1}, {KOLMO_ECO, 0, 1}, {KOLMO_HC, 0, 1}, {KOLMO_MAX, 0, 1}, {KOLMO_ULTRA, 1, 1},
    {KOLMO_ULTRA, 0, 1}, {KOLMO_ULTRA, 2, 1}, {KOLMO_FAST, 0, 1 | KOLMO_T_NATIVE_LEAF}, {KOLMO_FAST, 0, 1 | KOLMO_T_TYPED_HUF},
    {KOLMO_RT, 0, 3}, {KOLMO_FAST, 1, 3},
};

static int64_t kolmo_c(int k, char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options)
{
    int l = codec_options->level;
    if (l < 0 || l >= KOLMO_LEVELS) return 0;
    kolmo_load(k);
    kolmo_params p;
    memset(&p, 0, sizeof p);
    p.mode = kolmo_ladder[l].mode;
    p.level = kolmo_ladder[l].level;             // 0 = the mode's default coder level
    p.threads = 1;                               // one thread, like every other lzbench entry
    p.block = 0;                                 // default block size
    p.transforms = kolmo_ladder[l].transforms;   // 1 full analysis, 2 quick (-q), 3 minimal (-Q), plus opt-in bits
    int64_t r = kolmo_lib[k].c((const uint8_t *)inbuf, insize, (uint8_t *)outbuf, outsize, &p);
    return r < 0 ? 0 : r;
}

static int64_t kolmo_d(int k, char *inbuf, size_t insize, char *outbuf, size_t outsize)
{
    kolmo_load(k);
    int64_t r = kolmo_lib[k].d((const uint8_t *)inbuf, insize, (uint8_t *)outbuf, outsize, 1);
    return r < 0 ? 0 : r;
}

char *lzbench_kolmo_init(size_t, size_t, size_t) { kolmo_load(0); return (char *)malloc(16); }
char *lzbench_kolmo_ctl_init(size_t, size_t, size_t) { kolmo_load(1); return (char *)malloc(16); }
void lzbench_kolmo_deinit(char *workmem) { free(workmem); }

int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options)
{
    return kolmo_c(0, inbuf, insize, outbuf, outsize, codec_options);
}

int64_t lzbench_kolmo_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *)
{
    return kolmo_d(0, inbuf, insize, outbuf, outsize);
}

int64_t lzbench_kolmo_ctl_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options)
{
    return kolmo_c(1, inbuf, insize, outbuf, outsize, codec_options);
}

int64_t lzbench_kolmo_ctl_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *)
{
    return kolmo_d(1, inbuf, insize, outbuf, outsize);
}
#endif // BENCH_REMOVE_KOLMO
