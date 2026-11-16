/* bsc-like driver over libbsc 3.4.0: blocks of B bytes (bsc's default 25 MiB), LZP 15/72, BWT, static QLFC.
   OpenMP: blocks in parallel and libbsc's own multithreading, as the bsc tool does. Usage: bscdrv threads file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "libbsc/libbsc.h"
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc, char **argv) {
    int th = atoi(argv[1]); omp_set_num_threads(th);
    int feat = LIBBSC_FEATURE_FASTMODE | (th > 1 ? LIBBSC_FEATURE_MULTITHREADING : 0);
    bsc_init(feat);
    FILE *f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *in = malloc(n), *out = malloc(n + n / 16 + 4096), *d = malloc(n);
    if (fread(in, 1, n, f) != (size_t)n) return 2;
    long B = getenv("BSC_BLOCK") ? atol(getenv("BSC_BLOCK")) : 25L << 20;
    int nb = (int)((n + B - 1) / B); long *cs = calloc(nb, sizeof(long)); long *co = calloc(nb, sizeof(long));
    for (int b = 0; b < nb; b++) co[b] = b * (B + 1024);
    unsigned char *tmp = malloc((size_t)nb * (B + 1024));
    double t0 = now();
    #pragma omp parallel for schedule(dynamic, 1) num_threads(th > nb ? nb : th)
    for (int b = 0; b < nb; b++) { long s = (long)b * B, m = n - s < B ? n - s : B;
        cs[b] = bsc_compress(in + s, tmp + co[b], (int)m, 15, 72, LIBBSC_BLOCKSORTER_BWT, LIBBSC_CODER_QLFC_STATIC, feat); }
    double t1 = now(); long tot = 0; for (int b = 0; b < nb; b++) tot += cs[b];
    #pragma omp parallel for schedule(dynamic, 1) num_threads(th > nb ? nb : th)
    for (int b = 0; b < nb; b++) { long s = (long)b * B, m = n - s < B ? n - s : B;
        bsc_decompress(tmp + co[b], (int)cs[b], d + s, (int)m, feat); }
    double t2 = now();
    printf("%s threads=%d %ld -> %ld comp %.2f s dec %.2f s ok=%d\n", argv[2], th, n, tot, t1 - t0, t2 - t1, !memcmp(d, in, n));
    return 0;
}
