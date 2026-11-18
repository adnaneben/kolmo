/* Per-stage timing of the block-sorting pipeline of codec 18 (LZP 17/96, BWT, adaptive QLFC, >= 4 MiB coder blocks),
   in memory, one block per file. Usage: stages THREADS FILE... ; prints MB/s of each stage relative to the input
   size, best of KOLMO_REPS (default 2) runs. Build: see benchmark/w4/bwt/README in RESULTS.md. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include "algo/lzp.h"
#include "algo/qlfc.h"
#include "algo/sais/sais.h"
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
#define BEST(var, stmt) do { double _b = 1e9; for (int _r = 0; _r < reps; _r++) { double _t = now(); stmt; _t = now() - _t; if (_t < _b) _b = _t; } var = _b; } while (0)
int main(int argc, char **argv) {
    int th = atoi(argv[1]), reps = getenv("KOLMO_REPS") ? atoi(getenv("KOLMO_REPS")) : 2;
    for (int f = 2; f < argc; f++) {
        FILE *fp = fopen(argv[f], "rb"); fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
        uint8_t *in = malloc(n + 64), *lz = malloc(n + 64), *bw = malloc(n + 64), *q = malloc(n + 4096), *d = malloc(n + 64), *d2 = malloc(n + 64);
        int32_t *A, I[256];
        if (getenv("HUGE")) { size_t sz = ((size_t)(n + 1) * 4 + (2u << 20) - 1) & ~(size_t)((2u << 20) - 1);
            A = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); madvise(A, sz, MADV_HUGEPAGE); }
        else A = malloc((n + 1) * 4);
        if (fread(in, 1, n, fp) != (size_t)n) return 2; fclose(fp);
        double tl, tb, tq, tuq, tub, tul; int m = 0, qs = 0, r = 1;
        BEST(tl, m = kolmo_lzp_compress(in, lz, (int)n, 17, 96, th));
        if (m <= 0) { memcpy(lz, in, n); m = (int)n; }
        while (((m - 1) / r) > 255 || r < (m / 8 > 1 ? 0 : 0)) r <<= 1;
        { int mod = m / 8; mod |= mod >> 1; mod |= mod >> 2; mod |= mod >> 4; mod |= mod >> 8; mod |= mod >> 16; mod >>= 1; if (mod + 1 > r) r = mod + 1; }
        BEST(tb, kolmo_sais_bwt_aux(lz, bw, A, m, r, I, th));
        BEST(tq, qs = kolmo_qlfc_compress_min(bw, q, m, 2, 4 << 20, th));
        BEST(tuq, kolmo_qlfc_decompress(q, qs, d, m, 2, th));
        BEST(tub, kolmo_sais_unbwt_aux(d, d2, A, m, r, I, th));
        int ok = !memcmp(d2, lz, m);
        BEST(tul, kolmo_lzp_decompress(d2, m, d, (int)n, 17, 96, th));
        ok = ok && !memcmp(d, in, n);
        double mb = n / 1e6;
        printf("%-14s T%d n=%ld lzp=%d out=%d | enc lzp %.0f bwt %.1f qlfc %.1f MB/s | dec qlfc %.1f unbwt %.1f unlzp %.0f MB/s | enc-noq %.1f dec-noq %.1f %s\n",
               strrchr(argv[f], '/') ? strrchr(argv[f], '/') + 1 : argv[f], th, n, m, qs, mb / tl, mb / tb, mb / tq, mb / tuq, mb / tub, mb / tul,
               mb / (tl + tb), mb / (tub + tul), ok ? "ok" : "BAD");
        free(in); free(lz); free(bw); free(q); free(d); free(d2); if (!getenv("HUGE")) free(A);
    }
    return 0;
}
