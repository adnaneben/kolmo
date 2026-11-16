/* proploop: run one Kolmo compression or decompression in a loop for profiling (attach `sample` to it) or timing.
 *   proploop MODE c|d SECONDS FILE        MODE: speed balanced eco hc ratio x x1
 * One thread, in memory. Prints the number of runs and the fastest run in ms. */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <mach-o/dyld.h>
#include <sys/ucontext.h>

/* PROPLOOP_PCS=file: also sample the program counter every 1 ms of CPU time (SIGPROF) and write the addresses,
 * minus the image slide, for atos -i (line-level profile with inlined functions). */
static uint64_t *pcs; static volatile size_t npc; enum { MAXPC = 1 << 22 };
static void on_prof(int sig, siginfo_t *si, void *uc) {
    (void)sig; (void)si;
    if (npc < MAXPC) pcs[npc++] = ((ucontext_t *)uc)->uc_mcontext->__ss.__pc;
}

#ifndef NO_FORCE_C
extern int kolmo_force_c;             /* PROPLOOP_FORCE_C=1: every CPU kernel takes its C reference (A/B timing) */
#endif
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: proploop MODE c|d SECONDS FILE\n"); return 2; }
    const char *m = argv[1]; int dec = argv[2][0] == 'd'; double secs = atof(argv[3]);
    kolmo_params P = {KOLMO_FAST, 0, 1, 0, 1};
#ifndef NO_FORCE_C
    if (getenv("PROPLOOP_FORCE_C")) kolmo_force_c = atoi(getenv("PROPLOOP_FORCE_C"));
#endif
    if (!strcmp(m, "speed")) { P.mode = KOLMO_RT; P.transforms = 3; }
    else if (!strcmp(m, "eco")) P.mode = KOLMO_ECO;
    else if (!strcmp(m, "hc")) P.mode = KOLMO_HC;
    else if (!strcmp(m, "ratio")) P.mode = KOLMO_MAX;
    else if (!strcmp(m, "x")) P.mode = KOLMO_ULTRA;
    else if (!strcmp(m, "x1")) { P.mode = KOLMO_ULTRA; P.level = 1; }
    FILE *f = fopen(argv[4], "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *src = malloc(n + 1); if (fread(src, 1, n, f) != n) return 1; fclose(f);
    size_t cap = n + n / 2 + (1 << 20); uint8_t *z = malloc(cap), *back = malloc(n + 1);
    /* PROPLOOP_FRAME=path: decompression reuses the frame stored there (written on first use) */
    const char *fr = dec ? getenv("PROPLOOP_FRAME") : NULL; int64_t zn = 0; FILE *ff = fr ? fopen(fr, "rb") : NULL;
    if (ff) { zn = (int64_t)fread(z, 1, cap, ff); fclose(ff); }
    else {
        zn = kolmo_compress(src, n, z, cap, &P);
        if (fr && zn > 0 && (ff = fopen(fr, "wb"))) { fwrite(z, 1, (size_t)zn, ff); fclose(ff); }
    }
    if (zn <= 0) { fprintf(stderr, "compress failed %lld\n", (long long)zn); return 1; }
    const char *pcf = getenv("PROPLOOP_PCS");
    if (pcf) {
        pcs = malloc(8 * (size_t)MAXPC);
        struct sigaction sa = {0}; sa.sa_sigaction = on_prof; sa.sa_flags = SA_SIGINFO | SA_RESTART; sigaction(SIGPROF, &sa, NULL);
        struct itimerval it = {{0, 1000}, {0, 1000}}; setitimer(ITIMER_PROF, &it, NULL);
    }
    double best = 1e9, t0 = now(); int runs = 0;
    do {
        double a = now();
        int64_t r = dec ? kolmo_decompress(z, (size_t)zn, back, n, 1) : kolmo_compress(src, n, z, cap, &P);
        double b = now() - a; if (b < best) best = b; runs++;
        if (r <= 0) { fprintf(stderr, "run failed %lld\n", (long long)r); return 1; }
    } while (now() - t0 < secs);
    if (pcf) {
        struct itimerval it = {{0, 0}, {0, 0}}; setitimer(ITIMER_PROF, &it, NULL);
        FILE *g = fopen(pcf, "w"); intptr_t sl = _dyld_get_image_vmaddr_slide(0);
        for (size_t i = 0; i < npc; i++) fprintf(g, "0x%llx\n", (unsigned long long)(pcs[i] - (uint64_t)sl));
        fclose(g);
    }
    if (dec && memcmp(back, src, n)) { fprintf(stderr, "round trip differs\n"); return 1; }
    printf("%s %s %zu -> %lld runs %d best %.2f ms\n", m, dec ? "d" : "c", n, (long long)zn, runs, best * 1e3);
    return 0;
}
