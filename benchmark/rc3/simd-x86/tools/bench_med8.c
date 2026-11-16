#include "arch/cpu.h"
#include "algo/predict.h"
#include "kbench.h"
#include <stdlib.h>
#include <string.h>
/* forward transform as in kgraph (kg_img8_fwd), to get realistic residuals */
static uint8_t zz8(uint8_t d) { int8_t v = (int8_t)d; return (uint8_t)(((unsigned)v << 1) ^ (unsigned)(v >> 7)); }
static void fwd(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r) {
    size_t x = 0;
    for (; x < C; x++) r[x] = zz8(p[x]);
    for (; x < W; x++) r[x] = zz8((uint8_t)(p[x] - p[x - C]));
    for (size_t y = W; y + W <= m; y += W) { const uint8_t *q = p + y, *u = q - W; uint8_t *o = r + y;
        for (x = 0; x < C; x++) o[x] = zz8((uint8_t)(q[x] - u[x]));
        for (; x < W; x++) o[x] = zz8((uint8_t)(q[x] - kolmo_med8(q[x - C], u[x], u[x - C]))); }
}
int main(int argc, char **argv) {
    /* argv: file offset W C */
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *s = malloc(n); if (fread(s, 1, n, f) != n) return 1; fclose(f);
    size_t off = atol(argv[2]), W = atol(argv[3]); unsigned C = atoi(argv[4]);
    size_t m = (n - off) / W * W; const uint8_t *img = s + off;
    uint8_t *r = malloc(m), *o = malloc(m + 64), *ref = malloc(m + 64);
    fwd(img, m, W, C, r);
    kolmo_med8_inv_c(r, m, W, C, ref);
    if (memcmp(ref, img, m)) { printf("reference does not invert\n"); return 1; }
    double base = 0;
    for (int l = 0; l <= kolmo_cpu_max_level(); l++) {
        kolmo_kernels k; kolmo_kernels_for_level(l, &k); double b, md;
        BENCH(25, m, b, md, k.med8_inv(r, m, W, C, o));
        if (!l) base = b;
        printf("%-30s W=%-5zu C=%u %-6s %6.3f ns/B (median %6.3f)  %5.2fx  %s\n", argv[1] + (strrchr(argv[1], '/') ? strrchr(argv[1], '/') - argv[1] + 1 : 0), W, C,
               kolmo_isa_name(l), b, md, base / b, memcmp(o, img, m) ? "MISMATCH" : "same bytes");
    }
    return 0;
}
