/* khufbench: fixed (table) cost of a khuf block decode against the per-byte cost (decmap lane). khufbench FILE */
#define _GNU_SOURCE
#include "algo/khuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); static uint8_t s[1 << 20], c[(1 << 20) + 4096], d[1 << 17];
    size_t n = fread(s, 1, sizeof s, f); fclose(f);
    size_t sz[] = {4096, 16384, 65536, 131072};
    for (int k = 0; k < 4; k++) {
        size_t b = sz[k], nb = n / b, z[512], off[512], o = 0;
        for (size_t i = 0; i < nb; i++) { off[i] = o; z[i] = kolmo_khuf_compress(s + i * b, b, c + o, sizeof c - o); o += z[i]; }
        double best = 1e9;
        for (int rep = 0; rep < 20; rep++) {
            double t0 = now();
            for (size_t i = 0; i < nb; i++) if (z[i] && (kolmo_khuf_decompress(c + off[i], z[i], d, b) || memcmp(d, s + i * b, b))) { puts("bad"); return 1; }
            double t = now() - t0; if (t < best) best = t;
        }
        printf("block %6zu: %zu blocks, coded %zu bytes, %.3f ns/byte, %.0f ns/block\n", b, nb, o, best / (nb * b) * 1e9, best / nb * 1e9);
    }
    return 0;
}
