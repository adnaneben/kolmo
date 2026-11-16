/* Decode timing of a plane-aligned rANS leaf (4 planes of 1 MiB: one skewed, three constant) with whichever
 * rans_decompress_all the include path provides; prints the median of 31 decodes and a digest of the output. */
#include "kolmo.c"
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
int main(void) {
    size_t L = 1 << 20, n = 4 * L, cap = n + n / 8 + 4096; uint8_t *a = malloc(n), *z = malloc(cap), *b = malloc(n);
    uint64_t s = 1; for (size_t i = 0; i < L; i++) { s = s * 6364136223846793005ull + 1442695040888963407ull; a[i] = (uint8_t)((s >> 59) * (s >> 61)); }
    memset(a + L, 0, L); memset(a + 2 * L, 0x55, L); memset(a + 3 * L, 7, L);
    size_t zn = leaf_compress_planes(C_RANS, a, n, 4, z, cap); double t[31];
    for (int r = 0; r < 31; r++) { double t0 = now(); if (rans_decompress_all(z, zn, b, n) || memcmp(a, b, n)) return 1; t[r] = now() - t0; }
    qsort(t, 31, sizeof t[0], cmpd); printf("leaf %zu bytes -> %zu bytes, median decode %.3f ms\n", n, zn, t[15] * 1e3); return 0;
}
