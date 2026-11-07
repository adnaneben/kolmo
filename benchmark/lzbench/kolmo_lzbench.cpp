// lzbench wrapper for kolmo (https://github.com/adnaneben/kolmo).
//
// Fairness: kolmo runs single-threaded (threads = 1), with its default block size and settings,
// exactly as a user gets it from the CLI. kolmo is linked as one object with all internal symbols
// localized, so its vendored codecs never interact with the ones lzbench builds for other entries.
//
// Levels 0..12 are a ladder from fastest to strongest. Every level is an ordinary kolmo setting,
// reproducible with `kolmo -T1 -m <mode> [-l <coder level>] [-q]` (-q: quick analysis, cheap transforms only):
//   0  rt              transforms + LZ4
//   1  fast -l 1 -q    quick analysis + zstd 1
//   2  fast -l 2 -q    quick analysis + zstd 2
//   3  fast -q         quick analysis + zstd 3
//   4  fast            transforms + zstd 3 (mode default)
//   5  hc -l 9         transforms + zstd 9
//   6  hc -l 12        transforms + zstd 12
//   7  max -l 1        transforms + BWT for every block
//   8  eco             transforms + brotli 8 (mode default)
//   9  hc              transforms + zstd 19 (mode default; 22 for text)
//  10  max             transforms + BWT (text) or LZMA2 (binary) per block
//  11  ultra -l 1      transforms + context mixing, model 1 (kolmo 1.0.0's model; CM only, faster)
//  12  ultra           transforms + context mixing, model 2 (best of CM, LZMA2 and BWT per block)
#ifndef BENCH_REMOVE_KOLMO
#include "../../bench/codecs.h"
#include "czn.h"

static const struct { czn_mode mode; int level, transforms; } kolmo_ladder[13] = {
    {CZN_RT, 0, 1}, {CZN_FAST, 1, 2}, {CZN_FAST, 2, 2}, {CZN_FAST, 0, 2}, {CZN_FAST, 0, 1}, {CZN_HC, 9, 1},
    {CZN_HC, 12, 1}, {CZN_MAX, 1, 1}, {CZN_ECO, 0, 1}, {CZN_HC, 0, 1}, {CZN_MAX, 0, 1}, {CZN_ULTRA, 1, 1},
    {CZN_ULTRA, 0, 1},
};

int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize,
                               codec_options_t *codec_options)
{
    int l = codec_options->level;
    if (l < 0 || l > 12) return 0;
    czn_params p;
    p.mode = kolmo_ladder[l].mode;
    p.level = kolmo_ladder[l].level;   // 0 = the mode's default coder level
    p.threads = 1;                     // single-threaded, like every other lzbench entry
    p.block = 0;                       // default block size
    p.transforms = kolmo_ladder[l].transforms;   // 1: full analysis (default), 2: quick analysis (-q)
    int64_t r = czn_compress((const uint8_t *)inbuf, insize, (uint8_t *)outbuf, outsize, &p);
    return r < 0 ? 0 : r;
}

int64_t lzbench_kolmo_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize,
                                 codec_options_t *)
{
    int64_t r = czn_decompress((const uint8_t *)inbuf, insize, (uint8_t *)outbuf, outsize, 1);
    return r < 0 ? 0 : r;
}
#endif // BENCH_REMOVE_KOLMO
