/* CPU kernels (src/arch) against their C references. Each kernel runs with kolmo_force_c = 0 and 1 on random and
 * edge-case inputs, well formed and damaged; the two must return the same code and, on success, the same bytes.
 * On builds without kernels (x86-64 today, NO_ASM=1) both runs take the reference and the test still passes.
 * `make check-arch` runs it optimised and under AddressSanitizer/UBSan. */
#include "../src/kolmo.c"
#include <stdio.h>

static uint64_t rs = 0x243F6A8885A308D3ull;
static uint64_t rnd64(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs * 0x2545F4914F6CDD1Dull; }
static uint32_t rnd(void) { return (uint32_t)(rnd64() >> 32); }
static long checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; if (fails < 20) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)

/* ---------------------------------------------------------------- word transform decoder */
/* text from a Zipf-like vocabulary: words of 1..60 letters, some capitalised or all caps, punctuation, digits,
 * newlines, and (ctl) control bytes 1..31 so that the code prefixes must come from bytes >= 32 */
static size_t mk_text(uint8_t *t, size_t n, int ctl, int vocab) {
    static uint8_t words[4096][64]; static uint8_t wlen[4096];
    for (int i = 0; i < vocab; i++) {
        int L = 1 + (int)(rnd() % (rnd() % 8 ? 9 : 60)); wlen[i] = (uint8_t)L;
        for (int k = 0; k < L; k++) words[i][k] = (uint8_t)('a' + rnd() % 26);
    }
    size_t o = 0;
    while (o + 200 < n) {
        uint32_t r = rnd();
        int w = (int)((r % 1000) * (r % 1000) / 1000 * vocab / 1000);
        int L = wlen[w]; memcpy(t + o, words[w], L);
        uint32_t s = rnd() % 32;
        if (s == 0) t[o] = (uint8_t)(t[o] & ~32);                              /* Capitalised */
        else if (s == 1) for (int k = 0; k < L; k++) t[o + k] &= (uint8_t)~32;  /* CAPS */
        o += L;
        uint32_t g = rnd() % 16;
        if (g < 10) t[o++] = ' ';
        else if (g < 12) { t[o++] = ','; t[o++] = ' '; }
        else if (g == 12) { t[o++] = '.'; t[o++] = '\n'; }
        else if (g == 13) { int d = 1 + rnd() % 6; for (int k = 0; k < d; k++) t[o++] = (uint8_t)('0' + rnd() % 10); t[o++] = ' '; }
        else if (g == 14 && ctl) t[o++] = (uint8_t)(1 + rnd() % 31);
        else { int d = rnd() % 70; for (int k = 0; k < d; k++) t[o++] = (uint8_t)(rnd() % 3 ? ' ' : '-'); }
    }
    return o;
}
static void word_case(const uint8_t *txt, size_t n, int damage) {
    size_t cap = n + 65536; uint8_t *t = malloc(cap), *a = malloc(n + 64), *b = malloc(n + 64);
    int64_t tn = word_encode(txt, n, t, cap);
    if (tn <= 0) { free(t); free(a); free(b); return; }
    if (!damage) {
        kolmo_force_c = 1; int ra = word_decode(t, (size_t)tn, a, n);
        kolmo_force_c = 0; int rb = word_decode(t, (size_t)tn, b, n);
        CHECK(ra == 0 && rb == 0 && !memcmp(a, txt, n) && !memcmp(b, txt, n), "word round trip n=%zu (%d %d)", n, ra, rb);
    } else {
        size_t hdr = 6 + t[0];
        for (int d = 0; d < damage; d++) {
            uint8_t *e = malloc((size_t)tn); memcpy(e, t, (size_t)tn);
            int nf = 1 + rnd() % 4;
            for (int f = 0; f < nf; f++) {
                size_t at = hdr + rnd() % ((size_t)tn - hdr);
                uint32_t how = rnd() % 4;
                if (how == 0) e[at] = t[1 + rnd() % t[0]];                    /* a prefix byte where none was */
                else if (how == 1) e[at] = t[1 + t[0]];                       /* a capital marker */
                else if (how == 2) e[at] = 0xFF;                              /* a large code low byte */
                else e[at] ^= (uint8_t)(1 + rnd() % 255);
            }
            size_t len = rnd() % 8 ? (size_t)tn : hdr + rnd() % ((size_t)tn - hdr);
            size_t on = rnd() % 4 ? n : n - rnd() % 64;
            kolmo_force_c = 1; int ra = word_decode(e, len, a, on);
            kolmo_force_c = 0; int rb = word_decode(e, len, b, on);
            CHECK(ra == rb && (ra || !memcmp(a, b, on)), "damaged word stream n=%zu: %d %d", n, ra, rb);
            free(e);
        }
    }
    free(t); free(a); free(b);
}
static void test_word(void) {
    size_t N = 1 << 20; uint8_t *txt = malloc(N);
    for (int it = 0; it < 60; it++) {
        int ctl = it % 3 == 2, vocab = it % 4 == 0 ? 64 : it % 4 == 1 ? 600 : 4000;
        size_t n = mk_text(txt, 65536 + rnd() % (N - 65536 - 100), ctl, vocab);
        word_case(txt, n, 0);
        word_case(txt, n, it < 30 ? 40 : 0);
    }
    free(txt);
}

/* ---------------------------------------------------------------- byte-plane shuffle */
static void test_shuffle(void) {
    size_t N = 1 << 16; uint8_t *in = malloc(N * 32 + 64), *a = malloc(N * 32 + 64), *b = malloc(N * 32 + 64), *back = malloc(N * 32 + 64);
    for (int it = 0; it < 400; it++) {
        int w = 1 << (rnd() % 4), c = 1 + (int)(rnd() % (it % 5 ? 4 : 8)), delta = rnd() & 1;
        int64_t rows = it < 64 ? it : (int64_t)(rnd() % (it % 7 ? 300 : N));
        size_t n = (size_t)rows * w * c;
        int kind = rnd() % 3;                       /* random bytes, a smooth walk, values at the wrap-around edges */
        uint64_t v = rnd64();
        for (size_t i = 0; i < n; i++) {
            if (kind == 0) in[i] = (uint8_t)rnd();
            else if (kind == 1) { if (i % w == 0) v += (uint64_t)(int64_t)(int)(rnd() % 64 - 32); in[i] = (uint8_t)(v >> (8 * (i % w))); }
            else in[i] = (uint8_t)(rnd() & 1 ? 0xFF : 0x00);
        }
        memset(a, 0xA5, n + 64); memset(b, 0xA5, n + 64);
        kolmo_force_c = 1; shuffle_fwd(in, rows, w, c, delta, a);
        kolmo_force_c = 0; shuffle_fwd(in, rows, w, c, delta, b);
        CHECK(!memcmp(a, b, n + 64), "shuffle_fwd w=%d c=%d delta=%d rows=%lld", w, c, delta, (long long)rows);
        memset(back, 0x5A, n + 64); memset(b, 0x5A, n + 64);
        kolmo_force_c = 1; shuffle_inv(a, rows, w, c, delta, back);
        kolmo_force_c = 0; shuffle_inv(a, rows, w, c, delta, b);
        CHECK(!memcmp(back, b, n + 64) && !memcmp(back, in, n), "shuffle_inv w=%d c=%d delta=%d rows=%lld", w, c, delta, (long long)rows);
    }
    free(in); free(a); free(b); free(back);
}

int main(void) {
    printf("kernels: %s\n", KOLMO_NEON ? "NEON" : "none (C references only)");
    test_shuffle();
    test_word();
    kolmo_force_c = 0;
    printf("%ld checks, %ld failures\n", checks, fails);
    if (fails) { printf("FAILED\n"); return 1; }
    printf("ALL PASSED\n");
    return 0;
}
