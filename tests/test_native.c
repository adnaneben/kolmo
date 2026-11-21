/* Kolmo's own coders (native lane, rc3 wave 2): round trips at every level on edge-case and generated inputs, decoders
 * on truncated and corrupted payloads (they must fail cleanly or return some output, never read or write out of
 * bounds: run under ASan/UBSan by make check-native), complete length-limited Huffman codes, and pinned khash64
 * values (the hash is a format decision: its values must not change).
 * usage: test-native [ITERATIONS]   (make check-native) */
#include "algo/khuf.h"
#include "algo/klz.h"
#include "algo/klzma.h"
#include "algo/krans.h"
#include "algo/khash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs = 0x2545F4914F6CDD1Dull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 16); }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

/* inputs: kind 0 random, 1 zeros, 2 small alphabet, 3 text-like words, 4 periodic with noise, 5 two bytes, 6 long runs */
static void gen(uint8_t *p, size_t n, int kind) {
    static const char *w[] = {"the ", "kolmo ", "compress ", "of ", "and ", "block ", "x", "\n", "1234 ", "data "};
    size_t i = 0;
    switch (kind) {
        case 0: for (; i < n; i++) p[i] = (uint8_t)rnd(); break;
        case 1: memset(p, 0, n); break;
        case 2: for (; i < n; i++) p[i] = (uint8_t)("ACGT"[rnd() & 3]); break;
        case 3: while (i < n) { const char *s = w[rnd() % 10]; while (*s && i < n) p[i++] = (uint8_t)*s++; } break;
        case 4: for (; i < n; i++) p[i] = (uint8_t)((i % 37) * 7 + ((rnd() & 63) == 0 ? rnd() : 0)); break;
        case 5: for (; i < n; i++) p[i] = (uint8_t)(rnd() % 50 == 0 ? 'b' : 'a'); break;
        default: while (i < n) { size_t r = 1 + rnd() % 3000; uint8_t c = (uint8_t)rnd(); while (r-- && i < n) p[i++] = c; } break;
    }
}
typedef size_t (*cfn)(const uint8_t *, size_t, uint8_t *, size_t, int);
typedef int (*dfn)(const uint8_t *, size_t, uint8_t *, size_t);
static size_t khuf_c(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int lv) { (void)lv; return kolmo_khuf_compress_chunks(in, n, out, cap); }
static size_t krans_c(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int lv) { (void)lv; return kolmo_krans_compress(in, n, out, cap); }
static const struct { const char *name; cfn c; dfn d; int lo, hi, may_refuse; } C[] = {
    {"khuf", khuf_c, kolmo_khuf_decompress_chunks, 0, 0, 0},
    {"klz", kolmo_klz_compress, kolmo_klz_decompress, 1, 3, 0},
    {"klzh", kolmo_klzh_compress, kolmo_klzh_decompress, 1, 9, 1},
    {"klzma", kolmo_klzma_compress, kolmo_klzma_decompress, 1, 9, 1},
    {"klzh2", kolmo_klzh2_compress, kolmo_klzh2_decompress, 1, 9, 1},   /* night 2: codec 32 */
    {"krans", krans_c, kolmo_krans_decompress, 0, 0, 0},
};
static void roundtrip(int k, const uint8_t *in, size_t n, int lv, int kind) {
    size_t cap = n + n / 8 + 4096; uint8_t *z = malloc(cap), *r = malloc(n + 1);
    size_t zn = C[k].c(in, n, z, cap, lv);
    if (!zn) { CHECK(C[k].may_refuse, "%s level %d kind %d n %zu: refused", C[k].name, lv, kind, n); free(z); free(r); return; }
    CHECK(!C[k].d(z, zn, r, n) && !memcmp(r, in, n), "%s level %d kind %d n %zu: round trip", C[k].name, lv, kind, n);
    /* truncated and corrupted payloads: no crash (ASan), and any accepted output stays in bounds */
    if (zn > 1) C[k].d(z, zn - 1 - rnd() % (zn - 1 < 16 ? zn - 1 : 16), r, n);
    for (int t = 0; t < 4; t++) {
        uint8_t *y = malloc(zn); memcpy(y, z, zn);
        for (int f = 0; f < 1 + t; f++) y[rnd() % zn] ^= (uint8_t)(1u << (rnd() & 7));
        C[k].d(y, zn, r, n);
        if (n > 1) C[k].d(y, zn, r, n - 1);   /* wrong expected size */
        free(y);
    }
    /* the encoder never writes past cap: a tight buffer either fits or is refused */
    if (zn > 8) { uint8_t *t = malloc(zn - 1); size_t q = C[k].c(in, n, t, zn - 1, lv); CHECK(!q || q <= zn - 1, "%s: wrote past cap", C[k].name); free(t); }
    free(z); free(r);
}
static void test_lengths(void) {
    for (int it = 0; it < 2000; it++) {
        uint32_t cnt[256] = {0}; int m = 2 + (int)(rnd() % 255);
        for (int s = 0; s < m; s++) cnt[rnd() & 255] = (it & 1) ? 1u + rnd() % 1000 : 1u << (rnd() % 24);   /* skewed: forces the limit */
        uint8_t len[256]; int got = kolmo_khuf_lengths(cnt, 256, KHUF_MAXLEN, len);
        uint64_t kraft = 0; int present = 0;
        for (int s = 0; s < 256; s++) { if (cnt[s]) { present++; CHECK(len[s] >= 1 && len[s] <= KHUF_MAXLEN, "length out of range"); kraft += 1ull << (KHUF_MAXLEN - len[s]); } else CHECK(!len[s], "absent symbol has a length"); }
        if (present >= 2) CHECK(got == present && kraft == (1ull << KHUF_MAXLEN), "Huffman code not complete (%d symbols)", present);
    }
}
static const struct { size_t n; uint64_t h0, hs; } KH[] = {
{0, 0x08755A3FB62E0DC1ull, 0x3C9E0A316DBBE692ull},
{1, 0x31885CA34F45F74Cull, 0x5F3688B8820377ECull},
{3, 0x9856737DB2E4ED72ull, 0xD2D806633A2A1D79ull},
{4, 0xA7AF696D3E5B106Eull, 0x12D4E4DE3C278338ull},
{7, 0x46426558B2277C83ull, 0x592C2AE5B371E9D0ull},
{8, 0xDE8C354D2F1EF7D5ull, 0x5C22031871AD3880ull},
{15, 0x6D58F4C1DE463283ull, 0x5A51E4AFC3295A70ull},
{16, 0x41B0BF763F4CEE19ull, 0x30B1B02ED93C617Bull},
{17, 0xB74CF8CA1B87530Full, 0xBFEF1BF01B895CEEull},
{32, 0x0D699B526404FA82ull, 0x62D011CA34547FE9ull},
{33, 0x9EC239F28C0BC102ull, 0xB4F59DFCE08E19EAull},
{63, 0xF63E0BB00B9A1852ull, 0xAF53B8583F17550Full},
{64, 0x9D4EEB21DEEFECE2ull, 0xAA5D4F255F0B9AB4ull},
{65, 0x198AC58D359DA5FCull, 0x124C2AB8A668372Eull},
{100, 0xA9F050D9B5DBEA72ull, 0x65C3E02918805A69ull},
{1023, 0x8BF14A87F5394677ull, 0x98FAA014469387CCull},
{1024, 0x0FA91243D2F229F2ull, 0xBC1B633A43F7817Eull},
{1025, 0x3DB884EB92B67548ull, 0x09A80AD0E483C248ull},
{4096, 0x4DDE8D736C85D8F4ull, 0xB070C164395D3D20ull},
{5000, 0xBF419B88FC1E9E2Full, 0x76E6BCDE84B04E48ull},
};
static void test_hash(void) {
    uint8_t b[5000]; for (int i = 0; i < 5000; i++) b[i] = (uint8_t)(i * 131 + 7);
    for (size_t k = 0; k < sizeof KH / sizeof *KH; k++) {
        CHECK(kolmo_khash64(b, KH[k].n, 0) == KH[k].h0, "khash64 length %zu seed 0 changed", KH[k].n);
        CHECK(kolmo_khash64(b, KH[k].n, 0x637a6e31) == KH[k].hs, "khash64 length %zu seed changed", KH[k].n);
    }
    uint8_t *u = malloc(5001); memcpy(u + 1, b, 5000);   /* unaligned input: the same value */
    CHECK(kolmo_khash64(u + 1, 5000, 7) == kolmo_khash64(b, 5000, 7), "khash64 depends on alignment");
    free(u);
}
/* KLZH edge cases at every level (the optimal parse of 7-9, the lazy parse of 5-6): empty, 1 and 15 bytes, 1 MiB of
 * zeros, 1 MiB random, and with big set a match at exactly the 16 MiB window edge (offset 2^24 - 1) next to one just
 * past it: at level 9 the first must be found (the output shrinks), and every output must decode with the decoder. */
static size_t klzh_rt(const uint8_t *in, size_t n, int lv) {
    size_t cap = kolmo_klzh_bound(n); uint8_t *z = malloc(cap), *r = malloc(n + 1);
    size_t zn = kolmo_klzh_compress(in, n, z, cap, lv);
    if (zn) CHECK(!kolmo_klzh_decompress(z, zn, r, n) && !memcmp(r, in, n), "klzh edge: level %d n %zu round trip", lv, n);
    free(z); free(r); return zn ? zn : n;
}
static void test_klzh_edges(int big) {
    static const size_t sz[] = {0, 1, 15};
    uint8_t *m = malloc(1u << 20);
    for (int lv = 1; lv <= 9; lv++) {
        for (size_t k = 0; k < 3; k++) { gen(m, sz[k], 3); klzh_rt(m, sz[k], lv); }
        memset(m, 0, 1u << 20); CHECK(klzh_rt(m, 1u << 20, lv) < 4096, "klzh level %d: 1 MiB of zeros does not shrink", lv);
        gen(m, 1u << 20, 0); klzh_rt(m, 1u << 20, lv);
    }
    free(m);
    if (!big) return;
    const size_t W = (1u << 24) - 1, L = 1u << 16, n = W + 2 * L + 64; uint8_t *a = malloc(n), *b = malloc(n);
    for (size_t i = 0; i < n; i++) a[i] = (uint8_t)("abcdefghijklmnop"[rnd() & 15]);   /* short matches only */
    memcpy(b, a, n);
    memcpy(a + W, a, L);                                   /* a: L bytes at offset exactly 2^24 - 1 */
    memcpy(a + W + L + 32, a + L - 1 + 32, L);             /* and L bytes at offset 2^24 + ... past the window */
    for (int lv = 1; lv <= 9; lv++) {
        size_t za = klzh_rt(a, n, lv);
        if (lv == 9) { size_t zb = klzh_rt(b, n, lv); CHECK(za + L / 4 < zb, "klzh level 9: the match at the window edge was not used (%zu vs %zu)", za, zb); }
    }
    free(a); free(b);
}
int main(int argc, char **argv) {
    int iters = argc > 1 ? atoi(argv[1]) : 1;
    test_lengths(); test_hash(); test_klzh_edges(iters > 1);
    static const size_t sizes[] = {0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 64, 100, 255, 1000, 4095, 65536, 131071, 131072, 131073, 300000};
    for (int it = 0; it < iters; it++)
        for (int k = 0; k < (int)(sizeof C / sizeof *C); k++)
            for (size_t si = 0; si < sizeof sizes / sizeof *sizes; si++)
                for (int kind = 0; kind < 7; kind++) {
                    size_t n = sizes[si]; if (k == 3 && n > 65536 && kind != 3) continue;   /* keep the LZMA-class run short */
                    uint8_t *in = malloc(n + 1); gen(in, n, kind);
                    int lv = C[k].lo + (int)((si + (size_t)kind + (size_t)it) % (size_t)(C[k].hi - C[k].lo + 1));
                    if ((k == 0 || k == 5) && n == 0) { free(in); continue; }   /* khuf and krans: n >= 1 */
                    roundtrip(k, in, n, lv, kind);
                    free(in);
                }
    /* KLZMA with Kolmo's lc/lp/pb digits (codec 6 style), and a long-distance repeat over a 2 MiB block */
    { size_t n = 2u << 20; uint8_t *in = malloc(n); gen(in, n / 2, 0); memcpy(in + n / 2, in, n / 2); in[n - 5] ^= 1;
      roundtrip(3, in, n, 10 * (100 * (0 + 1) + 10 * 2 + 0) + 4, 9); roundtrip(2, in, n, 5, 9); roundtrip(4, in, n, 5, 9); roundtrip(1, in, n, 1, 9); free(in); }
    /* KRANS: one symbol, two symbols at the frequency floor, all 256 symbols, and lengths around the 8-way groups */
    for (size_t n = 1; n < 70; n++) for (int shape = 0; shape < 4; shape++) {
        uint8_t *in = malloc(n); for (size_t i = 0; i < n; i++) in[i] = shape == 0 ? 7 : shape == 1 ? (uint8_t)(i == n / 2 ? 200 : 3) : shape == 2 ? (uint8_t)rnd() : (uint8_t)(i % 256);
        roundtrip(5, in, n, 0, 10 + shape); free(in);
    }
    { size_t n = 1u << 20; uint8_t *in = malloc(n); for (size_t i = 0; i < n; i++) in[i] = (uint8_t)i; roundtrip(5, in, n, 0, 14);
      for (size_t i = 0; i < n; i++) in[i] = (rnd() % 1000) ? 0 : (uint8_t)(1 + rnd() % 255); roundtrip(5, in, n, 0, 15); free(in); }
    if (fails) { fprintf(stderr, "test-native: %d failures\n", fails); return 1; }
    printf("test-native: OK\n");
    return 0;
}
