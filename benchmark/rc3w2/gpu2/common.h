/* gpu2 benchmark helpers: a persistent CPU thread pool, timing, an energy proxy and GPU utilisation. Apache-2.0,
 * Kolmo contributors. macOS only (research harness). */
#ifndef GPU2_COMMON_H
#define GPU2_COMMON_H
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static double now_s(void) {
    static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-9;
}

/* ---- pool: par_run(T, n, fn, ctx) calls fn(ctx, i) for i in [0, n) on T threads (the caller is one of them).
 * Every call wakes workers 0 .. T-2 and waits until each has finished that generation (no straggler can read the
 * next call's job while it is being set up). */
typedef void (*par_fn)(void *ctx, uint32_t i);
static struct {
    pthread_t th[64]; int nth; pthread_mutex_t mu; pthread_cond_t go, done;
    uint64_t gen; par_fn fn; void *ctx; uint32_t n; atomic_uint next; int want, finished;
} P = {.mu = PTHREAD_MUTEX_INITIALIZER, .go = PTHREAD_COND_INITIALIZER, .done = PTHREAD_COND_INITIALIZER};
static void par_drain(void) { uint32_t i; while ((i = atomic_fetch_add(&P.next, 1)) < P.n) P.fn(P.ctx, i); }
static void *par_worker(void *arg) {
    int id = (int)(intptr_t)arg; uint64_t seen = 0;
    for (;;) {
        pthread_mutex_lock(&P.mu);
        while (P.gen == seen || id >= P.want) { seen = P.gen; pthread_cond_wait(&P.go, &P.mu); }
        seen = P.gen;
        pthread_mutex_unlock(&P.mu);
        par_drain();
        pthread_mutex_lock(&P.mu);
        if (++P.finished == P.want) pthread_cond_signal(&P.done);
        pthread_mutex_unlock(&P.mu);
    }
    return NULL;
}
static void par_run(int T, uint32_t n, par_fn fn, void *ctx) {
    if (T <= 1 || n <= 1) { for (uint32_t i = 0; i < n; i++) fn(ctx, i); return; }
    if (T > 64) T = 64;
    pthread_mutex_lock(&P.mu);
    while (P.nth < T - 1) { pthread_create(&P.th[P.nth], NULL, par_worker, (void *)(intptr_t)P.nth); P.nth++; }
    P.fn = fn; P.ctx = ctx; P.n = n; atomic_store(&P.next, 0); P.want = T - 1; P.finished = 0; P.gen++;
    pthread_cond_broadcast(&P.go);
    pthread_mutex_unlock(&P.mu);
    par_drain();
    pthread_mutex_lock(&P.mu);
    while (P.finished < P.want) pthread_cond_wait(&P.done, &P.mu);
    pthread_mutex_unlock(&P.mu);
}

/* ---- GPU device utilisation (IOAccelerator PerformanceStatistics), sampled every 5 ms by a thread */
static struct { pthread_t th; atomic_int run, on; double sum; long cnt; pthread_mutex_t mu; io_object_t svc; } U = {.mu = PTHREAD_MUTEX_INITIALIZER};
static int gpu_util_now(void) {
    if (!U.svc) U.svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOAccelerator"));
    if (!U.svc) return -1;
    CFTypeRef ps = IORegistryEntryCreateCFProperty(U.svc, CFSTR("PerformanceStatistics"), kCFAllocatorDefault, 0);
    if (!ps) return -1;
    long long x = -1;
    CFNumberRef v = CFDictionaryGetValue(ps, CFSTR("Device Utilization %"));
    if (v) CFNumberGetValue(v, kCFNumberLongLongType, &x);
    CFRelease(ps);
    return (int)x;
}
static void *util_thread(void *a) {
    (void)a;
    while (atomic_load(&U.run)) {
        if (atomic_load(&U.on)) { int u = gpu_util_now(); if (u >= 0) { pthread_mutex_lock(&U.mu); U.sum += u; U.cnt++; pthread_mutex_unlock(&U.mu); } }
        usleep(5000);
    }
    return NULL;
}
static void util_start(void) { if (!atomic_load(&U.run)) { atomic_store(&U.run, 1); pthread_create(&U.th, NULL, util_thread, NULL); } }

/* ---- one measurement: wall, CPU seconds, CPU energy (nJ, rusage), GPU utilisation */
typedef struct { double t0, c0; uint64_t e0; double wall, cpu, gpu_util; uint64_t nj; } meas_t;
static double cpu_s(void) { struct rusage r; getrusage(RUSAGE_SELF, &r); return r.ru_utime.tv_sec + r.ru_stime.tv_sec + (r.ru_utime.tv_usec + r.ru_stime.tv_usec) * 1e-6; }
static uint64_t energy_nj(void) { struct rusage_info_v6 ri; if (proc_pid_rusage(getpid(), RUSAGE_INFO_V6, (rusage_info_t *)&ri)) return 0; return ri.ri_energy_nj; }
static void meas_begin(meas_t *m) {
    pthread_mutex_lock(&U.mu); U.sum = 0; U.cnt = 0; pthread_mutex_unlock(&U.mu); atomic_store(&U.on, 1);
    m->e0 = energy_nj(); m->c0 = cpu_s(); m->t0 = now_s();
}
static void meas_end(meas_t *m) {
    m->wall = now_s() - m->t0; m->cpu = cpu_s() - m->c0; m->nj = energy_nj() - m->e0;
    atomic_store(&U.on, 0);
    pthread_mutex_lock(&U.mu); m->gpu_util = U.cnt ? U.sum / U.cnt : -1; pthread_mutex_unlock(&U.mu);
}

static int lock_present(void) {
    const char *p = getenv("GPU2_LOCK");
    return p && access(p, F_OK) == 0;
}

static uint8_t *slurp_aligned(const char *path, size_t *n, void *(*alloc)(size_t)) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long m = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = alloc((size_t)m + 64);
    if (b && fread(b, 1, (size_t)m, f) != (size_t)m) b = NULL;
    fclose(f); *n = (size_t)m; return b;
}
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static double median(double *v, int n) { qsort(v, (size_t)n, sizeof *v, cmp_d); return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2; }
#endif
