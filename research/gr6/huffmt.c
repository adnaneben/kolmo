/* Plain Huffman baseline (zstd's huff0, 4 streams, 128 KiB chunks), chunks spread over T threads.
 * usage: huffmt file threads   -> prints size, ratio, compress and decompress MB/s (best of 5, wall clock) */
#define HUF_STATIC_LINKING_ONLY
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common/huf.h"
#include "common/error_private.h"

#define B (128 * 1024)
typedef struct { const unsigned char *in; unsigned char *out, *dec; size_t n, nb, *csz; int t, T, mode; } job;
static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

static void *work(void *arg) {
    job *J = arg; unsigned *wk = malloc(HUF_WORKSPACE_SIZE + 256); HUF_CElt *ct = malloc(sizeof(HUF_CElt) * HUF_CTABLE_SIZE_ST(255));
    HUF_DTable *dt = malloc(sizeof(HUF_DTable) * HUF_DTABLE_SIZE(HUF_TABLELOG_MAX)); unsigned *dwk = malloc(HUF_DECOMPRESS_WORKSPACE_SIZE * 4);
    for (size_t b = J->t; b < J->nb; b += J->T) {
        size_t len = b + 1 < J->nb ? B : J->n - b * B; unsigned char *o = J->out + b * (B + 1024);
        if (J->mode == 0) {
            HUF_repeat rep = HUF_repeat_none;
            size_t c = HUF_compress4X_repeat(o, B + 1024, J->in + b * B, len, 255, 11, wk, HUF_WORKSPACE_SIZE, ct, &rep, 0);
            if (HUF_isError(c) || c <= 1) { memcpy(o, J->in + b * B, len); c = len; }   /* stored (and single-symbol) chunks */
            J->csz[b] = c;
        } else if (J->csz[b] == len) memcpy(J->dec + b * B, o, len);
        else { dt[0] = (HUF_DTable)(HUF_TABLELOG_MAX) * 0x01000001;
               HUF_decompress4X_hufOnly_wksp(dt, J->dec + b * B, len, o, J->csz[b], dwk, HUF_DECOMPRESS_WORKSPACE_SIZE * 4, 0); }
    }
    free(wk); free(ct); free(dt); free(dwk); return NULL;
}
static void run(job *base, int T, int mode) {
    pthread_t th[64]; job J[64];
    for (int t = 0; t < T; t++) { J[t] = *base; J[t].t = t; J[t].T = T; J[t].mode = mode; }
    for (int t = 1; t < T; t++) pthread_create(&th[t], NULL, work, &J[t]);
    work(&J[0]);
    for (int t = 1; t < T; t++) pthread_join(th[t], NULL);
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    int T = argc > 2 ? atoi(argv[2]) : 1;
    unsigned char *in = malloc(n), *dec = malloc(n + 64);
    if (fread(in, 1, n, f) != n) return 1;
    size_t nb = (n + B - 1) / B; unsigned char *out = malloc(nb * (B + 1024)); size_t *csz = calloc(nb, sizeof(size_t));
    job J = {in, out, dec, n, nb, csz, 0, T, 0};
    double tc = 1e9, td = 1e9;
    for (int r = 0; r < 5; r++) {
        double a = now(); run(&J, T, 0); double b = now(); run(&J, T, 1); double c = now();
        if (b - a < tc) tc = b - a; if (c - b < td) td = c - b;
    }
    size_t tot = 0; for (size_t b = 0; b < nb; b++) tot += csz[b] + 4;
    printf("%zu %zu %.3f %.1f %.1f %s\n", n, tot, (double)n / tot, n / tc / 1e6, n / td / 1e6, memcmp(in, dec, n) ? "FAIL" : "OK");
    return 0;
}
