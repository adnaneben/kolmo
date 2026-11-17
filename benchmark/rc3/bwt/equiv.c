/* Equivalence harness: Kolmo's C port of the block-sorting pipeline against libbsc 3.4.0 (C++).
   Build (needs vendor/libbsc, i.e. a checkout before its removal, see RESULTS.md):
     benchmark/rc3/bwt/equiv.sh FILE...
   For every file and every block size of the list, compresses with bsc_compress and kolmo_bsc_compress (static and
   adaptive QLFC, Kolmo's LZP 15/72, and without LZP), compares the bytes, and decodes each output with the other
   library. Kolmo runs with 1 and 4 threads. Exit status 0 only if everything matches. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libbsc/libbsc.h"
#include "codecs/bwt_stream.h"
#include "algo/qlfc.h"

static uint8_t *readall(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)s + 1); *n = fread(b, 1, (size_t)s, f); fclose(f); return b;
}

static long fails = 0, checks = 0;

static void one(const char *name, const uint8_t *in, int n, int hash, int minlen, int coder) {
    size_t cap = (size_t)n + 4096;
    uint8_t *a = malloc(cap), *b = malloc(cap), *d = malloc((size_t)n + 64);
    int ra = bsc_compress(in, a, n, hash, minlen, LIBBSC_BLOCKSORTER_BWT, coder, LIBBSC_FEATURE_FASTMODE);
    for (int th = 1; th <= 4; th += 3) {
        int rb = kolmo_bsc_compress(in, b, n, hash, minlen, coder, th);
        checks++;
        if (ra != rb || (ra > 0 && memcmp(a, b, (size_t)ra))) {
            size_t k = 0; if (ra > 0 && rb > 0) while (k < (size_t)ra && a[k] == b[k]) k++;
            printf("MISMATCH %s n=%d lzp=%d/%d coder=%d threads=%d: libbsc %d kolmo %d first diff %zu\n", name, n, hash, minlen, coder, th, ra, rb, k);
            fails++; continue;
        }
        memset(d, 0xAA, (size_t)n);
        int r = kolmo_bsc_decompress(a, ra, d, n, th);
        if (r != 0 || memcmp(d, in, (size_t)n)) { printf("KOLMO DECODE FAIL %s n=%d coder=%d th=%d r=%d\n", name, n, coder, th, r); fails++; }
    }
    memset(d, 0x55, (size_t)n);
    int r = bsc_decompress(b, ra, d, n, LIBBSC_FEATURE_FASTMODE);
    if (r != 0 || memcmp(d, in, (size_t)n)) { printf("LIBBSC DECODE FAIL %s n=%d coder=%d r=%d\n", name, n, coder, r); fails++; }
    free(a); free(b); free(d);
}

int main(int argc, char **argv) {
    bsc_init(LIBBSC_FEATURE_FASTMODE);
    static const int sizes[] = {0, 1, 27, 28, 29, 100, 1000, 4095, 65535, 65536, 65537, 200000, 262143, 262144, 300000,
                                1048575, 1048576, 2400000, 4194304, 9500000, 16777216, 40000000, 128 << 20};
    int full = getenv("EQUIV_FULL") != NULL;
    if (argc > 1 && !strcmp(argv[1], "--synthetic")) {        /* edge cases: flag bytes, runs, tiny alphabets, noise */
        uint64_t x = 88172645463325252ull;
        static const int ns[] = {29, 64, 1000, 65536, 70000, 262144, 600000, 1048576, 3000000};
        for (int kind = 0; kind < 9; kind++) for (unsigned s = 0; s < sizeof(ns) / sizeof(ns[0]); s++) {
            int n = ns[s]; uint8_t *in = malloc((size_t)n);
            for (int i = 0; i < n; i++) {
                x ^= x << 13; x ^= x >> 7; x ^= x << 17;
                switch (kind) {
                    case 0: in[i] = 0; break;                                        /* one symbol */
                    case 1: in[i] = (uint8_t)x; break;                               /* noise */
                    case 2: in[i] = (x & 3) ? 0xf2 : (uint8_t)(x >> 8); break;       /* LZP flag byte everywhere */
                    case 3: in[i] = (uint8_t)("abcabcabd"[i % 9]); break;            /* periodic */
                    case 4: in[i] = (uint8_t)(x % 3); break;                         /* tiny alphabet */
                    case 5: in[i] = (uint8_t)((i / 1000) & 0xff); break;             /* long runs */
                    case 6: in[i] = i < n / 2 ? (uint8_t)x : in[i - n / 2]; break;   /* long repeat (LZP matches) */
                    case 7: in[i] = (i % 97 == 0) ? 0xf2 : (uint8_t)("the quick brown fox "[i % 20]); break;
                    default: in[i] = (uint8_t)(i * 7 + (i >> 9)); break;            /* all 256 symbols, sorted-ish */
                }
            }
            char name[64]; snprintf(name, sizeof(name), "synthetic-%d", kind);
            one(name, in, n, 15, 72, LIBBSC_CODER_QLFC_STATIC);
            one(name, in, n, 15, 72, LIBBSC_CODER_QLFC_ADAPTIVE);
            one(name, in, n, 0, 0, LIBBSC_CODER_QLFC_STATIC);
            one(name, in, n, 16, 32, LIBBSC_CODER_QLFC_STATIC);
            free(in);
        }
        printf("synthetic: %ld checks, %ld failures\n", checks, fails);
        return fails ? 1 : 0;
    }
    for (int f = 1; f < argc; f++) {
        size_t n; uint8_t *in = readall(argv[f], &n);
        if (!in) { printf("cannot read %s\n", argv[f]); return 2; }
        long before = fails;
        for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t m = (size_t)sizes[s];
            if (m > n) m = n;
            if (s > 0 && m == (size_t)sizes[s - 1] && m == n && sizes[s - 1] >= (int)n) break;
            size_t off = n > m ? (n - m) / 3 : 0;
            one(argv[f], in + off, (int)m, 15, 72, LIBBSC_CODER_QLFC_STATIC);
            if (full || m <= (4u << 20)) {
                one(argv[f], in + off, (int)m, 15, 72, LIBBSC_CODER_QLFC_ADAPTIVE);
                one(argv[f], in + off, (int)m, 0, 0, LIBBSC_CODER_QLFC_STATIC);
            }
            if (m == n) break;
        }
        printf("%s: %zu bytes, %s\n", argv[f], n, fails == before ? "identical" : "MISMATCH");
        fflush(stdout);
        free(in);
    }
    printf("%ld checks, %ld failures\n", checks, fails);
    return fails ? 1 : 0;
}
