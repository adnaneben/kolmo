// Per-stage timing of libbsc 3.4.0 on one block (test tool only): LZP 15/72, BWT, adaptive QLFC, and the inverses.
// Usage: bscstages THREADS FILE... (OpenMP threads; libbsc multithreading on when THREADS > 1). Best of 2 runs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "libbsc/libbsc.h"
#include "libbsc/lzp/lzp.h"
#include "libbsc/bwt/bwt.h"
#include "libbsc/coder/coder.h"
static double now(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc, char **argv) {
    int th = atoi(argv[1]); omp_set_num_threads(th);
    int feat = LIBBSC_FEATURE_FASTMODE | (th > 1 ? LIBBSC_FEATURE_MULTITHREADING : 0);
    bsc_init(feat);
    for (int f = 2; f < argc; f++) {
        FILE *fp = fopen(argv[f], "rb"); fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
        unsigned char *in = (unsigned char*)malloc(n), *lz = (unsigned char*)malloc(n + 64), *w = (unsigned char*)malloc(n + 64), *q = (unsigned char*)malloc(n + 4096), *d = (unsigned char*)malloc(n + 64);
        if (fread(in, 1, n, fp) != (size_t)n) return 2; fclose(fp);
        double best[6] = {1e9,1e9,1e9,1e9,1e9,1e9}; int m = 0, qs = 0, idx = 0; unsigned char k = 0; int I[256]; bool ok = true;
        for (int r = 0; r < 2; r++) {
            double t0 = now(); m = bsc_lzp_compress(in, lz, (int)n, 15, 72, feat); double t1 = now();
            if (m <= 0) { memcpy(lz, in, n); m = (int)n; }
            memcpy(w, lz, m);
            double t2 = now(); idx = bsc_bwt_encode(w, m, &k, I, feat); double t3 = now();
            qs = bsc_coder_compress(w, q, m, LIBBSC_CODER_QLFC_ADAPTIVE, feat); double t4 = now();
            int dm = bsc_coder_decompress(q, d, LIBBSC_CODER_QLFC_ADAPTIVE, feat); double t5 = now();
            bsc_bwt_decode(d, m, idx, k, I, feat); double t6 = now();
            ok = ok && dm == m && !memcmp(d, lz, m);
            unsigned a; int o = bsc_lzp_decompress(d, w, m, 15, 72, &a, feat); double t7 = now();
            ok = ok && o == n && !memcmp(w, in, n);
            double ts[6] = {t1-t0, t3-t2, t4-t3, t5-t4, t6-t5, t7-t6};
            for (int i = 0; i < 6; i++) if (ts[i] < best[i]) best[i] = ts[i];
        }
        double mb = n / 1e6;
        printf("%-14s T%d n=%ld lzp=%d out=%d | enc lzp %.0f bwt %.1f qlfc %.1f MB/s | dec qlfc %.1f unbwt %.1f unlzp %.0f MB/s | enc-noq %.1f dec-noq %.1f %s\n",
               strrchr(argv[f], '/') ? strrchr(argv[f], '/') + 1 : argv[f], th, n, m, qs, mb/best[0], mb/best[1], mb/best[2], mb/best[3], mb/best[4], mb/best[5],
               mb/(best[0]+best[1]), mb/(best[4]+best[5]), ok ? "ok" : "BAD");
        free(in); free(lz); free(w); free(q); free(d);
    }
}
