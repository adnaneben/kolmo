/* Static Huffman (zstd's huff0, 4 streams, 128 KiB blocks) on a file: ratio, compress and decompress MB/s.
 * The fair "best in segment" for a context-free symbol code such as the 6-bit k6 mapping. */
#define HUF_STATIC_LINKING_ONLY
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common/huf.h"
#include "common/error_private.h"

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *in = malloc(n), *out = malloc(n + n / 8 + 1024 * 64), *dec = malloc(n + 64);
    if (fread(in, 1, n, f) != n) return 1;
    const size_t B = 128 * 1024; size_t nb = (n + B - 1) / B;
    size_t *csz = malloc(nb * sizeof *csz), *off = malloc(nb * sizeof *off);
    static unsigned wk[HUF_WORKSPACE_SIZE / 4 + 64]; static HUF_CElt ct[HUF_CTABLE_SIZE_ST(255)];
    HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX);
    static unsigned dwk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32 * 4];
    double tc = 1e9, td = 1e9; size_t total = 0;
    for (int r = 0; r < 5; r++) {
        double a = now(); size_t o = 0;
        for (size_t b = 0; b < nb; b++) {
            size_t len = b + 1 < nb ? B : n - b * B; HUF_repeat rep = HUF_repeat_none;
            size_t c = HUF_compress4X_repeat(out + o, B + 1024, in + b * B, len, 255, 11, wk, sizeof wk, ct, &rep, 0);
            if (HUF_isError(c) || c == 0) { fprintf(stderr, "block %zu not compressible\n", b); return 1; }
            csz[b] = c; off[b] = o; o += c;
        }
        double m = now();
        for (size_t b = 0; b < nb; b++) {
            size_t len = b + 1 < nb ? B : n - b * B;
            size_t d = HUF_decompress4X_hufOnly_wksp(dt, dec + b * B, len, out + off[b], csz[b], dwk, sizeof dwk, 0);
            if (HUF_isError(d)) { fprintf(stderr, "decode error\n"); return 1; }
        }
        double e = now(); if (m - a < tc) tc = m - a; if (e - m < td) td = e - m; total = o;
    }
    printf("huff0 %s ratio %.3f  enc %.0f MB/s  dec %.0f MB/s  %s\n", argv[1], (double)n / total, n / tc / 1e6, n / td / 1e6,
           memcmp(in, dec, n) ? "FAIL" : "OK");
    return 0;
}
