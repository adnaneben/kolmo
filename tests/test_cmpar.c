/* Codec 25 (context mixing over parts, codecs/cm_par.c) at -x: the column path (a table) and the halves path (a large
 * non-text stream; the split threshold is lowered here so the test stays small). Each input is compressed at 1 and 4
 * threads: the frames must be identical, decode at 1 and 4 threads, and record codec 25 in the block index (the halves
 * path once wrote codec 7 over a codec-25 payload on plain blocks). Then damaged codec-25 payloads must fail or decode
 * without reading or writing out of bounds (run under AddressSanitizer by `make check-cmpar`). */
#define CMP_SPLIT_MIN ((size_t)96 << 10)
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 0x2545F4914F6CDD1Dull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }

static int check(const char *name, const uint8_t *in, size_t n, int want25) {
    kolmo_params p = {KOLMO_ULTRA, 0, 1, 0, 1};
    size_t cap = kolmo_bound(n, &p); uint8_t *z1 = malloc(cap), *z4 = malloc(cap), *out = malloc(n + 1); int fails = 0;
    int64_t a = kolmo_compress(in, n, z1, cap, &p); p.threads = 4;
    int64_t b = kolmo_compress(in, n, z4, cap, &p);
    if (a <= 0 || b != a || memcmp(z1, z4, (size_t)a)) { printf("%s: frames differ between 1 and 4 threads (%lld, %lld)\n", name, (long long)a, (long long)b); fails++; goto done; }
    for (int t = 1; t <= 4; t += 3) {
        memset(out, 0x5A, n);
        if (kolmo_decompress(z1, (size_t)a, out, n, t) != (int64_t)n || memcmp(in, out, n)) { printf("%s: round trip failed at %d threads\n", name, t); fails++; }
    }
    int c25 = 0; uint32_t nb = 0;                            /* block index: 32-byte header, then 16 bytes per block (codec at 12) */
    while (32 + 16 * (size_t)(nb + 1) <= (size_t)a && nb < 64) { if (z1[32 + 16 * nb + 12] == C_CMP) c25 = 1; uint32_t len; memcpy(&len, z1 + 32 + 16 * nb, 4); nb++; if (len == n) break; }
    if (want25 && !c25) { printf("%s: codec 25 not chosen\n", name); fails++; }
    if (c25 && z1[4] != 11) { printf("%s: codec 25 without container version 11\n", name); fails++; }
    printf("%s: %zu -> %lld bytes, codec 25 %s\n", name, n, (long long)a, c25 ? "yes" : "no");
done:
    free(z1); free(z4); free(out);
    return fails;
}

int main(void) {
    int fails = 0;
    /* a table: the column path */
    size_t cap = 400000, n = 0; uint8_t *t = malloc(cap);
    n += (size_t)snprintf((char *)t, cap, "country,year,population,co2,share\n");
    for (int i = 0; n + 128 < cap - 4096 && i < 9000; i++)
        n += (size_t)snprintf((char *)t + n, cap - n, "C%d,%d,%d,%.2f,%.4f\n", i / 40, 1980 + i % 40, 1000000 + i * 37, (rnd() % 100000) / 100.0, (rnd() % 10000) / 10000.0);
    fails += check("table", t, n, 1);
    /* a non-text stream above the (lowered) split threshold: the halves path. Each of 48 byte values (most of them not
     * printable) depends on the symbol two back (a sparse context), with noise: no structure transform applies, and
     * context mixing beats both LZMA2 (by 28%) and block sorting (by 8%), which `-x` also tries on binary blocks. (An
     * order-2 Markov source, used before, is coded smaller by block sorting.) */
    size_t m = 256u << 10; uint8_t *b = malloc(m), sym[48], nx[48][48][4];
    for (int k = 0; k < 48; k++) sym[k] = (uint8_t)(k < 16 ? k * 3 : 128 + k * 2);
    for (int x = 0; x < 48; x++) for (int y = 0; y < 48; y++) for (int z2 = 0; z2 < 4; z2++) nx[x][y][z2] = (uint8_t)(rnd() % 48);
    for (size_t i = 0, p1 = 0, p2 = 0; i < m; i++) { uint32_t r = rnd() % 16; size_t c = r < 12 ? nx[p1][1][r & 3] : rnd() % 48; b[i] = sym[c]; p1 = p2; p2 = c; }
    fails += check("halves", b, m, 1);
    /* damaged codec-25 payloads: the halves of b, coded directly, then corrupted */
    size_t parts[2] = {m / 2, m - m / 2}, zc = cmp_bound(m); uint8_t *z = malloc(zc), *o = malloc(m + 1);
    size_t zn = cmp_compress_model(b, m, parts, 2, z, zc, 2, 2);
    if (!zn || cmp_decompress(z, zn, o, m, 2) || memcmp(o, b, m)) { printf("cmp: direct round trip failed\n"); fails++; }
    if (!cmp_decompress(z, zn, o, m + 1, 2)) { printf("cmp: length not checked\n"); fails++; }
    for (int k = 0; k < 300 && zn; k++) {
        uint8_t *d = malloc(zn); memcpy(d, z, zn);
        size_t at = k < 150 ? rnd() % (zn < 16 ? zn : 16) : rnd() % zn; d[at] = k % 3 ? (uint8_t)rnd() : (uint8_t)(d[at] ^ (1u << (rnd() % 8)));
        size_t dn = k % 7 == 0 ? rnd() % (zn + 1) : zn;
        cmp_decompress(d, dn, o, m, 1 + k % 3); free(d);
    }
    free(z); free(o); free(t); free(b);
    printf(fails ? "CMPAR TESTS FAILED (%d)\n" : "CMPAR TESTS PASSED\n", fails);
    return fails != 0;
}
