/* line-class transform: round trips on generated fixed-width text, and decoding of damaged input (must fail or
 * return, never read or write out of bounds; run under AddressSanitizer by `make check-lines`). */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
int main(void) {
    int fails = 0, coded = 0;
    for (int it = 0; it < 300; it++) {
        size_t n = 4096 + rnd() % 300000; uint8_t *in = malloc(n + 8), *t = malloc(n + TSLACK + 64), *out = malloc(n + 8);
        uint32_t lens[6]; int nl = 1 + rnd() % 6; for (int i = 0; i < nl; i++) lens[i] = it % 7 == 0 ? rnd() % 5000 : 1 + rnd() % 120;
        size_t p = 0;
        while (p < n) {
            uint32_t len = rnd() % 10 == 0 ? rnd() % 200 : lens[rnd() % nl];
            for (uint32_t j = 0; j < len && p < n; j++) { uint32_t r = rnd(); in[p++] = r % 50 == 0 ? (uint8_t)r : (uint8_t)(j % 9 == 4 ? '.' : j % 3 ? ' ' : '0' + r % 10); if (in[p - 1] == '\n') in[p - 1] = 'x'; }
            if (p < n && (it % 5 || rnd() % 1000)) in[p++] = '\n';
        }
        int64_t tn = lines_encode(in, n, t, n + TSLACK);
        if (tn > 0) {
            coded++;
            memset(out, 0xA5, n);
            if (lines_decode(t, (size_t)tn, out, n) || memcmp(in, out, n)) { printf("ROUND TRIP FAIL it=%d n=%zu\n", it, n); fails++; }
            if (!lines_decode(t, (size_t)tn, out, n - 1) || !lines_decode(t, (size_t)tn - 1, out, n)) { printf("LENGTH NOT CHECKED it=%d\n", it); fails++; }
            for (int m = 0; m < 200; m++) {                 /* damage: header fields, class ids, anywhere */
                uint8_t *d = malloc((size_t)tn); memcpy(d, t, (size_t)tn);
                size_t at = m < 80 ? rnd() % (14 + 8 * (size_t)d[1]) : rnd() % (size_t)tn;
                d[at] = m % 3 ? (uint8_t)rnd() : (uint8_t)(d[at] ^ (1u << (rnd() % 8)));
                size_t dn = m % 17 == 0 ? rnd() % ((size_t)tn + 1) : (size_t)tn;
                uint8_t *o2 = malloc(n + 1); lines_decode(d, dn, o2, n); free(o2); free(d);
            }
        }
        free(in); free(t); free(out);
    }
    /* through the container: a fixed-width file at balanced, decoded and compared */
    { size_t n = 3u << 20; uint8_t *in = malloc(n), *z = malloc(n + (1u << 20)), *b = malloc(n); size_t p = 0;
      while (p + 80 < n) { int w = rnd() % 3 ? 69 : 21; for (int j = 0; j < w; j++) in[p++] = (uint8_t)(j % 10 == 5 ? '.' : j % 10 < 3 ? ' ' : '0' + rnd() % 10); in[p++] = '\n'; }
      n = p; kolmo_params P = {KOLMO_FAST, 0, 1, 0, 1};
      int64_t zn = kolmo_compress(in, n, z, n + (1u << 20), &P);
      if (zn <= 0 || z[4] != 8 || kolmo_decompress(z, (size_t)zn, b, n, 1) != (int64_t)n || memcmp(in, b, n)) { printf("CONTAINER FAIL zn=%lld ver=%d\n", (long long)zn, zn > 4 ? z[4] : -1); fails++; }
      else printf("container: %zu -> %lld bytes, format version %d\n", n, (long long)zn, z[4]);
      free(in); free(z); free(b); }
    printf(fails ? "LINES TESTS FAILED (%d)\n" : "LINES TESTS PASSED (%d inputs coded, 200 damaged copies each)\n", fails ? fails : coded);
    return fails != 0;
}
