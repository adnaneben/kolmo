#ifndef BENCH_REMOVE_KOLMO
#include "misc/kolmo/src/kolmo.h"
#include <string.h> // memset

// lzbench level n = `kolmo -n` for 1..9, and 10 = `kolmo -U` (ultra): kolmo_params_for_level is the table the
// command line uses, so the bytes are the same as `kolmo -T 1 -<n>`. One thread, default block size.
int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options)
{
    kolmo_params p;
    memset(&p, 0, sizeof p);
    if (kolmo_params_for_level((int)codec_options->level, &p) != 0) return 0;
    p.threads = 1;
    int64_t r = kolmo_compress((const uint8_t*)inbuf, insize, (uint8_t*)outbuf, outsize, &p);
    return r < 0 ? 0 : r;
}

int64_t lzbench_kolmo_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options)
{
    int64_t r = kolmo_decompress((const uint8_t*)inbuf, insize, (uint8_t*)outbuf, outsize, 1);
    return r < 0 ? 0 : r;
}
#endif // BENCH_REMOVE_KOLMO
