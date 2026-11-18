/* Block-sorting codec tests (src/codecs/bwt.c, src/algo/lzp.c, src/algo/qlfc.c, src/algo/sais).
   1. Codec 5 compatibility: the bytes Kolmo writes for a set of generated inputs hash to the values libbsc 3.4.0
      wrote (captured with benchmark/rc3/bwt/equiv.sh before libbsc was removed from the tree).
   2. Round trips with 1 and 4 threads, static and adaptive QLFC, with and without LZP: same bytes for every thread
      count, exact decode.
   3. Damaged streams: random byte flips and truncations fed to the LZP and QLFC decoders and to the block decoder
      must fail cleanly (run under ASan/UBSan by make check-bwt).
   Run with --print to print the hash table (only to refresh it on purpose, from a build proven equal to libbsc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "codecs/bwt_stream.h"
#include "algo/lzp.h"
#include "algo/qlfc.h"
#include "algo/bytes.h"
#include "algo/sais/sais.h"
#ifdef GOLDEN_FROM_LIBBSC
#include "libbsc/libbsc.h"
#endif

static uint64_t fnv(const uint8_t *p, size_t n) { uint64_t h = 1469598103934665603ull; while (n--) { h ^= *p++; h *= 1099511628211ull; } return h; }

static uint8_t *gen(int kind, int n) {
    uint8_t *in = malloc((size_t)n + 1); uint64_t x = 88172645463325252ull + (uint64_t)kind;
    static const char *words[] = {"the ", "block ", "sorting ", "kolmo ", "data ", "\n", "run ", "of ", "a ", "1234 "};
    int w = 0, wp = 0;
    for (int i = 0; i < n; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        switch (kind) {
            case 0: in[i] = 0; break;
            case 1: in[i] = (uint8_t)x; break;
            case 2: in[i] = (x & 3) ? 0xf2 : (uint8_t)(x >> 8); break;
            case 3: in[i] = (uint8_t)("abcabcabd"[i % 9]); break;
            case 4: in[i] = (uint8_t)(x % 3); break;
            case 5: in[i] = (uint8_t)((i / 1000) & 0xff); break;
            case 6: in[i] = i < n / 2 ? (uint8_t)x : in[i - n / 2]; break;
            case 7: in[i] = (i % 97 == 0) ? 0xf2 : (uint8_t)("the quick brown fox "[i % 20]); break;
            default:                                                       /* text-like: words with repeats */
                if (!words[w][wp]) { w = (int)(x % 10); wp = 0; if ((x >> 20) % 50 == 0 && i > 5000) { int L = 200 + (int)(x >> 40) % 300, s = i - 4000; for (int k = 0; k < L && i < n; k++) in[i++] = in[s + k]; i--; continue; } }
                in[i] = (uint8_t)words[w][wp++]; break;
        }
    }
    return in;
}

static const int SIZES[] = {29, 1000, 65536, 70000, 262144, 1048576, 3000000};
#define NKIND 9
#define NSIZE ((int)(sizeof(SIZES) / sizeof(SIZES[0])))
/* fnv-1a of kolmo_bsc_compress(LZP 15/72, static QLFC), equal to libbsc's bsc_compress output. */
static const uint64_t GOLDEN[NKIND][NSIZE] = {
#include "test_bwt_golden.h"
};

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static void roundtrip(const uint8_t *in, int n, int hash, int minlen, int coder, const char *what) {
    uint8_t *a = malloc((size_t)n + 64), *b = malloc((size_t)n + 64), *d = malloc((size_t)n + 1);
    int ra = kolmo_bsc_compress(in, a, n, hash, minlen, coder, 1);
    int rb = kolmo_bsc_compress(in, b, n, hash, minlen, coder, 4);
    CHECK(ra > 0 && ra == rb && !memcmp(a, b, (size_t)ra), "%s n=%d coder=%d: thread count changed the output", what, n, coder);
    for (int th = 1; th <= 4; th += 3) {
        memset(d, 0x5a, (size_t)n);
        int r = kolmo_bsc_decompress(a, ra, d, n, th);
        CHECK(r == 0 && !memcmp(d, in, (size_t)n), "%s n=%d coder=%d threads=%d: round trip (%d)", what, n, coder, th, r);
    }
    free(a); free(b); free(d);
}

/* Damage: the block decoder (adler checks reject most damage), then the stage decoders directly, which have no
   checksum in front of them and must survive arbitrary bytes. */
static void damage(const uint8_t *in, int n, uint64_t seed) {
    uint8_t *c = malloc((size_t)n + 64), *d = malloc((size_t)n + 64), *t = malloc((size_t)n + 64);
    int rc = kolmo_bsc_compress(in, c, n, 15, 72, KOLMO_QLFC_ADAPTIVE, 1);
    if (rc <= KOLMO_BSC_HEADER_SIZE) { free(c); free(d); free(t); return; }
    uint64_t x = seed;
    for (int trial = 0; trial < 60; trial++) {
        memcpy(t, c, (size_t)rc);
        int len = rc;
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        if (trial % 3 == 2) len = KOLMO_BSC_HEADER_SIZE + (int)(x % (uint64_t)(rc - KOLMO_BSC_HEADER_SIZE));
        else for (int k = 0; k < 1 + trial % 4; k++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; t[KOLMO_BSC_HEADER_SIZE + x % (uint64_t)(rc - KOLMO_BSC_HEADER_SIZE)] ^= (uint8_t)(1 + (x >> 32) % 255); }
        int r = kolmo_bsc_decompress(t, len, d, n, 1 + trial % 2 * 3);
        CHECK(r != 0 || !memcmp(d, in, (size_t)n), "damaged block decoded to wrong bytes without an error");
        int payload = len - KOLMO_BSC_HEADER_SIZE;
        (void)kolmo_qlfc_decompress(t + KOLMO_BSC_HEADER_SIZE, payload, d, n, KOLMO_QLFC_ADAPTIVE, 2);
        (void)kolmo_qlfc_decompress(t + KOLMO_BSC_HEADER_SIZE, payload, d, n, KOLMO_QLFC_STATIC, 1);
        (void)kolmo_lzp_decompress(t + KOLMO_BSC_HEADER_SIZE, payload, d, n, 15, 72, 2);
    }
    free(c); free(d); free(t);
}

/* Suffix sorter (src/algo/sais): the BWT and its indexes are the same for 2 to 6 threads as for one (one thread is
   libsais's own code path, pinned to libsais's bytes by the golden hashes above), and both inverses restore the input.
   Out-of-range indexes must be refused. */
static void sais_threads(void) {
    static const int ns[] = {2, 3, 7, 100, 4096, 65536, 70001, 300000, 1000003};
    for (int kind = 0; kind < 9; kind++) for (unsigned s = 0; s < sizeof(ns) / sizeof(ns[0]); s++) {
        int n = ns[s]; uint8_t *T = gen(kind, n), *U1 = malloc((size_t)n), *U = malloc((size_t)n), *R = malloc((size_t)n);
        int32_t *A = malloc(4 * (size_t)n + 8), I1[1024], I[1024];
        for (int rl = 1; rl <= 19; rl += 9) {
            int32_t r = 1 << rl;
            if ((n - 1) / r > 1000) continue;
            CHECK(kolmo_sais_bwt_aux(T, U1, A, n, r, I1, 1) == 0, "sais: bwt failed (n %d)", n);
            for (int th = 1; th <= 6; th++) {
                if (th > 1) {
                    int rc = kolmo_sais_bwt_aux(T, U, A, n, r, I, th);
                    CHECK(rc == 0 && !memcmp(U, U1, (size_t)n) && !memcmp(I, I1, 4 * (size_t)((n - 1) / r + 1)),
                          "sais: %d threads changed the BWT (kind %d n %d r %d)", th, kind, n, r);
                }
                memset(R, 0, (size_t)n);
                CHECK(kolmo_sais_unbwt_aux(U1, R, A, n, r, I1, th) == 0 && !memcmp(R, T, (size_t)n), "sais: unbwt_aux (kind %d n %d th %d)", kind, n, th);
                memset(R, 0, (size_t)n);
                CHECK(kolmo_sais_unbwt(U1, R, A, n, I1[0], th) == 0 && !memcmp(R, T, (size_t)n), "sais: unbwt (kind %d n %d th %d)", kind, n, th);
            }
            I[0] = n + 1; CHECK(kolmo_sais_unbwt(U1, R, A, n, n + 1, 2) != 0, "sais: index out of range accepted");
        }
        free(T); free(U1); free(U); free(R); free(A);
    }
}

/* Codec 18: round trips for several parameter sets, the same bytes for 1 and 4 threads, damaged payloads refused or
   decoded without a memory error (ASan build). */
static void codec18(void) {
    static const kolmo_bwt_params PS[] = {KOLMO_BWT_DEFAULTS, {15, 72, KOLMO_QLFC_STATIC, 0, 0}, {0, 0, KOLMO_QLFC_ADAPTIVE, 3, 0},
                                          {16, 32, KOLMO_QLFC_STATIC, 12, 1 << 16}};
    static const int ns[] = {2, 3, 40, 1000, 65536, 300000, 1048576, 3000000};
    for (int kind = 0; kind < NKIND; kind++) for (unsigned s = 0; s < sizeof(ns) / sizeof(ns[0]); s++) for (unsigned q = 0; q < 4; q++) {
        int n = ns[s]; if (q && n > 1048576) continue;
        uint8_t *in = gen(kind, n); size_t cap = (size_t)n + 64 + 4 * 256;
        uint8_t *a = malloc(cap), *b = malloc(cap), *d = malloc((size_t)n + 1);
        int ra = kolmo_bwt_compress(in, a, cap, n, &PS[q], 1), rb = kolmo_bwt_compress(in, b, cap, n, &PS[q], 4);
        /* refusals (KB_NOT_COMPRESSIBLE: the container stores such blocks) must not depend on threads either */
        CHECK(ra == rb && (ra <= 0 || !memcmp(a, b, (size_t)ra)), "codec 18: threads changed the output (kind %d n %d params %u: %d %d)", kind, n, q, ra, rb);
        CHECK(ra > 0 || ra == KB_NOT_COMPRESSIBLE, "codec 18: error %d (kind %d n %d params %u)", ra, kind, n, q);
        if (ra > 0) {
            for (int th = 1; th <= 4; th += 3) {
                memset(d, 0x33, (size_t)n);
                CHECK(kolmo_bwt_decompress(a, (size_t)ra, d, n, th) == 0 && !memcmp(d, in, (size_t)n), "codec 18: round trip (kind %d n %d params %u th %d)", kind, n, q, th);
            }
            if (q == 0 && n <= 300000) {
                uint64_t x = (uint64_t)(kind * 977 + s * 31 + 5);
                for (int t = 0; t < 40; t++) {
                    memcpy(b, a, (size_t)ra); int len = ra;
                    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
                    if (t % 4 == 3) len = (int)(x % (uint64_t)ra);
                    else for (int k = 0; k <= t % 3; k++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; b[x % (uint64_t)ra] ^= (uint8_t)(1 + (x >> 40) % 255); }
                    (void)kolmo_bwt_decompress(b, (size_t)len, d, n, 1 + (t & 1));
                }
            }
        }
        free(in); free(a); free(b); free(d);
    }
}

/* w4: the unrolled LZP encoder (hash sizes <= 17) writes the bytes of the position-by-position reference, and codec
   18 writes the same bytes whether it works inside the caller's buffer (cap >= n + 64 + 1029) or in its own copy. */
extern int kolmo_lzp_force_ref;
static void lzp_fast_vs_ref(void) {
    static const int ns[] = {200, 1000, 4096, 65536, 300000, 1048576};
    static const int hm[][2] = {{10, 16}, {12, 17}, {15, 72}, {16, 24}, {16, 32}, {17, 96}, {17, 255}, {14, 40}, {18, 96}, {20, 64}};
    for (int kind = 0; kind < NKIND; kind++) for (unsigned s = 0; s < sizeof(ns) / sizeof(ns[0]); s++) {
        int n = ns[s]; uint8_t *in = gen(kind, n), *a = malloc((size_t)n + 64), *b = malloc((size_t)n + 64);
        for (unsigned p = 0; p < sizeof(hm) / sizeof(hm[0]); p++) for (int th = 1; th <= 4; th += 3) {
            kolmo_lzp_force_ref = 1; int rb = kolmo_lzp_compress(in, b, n, hm[p][0], hm[p][1], th);
            kolmo_lzp_force_ref = 0; int ra = kolmo_lzp_compress(in, a, n, hm[p][0], hm[p][1], th);
            CHECK(ra == rb && (ra <= 0 || !memcmp(a, b, (size_t)ra)), "lzp fast != reference (kind %d n %d hash %d min %d th %d: %d %d)", kind, n, hm[p][0], hm[p][1], th, ra, rb);
        }
        free(in); free(a); free(b);
    }
    static const int cs[] = {2, 3, 40, 1000, 65536, 300000, 1048576};
    static const kolmo_bwt_params P = KOLMO_BWT_DEFAULTS, P2 = {16, 32, KOLMO_QLFC_STATIC, 12, 1 << 16, 0};
    for (int kind = 0; kind < NKIND; kind++) for (unsigned s = 0; s < sizeof(cs) / sizeof(cs[0]); s++) for (int q = 0; q < 2; q++) {
        int n = cs[s]; uint8_t *in = gen(kind, n); size_t big = (size_t)n + 4096, small = (size_t)n + 64 + 4 * 256;
        uint8_t *a = malloc(big), *b = malloc(small);
        int ra = kolmo_bwt_compress(in, a, big, n, q ? &P2 : &P, 2), rb = kolmo_bwt_compress(in, b, small, n, q ? &P2 : &P, 2);
        CHECK((ra == rb || (rb == KB_NOT_COMPRESSIBLE && ra > 0 && (size_t)ra > small)) && (rb <= 0 || !memcmp(a, b, (size_t)ra)),
              "codec 18: in-place and copied encoders differ (kind %d n %d params %d: %d %d)", kind, n, q, ra, rb);
        free(in); free(a); free(b);
    }
}

int main(int argc, char **argv) {
    int print = argc > 1 && !strcmp(argv[1], "--print");
#ifdef GOLDEN_FROM_LIBBSC
    bsc_init(LIBBSC_FEATURE_FASTMODE);
#endif
    for (int k = 0; k < NKIND; k++) {
        if (print) printf("    {");
        for (int s = 0; s < NSIZE; s++) {
            int n = SIZES[s]; uint8_t *in = gen(k, n), *out = malloc((size_t)n + 64);
#ifdef GOLDEN_FROM_LIBBSC                 /* benchmark/rc3/bwt: the table is printed by libbsc itself */
            int r = bsc_compress(in, out, n, 15, 72, LIBBSC_BLOCKSORTER_BWT, LIBBSC_CODER_QLFC_STATIC, LIBBSC_FEATURE_FASTMODE);
#else
            int r = kolmo_bsc_compress(in, out, n, 15, 72, KOLMO_QLFC_STATIC, 1);
#endif
            uint64_t h = r > 0 ? fnv(out, (size_t)r) : 0;
            if (print) printf("0x%016llxull%s", (unsigned long long)h, s + 1 < NSIZE ? ", " : "},\n");
            else {
                CHECK(h == GOLDEN[k][s], "codec 5 bytes differ from libbsc's: kind %d n=%d", k, n);
                roundtrip(in, n, 15, 72, KOLMO_QLFC_STATIC, "lzp+static");
                if (n <= 1048576) {
                    roundtrip(in, n, 15, 72, KOLMO_QLFC_ADAPTIVE, "lzp+adaptive");
                    roundtrip(in, n, 0, 0, KOLMO_QLFC_STATIC, "static");
                    damage(in, n, (uint64_t)(k * 131 + s + 7));
                }
            }
            free(in); free(out);
        }
    }
    if (print) return 0;
    sais_threads();
    codec18();
    lzp_fast_vs_ref();
    printf(fails ? "BWT TESTS FAILED (%d)\n" : "BWT TESTS PASSED\n", fails);
    return fails != 0;
}
