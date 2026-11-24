/* R03 prototype (cost estimate only, no bitstream): direct binary context mixing over the BWT of one block, with
   optional "anchor" contexts = the first one/two bytes of the sorted suffix each BWT symbol precedes (derivable by a
   decoder from symbol/bigram counts). Prints the ideal code length in bytes for several context sets.
   usage: bwtcm file [mask]   mask bit0: BWT order0/1/2+run contexts, bit1: F1 anchor, bit2: F2 anchor */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "algo/sais/sais.h"
#include "codecs/bwt_stream.h"
#define NI 12
static float stretch_t[4096]; static float lg[4097];
static inline float squash(float x) { if (x > 20) x = 20; if (x < -20) x = -20; return 1.0f / (1.0f + expf(-x)); }
typedef struct { uint16_t p; } ctr;  /* 16-bit prob of bit 1, 2 rates packed in alternate tables */
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *T = malloc(n + 8), *U = malloc(n + 8); if (fread(T, 1, n, f) != (size_t)n) return 1; fclose(f);
    int mask = argc > 2 ? atoi(argv[2]) : 7;
    int32_t *A = malloc((size_t)n * 4 + 16), I[256];
    int r = 1; while (((n - 1) / r) > 200) r <<= 1;
    if (kolmo_sais_bwt_aux(T, U, A, (int32_t)n, r, I, 1) != 0) { puts("bwt fail"); return 1; }
    free(A);
    /* anchors: U[0] precedes the empty suffix; U[j>=1] precedes the j-th smallest of suffixes 1..n-1 */
    uint16_t *F2 = malloc((size_t)n * 2);
    { static long c1[256], c2[256][256], lone[256]; 
      for (long i = 1; i < n; i++) c1[T[i]]++;
      for (long i = 1; i + 1 < n; i++) c2[T[i]][T[i + 1]]++;
      lone[T[n - 1]] = 1;
      long j = 1; F2[0] = 0;
      for (int a = 0; a < 256; a++) { if (lone[a]) F2[j++] = (uint16_t)(a << 8);
        for (int b = 0; b < 256; b++) for (long k = 0; k < c2[a][b]; k++) F2[j++] = (uint16_t)(a << 8 | b); }
      if (j != n) { printf("anchor count mismatch %ld %ld\n", j, n); return 1; } }
    for (int i = 0; i < 4096; i++) { float p = (i + 0.5f) / 4096; stretch_t[i] = logf(p / (1 - p)); }
    /* context tables */
    size_t HS = 1u << 24;
    uint16_t *t[NI]; for (int i = 0; i < NI; i++) { t[i] = malloc(HS * 2); for (size_t k = 0; k < HS; k++) t[i][k] = 32768; }
    static float w[2048][NI]; for (int i = 0; i < 2048; i++) for (int k = 0; k < NI; k++) w[i][k] = 0.25f;
    static uint16_t apm[1024 * 33]; for (int i = 0; i < 1024; i++) for (int k = 0; k < 33; k++) apm[i * 33 + k] = (uint16_t)(squash((k - 16) / 2.0f) * 65535);
    double bits = 0; uint32_t c1 = 0, c2 = 0, c3 = 0; int run = 0; long seg = 0;
    for (long i = 0; i < n; i++) {
        int c = U[i], f1 = F2[i] >> 8, f2 = F2[i];
        if (i && F2[i] != F2[i - 1]) seg = 0;
        int runq = run > 15 ? 15 : run, segq = seg < 1 ? 0 : seg < 3 ? 1 : seg < 8 ? 2 : seg < 32 ? 3 : seg < 128 ? 4 : seg < 1024 ? 5 : 6;
        uint32_t cx[NI]; int ni = 0;
        if (mask & 1) {
            cx[ni++] = 0;
            cx[ni++] = (c1 + 1) << 8;
            cx[ni++] = ((c2 << 8 | c1) * 0x9E3779B1u) >> 8 << 8;
            cx[ni++] = (((c3 << 16 | c2 << 8 | c1) + 0x1234567) * 0x85EBCA6Bu) >> 8 << 8;
            cx[ni++] = ((runq << 8 | c1) * 0xC2B2AE35u + 77) >> 8 << 8;
        }
        if (mask & 2) {
            cx[ni++] = ((f1 + 1000) * 0x27D4EB2Fu) >> 8 << 8;
            cx[ni++] = ((f1 << 8 | c1) * 0x165667B1u + 5) >> 8 << 8;
            cx[ni++] = ((f1 << 12 | runq << 8 | c1) * 0x9E3779B1u + 99) >> 8 << 8;
        }
        if (mask & 4) {
            cx[ni++] = ((f2 + 70000) * 0x85EBCA6Bu) >> 8 << 8;
            cx[ni++] = ((f2 << 8 | c1) * 0xC2B2AE35u + 3) >> 8 << 8;
            cx[ni++] = (((uint32_t)f2 << 16 | c2 << 8 | c1) * 0x27D4EB2Fu + 11) >> 8 << 8;
            cx[ni++] = (((uint32_t)f2 << 12 | runq << 8 | c1) * 0x165667B1u + 13) >> 8 << 8;
        }
        int node = 1;
        for (int b = 7; b >= 0; b--) {
            int bit = (c >> b) & 1; float st[NI], dot = 0;
            int ws = (mask & 6) ? (segq * 256 + node) : ((run > 2 ? 2 : run) * 256 + node);
            ws &= 2047;
            uint16_t *pp[NI];
            for (int k = 0; k < ni; k++) { pp[k] = &t[k][(cx[k] + node * 0x9E3779B1u * (k ? 1 : 0) + (k ? 0 : node)) & (HS - 1)]; st[k] = stretch_t[*pp[k] >> 4]; dot += w[ws][k] * st[k]; }
            float p = squash(dot); if (p < 1e-5f) p = 1e-5f; if (p > 1 - 1e-5f) p = 1 - 1e-5f;
            /* APM on c1/node */
            int pos = (int)((dot < -7.99f ? -7.99f : dot > 7.99f ? 7.99f : dot) * 2 + 16 + 0.5f); 
            uint16_t *ap = &apm[((c1 & 3) * 256 + node) * 33 + pos]; (void)ap;
            float pf = p;
            bits -= log2f(bit ? pf : 1 - pf);
            float err = (bit - p) * 0.02f;
            for (int k = 0; k < ni; k++) { w[ws][k] += err * st[k];
                int v = *pp[k]; int rate = k < 2 ? 5 : 4; if (bit) v += (65535 - v) >> rate; else v -= v >> rate; *pp[k] = (uint16_t)v; }
            node = node * 2 + bit;
        }
        if (c == (int)c1) run++; else run = 0;
        c3 = c2; c2 = c1; c1 = c; seg++;
    }
    /* cost of sending bigram counts when F2 is used: counted as 2 bytes per nonzero bigram (upper bound, crude) */
    long nz = 0; { static uint8_t seen[65536]; for (long i = 1; i < n; i++) if (!seen[F2[i]]) { seen[F2[i]] = 1; nz++; } }
    printf("%s n=%ld mask=%d cm_bytes=%.0f distinct_bigram_anchors=%ld\n", argv[1], n, mask, bits / 8, nz);
    return 0;
}
