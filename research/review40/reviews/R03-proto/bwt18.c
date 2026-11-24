/* R03 harness: codec-18 stage sizes on one in-memory block. usage: bwt18 file [lzp_min(0=off)] [reverse 0/1] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "codecs/bwt_stream.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *in = malloc(n), *out = malloc(n + n / 8 + 4096), *dec = malloc(n);
    if (fread(in, 1, n, f) != (size_t)n) return 1; fclose(f);
    int lzp = argc > 2 ? atoi(argv[2]) : 96, rev = argc > 3 ? atoi(argv[3]) : 0, lh = argc > 4 ? atoi(argv[4]) : 17;
    if (rev) for (long i = 0, j = n - 1; i < j; i++, j--) { uint8_t t = in[i]; in[i] = in[j]; in[j] = t; }
    kolmo_bwt_params P = KOLMO_BWT_DEFAULTS; P.lzp_min = lzp; P.lzp_hash = lzp ? lh : 0;
    int r = kolmo_bwt_compress(in, out, n + n / 8 + 4096, (int)n, &P, 1);
    int ok = r > 0 && kolmo_bwt_decompress(out, r, dec, (int)n, 1) == 0 && !memcmp(in, dec, n);
    printf("%s n=%ld lzp=%d/%d rev=%d codec18=%d roundtrip=%s\n", argv[1], n, lh, lzp, rev, r, ok ? "ok" : "FAIL");
    return !ok;
}
