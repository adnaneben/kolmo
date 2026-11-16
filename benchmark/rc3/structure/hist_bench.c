/* Byte histogram: the assembly kernel of this ISA against its C reference (src/algo/hist.c), best of 7 runs of
 * 256 MiB each. Build and run from the repository root:
 *   make build/libkolmoarch.a build/libvendor.a
 *   cc -O3 -ffp-contract=off -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include \
 *      benchmark/rc3/structure/hist_bench.c build/libkolmoarch.a build/libvendor.a -llzma -lpthread -lm -lstdc++ -o build/hist-bench
 *   ./build/hist-bench
 * (on macOS add -I$(brew --prefix)/include -L$(brew --prefix)/lib and -lc++ for -lstdc++) */
#include "../../../src/kolmo.c"
#include <time.h>
#if defined(__x86_64__)
#define KERNEL kolmo_hist_x86_64
#elif defined(__aarch64__)
#define KERNEL kolmo_hist_aarch64
#endif
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(void) {
#if defined(KERNEL) && !defined(KOLMO_NO_ASM)
    size_t sizes[] = {256, 1024, 4096, 16384, 65536, 1 << 20};
    const char *kinds[] = {"random", "skewed (90% 4 symbols)", "text-like (zipf)", "runs (one value)"};
    uint8_t *b = malloc((1 << 20) + 16); uint32_t h[256], g[256]; uint64_t r = 0x2545F4914F6CDD1Dull;
    for (int k = 0; k < 4; k++) {
        for (size_t i = 0; i < (1u << 20) + 16; i++) {
            r ^= r << 13; r ^= r >> 7; r ^= r << 17;
            b[i] = k == 0 ? (uint8_t)r : k == 1 ? (uint8_t)(r % 100 < 90 ? 'a' + (r >> 8) % 4 : r >> 16)
                 : k == 2 ? (uint8_t)(' ' + __builtin_ctzll(r | (1ull << 40)) * 2 + (r >> 60) % 2) : 0;
        }
        for (int s = 0; s < 6; s++) {
            size_t n = sizes[s], reps = (256u << 20) / n; double best[2] = {1e9, 1e9};
            for (int t = 0; t < 7; t++) for (int v = 0; v < 2; v++) {
                double t0 = now();
                for (size_t q = 0; q < reps; q++) {
                    if (v) KERNEL(b + (q & 7), n, h); else kolmo_hist_c(b + (q & 7), n, g);
                    __asm__ volatile("" ::: "memory");
                }
                double dt = now() - t0; if (dt < best[v]) best[v] = dt;
            }
            kolmo_hist_c(b, n, g); KERNEL(b, n, h);
            printf("%-24s n=%-8zu C %5.2f GB/s   asm %5.2f GB/s   %+6.1f%%   %s\n", kinds[k], n, reps * (double)n / best[0] / 1e9,
                   reps * (double)n / best[1] / 1e9, (best[0] / best[1] - 1) * 100, memcmp(h, g, sizeof h) ? "MISMATCH" : "same counts");
        }
    }
#else
    printf("no assembly histogram kernel for this ISA or NO_ASM build\n");
#endif
    return 0;
}
