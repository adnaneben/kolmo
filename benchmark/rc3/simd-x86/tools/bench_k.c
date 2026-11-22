#include "arch/kernels.h"
#include "algo/predict.h"
#include "algo/mixer.h"
#include "kbench.h"
#include <stdlib.h>
#include <string.h>
static uint64_t rs = 1; static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)rs; }
int main(int argc, char **argv) {
    /* real 16-bit and 8-bit images from argv[1] (x-ray, W = 2048 samples?) and argv[2] (RGB npy, W = 2304, C = 3) */
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t n16 = (ftell(f) - 128) / 2; fseek(f, 128, SEEK_SET);
    uint16_t *a = malloc(2 * n16); if (fread(a, 2, n16, f) != n16) return 1; fclose(f);
    f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); size_t n8 = ftell(f) - 128; fseek(f, 128, SEEK_SET);
    uint8_t *img = malloc(n8); if (fread(img, 1, n8, f) != n8) return 1; fclose(f);
    uint8_t *lo = malloc(n16), *hi = malloc(n16), *r8 = malloc(n8);
    int32_t x[63], w[4][65536 / 64][63]; int e[4] = {1234, -2222, 333, -4000};
    for (int i = 0; i < 63; i++) x[i] = (int)(rnd() % 4095) - 2047;
    for (int s = 0; s < 4; s++) for (int r = 0; r < 1024; r++) for (int i = 0; i < 63; i++) w[s][r][i] = (int32_t)(rnd() % 100000) - 50000;
    uint32_t hh[30], ho[30]; for (int i = 0; i < 30; i++) hh[i] = rnd();
    printf("%-10s %-6s %10s %8s\n", "kernel", "isa", "ns/unit", "vs C");
    double base[8] = {0};
    for (int l = 0; l <= kolmo_cpu_max_level(); l++) {
        kolmo_kernels k; kolmo_kernels_for_level(l, &k); double b, md; volatile uint64_t sink = 0; int64_t s4[4];
        int rep = 2000;
#define ROW(i, name, unit) do { if (!l) base[i] = b; printf("%-10s %-6s %10.4f %7.2fx  (%s)\n", name, kolmo_isa_name(l), b, base[i] / b, unit); } while (0)
        BENCH(15, n16, b, md, k.img16_fwd(a, (int64_t)n16, 2048, 2, lo, hi)); ROW(0, "img16_fwd", "ns/sample, MED");
        BENCH(15, (double)4096 * 512, b, md, for (int w0 = 16; w0 < 16 + 512; w0++) sink += k.sad16(a + 100000 + w0, a + 100000, 4096)); ROW(1, "sad16", "ns/sample");
        BENCH(15, n8, b, md, k.med8_fwd(img, n8 / 2304 * 2304, 2304, 3, r8)); ROW(2, "med8_fwd", "ns/byte, RGB");
        BENCH(15, (double)rep * 63, b, md, for (int t = 0; t < rep; t++) { k.cm_dot(x, 63, w[0][t & 1023], w[1][t & 1023], w[2][t & 1023], w[3][t & 1023], s4); sink += (uint64_t)s4[t & 3]; }); ROW(3, "cm_dot", "ns/input, 4 sets x 63");
        BENCH(15, (double)rep * 63, b, md, for (int t = 0; t < rep; t++) k.cm_train(x, 63, w[0][t & 1023], w[1][t & 1023], w[2][t & 1023], w[3][t & 1023], e, 12)); ROW(4, "cm_train", "ns/input, 4 sets x 63");
        BENCH(15, (double)rep * 29, b, md, for (int t = 0; t < rep; t++) { k.cm_hash(hh, 29, (uint32_t)t, ho); sink += ho[t % 29]; }); ROW(5, "cm_hash", "ns/hash, 29 contexts");
        (void)sink;
    }
    return 0;
}
