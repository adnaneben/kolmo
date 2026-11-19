/* unbwtbench: inverse BWT stage alone against the index sampling interval (decmap lane). unbwtbench FILE... */
#define _GNU_SOURCE
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
        uint8_t *s = malloc(n), *b = malloc(n), *d = malloc(n); int32_t *A = malloc((size_t)n * 4 + 16), *P = malloc((size_t)n * 4 + 16);
        if (fread(s, 1, n, f) != (size_t)n) return 1; fclose(f);
        for (int k = 8; k <= 256; k *= 2) {
            int rlog = 1; while ((((int64_t)n - 1) >> rlog) >= k) rlog++;
            int32_t r = 1 << rlog, I[257];
            if (kolmo_sais_bwt_aux(s, b, A, n, r, I, T)) return 1;
            double bt[2] = {1e9, 1e9};
            for (int ti = 0; ti < 2; ti++) for (int rep = 0; rep < 5; rep++) {
                double t0 = now(); kolmo_sais_unbwt_aux(b, d, P, n, r, I, ti ? T : 1); double t = now() - t0;
                if (memcmp(d, s, n)) { printf("MISMATCH\n"); return 1; }
                if (t < bt[ti]) bt[ti] = t;
            }
            printf("%-12s n=%d idx=%3d  unbwt T1 %.0f MB/s  T%d %.0f MB/s\n", strrchr(argv[a], '/') + 1, n, (n - 1) / r + 1, n / bt[0] / 1e6, T, n / bt[1] / 1e6); fflush(stdout);
        }
    }
    return 0;
}
