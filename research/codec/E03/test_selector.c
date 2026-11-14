/* Unit inputs, not measured real corpus. */
#include <stdio.h>
#include "selector.h"
static uint64_t state = 171721;
static uint64_t next(void) { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; }
static void layout(uint8_t *p, const uint64_t *v, size_t n, unsigned w, unsigned plane) {
    for (size_t i = 0; i < n; i++) for (unsigned j = 0; j < w; j++)
        p[plane ? j * n + i : i * w + j] = (uint8_t)(v[i] >> (8 * j));
}
int main(void) {
    size_t counts[] = {0, 16, 127, 128, 129, 1023, 1024, 1025, 4097, 8193};
    uint64_t *v = malloc(8193 * 8); uint8_t *a = malloc(8193 * 8), *b = malloc(8193 * 8);
    if (!v || !a || !b) return 2;
    size_t cases = 0;
    for (unsigned w = 1; w <= 8; w *= 2) for (size_t c = 0; c < sizeof counts / sizeof *counts; c++)
        for (unsigned kind = 0; kind < 4; kind++) {
            size_t n = counts[c];
            for (size_t i = 0; i < n; i++) {
                uint64_t x = kind == 0 ? 0 : kind == 1 ? next() : kind == 2 ?
                    (i % 128 ? 0 : UINT64_MAX) : i & 7;
                v[i] = w == 8 ? x : x & ((1ull << (w * 8)) - 1);
            }
            layout(a, v, n, w, 0); layout(b, v, n, w, 1);
            e03_sample x, y;
            if (e03_sample_method(a, n, w, 0, &x) || e03_sample_method(b, n, w, 1, &y) ||
                x.values != y.values || x.original_bytes != y.original_bytes ||
                x.local_bytes != y.local_bytes || x.patched_bytes != y.patched_bytes ||
                x.method != y.method || x.values > 1024 || (x.method != 0 && x.method != 2 && x.method != 3)) return 3;
            if (n < 1024 && (x.method || x.values)) return 4;
            if (n >= 1024 && kind == 0 && x.method != 2) return 5;
            if (n >= 1024 && kind == 1 && x.method != 0) return 6;
            if (n >= 1024 && kind == 2 && w >= 2 && x.method != 3) return 7;
            cases++;
        }
    e03_sample s;
    if (!e03_sample_method(a, EC_MAX_VALUES + 1u, 8, 0, &s) ||
        !e03_sample_method(a, 1024, 3, 0, &s) || !e03_sample_method(a, 1024, 4, 2, &s)) return 8;
    free(v); free(a); free(b);
    printf("E03_SELECTOR_UNIT cases=%zu invalid_arguments=3 PASSED\n", cases);
    return 0;
}
