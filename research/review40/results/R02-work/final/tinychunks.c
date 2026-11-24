/* R02 experiment (not part of Kolmo): decode time of a codec 33 leaf made only of one-byte coded chunks.
 * tinychunks ctx: every chunk declares 3696 contexts (P 7, eq 3, bq 3, one lag): valid at commit 89d62b8e, refused
 * after the 256-byte rule; tinychunks one: every chunk has one context (valid before and after).
 * Prints rc, chunks, input bytes, seconds, microseconds per chunk to the file given as argv[2] (appended). */
#include "algo/krans.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int main(int argc, char **argv) {
    static const uint8_t ctx[16] = {2, 1, 48, 15, 1, 1, 0, 0, 0, 0xDF, 0x01, 4, 0, 0, 1, 0}, one[14] = {2, 1, 0, 0, 0, 0, 0, 0xDF, 0x01, 4, 0, 0, 1, 0};
    if (argc < 3) return 2;
    const int c = !strcmp(argv[1], "ctx"); const size_t n = 200000, k = c ? 16 : 14;
    uint8_t *in = malloc(n * k), *out = malloc(n);
    for (size_t i = 0; i < n; i++) memcpy(in + k * i, c ? ctx : one, k);
    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
    int rc = kolmo_krans_decompress(in, n * k, out, n);
    clock_gettime(CLOCK_MONOTONIC, &b);
    double t = (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9;
    FILE *f = fopen(argv[2], "a"); if (!f) return 2;
    fprintf(f, "%s %s rc %d: %zu chunks, %zu input bytes, %.4f s, %.2f us per chunk, %.2f MB/s of input\n", argv[0], argv[1], rc, n, n * k, t, t / n * 1e6, n * k / t / 1e6);
    fclose(f); return 0;
}
