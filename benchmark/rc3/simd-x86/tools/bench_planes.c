#include "arch/cpu.h"
#include "algo/planes.h"
#include "kbench.h"
#include <stdlib.h>
#include <string.h>
/* the rc2 loops (shuffle_fwd/inv with c = 1), 4096-row blocks */
#define SB 4096
static void old_split(const uint8_t *in, int64_t rows, int w, int delta, uint8_t *out) {
    uint64_t pv = 0, tmp[SB], mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r0 = 0; r0 < rows; r0 += SB) { int64_t nb = rows - r0 < SB ? rows - r0 : SB; const uint8_t *p = in + r0 * w;
        for (int64_t r = 0; r < nb; r++, p += w) { uint64_t v = 0; memcpy(&v, p, w); tmp[r] = delta ? ((v - pv) & mask) : v; pv = v; }
        for (int b = 0; b < w; b++) { uint8_t *o = out + (int64_t)b * rows + r0; for (int64_t r = 0; r < nb; r++) o[r] = (uint8_t)(tmp[r] >> (8 * b)); } }
}
static void old_merge(const uint8_t *in, int64_t rows, int w, int delta, uint8_t *out) {
    uint64_t pv = 0, tmp[SB], mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r0 = 0; r0 < rows; r0 += SB) { int64_t nb = rows - r0 < SB ? rows - r0 : SB; memset(tmp, 0, nb * 8);
        for (int b = 0; b < w; b++) { const uint8_t *q = in + (int64_t)b * rows + r0; for (int64_t r = 0; r < nb; r++) tmp[r] |= (uint64_t)q[r] << (8 * b); }
        uint8_t *p = out + r0 * w; for (int64_t r = 0; r < nb; r++, p += w) { uint64_t v = delta ? ((pv + tmp[r]) & mask) : tmp[r]; memcpy(p, &v, w); pv = v; } }
}
int main(int argc, char **argv) {
    size_t n = argc > 1 ? (size_t)atol(argv[1]) : (16u << 20);
    uint8_t *in = malloc(n), *a = malloc(n + 64), *b = malloc(n + 64);
    for (size_t i = 0; i < n; i++) in[i] = (uint8_t)(i * 2654435761u >> 13);
    memset(a, 1, n + 64); memset(b, 1, n + 64);
    kolmo_kernels k; kolmo_kernels_for_level(KOLMO_ISA_SSE2, &k);
    for (unsigned w = 2; w <= 8; w *= 2) for (int d = 0; d < 2; d++) {
        size_t rows = n / w; double t0, t1, t2, t3, md;
        BENCH(15, n, t0, md, old_split(in, rows, w, d, a)); BENCH(15, n, t1, md, k.planes_split(in, rows, w, d, b));
        int same = !memcmp(a, b, n);
        BENCH(15, n, t2, md, old_merge(a, rows, w, d, b)); BENCH(15, n, t3, md, k.planes_merge(a, rows, w, d, b));
        printf("n=%zu w=%u delta=%d  split old %.3f new %.3f ns/B (%.2fx)  merge old %.3f new %.3f ns/B (%.2fx) %s\n", n, w, d, t0, t1, t0 / t1, t2, t3, t2 / t3, same && !memcmp(b, in, n) ? "ok" : "MISMATCH");
    }
}
