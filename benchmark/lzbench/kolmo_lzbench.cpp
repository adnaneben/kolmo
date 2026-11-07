// lzbench wrapper for kolmo (https://github.com/adnaneben/kolmo).
//
// Fairness: kolmo runs single-threaded (threads = 1), with its default block size and transforms,
// exactly as a user gets it from `kolmo -T1 -m <mode>`. lzbench levels 0..5 map to kolmo's modes:
//   0 rt (LZ4) | 1 fast (zstd 3) | 2 eco (brotli 8) | 3 hc (zstd 19/22) | 4 max (BWT/LZMA2) | 5 ultra (CM)
// kolmo is linked as one object with all internal symbols localized, so its vendored codecs never
// interact with the ones lzbench builds for other entries.
#ifndef BENCH_REMOVE_KOLMO
#include "../../bench/codecs.h"
#include "czn.h"

int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize,
                               codec_options_t *codec_options)
{
    czn_params p;
    p.mode = (czn_mode)codec_options->level;
    p.level = 0;          // mode default
    p.threads = 1;        // single-threaded, like every other lzbench entry
    p.block = 0;          // default block size
    p.transforms = 1;     // default: analyse and transform
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
