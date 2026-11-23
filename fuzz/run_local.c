/* Local driver so the OSS-Fuzz targets can be exercised without libFuzzer (macOS clang ships no libFuzzer runtime)
 * under ASan/UBSan: seeds the targets with real .kolmo files and random inputs, plus mutations. Not coverage guided:
 * it proves the harness and the paths are clean, and with a time budget it runs a mutation campaign over the seeds.
 *   run_local N [files...]    each seed + 500 mutations of it, then N random inputs (CI smoke)
 *   run_local -S [files...]   a campaign of S seconds: seeds in turn, each mutated by one of several strategies (bit
 *                             flips, byte sets, 32-bit field overwrites with boundary values, truncation, insertion,
 *                             deletion, splices from another seed); prints execs and seeds at the end */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void){ rs ^= rs<<13; rs ^= rs>>7; rs ^= rs<<17; return rs; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#define MAXSEED (1u << 20)
static size_t mutate(uint8_t *m, size_t n, size_t cap, uint8_t *const *seeds, const size_t *sn, int ns) {
    if (!n) { m[0] = (uint8_t)rnd(); return 1; }
    static const uint32_t magic[] = {0, 1, 2, 3, 4, 7, 8, 15, 16, 255, 256, 4096, 65535, 65536, 0x7fffffff, 0x80000000u, 0xffffffffu, 0xfffffffeu};
    switch (rnd() % 8) {
    case 0: case 1: { int k = 1 + (int)(rnd() % 6); while (k--) m[rnd() % n] ^= (uint8_t)(1u << (rnd() % 8)); break; }
    case 2: { int k = 1 + (int)(rnd() % 4); while (k--) m[rnd() % n] = (uint8_t)rnd(); break; }
    case 3: if (n >= 4) {                                   /* a 32-bit field (sizes, counts, offsets) */
        size_t at = rnd() % 4 == 0 ? 32 + (rnd() % 64) * 4 : rnd() % (n - 3); if (at > n - 4) at = n - 4;
        uint32_t v = magic[rnd() % (sizeof magic / sizeof *magic)];
        if (rnd() & 1) { uint32_t o; memcpy(&o, m + at, 4); v = rnd() & 1 ? o + (uint32_t)(rnd() % 33) - 16 : o * 2 + 1; }
        memcpy(m + at, &v, 4); } break;
    case 4: n = 1 + rnd() % n; break;                       /* truncate */
    case 5: if (n < cap) { size_t at = rnd() % (n + 1); memmove(m + at + 1, m + at, n - at); m[at] = (uint8_t)rnd(); n++; } break;
    case 6: if (n > 1) { size_t at = rnd() % n, k = 1 + rnd() % (n - at < 16 ? n - at : 16); memmove(m + at, m + at + k, n - at - k); n -= k; } break;
    case 7: if (ns > 1) {                                   /* splice a run of another seed over this one */
        int o = (int)(rnd() % (uint64_t)ns); if (!sn[o]) break;
        size_t from = rnd() % sn[o], len = 1 + rnd() % (sn[o] - from < 256 ? sn[o] - from : 256), at = rnd() % n;
        if (len > n - at) len = n - at;
        memcpy(m + at, seeds[o] + from, len); } break;
    }
    return n;
}
int main(int argc, char **argv) {
    long iters = argc > 1 ? atol(argv[1]) : 200000;
    if (argc > 1 && iters == 0 && argv[1][0] == '-') iters = -1;
    uint8_t *buf = malloc(4<<20);  /* scratch region at +2MiB */
    if (iters < 0) {                                        /* timed campaign */
        double budget = (double)-iters, t0 = now(); int ns = 0;
        uint8_t **seeds = calloc((size_t)argc, sizeof *seeds); size_t *sn = calloc((size_t)argc, sizeof *sn);
        for (int a = 2; a < argc; a++) {
            FILE *f = fopen(argv[a], "rb"); if (!f) continue;
            uint8_t *s = malloc(MAXSEED); size_t n = fread(s, 1, MAXSEED, f); fclose(f);
            seeds[ns] = s; sn[ns] = n; ns++;
            LLVMFuzzerTestOneInput(s, n);
        }
        if (!ns) { fprintf(stderr, "run_local: a campaign needs seed files\n"); free(seeds); free(sn); free(buf); return 1; }
        uint8_t *m = malloc(MAXSEED + 64); long execs = 0;
        while (now() - t0 < budget) {
            for (int i = 0; i < ns && now() - t0 < budget; i++) {
                for (int it = 0; it < 64; it++) {
                    size_t n = sn[i]; memcpy(m, seeds[i], n);
                    int depth = 1 + (int)(rnd() % 3); while (depth--) n = mutate(m, n, MAXSEED + 64, seeds, sn, ns);
                    LLVMFuzzerTestOneInput(m, n); execs++;
                }
            }
        }
        printf("local fuzz campaign done: %ld execs over %d seeds in %.0f s\n", execs, ns, now() - t0);
        for (int i = 0; i < ns; i++) free(seeds[i]);         /* LeakSanitizer runs at exit: the driver frees its own */
        free(seeds); free(sn); free(m); free(buf); return 0;
    }
    /* seed corpus: any files passed after the count */
    for (int a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) continue;
        size_t n = fread(buf, 1, 1<<20, f); fclose(f);
        LLVMFuzzerTestOneInput(buf, n);
        for (int it = 0; it < 500; it++) { size_t m = n ? n : 1; uint8_t *mut = buf + (2<<20); memcpy(mut, buf, m);
            int nf = 1 + rnd()%6; for (int q=0;q<nf;q++) mut[rnd()%m] ^= (uint8_t)(1+rnd()%255);
            LLVMFuzzerTestOneInput(mut, m); }
    }
    for (long it = 0; it < iters; it++) {
        size_t n = rnd() % 4096;
        for (size_t i = 0; i < n; i += 8) { uint64_t r = rnd(); memcpy(buf+i, &r, n-i<8?n-i:8); }
        if (n >= 4 && rnd()%2) memcpy(buf, "CZN1", 4);          /* bias toward plausible headers */
        LLVMFuzzerTestOneInput(buf, n);
    }
    puts("local fuzz driver done");
    free(buf); return 0;
}
