/* Context mixing with the image model (block codec 15): round trips on generated images, and decoding of damaged
 * streams (must fail or return, never read or write out of bounds; run under AddressSanitizer by `make check-cmimg`). */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
int main(void) {
    int fails = 0, coded = 0;
    for (int it = 0; it < 24; it++) {
        uint32_t ch = 1 + (uint32_t)it % 4, wpx = 8 + rnd() % 200, hpx = 8 + rnd() % 120, head = it % 3 ? rnd() % 300 : 0, tail = rnd() % 9;
        uint32_t iw = wpx * ch; size_t n = head + (size_t)iw * hpx + tail; uint8_t *in = malloc(n), *z = malloc(n + n / 8 + 4096), *out = malloc(n);
        for (size_t i = 0; i < n; i++) in[i] = (uint8_t)rnd();
        for (uint32_t y = 0; y < hpx; y++) for (uint32_t x = 0; x < wpx; x++) for (uint32_t c = 0; c < ch; c++)
            in[head + (size_t)y * iw + x * ch + c] = (uint8_t)(x * 2 + y * (c + 1) + (rnd() % 7) + (it & 1 ? 40 * ((x / 16 + y / 16) & 1) : 0));
        size_t zn = cm_compress_img(in, n, iw, ch, head, z, n + n / 8 + 4096);
        if (!zn) { printf("NOT CODED it=%d\n", it); fails++; free(in); free(z); free(out); continue; }
        coded++;
        memset(out, 0x5A, n);
        if (cm_decompress_img(z, zn, out, n) || memcmp(in, out, n)) { printf("ROUND TRIP FAIL it=%d n=%zu\n", it, n); fails++; }
        if (!cm_decompress_img(z, zn, out, n + 1)) { printf("LENGTH NOT CHECKED it=%d\n", it); fails++; }
        for (int m = 0; m < 40; m++) {                       /* damage: the parameters (first 13 bytes) or the coded bytes */
            uint8_t *d = malloc(zn); memcpy(d, z, zn);
            size_t at = m < 24 ? rnd() % 13 : rnd() % zn; d[at] = m % 3 ? (uint8_t)rnd() : (uint8_t)(d[at] ^ (1u << (rnd() % 8)));
            size_t dn = m % 9 == 0 ? rnd() % (zn + 1) : zn;
            uint8_t *o2 = malloc(n + 1); cm_decompress_img(d, dn, o2, n); free(o2); free(d);
        }
        free(in); free(z); free(out);
    }
    /* model 3 (extended header): text with repeats and one-byte edits; round trip, then damaged copies */
    for (int it = 0; it < 6; it++) {
        size_t n = 12000 + rnd() % 20000; uint8_t *in = malloc(n), *z = malloc(n + n / 8 + 4096), *out = malloc(n);
        static const char *w[] = {"alpha ", "beta ", "gamma, ", "delta.\n", "{\"key\":\"value\"}", "the quick brown fox ", "0123456789 "};
        size_t i = 0; while (i < n) {
            if (i > 4000 && rnd() % 4 == 0) { size_t from = rnd() % (i - 2000), len = 200 + rnd() % 1500; for (size_t k = 0; k < len && i < n; k++) in[i++] = in[from + k]; if (i < n) in[i - 1 - rnd() % 100] ^= 1; }
            else { const char *q = w[rnd() % 7]; for (; *q && i < n; q++) in[i++] = (uint8_t)*q; }
        }
        size_t zn = cm_compress_m(in, n, z, n + n / 8 + 4096, 3);
        if (!zn || z[3] >> 4 != 8 || cm_decompress(z, zn, out, n) || memcmp(in, out, n)) { printf("MODEL 3 ROUND TRIP FAIL it=%d\n", it); fails++; }
        for (int m = 0; zn && m < 12; m++) {
            uint8_t *d = malloc(zn); memcpy(d, z, zn);
            size_t at = m < 12 ? rnd() % 5 : rnd() % zn; d[at] = (uint8_t)rnd();
            uint8_t *o2 = malloc(n + 1); cm_decompress(d, m % 9 == 0 ? rnd() % (zn + 1) : zn, o2, n); free(o2); free(d);
        }
        free(in); free(z); free(out);
    }
    /* through the container at ultra: a .npy RGB image must come out as an image-model block and round trip */
    {   uint32_t wpx = 320, hpx = 240, ch = 3, head = 128; size_t n = head + (size_t)wpx * hpx * ch; uint8_t *in = calloc(n, 1), *z = malloc(n + (1u << 20)), *b = malloc(n);
        memset(in, ' ', head); memcpy(in, "\x93NUMPY\x01\x00\x76\x00{'descr': '|u1', 'fortran_order': False, 'shape': (240, 320, 3), }", 76); in[head - 1] = '\n';
        for (uint32_t y = 0; y < hpx; y++) for (uint32_t x = 0; x < wpx; x++) for (uint32_t c = 0; c < ch; c++)
            in[head + ((size_t)y * wpx + x) * ch + c] = (uint8_t)(128 + 60 * sin(x * 0.05 + c) + 50 * cos(y * 0.07) + (rnd() % 9));
        kolmo_params P = {KOLMO_ULTRA, 0, 1, 0, 1};
        int64_t zn = kolmo_compress(in, n, z, n + (1u << 20), &P);
        if (zn <= 0 || kolmo_decompress(z, (size_t)zn, b, n, 1) != (int64_t)n || memcmp(in, b, n)) { printf("CONTAINER FAIL zn=%lld\n", (long long)zn); fails++; }
        else printf("container: %zu -> %lld bytes\n", n, (long long)zn);
        free(in); free(z); free(b);
    }
    printf(fails ? "IMAGE MODEL TESTS FAILED (%d)\n" : "IMAGE MODEL TESTS PASSED (%d images, 40 damaged copies each)\n", fails ? fails : coded);
    return fails != 0;
}
