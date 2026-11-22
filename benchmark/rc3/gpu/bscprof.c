/* time the parts of Kolmo's BWT codec on one file: LZP, libsais BWT, QLFC (via bsc_compress) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "libbsc/libbsc.h"
#include "libbsc/bwt/libsais/libsais.h"
#include "libbsc/lzp/lzp.h"
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    uint8_t *s = malloc(n), *o = malloc(n + 4096), *u = malloc(n); int32_t *A = malloc(4 * (size_t)n);
    fread(s, 1, n, f); fclose(f);
    bsc_init(LIBBSC_FEATURE_FASTMODE);
    double t0 = now();
    int r = bsc_compress(s, o, (int)n, LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, LIBBSC_BLOCKSORTER_BWT, LIBBSC_CODER_QLFC_STATIC, LIBBSC_FEATURE_FASTMODE);
    double t1 = now();
    memcpy(u, s, n);
    int l = bsc_lzp_compress(s, u, (int)n, LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, LIBBSC_FEATURE_FASTMODE);
    double t2 = now();
    int m = l > 0 ? l : (int)n; uint8_t *src = l > 0 ? u : s;
    int mod = m / 8; mod |= mod >> 1; mod |= mod >> 2; mod |= mod >> 4; mod |= mod >> 8; mod |= mod >> 16; mod >>= 1;
    int32_t I[256];
    uint8_t *b = malloc(m);
    double t3 = now();
    int idx = libsais_bwt_aux(src, b, A, m, 0, NULL, mod + 1, I);
    double t4 = now();
    int32_t *SA = A; double t5 = now(); int rs = libsais(src, SA, m, 0, NULL); double t6 = now();
    printf("%s n=%ld bsc=%d total=%.3f lzp=%d %.3f bwt(n=%d)=%.3f sa=%.3f (idx %d rs %d) MB/s bwt=%.1f\n", argv[1], n, r, t1 - t0, l, t2 - t1, m, t4 - t3, t6 - t5, idx, rs, m / 1e6 / (t4 - t3));
    return 0;
}
