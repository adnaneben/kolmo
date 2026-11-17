/* GPU suffix array and BWT against their C reference (libsais): random and edge-case inputs, then optional files.
 *   test_gpu_sa                 random and edge cases (exit 0 when every result equals libsais', or no GPU)
 *   test_gpu_sa -t FILE...      also time GPU and CPU on each file (fastest of 3), and check the results */
#include "arch/metal/metal.h"
#include "core/hetero.h"
#include "kolmo.h"
#include "libbsc/bwt/libsais/libsais.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)rs; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static int check(const uint8_t *t, int32_t n, const char *what) {
    int32_t *a = malloc(4 * (size_t)n), *b = malloc(4 * (size_t)n), *A = malloc(4 * (size_t)n);
    uint8_t *u = malloc(n), *v = malloc(n);
    int r = (n / 8) | 1; while (r & (r - 1)) r &= r - 1; if (r < 2) r = 2;
    int32_t *I = malloc(4 * ((size_t)(n - 1) / r + 2)), *J = malloc(4 * ((size_t)(n - 1) / r + 2));
    int bad = 0, rc;
    if (libsais(t, a, n, 0, NULL)) { printf("libsais failed on %s\n", what); bad = 1; }
    else if ((rc = kolmo_metal_sa(t, b, n))) { printf("GPU suffix array failed on %s (n=%d): %d\n", what, n, rc); bad = 1; }
    else if (memcmp(a, b, 4 * (size_t)n)) {
        int32_t p = 0; while (a[p] == b[p]) p++;
        printf("suffix array differs on %s (n=%d) at %d: %d vs %d\n", what, n, p, a[p], b[p]); bad = 1;
    }
    if (!bad) {
        if (libsais_bwt_aux(t, u, A, n, 0, NULL, r, I) || kolmo_metal_bwt_aux(t, v, n, r, J)) { printf("BWT failed on %s\n", what); bad = 1; }
        else if (memcmp(u, v, n) || memcmp(I, J, 4 * ((size_t)(n - 1) / r + 1))) { printf("BWT differs on %s (n=%d)\n", what, n); bad = 1; }
    }
    free(a); free(b); free(A); free(u); free(v); free(I); free(J);
    return bad;
}

int main(int argc, char **argv) {
    if (!kolmo_metal_available()) { printf("test_gpu_sa: no Metal device, nothing to check\n"); return 0; }
    int bad = 0, tests = 0;
#ifdef KOLMO_METAL_BENCH
    if (argc > 2 && !strcmp(argv[1], "-b")) {
        void kolmo_metal_bench_sort(const uint8_t *t, uint32_t n);
        FILE *f = fopen(argv[2], "rb"); if (!f) return 1;
        fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
        uint8_t *t = malloc(n); if (fread(t, 1, n, f) != (size_t)n) return 1;
        for (int k = 0; k < 3; k++) kolmo_metal_bench_sort(t, (uint32_t)n);
        return 0;
    }
#endif
    if (argc > 1 && !strcmp(argv[1], "-t")) {
        for (int a = 2; a < argc; a++) {
            FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); return 1; }
            fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
            uint8_t *t = malloc(n), *u = malloc(n); int32_t *A = malloc(4 * (size_t)n), I[256], J[256];
            if (fread(t, 1, n, f) != (size_t)n) return 1;
            fclose(f);
            int r = (int)(n / 8) | 1; while (r & (r - 1)) r &= r - 1; r *= 2;   /* bsc: mod + 1, mod = 2^k - 1 */
            double cpu = 1e9, gpu = 1e9, gk = 0; int rounds = 0;
            for (int k = 0; k < 3; k++) {
                double s = now(); libsais_bwt_aux(t, u, A, (int32_t)n, 0, NULL, r, I); double e = now() - s; if (e < cpu) cpu = e;
                uint8_t *v = malloc(n);
                s = now(); int rc = kolmo_metal_bwt_aux(t, v, (int32_t)n, r, J); e = now() - s;
                if (rc) { printf("%s: GPU failed (%d)\n", argv[a], rc); bad = 1; free(v); break; }
                if (e < gpu) { gpu = e; gk = kolmo_metal_last_gpu_seconds(); rounds = kolmo_metal_last_rounds(); }
                if (memcmp(u, v, n) || memcmp(I, J, 4 * (size_t)((n - 1) / r + 1))) { printf("%s: BWT differs\n", argv[a]); bad = 1; }
                free(v);
            }
            printf("%-40s n=%10ld  libsais %7.3f s (%6.1f MB/s)  GPU %7.3f s (%6.1f MB/s, kernels %.3f s, %d rounds)  %.2fx\n",
                   argv[a], n, cpu, n / cpu / 1e6, gpu, n / gpu / 1e6, gk, rounds, cpu / gpu);
            free(t); free(u); free(A);
        }
        return bad;
    }
    /* edge cases: tiny, one symbol, two symbols, periodic, zero bytes (the zero padding of the first sort) */
    for (int32_t n = 2; n < 300; n++) {
        uint8_t *t = malloc(n);
        for (int kind = 0; kind < 7; kind++) {
            for (int32_t i = 0; i < n; i++)
                t[i] = kind == 0 ? 0 : kind == 1 ? 'a' : kind == 2 ? (uint8_t)(i % 2) : kind == 3 ? (uint8_t)"abcab"[i % 5]
                     : kind == 4 ? (uint8_t)(rnd() % 2) : kind == 5 ? (uint8_t)(i < n / 2 ? 0 : rnd() % 3) : (uint8_t)rnd();
            char w[64]; snprintf(w, sizeof w, "edge kind %d n %d", kind, n);
            bad |= check(t, n, w); tests++;
            if (bad && getenv("STOP")) return 1;
        }
        free(t);
    }
    for (int it = 0; it < 60; it++) {
        int32_t n = 2 + (int32_t)(rnd() % (it < 40 ? 70000 : 3000000));
        int alpha = 1 + (int)(rnd() % (it % 4 == 0 ? 3 : 256));
        uint8_t *t = malloc(n);
        for (int32_t i = 0; i < n; i++) t[i] = (uint8_t)(rnd() % alpha);
        if (it % 5 == 1 && n > 1000) {                                 /* long repeats: copies of an earlier stretch */
            for (int32_t i = 1000; i < n; i++) if (rnd() % 1000) t[i] = t[i - 997];
        }
        char w[64]; snprintf(w, sizeof w, "random %d (alpha %d)", it, alpha);
        bad |= check(t, n, w); tests++;
        free(t);
    }
    printf("test_gpu_sa: %d inputs, %s\n", tests, bad ? "FAILED" : "all equal to libsais");
    /* frames: ratio (4 MiB blocks) and -x (1 MiB blocks) on two threads, with and without the GPU worker, must be the
     * same bytes, and the GPU must have sorted some blocks. Text-like data from a random vocabulary of 4096 words with a
     * skewed choice (LZP, which runs before the sort, leaves most of it). */
    size_t n = 24u << 20;
    uint8_t *src = malloc(n), *a = NULL, *b = NULL, *back = malloc(n);
    char vocab[4096][12];
    for (int w = 0; w < 4096; w++) {
        int L = 2 + (int)(rnd() % 9);
        for (int k = 0; k < L; k++) vocab[w][k] = (char)('a' + rnd() % 26);
        vocab[w][L] = rnd() % 8 ? ' ' : '\n'; vocab[w][L + 1] = 0;
    }
    for (size_t i = 0; i < n; ) {
        uint32_t r = rnd(); int w = (int)((r & 4095) & (r >> 12 & 4095));     /* low ids more often */
        size_t L = strlen(vocab[w]); if (i + L > n) L = n - i;
        memcpy(src + i, vocab[w], L); i += L;
    }
    kolmo_mode modes[2] = {KOLMO_MAX, KOLMO_ULTRA};
    for (int mi = 0; mi < 2; mi++) {
        kolmo_params P = {modes[mi], 0, 2, mi ? 1u << 20 : 4u << 20, 1};
        size_t cap = kolmo_bound(mi ? n / 6 : n, &P);
        a = realloc(a, cap); b = realloc(b, cap);
        size_t len = mi ? n / 6 : n;
        kolmo_set_gpu(0); int64_t za = kolmo_compress(src, len, a, cap, &P);
        unsigned long before = kolmo_gpu_sort_count();
        kolmo_set_gpu(1); int64_t zb = kolmo_compress(src, len, b, cap, &P);
        kolmo_set_gpu(0);
        unsigned long sorts = kolmo_gpu_sort_count() - before;
        int same = za > 0 && za == zb && !memcmp(a, b, (size_t)za);
        int rt = zb > 0 && kolmo_decompress(b, (size_t)zb, back, len, 2) == (int64_t)len && !memcmp(back, src, len);
        printf("test_gpu_sa: %s, %zu bytes in %s blocks: %lld bytes, GPU worker %s (%lu GPU sorts), round trip %s\n",
               mi ? "-x" : "ratio", len, mi ? "1 MiB" : "4 MiB", (long long)za, same ? "same bytes" : "DIFFERENT", sorts, rt ? "ok" : "FAILED");
        if (!same || !rt || (!mi && !sorts)) bad = 1;      /* ratio must have used the GPU; -x may not block-sort */
    }
    free(src); free(a); free(b); free(back);
    return bad;
}
