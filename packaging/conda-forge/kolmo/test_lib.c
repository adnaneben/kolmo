/* Links against the installed libkolmo and round-trips a buffer at level 1 and level 9. */
#include <kolmo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t n = 1 << 16;
    uint8_t *src = malloc(n), *back = malloc(n);
    for (size_t i = 0; i < n; i++) src[i] = (uint8_t)("row,1234,5678,ok\n"[i % 17]);
    for (int level = 1; level <= 9; level += 8) {
        kolmo_params p;
        memset(&p, 0, sizeof p);
        if (kolmo_params_for_level(level, &p) != 0) return 1;
        size_t cap = kolmo_bound(n, &p);
        uint8_t *dst = malloc(cap);
        int64_t c = kolmo_compress(src, n, dst, cap, &p);
        if (c <= 0) { fprintf(stderr, "compress: %s\n", kolmo_strerror(c)); return 1; }
        int64_t d = kolmo_decompress(dst, (size_t)c, back, n, 1);
        if (d != (int64_t)n || memcmp(src, back, n) != 0) { fprintf(stderr, "round trip failed\n"); return 1; }
        printf("level %d: %zu -> %lld bytes\n", level, n, (long long)c);
        free(dst);
    }
    free(src);
    free(back);
    return 0;
}
