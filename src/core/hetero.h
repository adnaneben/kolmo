/* Heterogeneous scheduling: the GPU as one more worker beside the CPU thread pool. Apache-2.0, Kolmo contributors.
 *
 * Kolmo's block codecs are independent per block, so a block coded on the GPU worker gives the same bytes as on a CPU
 * worker as long as each GPU kernel equals its CPU reference (src/arch/metal). The scheduler only decides who codes
 * which block; it never changes what a block becomes. */
#ifndef KOLMO_CORE_HETERO_H
#define KOLMO_CORE_HETERO_H
#include <stddef.h>
#include <stdint.h>

/* 0: CPU only (default); 1: the GPU worker joins the slow modes when a GPU path exists. KOLMO_GPU=1 in the
 * environment sets 1 at the first query. Returns 1 when a GPU path is available (Metal device and kernels). */
int kolmo_gpu_set(int mode);
int kolmo_gpu_mode(void);
/* 1 on the GPU worker thread: its BWT sorts go to the GPU (see src/arch/metal/cubwt_metal.c) */
int kolmo_gpu_thread(void);
/* The GPU worker's suffix sort for n bytes may run now (size within limits, memory within the device's budget) */
int kolmo_gpu_sort_allowed(size_t n);
void kolmo_gpu_sort_done(double gpu_seconds, size_t n);   /* gpu_seconds < 0: the GPU failed, the CPU sorted */
unsigned long kolmo_gpu_sort_count(void);                  /* GPU sorts so far in this process */

/* Runs fn(arg) on the CPU pool (run(fn, arg, threads, units), Kolmo's pool) and, when the GPU mode is on and there
 * are more units than CPU threads, also on one extra thread marked as the GPU worker. fn must pull units from a
 * shared counter and must ask kolmo_gpu_take(remaining, cpu_threads) before each unit on the GPU worker. */
void kolmo_hetero_run(void (*run)(void *(*)(void *), void *, int, uint32_t), void *(*fn)(void *), void *arg, int threads,
                      uint32_t units);
/* GPU worker: take another unit only while the CPU workers have a backlog (remaining units > CPU threads), so the
 * GPU never holds the last blocks while CPU cores wait */
int kolmo_gpu_take(uint32_t remaining);

#endif
