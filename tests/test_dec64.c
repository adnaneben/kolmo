/* DEC64 graph node (decimal-valued binary64 tables): exact round trips on generated tables with awkward values, and
 * decoding of damaged payloads (must fail or return, never read or write out of bounds; run under AddressSanitizer
 * by `make check-dec64`). */
#include "../src/kolmo.c"
#include <stdio.h>
#include <float.h>
static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 11); }
static double special(void) {
    static const uint64_t bits[] = {0x8000000000000000ull, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000000ull,
                                    0x7ff8000000000001ull, 0xfff4000000abcdefull, 1ull, 0x000fffffffffffffull, 0x7fefffffffffffffull,
                                    0x4340000000000000ull /* 2^53 */, 0x4330000000000001ull, 0x3fd3333333333334ull /* 0.1+0.2 */};
    double d; uint64_t b = bits[rnd() % (sizeof bits / sizeof bits[0])]; memcpy(&d, &b, 8); return d;
}
int main(void) {
    int fails = 0, coded = 0, rejected = 0; codec_ctx cx = {0};
    for (int it = 0; it < 400; it++) {
        size_t cols = it % 4 == 0 ? 0 : 1 + rnd() % (it % 9 == 0 ? 300 : 12), tc = cols ? cols : 1 + rnd() % 6;
        size_t rows = 16 + rnd() % (it % 11 == 0 ? 40000 : 3000), h = it % 3 ? rnd() % 200 : 0, tail = rnd() % 23;
        /* every other plain array: text of a random length before each column, so the byte alignment keeps changing
         * (as in a pickle or an Arrow file) */
        size_t gap[8] = {0}, gaps = 0; if (!cols && it % 8 == 4) for (size_t c = 0; c < tc; c++) { gap[c] = 1 + rnd() % 90; gaps += gap[c]; }
        const int f32 = it % 5 == 3; const size_t W = f32 ? 4 : 8;      /* every fifth: binary32 values, 4-byte words */
        size_t n = h + gaps + rows * tc * W + tail; uint8_t *in = malloc(n + 8), *z = malloc(n + 4096), *out = malloc(n + 8);
        for (size_t i = 0; i < n; i++) in[i] = (uint8_t)('a' + rnd() % 26);
        int ce[512], kind[512]; double lvl[512];
        for (size_t c = 0; c < tc; c++) { ce[c] = (int)(rnd() % 7); kind[c] = (int)(rnd() % 10); lvl[c] = (double)(rnd() % 100000); }
        for (size_t i = 0; i < h; i++) in[i] = (uint8_t)rnd();
        for (size_t r = 0; r < rows; r++) for (size_t c = 0; c < tc; c++) {
            /* a table (cols > 0) is row by row; a plain array is column after column */
            size_t at = cols ? r * tc + c : c * rows + r, shift = 0; double v; uint32_t k = rnd();
            if (!cols) for (size_t j = 0; j <= c; j++) shift += gap[j];
            if (kind[c] == 0) { uint64_t b = (uint64_t)rnd() << 32 | rnd(); memcpy(&v, &b, 8); }           /* arbitrary bits */
            else if (kind[c] >= 8 && !cols && !f32) { int64_t iv = k % 53 == 0 ? (int64_t)((uint64_t)rnd() << 30) - (1ll << 40) : (int64_t)(k % 5000) - 300 + (int64_t)r; memcpy(&v, &iv, 8); }   /* an int64 column */
            else if (kind[c] == 1 && k % 3 == 0) v = NAN;                                                    /* a third missing */
            else if (k % 97 == 0) v = special();
            else {
                lvl[c] += (double)((int)(k % 2001) - 1000);
                if (kind[c] == 4) { static double tr[512]; if (r == 0) tr[c] = 0; tr[c] += 37.0 + (double)(k % 3); lvl[c] += tr[c] - (double)((int)(k % 2001) - 1000); }   /* a steady trend: second difference */
                char buf[64]; snprintf(buf, sizeof buf, "%.*f", ce[c], (kind[c] == 2 || kind[c] == 4 ? lvl[c] : kind[c] == 5 ? (double)((int)((k * 2654435761u >> 9) % 41) * 104729 - 2000000) /* few distinct values: dictionary */ : (double)((int)(k % 2000000) - 1000000)) / KG_P10[ce[c]]);
                v = strtod(buf, NULL);
                if (kind[c] == 3 && k % 5 == 0) v = v / 3.0;                                                 /* some values not decimal */
            }
            if (f32) {
                float fv = (float)v; if (kind[c] == 0) { uint32_t b = rnd() << 11 ^ rnd(); memcpy(&fv, &b, 4); }
                else if (kind[c] != 1 && k % 97 == 0) { static const uint32_t sp[] = {0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc00000u, 0x7fc00001u, 1u, 0x007fffffu, 0x7f7fffffu, 0x3e99999au}; uint32_t b = sp[rnd() % 9]; memcpy(&fv, &b, 4); }
                else if (kind[c] >= 2) { char buf[64]; snprintf(buf, sizeof buf, "%.*f", ce[c] % 4, (double)((int)(k % 200000) - 100000) / KG_P10[ce[c] % 4]); fv = strtof(buf, NULL); if (kind[c] == 3 && k % 5 == 0) fv /= 3.0f; }
                memcpy(in + h + shift + at * 4, &fv, 4);
            } else memcpy(in + h + shift + at * 8, &v, 8);
        }
        size_t zn = kg_encode_dec64(&cx, C_ZSTD, 3, in, n, cols && f32 ? cols | KG_DEC_F32 : cols, h, z, n + 4096);
        if (!zn) { rejected++; free(in); free(z); free(out); continue; }
        coded++;
        memset(out, 0xA5, n);
        if (kg_decode(&cx, z, zn, out, n) || memcmp(in, out, n)) { printf("ROUND TRIP FAIL it=%d n=%zu cols=%zu\n", it, n, cols); fails++; }
        if (!kg_decode(&cx, z, zn, out, n - 1) || !kg_decode(&cx, z, zn - 1, out, n)) { printf("LENGTH NOT CHECKED it=%d\n", it); fails++; }
        for (int m = 0; m < 150; m++) {                     /* damage: the program (first bytes), or anywhere */
            uint8_t *d = malloc(zn); memcpy(d, z, zn);
            size_t at = m < 90 ? rnd() % (zn < 64 + 12 * tc ? zn : 64 + 12 * tc) : rnd() % zn;
            d[at] = m % 3 ? (uint8_t)rnd() : (uint8_t)(d[at] ^ (1u << (rnd() % 8)));
            size_t dn = m % 17 == 0 ? rnd() % (zn + 1) : zn;
            uint8_t *o2 = malloc(n + 1); kg_decode(&cx, d, dn, o2, n); free(o2); free(d);
        }
        free(in); free(z); free(out);
    }
    codec_ctx_free(&cx);
    /* through the container: a .npy table (rows) and a headerless array (runs), at balanced and at ratio */
    for (int pass = 0; pass < 4; pass++) {
        size_t rows = 60000, cols = 7, h = pass & 1 ? 0 : 128, n = h + rows * cols * 8; uint8_t *in = calloc(n, 1), *z = malloc(n + (1u << 20)), *b = malloc(n);
        if (h) { memset(in, ' ', h); memcpy(in, "\x93NUMPY\x01\x00\x76\x00{'descr': '<f8', 'fortran_order': False, 'shape': (60000, 7), }", 71); in[h - 1] = '\n'; }
        for (size_t r = 0; r < rows; r++) for (size_t c = 0; c < cols; c++) {
            char buf[64]; snprintf(buf, sizeof buf, "%.*f", (int)c, (double)(rnd() % 100000) / KG_P10[c]); double v = c == 5 && rnd() % 4 == 0 ? NAN : strtod(buf, NULL);
            memcpy(in + h + ((pass & 1) ? c * rows + r : r * cols + c) * 8, &v, 8);
        }
        kolmo_params P = {pass & 2 ? KOLMO_MAX : KOLMO_FAST, 0, 1, 0, 1};
        int64_t zn = kolmo_compress(in, n, z, n + (1u << 20), &P);
        if (zn <= 0 || kolmo_decompress(z, (size_t)zn, b, n, 1) != (int64_t)n || memcmp(in, b, n)) { printf("CONTAINER FAIL pass=%d zn=%lld\n", pass, (long long)zn); fails++; }
        else if ((size_t)zn * 100 > n * 30) { printf("CONTAINER: DEC64 NOT USED pass=%d (%lld bytes)\n", pass, (long long)zn); fails++; }
        else printf("container pass %d: %zu -> %lld bytes\n", pass, n, (long long)zn);
        free(in); free(z); free(b);
    }
    printf(fails ? "DEC64 TESTS FAILED (%d)\n" : "DEC64 TESTS PASSED (%d tables coded, 150 damaged copies each)\n", fails ? fails : coded);
    printf("rejected as not decimal: %d\n", rejected);
    return fails != 0;
}
