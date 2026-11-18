/* qtool: QLFC experiments on codec 18's stream (w4 entropy lane).
 * usage: qtool FILE THREADS REPS MINBLOCK_KIB...  -- LZP 17/96 + BWT once, then per MINBLOCK: QLFC size, encode time,
 * decode time (best of REPS) at THREADS, checked. Built by build.sh (includes src/kolmo.c for the internals). */
#include "../../../src/kolmo.c"
#include <time.h>
#include "../../../src/algo/lzp.h"
#include "../../../src/algo/sais/sais.h"
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: qtool FILE THREADS REPS MINBLOCK_KIB...\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *in = malloc(n + 64); if (fread(in, 1, n, f) != (size_t)n) return 1; fclose(f);
    int T = atoi(argv[2]), reps = atoi(argv[3]);
    uint8_t *w = malloc(n + 64); int lz = kolmo_lzp_compress(in, w, (int)n, 17, 96, T); if (lz <= 0) { memcpy(w, in, n); lz = (int)n; }
    int32_t *A = malloc((size_t)lz * 4 + 4); int32_t I[256];
    int rlog = 1; while ((((int64_t)lz - 1) >> rlog) > 255) rlog++;
    if (kolmo_sais_bwt_aux(w, w, A, lz, 1 << rlog, I, T)) return 1;
    free(A);
    uint8_t *o = malloc((size_t)lz + 64), *d = malloc((size_t)lz + 64);
    {   /* the other decode stages: inverse BWT and LZP (best of REPS) */
        int32_t *P = malloc(((size_t)lz + 1) * 4); uint8_t *u = malloc((size_t)lz + 64), *x = malloc((size_t)n + 64); double bu = 1e9, bl = 1e9;
        for (int r = 0; r < reps; r++) {
            double t0 = now(); kolmo_sais_unbwt_aux(w, u, P, lz, 1 << rlog, I, T); double t = now() - t0; if (t < bu) bu = t;
            if (lz < n) { t0 = now(); kolmo_lzp_decompress(u, lz, x, (int)n, 17, 96, T); t = now() - t0; if (t < bl) bl = t; } else bl = 0;
        }
        printf("unbwt %.3f s lzp %.3f s (threads %d)\n", bu, bl, T); free(P); free(u); free(x);
    }
    printf("file %s n %ld lzp %d\n", argv[1], n, lz);
    if (getenv("QTOOL_DUMP")) { FILE *g = fopen(getenv("QTOOL_DUMP"), "wb"); fwrite(w, 1, (size_t)lz, g); fclose(g); }   /* the BWT output */
    for (int a = 4; a < argc; a++) {
        int mb = atoi(argv[a]) * 1024;
        double t0 = now(); int c = kolmo_qlfc_compress_min(w, o, lz, KOLMO_QLFC_ADAPTIVE, mb, T); double te = now() - t0;
        double best = 1e9;
        for (int r = 0; r < reps; r++) { t0 = now(); int k = kolmo_qlfc_decompress(o, c, d, lz + 64, KOLMO_QLFC_ADAPTIVE, T); double t = now() - t0; if (k != lz || memcmp(d, w, lz)) { printf("MISMATCH\n"); return 1; } if (t < best) best = t; }
        printf("minblock %6d KiB blocks %3d size %9d enc %.3f s dec %.3f s (%.1f MB/s of lzp output)\n", mb / 1024, o[0], c, te, best, lz / best / 1e6);
    }
    return 0;
}
