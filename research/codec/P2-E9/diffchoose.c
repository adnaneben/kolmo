/* Randomized differential test: fc_choose (P2-E9) vs production kg_num_choose on random arrays (research only). */
#define main e02c_harness_main
#include "../E02C-harness/harness/e02c.c"
#undef main
#include "fastint9.inc"
static uint64_t S = 88172645463325252ull;
static uint64_t R(void) { S ^= S << 13; S ^= S >> 7; S ^= S << 17; return S; }
int main(int argc, char **argv) {
    long iters = argc > 1 ? atol(argv[1]) : 100000; S = (uint64_t)(argc > 2 ? atoll(argv[2]) : 1) * 2654435761ull + 1;
    static uint8_t buf[8 * 40000]; long same = 0;
    for (long it = 0; it < iters; it++) {
        static const int W[] = {2, 4, 8}; int w = W[R() % 3];
        size_t cnt = R() % 4 == 0 ? 16 + R() % 64 : R() % 3 == 0 ? 16384 + R() % 4 : 16 + R() % 30000;
        int kind = (int)(R() % 7); uint64_t g = R() % 4 == 0 ? 1 + R() % 1000 : 1, base = R(), v = R();
        for (size_t q = 0; q < cnt; q++) {
            switch (kind) {
            case 0: v = R(); break;                                       /* random */
            case 1: v += (R() % 64) - 32; break;                         /* random walk */
            case 2: v += 7; break;                                        /* line */
            case 3: v = base + (R() % 8) * g; break;                      /* few values with a gcd */
            case 4: v = (q & 255) ? v : R(); break;                       /* runs */
            case 5: v = q % 2 ? (uint64_t)INT64_MAX : (uint64_t)INT64_MIN; break;   /* extremes */
            default: v = base + q * q * g; break;                         /* quadratic */
            }
            uint64_t x = v; memcpy(buf + q * w, &x, (size_t)w);
        }
        int o1 = 0, s1 = 1, o2 = 0, s2 = 1; double c1 = kg_num_choose(buf, cnt, w, &o1, &s1), c2 = fc_choose(buf, cnt, w, &o2, &s2);
        if (o1 != o2 || s1 != s2 || c1 != c2) { fprintf(stderr, "DIFF it=%ld w=%d cnt=%zu kind=%d: prod (%d,%d,%.17g) fused (%d,%d,%.17g)\n", it, w, cnt, kind, o1, s1, c1, o2, s2, c2); return 1; }
        same++;
    }
    printf("{\"kind\":\"p2e9_diffchoose\",\"iterations\":%ld,\"identical\":%ld}\n", iters, same);
    return 0;
}
