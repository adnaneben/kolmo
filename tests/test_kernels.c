/* Every dispatched kernel against its C reference, at every ISA level this CPU has, on random and edge-case inputs.
 * make check-kernels (part of check-rc). */
#include "arch/kernels.h"
#include "algo/predict.h"
#include "algo/planes.h"
#include "algo/mixer.h"
#include "algo/video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static long checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; if (fails < 20) { printf("FAIL: " __VA_ARGS__); printf("\n"); } } } while (0)

static void fill(uint8_t *b, size_t n, int kind) {
    for (size_t i = 0; i < n; i++)
        b[i] = kind == 0 ? (uint8_t)rnd() : kind == 1 ? (uint8_t)(rnd() % 3) : kind == 2 ? (uint8_t)(rnd() & 1 ? 0 : 255) : kind == 3 ? 0 : (uint8_t)(rnd() % 7 + 250);
}

static void test_med8(int level, const kolmo_kernels *k) {
    static const size_t Ws[] = {1, 2, 3, 15, 16, 17, 31, 32, 47, 48, 63, 64, 65, 80, 81, 96, 97, 100, 128, 129, 143, 191, 255, 256, 300, 352, 768, 1023, 2304};
    for (unsigned C = 1; C <= 4; C++) for (size_t wi = 0; wi < sizeof Ws / sizeof Ws[0]; wi++) {
        size_t W = Ws[wi] * (C == 1 ? 1 : C); if (W < C) continue;
        for (int kind = 0; kind < 5; kind++) {
            size_t rows = 1 + rnd() % 70, m = rows * W + (kind == 4 ? rnd() % W : 0);    /* sometimes a partial last row */
            uint8_t *r = malloc(m + 1), *a = malloc(m + 64), *b = malloc(m + 64);
            fill(r, m, kind); memset(a, 0xA5, m + 64); memset(b, 0xA5, m + 64);
            kolmo_med8_inv_c(r, m, W, C, a); k->med8_inv(r, m, W, C, b);
            CHECK(!memcmp(a, b, m + 64), "med8_inv %s C=%u W=%zu m=%zu kind=%d", kolmo_isa_name(level), C, W, m, kind);
            memset(a, 0xA5, m + 64); memset(b, 0xA5, m + 64);           /* forward, on the same bytes taken as an image */
            kolmo_med8_fwd_c(r, m, W, C, a); k->med8_fwd(r, m, W, C, b);
            CHECK(!memcmp(a, b, m + 64), "med8_fwd %s C=%u W=%zu m=%zu kind=%d", kolmo_isa_name(level), C, W, m, kind);
            uint8_t *back = malloc(m + 64); memset(back, 0, m + 64); kolmo_med8_inv_c(a, m, W, C, back);
            CHECK(!memcmp(back, r, m / W * W), "med8 round trip C=%u W=%zu m=%zu", C, W, m); free(back);
            free(r); free(a); free(b);
        }
    }
}

static void test_planes(int level, const kolmo_kernels *k) {
    static const size_t Rs[] = {0, 1, 15, 16, 17, 31, 32, 33, 47, 48, 63, 64, 100, 1000, 4095, 4096, 4097, 65537};
    for (unsigned w = 1; w <= 8; w *= 2) for (int delta = 0; delta < 2; delta++) for (size_t ri = 0; ri < sizeof Rs / sizeof Rs[0]; ri++)
        for (int kind = 0; kind < 5; kind++) {
            size_t rows = Rs[ri], n = rows * w;
            uint8_t *in = malloc(n + 1), *a = malloc(n + 64), *b = malloc(n + 64), *c = malloc(n + 64);
            fill(in, n, kind);
            memset(a, 0x5A, n + 64); memset(b, 0x5A, n + 64);
            kolmo_planes_split_c(in, rows, w, delta, a); k->planes_split(in, rows, w, delta, b);
            CHECK(!memcmp(a, b, n + 64), "planes_split %s w=%u delta=%d rows=%zu kind=%d", kolmo_isa_name(level), w, delta, rows, kind);
            memset(b, 0x5A, n + 64); memset(c, 0x5A, n + 64);
            kolmo_planes_merge_c(a, rows, w, delta, b); k->planes_merge(a, rows, w, delta, c);
            CHECK(!memcmp(b, c, n + 64), "planes_merge %s w=%u delta=%d rows=%zu kind=%d", kolmo_isa_name(level), w, delta, rows, kind);
            CHECK(!memcmp(b, in, n), "planes round trip w=%u delta=%d rows=%zu", w, delta, rows);
            free(in); free(a); free(b); free(c);
        }
}

static void test_mixer(int level, const kolmo_kernels *k) {
    enum { N = 72 };                                                /* rows padded past n, so a read past n would show */
    for (int it = 0; it < 4000; it++) {
        int n = it < 65 * 2 ? it / 2 : (int)(rnd() % 64), four = it & 1, sh = it % 3 == 0 ? 12 : (int)(rnd() % 16);
        int32_t x[N], w[4][N], v[4][N]; int e[4];
        for (int i = 0; i < N; i++) {
            int r = (int)(rnd() % 8);
            x[i] = r == 0 ? 2047 : r == 1 ? -2047 : r == 2 ? 0 : (int)(rnd() % 4095) - 2047;
            for (int s = 0; s < 4; s++) w[s][i] = it % 5 == 0 ? (int32_t)rnd() : (int32_t)(rnd() % (1u << 30)) - (1 << 29);
        }
        for (int s = 0; s < 4; s++) e[s] = it % 7 == 0 ? (s & 1 ? 16380 : -16380) : (int)(rnd() % 32761) - 16380;
        int64_t a[4], b[4];
        kolmo_cm_dot_c(x, n, w[0], w[1], four ? w[2] : NULL, w[3], a); k->cm_dot(x, n, w[0], w[1], four ? w[2] : NULL, w[3], b);
        CHECK(!memcmp(a, b, four ? 32 : 16), "cm_dot %s n=%d four=%d", kolmo_isa_name(level), n, four);
        {   uint32_t hh[N], o1[N + 8], o2[N + 8]; uint32_t add = it % 4 ? (uint32_t)rnd() : 0;
            for (int i = 0; i < N; i++) hh[i] = (uint32_t)rnd();
            memset(o1, 0x77, sizeof o1); memset(o2, 0x77, sizeof o2);
            kolmo_cm_hash_c(hh, n, add, o1); k->cm_hash(hh, n, add, o2);
            CHECK(!memcmp(o1, o2, sizeof o1), "cm_hash %s n=%d", kolmo_isa_name(level), n); }
        if (it % 5 == 0) continue;                                  /* full-range weights could overflow on update */
        memcpy(v, w, sizeof w);
        kolmo_cm_train_c(x, n, w[0], w[1], four ? w[2] : NULL, w[3], e, sh); k->cm_train(x, n, v[0], v[1], four ? v[2] : NULL, v[3], e, sh);
        CHECK(!memcmp(w, v, sizeof w), "cm_train %s n=%d four=%d shift=%d", kolmo_isa_name(level), n, four, sh);
    }
}

/* the 16-bit mixer (codec 7 model 4): inputs never -32768, weights over the whole int16 range (saturation) */
static void test_mixer16(int level, const kolmo_kernels *k) {
    enum { N = 80 };
    for (int it = 0; it < 4000; it++) {
        int n = 16 * (int)(rnd() % 5);
        int16_t x[N], w[4][N], v[4][N]; int e[4];
        for (int i = 0; i < N; i++) {
            int r = (int)(rnd() % 8);
            x[i] = (int16_t)(r == 0 ? 2047 : r == 1 ? -2047 : r == 2 ? 0 : r == 3 ? (it & 1 ? 32767 : -32767) : (int)(rnd() % 4095) - 2047);
            for (int s = 0; s < 4; s++) w[s][i] = (int16_t)(it % 5 == 0 ? (rnd() & 1 ? 32767 : -32768) + (int)(rnd() % 3) * (rnd() & 1 ? -1 : 1) : (int)(rnd() % 65536) - 32768);
        }
        for (int s = 0; s < 4; s++) e[s] = it % 7 == 0 ? (s & 1 ? 32767 : -32767) : (int)(rnd() % 65535) - 32767;
        int32_t a[4], b[4];
        kolmo_cm_dot16_c(x, n, w[0], w[1], w[2], w[3], a); k->cm_dot16(x, n, w[0], w[1], w[2], w[3], b);
        CHECK(!memcmp(a, b, sizeof a), "cm_dot16 %s n=%d", kolmo_isa_name(level), n);
        memcpy(v, w, sizeof w);
        kolmo_cm_train16_c(x, n, w[0], w[1], w[2], w[3], e); k->cm_train16(x, n, v[0], v[1], v[2], v[3], e);
        CHECK(!memcmp(w, v, sizeof w), "cm_train16 %s n=%d", kolmo_isa_name(level), n);
    }
}

static void test_video(int level, const kolmo_kernels *k) {
    static const size_t Ws[] = {1, 2, 15, 16, 17, 18, 31, 32, 33, 48, 49, 64, 176, 177, 352};
    for (size_t wi = 0; wi < sizeof Ws / sizeof Ws[0]; wi++) for (int kind = 0; kind < 5; kind++) for (int hasprev = 0; hasprev < 2; hasprev++) {
        size_t w = Ws[wi], h = 1 + rnd() % 40, n = w * h;
        uint8_t *cur = malloc(n), *prev = malloc(n), *ra = malloc(n), *rb = malloc(n), *da = malloc(n), *db = malloc(n);
        fill(cur, n, kind); fill(prev, n, kind == 4 ? 0 : kind);
        if (kind == 0 && hasprev) for (size_t i = 0; i < n; i++) prev[i] = (uint8_t)(cur[i] + rnd() % 5 - 2);   /* a near copy */
        const uint8_t *pv = hasprev ? prev : NULL;
        memset(ra, 0x33, n); memset(rb, 0x33, n); memset(da, 0x44, n); memset(db, 0x44, n);
        for (size_t y0 = 0; y0 < h; y0 += 16) {
            size_t y1 = y0 + 16 < h ? y0 + 16 : h;
            int ka = kolmo_vid_strip_enc_c(cur, pv, w, y0, y1, ra), kb = k->vid_strip_enc(cur, pv, w, y0, y1, rb);
            CHECK(ka == kb, "vid_strip_enc %s choice w=%zu y0=%zu kind=%d prev=%d: %d vs %d", kolmo_isa_name(level), w, y0, kind, hasprev, ka, kb);
            kolmo_vid_strip_dec_c(da, pv, w, y0, y1, ka, ra); k->vid_strip_dec(db, pv, w, y0, y1, ka, ra);
        }
        CHECK(!memcmp(ra, rb, n), "vid_strip_enc %s residuals w=%zu h=%zu kind=%d prev=%d", kolmo_isa_name(level), w, h, kind, hasprev);
        CHECK(!memcmp(da, db, n), "vid_strip_dec %s w=%zu h=%zu kind=%d prev=%d", kolmo_isa_name(level), w, h, kind, hasprev);
        CHECK(!memcmp(da, cur, n), "vid_strip round trip w=%zu h=%zu kind=%d prev=%d", w, h, kind, hasprev);
        /* forced predictors on random residuals: the decoders must agree whatever the residuals */
        for (int kk = 0; kk < (hasprev ? 3 : 1); kk++) {
            uint8_t *rr = malloc(n); fill(rr, n, 0);
            memset(da, 0x44, n); memset(db, 0x44, n);
            for (size_t y0 = 0; y0 < h; y0 += 16) { size_t y1 = y0 + 16 < h ? y0 + 16 : h;
                kolmo_vid_strip_dec_c(da, pv, w, y0, y1, kk, rr); k->vid_strip_dec(db, pv, w, y0, y1, kk, rr); }
            CHECK(!memcmp(da, db, n), "vid_strip_dec %s forced k=%d w=%zu h=%zu kind=%d", kolmo_isa_name(level), kk, w, h, kind);
            free(rr);
        }
        free(cur); free(prev); free(ra); free(rb); free(da); free(db);
    }
}

static void test_img16(int level, const kolmo_kernels *k) {
    for (int it = 0; it < 600; it++) {
        int64_t W = it < 40 ? 1 + it : 1 + (int64_t)(rnd() % 700), n = (int64_t)(rnd() % 5000); int mode = it % 3, kind = (it / 3) % 5;
        if (it % 50 == 0) n = 70000;
        uint16_t *a = malloc(2 * (size_t)n + 2); uint8_t *l1 = malloc((size_t)n + 64), *h1 = malloc((size_t)n + 64), *l2 = malloc((size_t)n + 64), *h2 = malloc((size_t)n + 64);
        for (int64_t i = 0; i < n; i++) a[i] = kind == 0 ? (uint16_t)rnd() : kind == 1 ? (uint16_t)(30000 + rnd() % 9) : kind == 2 ? (rnd() & 1 ? 0 : 65535)
                                         : kind == 3 ? (uint16_t)(i * 7 + rnd() % 3) : (uint16_t)(65530 + rnd() % 6);
        memset(l1, 1, (size_t)n + 64); memset(h1, 2, (size_t)n + 64); memset(l2, 1, (size_t)n + 64); memset(h2, 2, (size_t)n + 64);
        kolmo_img16_fwd_c(a, n, W, mode, l1, h1); k->img16_fwd(a, n, W, mode, l2, h2);
        CHECK(!memcmp(l1, l2, (size_t)n + 64) && !memcmp(h1, h2, (size_t)n + 64), "img16_fwd %s n=%lld W=%lld mode=%d kind=%d", kolmo_isa_name(level), (long long)n, (long long)W, mode, kind);
        if (n > 2) { size_t off = rnd() % (size_t)(n / 2 + 1), len = (size_t)n - off;
            CHECK(kolmo_sad16_c(a, a + off, len - off) == k->sad16(a, a + off, len - off), "sad16 %s n=%zu off=%zu", kolmo_isa_name(level), len - off, off); }
        free(a); free(l1); free(h1); free(l2); free(h2);
    }
    {   /* the largest sums: every difference 65535 */
        size_t n = 200000; uint16_t *x = malloc(2 * n), *y = malloc(2 * n);
        for (size_t i = 0; i < n; i++) { x[i] = i & 1 ? 0 : 65535; y[i] = i & 1 ? 65535 : 0; }
        CHECK(kolmo_sad16_c(x, y, n) == k->sad16(x, y, n), "sad16 %s saturated", kolmo_isa_name(level));
        free(x); free(y);
    }
}

int main(void) {
    int mx = kolmo_cpu_max_level();
    printf("CPU max level %s, dispatched %s\n", kolmo_isa_name(mx), kolmo_isa_name(kolmo_cpu_level()));
    for (int l = 0; l <= mx; l++) {
        if (mx == KOLMO_ISA_NEON && l != KOLMO_ISA_C && l != KOLMO_ISA_NEON) continue;
        kolmo_kernels k; kolmo_kernels_for_level(l, &k);
        long c0 = checks, f0 = fails;
        if (l == KOLMO_ISA_NEON) {
            CHECK(k.cm_dot16 != kolmo_cm_dot16_c, "NEON dot kernel was not dispatched");
            CHECK(k.cm_train16 != kolmo_cm_train16_c, "NEON training kernel was not dispatched");
        }
        test_med8(l, &k);
        test_planes(l, &k);
        test_mixer(l, &k); test_mixer16(l, &k);
        test_video(l, &k);
        test_img16(l, &k);
        printf("  %-6s %ld checks, %ld failures\n", kolmo_isa_name(l), checks - c0, fails - f0);
    }
    printf("%s: %ld checks, %ld failures\n", fails ? "FAILED" : "PASSED", checks, fails);
    return fails != 0;
}
