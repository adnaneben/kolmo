/* bwtbench: codec 18 decode time against the number of sampled BWT indexes (decmap lane, rc3 wave 3).
 *   bwtbench FILE... : for each index count k (default, 16, 32, 64, 128, 255) prints payload bytes and the best decode
 *   time at 1 thread and at all threads (separate runs), plus the inverse-BWT stage alone. */
#define _GNU_SOURCE
#include "codecs/bwt_stream.h"
#include "algo/qlfc.h"
#include "algo/sais/sais.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    int T = (int)sysconf(_SC_NPROCESSORS_ONLN);
    for (int a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); fseek(f, 0, SEEK_END); int n = (int)ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *s = malloc(n), *c = malloc((size_t)n + 4096), *d = malloc((size_t)n + 64);
        if (fread(s, 1, n, f) != (size_t)n) return 1; fclose(f);
        int ks[] = {0, 16, 32, 64, 128, 255};
        for (int ki = 0; ki < 6; ki++) {
            kolmo_bwt_params P = KOLMO_BWT_DEFAULTS;
            if (ks[ki]) { int r = 1; while (((int64_t)n - 1) >> r > ks[ki]) r++; P.index_log2 = r; }  /* at most k indexes (of the LZP output: fewer) */
            int z = kolmo_bwt_compress(s, c, (size_t)n + 4096, n, &P, T);
            if (z <= 0) { printf("fail\n"); return 1; }
            int k = c[1 + 2 + 4 + 1 + ((c[0] & 4) ? 0 : -2)];   /* k byte */
            double bt[2] = {1e9, 1e9};
            for (int ti = 0; ti < 2; ti++) for (int rep = 0; rep < 6; rep++) {
                double t0 = now(); int r = kolmo_bwt_decompress(c, (size_t)z, d, n, ti ? T : 1); double t = now() - t0;
                if (r || memcmp(d, s, n)) { printf("MISMATCH\n"); return 1; }
                if (t < bt[ti]) bt[ti] = t;
            }
            printf("%-28s k=%3d bytes=%d  dec T1 %.1f MB/s  T%d %.1f MB/s\n", strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a], k, z, n / bt[0] / 1e6, T, n / bt[1] / 1e6);
            fflush(stdout);
        }
        free(s); free(c); free(d);
    }
    return 0;
}
