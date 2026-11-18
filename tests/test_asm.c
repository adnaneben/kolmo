/* Assembly kernels against their C references (docs/LAYOUT.md: every kernel has a portable reference, and a test
 * compares the two on random and edge-case inputs). One function per kernel family; add yours to main().
 * make check-asm (part of check-rc) runs it optimised and with ASan/UBSan; with NO_ASM=1 only the references run. */
#include "../src/kolmo.c"

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static int fails;

typedef void (*hist_fn)(const uint8_t *, size_t, uint32_t *);

/* every length 0..300 at every alignment 0..15, then large inputs: random, skewed, runs, all 255 */
static void check_hist(const char *name, hist_fn k) {
    enum { MAX = 1 << 20 };
    uint8_t *buf = malloc(MAX + 64); uint32_t a[256 + 4], b[256 + 4];
    if (!buf) { printf("alloc\n"); exit(1); }
    for (int pattern = 0; pattern < 4; pattern++) {
        for (size_t i = 0; i < MAX + 64; i++) {
            uint64_t r = rnd();
            buf[i] = pattern == 0 ? (uint8_t)r : pattern == 1 ? (uint8_t)(r % 100 < 85 ? 'a' + r % 3 : r >> 8)
                   : pattern == 2 ? (uint8_t)((i / 97) & 3) : 255;
        }
        for (size_t al = 0; al < 16; al++)
            for (size_t n = 0; n <= 300; n++) {
                memset(a, 0x5A, sizeof a); memset(b, 0xC3, sizeof b);  /* the kernel must clear hc itself */
                a[256] = b[256] = 0xA5A5A5A5u;                         /* guard: nothing written past hc[255] */
                kolmo_hist_c(buf + al, n, a); k(buf + al, n, b);
                if (memcmp(a, b, 257 * 4)) { printf("%s: differs from kolmo_hist_c at n=%zu align=%zu pattern=%d\n", name, n, al, pattern); fails++; return; }
            }
        size_t sizes[] = {4095, 65536, 65537 + 3, MAX};
        for (int s = 0; s < 4; s++) {
            size_t n = sizes[s], al = rnd() % 16;
            kolmo_hist_c(buf + al, n, a); k(buf + al, n, b);
            uint64_t tot = 0; for (int c = 0; c < 256; c++) tot += b[c];
            if (memcmp(a, b, 256 * 4) || tot != n) { printf("%s: differs at n=%zu pattern=%d\n", name, n, pattern); fails++; return; }
        }
    }
    free(buf);
    printf("%s: same counts as kolmo_hist_c\n", name);
}

/* khuf's decode rounds (kolmo_khuf_dec4_c and the kernel): random tables (any entry with at most 12 bits and six
 * symbols is valid for the loop), random streams and starting states; same outputs, pointers and bit buffers */
#include "algo/khuf.h"
#if !defined(KOLMO_NO_ASM) && defined(__x86_64__)
void kolmo_khuf_dec4_x86_64(kolmo_khuf_st4 *st, size_t R, const uint64_t *dm, int W);
static void check_khuf4(void) {
    static uint64_t dm[4096]; uint8_t *in = malloc(1 << 16), *oa = malloc(1 << 16), *ob = malloc(1 << 16);
    for (int it = 0; it < 4000; it++) {
        int W = 11 + (int)(rnd() & 1), K = 56 / W; size_t R = 1 + rnd() % 300;
        int maxc = 1 + (int)(rnd() % 6);
        for (int x = 0; x < (1 << W); x++) {
            uint64_t c = 1 + rnd() % (uint64_t)maxc, nb = 1 + rnd() % (uint64_t)W, e = rnd() | rnd() << 32;
            dm[x] = (e & 0x00FFFFFFFFFFFF00ull) | nb | c << 56;
        }
        for (int i = 0; i < (1 << 16); i++) in[i] = (uint8_t)rnd();
        kolmo_khuf_st4 a, b; size_t span = R * (size_t)K * 6 + 16;
        for (int q = 0; q < 4; q++) {
            a.b[q] = (rnd() | rnd() << 32) & ~0ull << (rnd() % 64 | 8); a.c[q] = rnd() % 64 | (uint64_t)rnd() << 8;
            a.p[q] = in + (size_t)q * (R * 7 + 16); a.o[q] = oa + (size_t)q * span;
        }
        b = a; for (int q = 0; q < 4; q++) b.o[q] = ob + (a.o[q] - oa);
        memset(oa, 0, 1 << 16); memset(ob, 0, 1 << 16);
        kolmo_khuf_dec4_c(&a, R, dm, W); kolmo_khuf_dec4_x86_64(&b, R, dm, W);
        int ok = !memcmp(oa, ob, 1 << 16);
        for (int q = 0; q < 4; q++) ok &= a.b[q] == b.b[q] && a.c[q] == b.c[q] && a.p[q] == b.p[q] && a.o[q] - oa == b.o[q] - ob;
        if (!ok) { printf("kolmo_khuf_dec4_x86_64: mismatch (W %d, R %zu, iteration %d)\n", W, R, it); fails++; break; }
    }
    free(in); free(oa); free(ob);
}
#endif
int main(void) {
    int kernels = 0;
    check_hist("kg_hist (dispatch)", kg_hist);
#if !defined(KOLMO_NO_ASM) && defined(__x86_64__)
    check_hist("kolmo_hist_x86_64", kolmo_hist_x86_64); kernels++;
    if (__builtin_cpu_supports("bmi2")) { check_khuf4(); kernels++; } else printf("kolmo_khuf_dec4_x86_64: no BMI2 on this CPU, not checked\n");
#endif
#if !defined(KOLMO_NO_ASM) && defined(__aarch64__)
    check_hist("kolmo_hist_aarch64", kolmo_hist_aarch64); kernels++;
#endif
    if (fails) { printf("ASM KERNEL TESTS FAILED\n"); return 1; }
    printf("ASM KERNEL TESTS PASSED (%d assembly kernel%s checked%s)\n", kernels, kernels == 1 ? "" : "s",
           kernels ? "" : ": NO_ASM build or no kernels for this ISA, references only");
    return 0;
}
