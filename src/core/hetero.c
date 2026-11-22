/* Heterogeneous scheduling: the GPU as one more worker beside the CPU thread pool. Apache-2.0, Kolmo contributors.
 * See hetero.h. Measured on the Apple M5 (benchmark/rc3/gpu/RESULTS.md): the GPU suffix sort runs at 0.3 to 0.8
 * times the speed of one performance core running libsais, so the GPU only joins when every CPU worker already has
 * a block (it then adds work without taking any from a core) and is off unless asked for (KOLMO_GPU=1, --gpu). */
#include "core/hetero.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef KOLMO_GPU
#include "arch/metal/metal.h"
#endif

static atomic_int gpu_mode = -1;                       /* -1: not read from the environment yet */
static _Thread_local int tl_gpu, tl_cpu_threads;
static pthread_mutex_t st_mu = PTHREAD_MUTEX_INITIALIZER;
static unsigned long st_sorts, st_fail, st_runs; static double st_gpu_s; static size_t st_bytes;

static void stats_print(void) {
    fprintf(stderr, "kolmo gpu: %lu runs with the GPU worker, %lu GPU sorts (%.1f MB, %.3f s GPU time), %lu fell back to the CPU\n",
            st_runs, st_sorts, st_bytes / 1e6, st_gpu_s, st_fail);
}

static int available(void) {
#ifdef KOLMO_GPU
    return kolmo_metal_available();
#else
    return 0;
#endif
}

int kolmo_gpu_mode(void) {
    int m = atomic_load(&gpu_mode);
    if (m < 0) {
        const char *e = getenv("KOLMO_GPU");
        m = e && e[0] == '1';
        int expect = -1;
        if (atomic_compare_exchange_strong(&gpu_mode, &expect, m) && getenv("KOLMO_GPU_STATS")) atexit(stats_print);
        m = atomic_load(&gpu_mode);
    }
    return m;
}

int kolmo_gpu_set(int mode) {
    kolmo_gpu_mode();
    atomic_store(&gpu_mode, mode ? 1 : 0);
    return available();
}

int kolmo_gpu_thread(void) { return tl_gpu; }

int kolmo_gpu_sort_allowed(size_t n) {
#ifdef KOLMO_GPU
    /* below 1 MiB the command-buffer round trips cost more than the sort; above the budget the CPU sorts */
    if (!tl_gpu || n < ((size_t)1 << 20) || n > ((size_t)1 << 30)) return 0;
    return kolmo_metal_sa_bytes(n) <= kolmo_metal_budget();
#else
    (void)n; return 0;
#endif
}

void kolmo_gpu_sort_done(double gpu_seconds, size_t n) {
    pthread_mutex_lock(&st_mu);
    if (gpu_seconds < 0) st_fail++; else { st_sorts++; st_gpu_s += gpu_seconds; st_bytes += n; }
    pthread_mutex_unlock(&st_mu);
}

unsigned long kolmo_gpu_sort_count(void) {
    pthread_mutex_lock(&st_mu); unsigned long c = st_sorts; pthread_mutex_unlock(&st_mu);
    return c;
}

typedef struct { void *(*fn)(void *); void *arg; int cpu; } gpu_worker_t;
static void *gpu_worker(void *a) {
    gpu_worker_t *g = a;
    tl_gpu = 1; tl_cpu_threads = g->cpu;
    g->fn(g->arg);
    tl_gpu = 0;
    return NULL;
}

void kolmo_hetero_run(void (*run)(void *(*)(void *), void *, int, uint32_t), void *(*fn)(void *), void *arg, int threads,
                      uint32_t units) {
    pthread_t th; int started = 0;
    gpu_worker_t g = {fn, arg, threads};
    if (kolmo_gpu_mode() && units > (uint32_t)threads && available()) {
        pthread_attr_t at;
        if (!pthread_attr_init(&at)) {
            pthread_attr_setstacksize(&at, (size_t)8 << 20);
            started = !pthread_create(&th, &at, gpu_worker, &g);
            pthread_attr_destroy(&at);
        }
        if (started) { pthread_mutex_lock(&st_mu); st_runs++; pthread_mutex_unlock(&st_mu); }
    }
    run(fn, arg, threads, units);
    if (started) pthread_join(th, NULL);
}

int kolmo_gpu_take(uint32_t remaining) { return !tl_gpu || remaining > (uint32_t)tl_cpu_threads; }
