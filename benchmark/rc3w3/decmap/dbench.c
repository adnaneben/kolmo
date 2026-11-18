/* dbench: decode benchmark of Kolmo frames (decmap lane, rc3 wave 3).
 *   dbench -c MODE [-l level] [-T threads] FILE OUT.k      compress once (writes the frame)
 *   dbench -d [-T threads] [-s seconds] FRAME... print size, decode MB/s (best of repeated in-memory runs, >= 3 runs
 *                                                  and >= s seconds), output checked against FRAME's .src sibling hash
 * -r N: at least N runs (default 3).
 * MODE: speed balanced eco hc ratio x. One thread count per run; never mixed. */
#define _GNU_SOURCE
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static uint8_t *slurp(const char *f, size_t *n) { FILE *fp = fopen(f, "rb"); if (!fp) { perror(f); exit(1); }
    fseek(fp, 0, SEEK_END); *n = (size_t)ftell(fp); fseek(fp, 0, SEEK_SET); uint8_t *b = malloc(*n + 1);
    if (fread(b, 1, *n, fp) != *n) exit(1); fclose(fp); return b; }
static uint64_t fnv(const uint8_t *p, size_t n) { uint64_t h = 1469598103934665603ull; for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull; return h; }
int main(int argc, char **argv) {
    int T = 1, opt, dec = 0, level = 0, minr = 3; double secs = 1.0; const char *mode = NULL;
    while ((opt = getopt(argc, argv, "c:dl:T:s:r:")) != -1) switch (opt) {
        case 'c': mode = optarg; break; case 'd': dec = 1; break; case 'l': level = atoi(optarg); break;
        case 'T': T = atoi(optarg); break; case 's': secs = atof(optarg); break; case 'r': minr = atoi(optarg); break; default: return 2; }
    if (mode) {
        size_t n; uint8_t *s = slurp(argv[optind], &n);
        kolmo_params p = {KOLMO_FAST, level, T, 0, 1};
        p.mode = !strcmp(mode, "speed") ? KOLMO_RT : !strcmp(mode, "balanced") ? KOLMO_FAST : !strcmp(mode, "eco") ? KOLMO_ECO :
                 !strcmp(mode, "hc") ? KOLMO_HC : !strcmp(mode, "ratio") ? KOLMO_MAX : KOLMO_ULTRA;
        size_t cap = n + n / 2 + (1 << 20); uint8_t *d = malloc(cap);
        double t0 = now(); int64_t r = kolmo_compress(s, n, d, cap, &p); double tc = now() - t0;
        if (r <= 0) { fprintf(stderr, "compress failed %lld\n", (long long)r); return 1; }
        FILE *f = fopen(argv[optind + 1], "wb"); fwrite(d, 1, (size_t)r, f); fclose(f);
        char hp[4096]; snprintf(hp, sizeof hp, "%s.h", argv[optind + 1]); f = fopen(hp, "w");
        fprintf(f, "%zu %016llx\n", n, (unsigned long long)fnv(s, n)); fclose(f);
        printf("%s %s %zu %lld %.3f\n", argv[optind], mode, n, (long long)r, tc); return 0;
    }
    if (!dec) return 2;
    for (int a = optind; a < argc; a++) {
        size_t z; uint8_t *s = slurp(argv[a], &z); char hp[4096]; snprintf(hp, sizeof hp, "%s.h", argv[a]);
        size_t n = 0; unsigned long long h = 0; FILE *f = fopen(hp, "r"); if (!f || fscanf(f, "%zu %llx", &n, &h) != 2) return 1; fclose(f);
        uint8_t *d = malloc(n + 64); double best = 1e9, tot = 0; int runs = 0;
        while (runs < minr || tot < secs) {
            double t0 = now(); int64_t r = kolmo_decompress(s, z, d, n + 64, T); double t = now() - t0;
            if (r != (int64_t)n || (runs == 0 && fnv(d, n) != h)) { fprintf(stderr, "MISMATCH %s\n", argv[a]); return 1; }
            if (t < best) best = t; tot += t; runs++; if (runs > 200) break;
        }
        printf("%s %zu %zu %.2f\n", argv[a], n, z, n / best / 1e6); fflush(stdout); free(d); free(s);
    }
    return 0;
}
