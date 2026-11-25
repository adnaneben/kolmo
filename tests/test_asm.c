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

#if !defined(KOLMO_NO_ASM) && (defined(__aarch64__) || defined(__x86_64__))
#if defined(__aarch64__)
#define TEST_GEAR_SCAN kolmo_gear_aarch64
#else
#define TEST_GEAR_SCAN kolmo_gear_x86_64
#endif
#include <sys/mman.h>
#include <unistd.h>
size_t TEST_GEAR_SCAN(const uint8_t *, size_t, const uint64_t *, uint64_t, uint64_t *);

/* Exercise every tail and alignment, including a zero-length input. Compare
 * both the boundary and the carry into the second normalization interval. */
static void check_gear(void) {
    uint8_t bytes[272];
    uint64_t masks[] = {0, 1, 3, 0x85, UINT64_MAX, dd_mask(12), dd_mask(18)};
    dd_init();
    for (size_t i = 0; i < sizeof bytes; ++i) bytes[i] = (uint8_t)rnd();
    for (size_t a = 0; a < 16; ++a) {
        for (size_t n = 0; n <= 256; ++n) {
            for (size_t m = 0; m < sizeof masks / sizeof masks[0]; ++m) {
                uint64_t want = rnd(), got = want;
                size_t cut = 0;
                for (size_t j = 0; j < n; ++j) {
                    want = (want << 1) + dd_gear[bytes[a+j]];
                    if (!(want & masks[m])) { cut = j+1; break; }
                }
                size_t pos = TEST_GEAR_SCAN(bytes+a, n, dd_gear, masks[m], &got);
                if (got != want || pos != cut) {
                    printf("Gear mismatch at alignment %zu length %zu mask %zu\n", a, n, m);
                    ++fails;
                    return;
                }
            }
        }
    }
    /* ASan does not instrument handwritten loads. End each interval exactly
     * against an inaccessible page, including the zero-length case. */
    const long page = sysconf(_SC_PAGESIZE);
    if (page < 256) { ++fails; return; }
    uint8_t *mapping = mmap(NULL, 2 * (size_t)page, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) { ++fails; return; }
    if (mprotect(mapping + page, (size_t)page, PROT_NONE) != 0) {
        munmap(mapping, 2 * (size_t)page); ++fails; return;
    }
    uint64_t ones[256];
    for (int j = 0; j < 256; ++j) ones[j] = 1;
    for (size_t n = 0; n <= 256; ++n) {
        uint64_t h = 0;
        if (TEST_GEAR_SCAN(mapping + page - n, n, ones, 1, &h) != 0) ++fails;
    }
    munmap(mapping, 2 * (size_t)page);
    printf("Gear scanner: 257 guard-page tails passed\n");
    printf("Gear scanner: 28,784 matching positions and hash states\n");
}
#endif

int main(void) {
    int kernels = 0;
    check_hist("kg_hist (dispatch)", kg_hist);
#if !defined(KOLMO_NO_ASM) && defined(__x86_64__)
    check_hist("kolmo_hist_x86_64", kolmo_hist_x86_64); kernels++;
    check_gear(); kernels++;
#endif
#if !defined(KOLMO_NO_ASM) && defined(__aarch64__)
    check_hist("kolmo_hist_aarch64", kolmo_hist_aarch64); kernels++;
    check_gear(); kernels++;
#endif
    if (fails) { printf("ASM KERNEL TESTS FAILED\n"); return 1; }
    printf("ASM KERNEL TESTS PASSED (%d assembly kernel%s checked%s)\n", kernels, kernels == 1 ? "" : "s",
           kernels ? "" : ": NO_ASM build or no kernels for this ISA, references only");
    return 0;
}
