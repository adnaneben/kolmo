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
    size_t N = 1 << 16; uint8_t *in = malloc(N * 64 + 64), *a = malloc(N * 64 + 64), *b = malloc(N * 64 + 64), *back = malloc(N * 64 + 64);   /* rows of up to 8 x 8 bytes */
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

/* ---------------------------------------------------------------- image prediction */
static void fill_img(uint8_t *p, size_t n, size_t W, int kind) {
    double fx = 0.01 + (rnd() % 100) / 1000.0, fy = 0.01 + (rnd() % 100) / 1000.0;
    for (size_t i = 0; i < n; i++) {
        size_t y = i / (W ? W : 1), x = i % (W ? W : 1);
        if (kind == 0) p[i] = (uint8_t)rnd();
        else if (kind == 1) p[i] = (uint8_t)(128 + 100 * sin(x * fx) * cos(y * fy) + (int)(rnd() % 7) - 3);
        else if (kind == 2) p[i] = rnd() & 1 ? 0 : 255;                         /* saturating edges */
        else p[i] = (uint8_t)((x * 7 + y * 3) & 255);
    }
}
static void test_img8(void) {
    for (int it = 0; it < 300; it++) {
        size_t W = it < 40 ? 1 + it * 5 : 1 + rnd() % (rnd() % 4 ? 400 : 3000);
        unsigned C = 1 + rnd() % 4; if (W < C) W = C;
        size_t H = 1 + rnd() % (it % 3 ? 40 : 100), m = W * H + (it % 5 == 0 ? rnd() % W : 0);
        uint8_t *p = malloc(m + 64), *ra = malloc(m + 64), *rb = malloc(m + 64), *pa = malloc(m + 64), *pb = malloc(m + 64);
        fill_img(p, m, W, (int)(rnd() % 4));
        memset(ra, 0x11, m + 64); memset(rb, 0x11, m + 64);
        kolmo_force_c = 1; kg_img8_fwd(p, m, W, C, ra);
        kolmo_force_c = 0; kg_img8_fwd(p, m, W, C, rb);
        CHECK(!memcmp(ra, rb, m + 64), "img8 forward W=%zu C=%u H=%zu", W, C, H);
        if (it % 4 == 3) for (size_t i = 0; i < m; i++) ra[i] = (uint8_t)rnd();   /* any residuals decode */
        memset(pa, 0x22, m + 64); memset(pb, 0x22, m + 64);
        kolmo_force_c = 1; kg_img8_back(ra, m, W, C, pa);
        kolmo_force_c = 0; kg_img8_back(ra, m, W, C, pb);
        CHECK(!memcmp(pa, pb, m + 64), "img8 inverse W=%zu C=%u H=%zu", W, C, H);
        if (it % 4 != 3) { size_t full = m / W * W; CHECK(!memcmp(pa, p, full), "img8 round trip W=%zu C=%u", W, C); }
        free(p); free(ra); free(rb); free(pa); free(pb);
    }
}
static void test_img8_trial(void) {                    /* the planner's 8-bit image trial: same cost and layout */
    for (int it = 0; it < 16; it++) {
        static const size_t WS[] = {320, 640, 1024, 1366, 1920};
        size_t W = WS[rnd() % 5] * (1 + rnd() % 4), n = W * (40 + rnd() % 200) + rnd() % 100;
        uint8_t *p = malloc(n); fill_img(p, n, W, it % 4 == 0 ? 0 : 1);
        size_t bw1 = 0, bh1 = 0, bw2 = 0, bh2 = 0; unsigned bc1 = 0, bc2 = 0;
        kolmo_force_c = 1; double c1 = kg_img8_trial(p, n, 0, n, 1, 1.0, &bw1, &bc1, &bh1);
        kolmo_force_c = 0; double c2 = kg_img8_trial(p, n, 0, n, 1, 1.0, &bw2, &bc2, &bh2);
        CHECK(c1 == c2 && bw1 == bw2 && bc1 == bc2 && bh1 == bh2, "img8 trial W=%zu n=%zu", W, n);
        free(p);
    }
}
static void test_img16(void) {
    for (int it = 0; it < 300; it++) {
        int64_t W = 1 + (int64_t)(rnd() % (it % 3 ? 300 : 4096)), n = (int64_t)(rnd() % (it % 5 ? 20000 : 300000));
        int mode = (int)(rnd() % 3), kind = (int)(rnd() % 4);
        uint16_t *a = malloc(2 * (size_t)n + 64); uint8_t *l1 = malloc((size_t)n + 64), *h1 = malloc((size_t)n + 64), *l2 = malloc((size_t)n + 64), *h2 = malloc((size_t)n + 64);
        for (int64_t i = 0; i < n; i++) {
            int64_t y = i / W, x = i % W;
            a[i] = kind == 0 ? (uint16_t)rnd() : kind == 1 ? (uint16_t)(30000 + 20000 * sin(x * 0.03) * cos(y * 0.02) + (int)(rnd() % 9) - 4)
                 : kind == 2 ? (uint16_t)(rnd() & 1 ? 0 : 65535) : (uint16_t)(rnd() % 4096);
        }
        memset(l1, 1, (size_t)n + 64); memset(h1, 1, (size_t)n + 64); memset(l2, 1, (size_t)n + 64); memset(h2, 1, (size_t)n + 64);
        kolmo_force_c = 1; img16_fwd(a, n, W, mode, l1, h1);
        kolmo_force_c = 0; img16_fwd(a, n, W, mode, l2, h2);
        CHECK(!memcmp(l1, l2, (size_t)n + 64) && !memcmp(h1, h2, (size_t)n + 64), "img16 forward W=%lld n=%lld mode=%d", (long long)W, (long long)n, mode);
        uint16_t *b1 = malloc(2 * (size_t)n + 64), *b2 = malloc(2 * (size_t)n + 64);
        if (it % 4 == 3) for (int64_t i = 0; i < n; i++) { l1[i] = (uint8_t)rnd(); h1[i] = (uint8_t)rnd(); }   /* any residuals decode */
        memset(b1, 7, 2 * (size_t)n + 64); memset(b2, 7, 2 * (size_t)n + 64);
        kolmo_force_c = 1; img16_inv(l1, h1, n, W, mode, b1);
        kolmo_force_c = 0; img16_inv(l1, h1, n, W, mode, b2);
        CHECK(!memcmp(b1, b2, 2 * (size_t)n + 64) && (it % 4 == 3 || !memcmp(b1, a, 2 * (size_t)n)), "img16 inverse W=%lld n=%lld mode=%d", (long long)W, (long long)n, mode);
        free(a); free(l1); free(h1); free(l2); free(h2); free(b1); free(b2);
    }
#if KOLMO_NEON
    for (int it = 0; it < 2000; it++) {                       /* SAD: steps 1 and 4 against the plain sums */
        int64_t n = (int64_t)(rnd() % (it % 10 ? 300 : 70000)); int step = it & 1 ? 4 : 1;
        size_t len = (size_t)(n ? step * (n - 1) + 1 : 0);
        uint16_t *x = malloc(2 * len + 2), *y = malloc(2 * len + 2);
        for (size_t i = 0; i < len; i++) { x[i] = (uint16_t)(it % 3 ? rnd() : rnd() & 1 ? 65535 : 0); y[i] = (uint16_t)rnd(); }
        uint64_t ref = 0; for (int64_t k = 0; k < n; k++) ref += (uint64_t)abs((int)x[k * step] - (int)y[k * step]);
        CHECK(kolmo_sad_u16_neon(x, y, n, step) == ref, "sad n=%lld step=%d", (long long)n, step);
        free(x); free(y);
    }
#endif
    for (int it = 0; it < 12; it++) {                         /* the whole detector: the same plan either way */
        size_t W = 64 + rnd() % 1500, H = (1u << 19) / W + 2 + rnd() % 100, n = 2 * W * H + rnd() % 3;
        uint8_t *s = malloc(n);
        for (size_t i = 0; i < n / 2; i++) {
            size_t y = i / W, x = i % W; uint16_t v = (uint16_t)(2048 + 1500 * sin(y * 0.01 * (it + 1)) * cos(x * 0.013) + (int)(rnd() % 5));
            if (it & 1) { s[2 * i] = (uint8_t)(v >> 8); s[2 * i + 1] = (uint8_t)v; } else { s[2 * i] = (uint8_t)v; s[2 * i + 1] = (uint8_t)(v >> 8); }
        }
        img_plan_t pa = {0}, pb = {0};
        kolmo_force_c = 1; int fa = img16_detect(s, n, &pa);
        kolmo_force_c = 0; int fb = img16_detect(s, n, &pb);
        CHECK(fa == fb && (!fa || (pa.be == pb.be && pa.off == pb.off && pa.mode == pb.mode && pa.W == pb.W)), "img16_detect W=%zu: %d %d", W, fa, fb);
        free(s);
    }
}

/* ---------------------------------------------------------------- integer graph inverse, 32-bit elements */
static void test_intinv(void) {
#if KOLMO_NEON
    for (int it = 0; it < 3000; it++) {
        static const int OW[4] = {1, 2, 4, 8};
        int ow = OW[rnd() % 4], ord = (int)(rnd() % 3), sg = (int)(rnd() & 1);
        size_t m = it < 100 ? (size_t)it : rnd() % (it % 10 ? 500 : 100000);
        uint64_t g = rnd() % 3 ? 1 : (rnd() % 4 ? 1 + rnd() % 1000 : rnd64()), bias = rnd64(), v1 = rnd64(), v2 = rnd64();
        uint8_t *r = malloc(m * ow + 16), *a = malloc(4 * m + 64), *b = malloc(4 * m + 64);
        int kind = (int)(rnd() % 3);
        for (size_t i = 0; i < m * ow; i++) r[i] = kind == 0 ? (uint8_t)rnd() : kind == 1 ? (uint8_t)(i < m ? rnd() % 8 : 0) : (uint8_t)(rnd() & 1 ? 0xFF : 0);
        memset(a, 0x33, 4 * m + 64); memset(b, 0x33, 4 * m + 64);
        fi_pick(4, ow, sg)(r, m, ord, g, bias, v1, v2, a);
        int ok = kolmo_int_inv4_neon(r, m, ow, ord, g, bias, v1, v2, b);
        CHECK(ok && !memcmp(a, b, 4 * m + 64), "int inverse ow=%d ord=%d g=%llu m=%zu", ow, ord, (unsigned long long)g, m);
        free(r); free(a); free(b);
    }
#endif
}

/* ---------------------------------------------------------------- table decoder helpers */
static void test_table(void) {
    for (int it = 0; it < 20000; it++) {
        int n = 1 + (int)(rnd() % 64), R = (int)(rnd() % 256), dens = (int)(rnd() % 5);
        uint16_t rl[64]; uint8_t kd[256 + 16], ra[256 + 16], rb[256 + 16];
        for (int j = 0; j < 64; j++) rl[j] = dens == 0 ? 0 : dens == 4 ? (uint16_t)(1 + rnd() % 70000) : (uint16_t)(rnd() % (unsigned)(dens + 1) ? rnd() % 300 : 0);
        for (int t = 0; t < R + 16; t++) kd[t] = dens == 4 ? 1 : (uint8_t)(rnd() % (unsigned)(dens + 2));
        kolmo_force_c = 1; uint64_t ma = row_nonempty(rl, n); int qa = compact_rows(kd, R, ra);
        kolmo_force_c = 0; uint64_t mb = row_nonempty(rl, n); int qb = compact_rows(kd, R, rb);
        CHECK(ma == mb, "row_nonempty n=%d", n);
        CHECK(qa == qb && !memcmp(ra, rb, (size_t)qa), "compact_rows R=%d", R);
    }
#if KOLMO_NEON
    for (int it = 0; it < 3000; it++) {
        uint32_t cnt = it < 100 ? (uint32_t)it : rnd() % (it % 10 ? 2000 : 300000); uint8_t *k = malloc(cnt + 1);
        int lim = it % 7 == 0 ? 256 : 3;
        for (uint32_t i = 0; i < cnt; i++) k[i] = (uint8_t)(rnd() % (unsigned)lim);
        uint32_t n1 = 0, n2 = 0, bad = 0, m1, m2;
        for (uint32_t i = 0; i < cnt; i++) { n1 += k[i] == 1; n2 += k[i] == 2; bad |= k[i] > 2; }
        int b = kolmo_kind_count_neon(k, cnt, &m1, &m2);
        CHECK((uint32_t)b == bad && m1 == n1 && m2 == n2, "kind counts cnt=%u", cnt);
        free(k);
    }
#endif
}

int main(void) {
    printf("kernels: %s\n", KOLMO_NEON ? "NEON" : "none (C references only)");
    test_table();
    test_intinv();
    test_img8();
    test_img8_trial();
    test_img16();
    test_shuffle();
    test_word();
    kolmo_force_c = 0;
    printf("%ld checks, %ld failures\n", checks, fails);
    if (fails) { printf("FAILED\n"); return 1; }
    printf("ALL PASSED\n");
    return 0;
}
