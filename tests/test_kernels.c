/* Every dispatched kernel against its C reference, at every ISA level this CPU has, on random and edge-case inputs.
 * make check-kernels (part of check-rc). */
#include "arch/cpu.h"
#include "algo/predict.h"
#include "algo/planes.h"
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

int main(void) {
    int mx = kolmo_cpu_max_level();
    printf("CPU max level %s, dispatched %s\n", kolmo_isa_name(mx), kolmo_isa_name(kolmo_cpu_level()));
    for (int l = 0; l <= mx; l++) {
        kolmo_kernels k; kolmo_kernels_for_level(l, &k);
        long c0 = checks, f0 = fails;
        test_med8(l, &k);
        test_planes(l, &k);
        printf("  %-6s %ld checks, %ld failures\n", kolmo_isa_name(l), checks - c0, fails - f0);
    }
    printf("%s: %ld checks, %ld failures\n", fails ? "FAILED" : "PASSED", checks, fails);
    return fails != 0;
}
