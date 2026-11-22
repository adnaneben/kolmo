/* scratch: GR6+ alone on a file (whole, or a slice), size and time */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
size_t gr6_compress_pub(const uint8_t *in, size_t n, uint8_t *out, size_t cap);
int gr6_decompress_pub(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(n); fread(d, 1, n, f); fclose(f);
    size_t off = argc > 2 ? strtoull(argv[2], 0, 0) : 0, len = argc > 3 ? strtoull(argv[3], 0, 0) : n - off;
    uint8_t *c = malloc(len * 2 + 65536), *o = malloc(len + 64);
    double t0 = now(); size_t cl = gr6_compress_pub(d + off, len, c, len * 2 + 65536); double t1 = now();
    int r = gr6_decompress_pub(c, cl, o, len); double t2 = now();
    uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < cl; i++) h = (h ^ c[i]) * 1099511628211ull;
    printf("hash %016llx ", (unsigned long long)h);
    printf("%s off %zu len %zu: gr6 %zu (%.3fx) enc %.2fs dec %.2fs rt %s\n", argv[1], off, len, cl, (double)len / cl, t1 - t0, t2 - t1,
           !r && !memcmp(o, d + off, len) ? "ok" : "FAIL");
    return 0;
}
