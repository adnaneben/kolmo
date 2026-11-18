/* qdec: QLFC alone on a dumped BWT output (QTOOL_DUMP=file qtool ...): adaptive QLFC encode, then decode best of REPS,
 * checked. usage: qdec BWTFILE THREADS REPS MINBLOCK_KIB. Built from src/algo/qlfc.c and par.c only (any -march). */
#include "algo/qlfc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    if (argc < 5) return 2;
    FILE *f = fopen(argv[1], "rb"); if (!f) return 1; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *w = malloc(n + 64), *o = malloc(n + 64), *d = malloc(n + 64); if (fread(w, 1, n, f) != (size_t)n) return 1; fclose(f);
    int T = atoi(argv[2]), reps = atoi(argv[3]), mb = atoi(argv[4]) * 1024;
    double t0 = now(); int c = kolmo_qlfc_compress_min(w, o, (int)n, KOLMO_QLFC_ADAPTIVE, mb, T); double te = now() - t0, best = 1e9;
    for (int r = 0; r < reps; r++) { t0 = now(); int k = kolmo_qlfc_decompress(o, c, d, (int)n + 64, KOLMO_QLFC_ADAPTIVE, T); double t = now() - t0;
        if (k != n || memcmp(d, w, n)) { printf("MISMATCH\n"); return 1; } if (t < best) best = t; }
    printf("blocks %d size %d enc %.3f dec %.3f s (%.1f MB/s)\n", o[0], c, te, best, n / best / 1e6);
    return 0;
}
