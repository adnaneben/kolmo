/* kolmobench: in-memory benchmark of kolmo's modes.
 *   kolmobench [-T threads] [-x] [-o results.tsv] file...
 * The library rivals (zstd, LZ4, brotli, xz) were removed with the vendored codecs (container version 12): compare
 * against their command-line tools instead. -x and -X are accepted for compatibility.
 * Timing: best of repeated runs (>= 2 runs or >= 0.5 s), every output decoded and compared byte for byte. */
#define _GNU_SOURCE
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
typedef struct { const char *name; int kind, level, threads; } cfg_t;   /* kind: 10 + kolmo mode */

static size_t comp(const cfg_t *c, const uint8_t *s, size_t n, uint8_t *d, size_t cap) {
    kolmo_params p = {(kolmo_mode)(c->kind - 10), c->level, c->threads, 0, 1}; int64_t r = kolmo_compress(s, n, d, cap, &p); return r > 0 ? (size_t)r : 0;
}
static int decomp(const cfg_t *c, const uint8_t *s, size_t cn, uint8_t *d, size_t n) {
    return kolmo_decompress(s, cn, d, n, c->threads) == (int64_t)n;
}
int main(int argc, char **argv) {
    int T = (int)sysconf(_SC_NPROCESSORS_ONLN), slow = 0, vslow = 0, opt, status = 0, measured = 0; const char *outp = NULL, *only = NULL;
    while ((opt = getopt(argc, argv, "T:xXo:c:")) != -1) { if (opt == 'c') only = optarg; if (opt == 'T') T = atoi(optarg); else if (opt == 'x') slow = 1; else if (opt == 'X') slow = vslow = 1; else if (opt == 'o') outp = optarg; }
    if (optind == argc) { fprintf(stderr, "kolmobench: supply at least one input file\n"); return 1; }
    cfg_t C[] = {
        {"kolmo rt", 10, 0, 1}, {"kolmo rt -T", 10, 0, T}, {"kolmo fast", 11, 0, 1}, {"kolmo fast -T", 11, 0, T},
        {"kolmo eco", 12, 0, 1}, {"kolmo eco -T", 12, 0, T}, {"kolmo hc", 13, 0, 1}, {"kolmo hc -T", 13, 0, T},
        {"kolmo max", 14, 0, 1}, {"kolmo max -T", 14, 0, T}, {"kolmo ultra", 15, 0, 1}, {"kolmo ultra -T", 15, 0, T},
    };
    FILE *o = outp ? fopen(outp, "a") : NULL;
    if (outp && !o) { perror(outp); return 1; }
    for (int a = optind; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) { perror(argv[a]); status = 1; continue; }
        long length;
        if (fseek(f, 0, SEEK_END) || (length = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
            perror(argv[a]); fclose(f); status = 1; continue;
        }
        size_t n = (size_t)length;
        uint8_t *s = malloc(n ? n : 1), *back = malloc(n + 1);
        if (!s || !back || fread(s, 1, n, f) != n) {
            fprintf(stderr, "kolmobench: cannot allocate/read %s\n", argv[a]);
            free(s); free(back); fclose(f); status = 1; continue;
        }
        fclose(f);
        kolmo_params bp = {KOLMO_RT, 0, 0, 0, 1}; size_t cap = kolmo_bound(n, &bp);
        cap += 4096;
        uint8_t *d = malloc(cap);
        if (!d) { free(s); free(back); status = 1; continue; }
        const char *base = strrchr(argv[a], '/') ? strrchr(argv[a], '/') + 1 : argv[a];
        for (size_t k = 0; k < sizeof C / sizeof *C; k++) {
            const cfg_t *c = &C[k];
            if (c->threads > 1 && T <= 1) continue;
            if (only) {                                       /* -c "name1;name2": run only these settings */
                char buf[512]; snprintf(buf, sizeof buf, ";%s;", only); char key[64]; snprintf(key, sizeof key, ";%s;", c->name);
                if (!strstr(buf, key)) continue;
            }
            double bc = 1e9, bd = 1e9, tot = 0; size_t cl = 0; int ok = 1, runs = 0;
            while (runs < 10 && (runs < 3 || tot < 1.0)) {      /* compression: best of up to 10 runs or about 1 s */
                double t0 = now(); cl = comp(c, s, n, d, cap); double t1 = now();
                if (!cl) { ok = 0; break; }
                if (t1 - t0 < bc) bc = t1 - t0; tot += t1 - t0; runs++;
                if (t1 - t0 > 3.0) break;                      /* slow settings: one run */
            }
            for (int r = 0; ok && r < 30; r++) {               /* decompression: best of up to 30 runs or about 1 s */
                memset(back, 0, 64);
                double t1 = now(); int good = decomp(c, d, cl, back, n); double t2 = now();
                ok &= good && !memcmp(back, s, n);
                if (t2 - t1 < bd) bd = t2 - t1;
                tot += t2 - t1; if (r >= 4 && tot > 2.0) break;
            }
            double cm = n / bc / 1e6, dm = n / bd / 1e6;
            measured++; if (!ok) status = 1;
            printf("%-22s %-14s T=%-2d ratio %7.3f  c %8.1f MB/s  d %8.1f MB/s  %s\n", base, c->name, c->threads, cl ? (double)n / cl : 0, cm, dm, ok ? "ok" : "FAIL");
            if (fflush(stdout)) status = 1;
            if (o) {
                if (fprintf(o, "%s\t%s\t%d\t%zu\t%zu\t%.2f\t%.2f\t%d\n", base, c->name, c->threads, n, cl, cm, dm, ok) < 0 || fflush(o)) status = 1;
            }
        }
        free(s); free(back); free(d);
    }
    if (o && fclose(o)) status = 1;
    if (!measured) { fprintf(stderr, "kolmobench: no settings measured\n"); status = 1; }
    (void)slow; (void)vslow;
    return status;
}
