// lzbench wrapper for kolmo (https://github.com/adnaneben/kolmo).
//
// Fairness: kolmo runs single-threaded (threads = 1), with its default block size and transforms,
// exactly as a user gets it from the CLI. kolmo is linked as one object with all internal symbols
// localized, so its vendored codecs never interact with the ones lzbench builds for other entries.
//
// Levels 0..9 are a ladder from fastest to strongest. Every level is an ordinary kolmo setting,
// reproducible with `kolmo -T1 -m <mode> [-l <coder level>]`:
//   0  rt              transforms + LZ4
//   1  fast -l 1       transforms + zstd 1
//   2  fast -l 2       transforms + zstd 2
//   3  fast            transforms + zstd 3 (mode default)
//   4  hc -l 9         transforms + zstd 9
//   5  hc -l 12        transforms + zstd 12
//   6  eco             transforms + brotli 8 (mode default)
//   7  hc              transforms + zstd 19 (mode default; 22 for text)
//   8  max             transforms + BWT or LZMA2 per block
//   9  ultra           transforms + context mixing
#ifndef BENCH_REMOVE_KOLMO
#include "../../bench/codecs.h"
#include "czn.h"

static const struct { czn_mode mode; int level; } kolmo_ladder[10] = {
    {CZN_RT, 0}, {CZN_FAST, 1}, {CZN_FAST, 2}, {CZN_FAST, 0}, {CZN_HC, 9},
    {CZN_HC, 12}, {CZN_ECO, 0}, {CZN_HC, 0}, {CZN_MAX, 0}, {CZN_ULTRA, 0},
};

int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize,
                               codec_options_t *codec_options)
{
    int l = codec_options->level;
    if (l < 0 || l > 9) return 0;
    czn_params p;
    p.mode = kolmo_ladder[l].mode;
    p.level = kolmo_ladder[l].level;   // 0 = the mode's default coder level
    p.threads = 1;                     // single-threaded, like every other lzbench entry
    p.block = 0;                       // default block size
    p.transforms = 1;                  // default: analyse and transform
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
