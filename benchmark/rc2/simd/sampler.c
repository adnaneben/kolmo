/* Minimal sampling profiler for hosts without perf: LD_PRELOAD=./sampler.so prog ...
 * Every SAMPLER_US microseconds of CPU time (ITIMER_PROF, default 250 us), SIGPROF records the interrupted
 * instruction pointer of whichever thread took the signal. At exit it writes the samples as offsets into their
 * mapped files to $SAMPLER_OUT (one "file offset" line per sample), for symbolization with addr2line/nm.
 * build: cc -O2 -shared -fPIC sampler.c -o sampler.so */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ucontext.h>
#define MAXS (1 << 22)
static unsigned long *ips; static volatile long ns;
static void on_prof(int sig, siginfo_t *si, void *uc) {
    (void)sig; (void)si;
    long i = __atomic_fetch_add(&ns, 1, __ATOMIC_RELAXED);
    if (i < MAXS) ips[i] = (unsigned long)((ucontext_t *)uc)->uc_mcontext.gregs[REG_RIP];
}
__attribute__((constructor)) static void start(void) {
    ips = malloc(sizeof(*ips) * MAXS);
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = on_prof; sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, 0);
    long us = getenv("SAMPLER_US") ? atol(getenv("SAMPLER_US")) : 250;
    struct itimerval it = {{0, us}, {0, us}}; setitimer(ITIMER_PROF, &it, 0);
}
__attribute__((destructor)) static void stop(void) {
    struct itimerval it = {{0, 0}, {0, 0}}; setitimer(ITIMER_PROF, &it, 0);
    const char *fn = getenv("SAMPLER_OUT"); if (!fn) return;
    FILE *m = fopen("/proc/self/maps", "r"), *o = fopen(fn, "w"); if (!m || !o) return;
    struct { unsigned long lo, hi, off; char path[256]; } map[512]; int nm = 0; char line[512];
    while (nm < 512 && fgets(line, sizeof line, m)) {
        char perm[8], path[256] = "?"; unsigned long lo, hi, off;
        if (sscanf(line, "%lx-%lx %7s %lx %*s %*s %255s", &lo, &hi, perm, &off, path) >= 4 && perm[2] == 'x') {
            map[nm].lo = lo; map[nm].hi = hi; map[nm].off = off; strcpy(map[nm].path, path); nm++;
        }
    }
    long n = ns < MAXS ? ns : MAXS;
    for (long i = 0; i < n; i++) {
        int k = 0; while (k < nm && !(ips[i] >= map[k].lo && ips[i] < map[k].hi)) k++;
        if (k < nm) fprintf(o, "%s %lx\n", map[k].path, ips[i] - map[k].lo + map[k].off); else fprintf(o, "? 0\n");
    }
    fclose(o); fclose(m);
}
