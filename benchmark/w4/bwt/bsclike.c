/* bsc-like command-line driver over libbsc 3.4.0 (test tool only, never linked into Kolmo). Mirrors what
   `bsc e in out -m0 -e2 -b25` and `bsc d` do: 25 MiB blocks (-b), LZP 15/72, BWT, adaptive QLFC (-e2), fast mode;
   with OpenMP, blocks in parallel (each thread reads, codes and writes its own block, so peak memory follows
   threads x block size as in bsc) and libbsc's own multithreading only when threads >= 2 x blocks (bsc's rule).
   Container: "BSCX" | nblocks u32 | per block: index u32, data u32, packed u32, bytes. Not bsc's file format, the same
   blocks. Usage: bsclike e|d IN OUT [-bMB] [-eN] [-tTHREADS]; prints the wall time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>
#include "libbsc/libbsc.h"
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc, char **argv) {
    if (argc < 4) return 1;
    long B = 25L << 20; int coder = LIBBSC_CODER_QLFC_ADAPTIVE, th = omp_get_max_threads();
    for (int i = 4; i < argc; i++) {
        if (!strncmp(argv[i], "-b", 2)) B = atol(argv[i] + 2) << 20;
        if (!strncmp(argv[i], "-e", 2)) { int e = atoi(argv[i] + 2); coder = e == 0 ? LIBBSC_CODER_QLFC_FAST : e == 1 ? LIBBSC_CODER_QLFC_STATIC : LIBBSC_CODER_QLFC_ADAPTIVE; }
        if (!strncmp(argv[i], "-t", 2)) th = atoi(argv[i] + 2);
    }
    FILE *fi = fopen(argv[2], "rb"), *fo = fopen(argv[3], "wb");
    if (!fi || !fo) return 2;
    double t0 = now();
    if (argv[1][0] == 'e') {
        fseek(fi, 0, SEEK_END); long n = ftell(fi); fseek(fi, 0, SEEK_SET);
        if (B > n) B = n;
        int nb = B ? (int)((n + B - 1) / B) : 0;
        int mt = th >= 2 * nb, nthr = th > nb ? nb : th;
        if (nthr < nb && nthr > 0) { int r = (nb + nthr - 1) / nthr; nthr = (nb + r - 1) / r; }
        if (nthr < 1) nthr = 1;
        bsc_init(LIBBSC_FEATURE_FASTMODE | (mt ? LIBBSC_FEATURE_MULTITHREADING : 0));
        fwrite("BSCX", 1, 4, fo); fwrite(&nb, 4, 1, fo);
        int next = 0; long long outsz = 8;
        #pragma omp parallel num_threads(nthr)
        {
            unsigned char *buf = NULL;
            for (;;) {
                int b; long m;
                #pragma omp critical(in)
                { b = next++; }
                if (b >= nb) break;
                m = n - (long)b * B < B ? n - (long)b * B : B;
                if (!buf) buf = malloc(B + LIBBSC_HEADER_SIZE);
                #pragma omp critical(in)
                { fseek(fi, (long)b * B, SEEK_SET); if (fread(buf, 1, m, fi) != (size_t)m) exit(3); }
                int c = bsc_compress(buf, buf, (int)m, 15, 72, LIBBSC_BLOCKSORTER_BWT, coder,
                                     LIBBSC_FEATURE_FASTMODE | (mt ? LIBBSC_FEATURE_MULTITHREADING : 0));
                if (c < 0) exit(4);
                #pragma omp critical(out)
                { unsigned h[3] = { (unsigned)b, (unsigned)m, (unsigned)c }; fwrite(h, 4, 3, fo); fwrite(buf, 1, c, fo); outsz += 12 + c; }
            }
            free(buf);
        }
        fclose(fo);
        printf("encoded %ld -> %lld in %.3f s\n", n, outsz, now() - t0);
    } else {
        char sig[4]; int nb;
        if (fread(sig, 1, 4, fi) != 4 || fread(&nb, 4, 1, fi) != 1) return 3;
        long pos = 8; fseek(fi, 0, SEEK_END); long fsz = ftell(fi);
        /* index the blocks first (bsc reads headers as it goes; the scan is cheap) */
        long *off = malloc(sizeof(long) * (nb + 1)); unsigned (*hd)[3] = malloc(12 * (size_t)(nb + 1));
        for (int i = 0; i < nb; i++) { fseek(fi, pos, SEEK_SET); if (fread(hd[i], 4, 3, fi) != 3) return 3; off[i] = pos + 12; pos += 12 + hd[i][2]; }
        if (pos != fsz) return 3;
        long maxm = 0; for (int i = 0; i < nb; i++) if ((long)hd[i][1] > maxm) maxm = hd[i][1];
        int mt = th >= 2 * nb, nthr = th > nb ? nb : th; if (nthr < 1) nthr = 1;
        bsc_init(LIBBSC_FEATURE_FASTMODE | (mt ? LIBBSC_FEATURE_MULTITHREADING : 0));
        int next = 0;
        #pragma omp parallel num_threads(nthr)
        {
            unsigned char *buf = NULL;
            for (;;) {
                int i;
                #pragma omp critical(in)
                { i = next++; }
                if (i >= nb) break;
                if (!buf) buf = malloc(maxm + LIBBSC_HEADER_SIZE);
                #pragma omp critical(in)
                { fseek(fi, off[i], SEEK_SET); if (fread(buf, 1, hd[i][2], fi) != hd[i][2]) exit(3); }
                if (bsc_decompress(buf, hd[i][2], buf, hd[i][1], LIBBSC_FEATURE_FASTMODE | (mt ? LIBBSC_FEATURE_MULTITHREADING : 0))) exit(5);
                #pragma omp critical(out)
                { /* block b starts at b x B: every block but the last has the largest size */
                  fseek(fo, (long)hd[i][0] * maxm, SEEK_SET); fwrite(buf, 1, hd[i][1], fo); }
            }
            free(buf);
        }
        fclose(fo);
        printf("decoded in %.3f s\n", now() - t0);
    }
    return 0;
}
