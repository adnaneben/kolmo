/* GPU kernels on Apple's Metal: host interface. Apache-2.0, Kolmo contributors.
 *
 * Every function has a CPU reference that gives the same bytes; callers fall back to it whenever a function here
 * returns an error. The scheduler that decides when the GPU is used is in src/core/hetero.c. */
#ifndef KOLMO_ARCH_METAL_H
#define KOLMO_ARCH_METAL_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* 1 when a Metal device with 32-lane SIMD groups is present and the kernels compiled (first call compiles them) */
int kolmo_metal_available(void);
/* Bytes of GPU memory a suffix sort of n bytes needs (an estimate the scheduler compares with the device's budget). */
size_t kolmo_metal_sa_bytes(size_t n);
/* GPU memory one job may use: half of the device's recommended working set */
size_t kolmo_metal_budget(void);

/* Suffix array of t[0..n) (2 <= n <= 2^30), the same as libsais(t, sa, n, 0, NULL). Returns 0 or a negative error. */
int kolmo_metal_sa(const uint8_t *t, int32_t *sa, int32_t n);

/* BWT with auxiliary indexes in libsais' convention, the same bytes as libsais_bwt_aux(t, u, A, n, 0, NULL, r, I):
 * u[0] = t[n-1], then t[SA[p]-1] for every p but the one with SA[p] = 0, and I[k] = 1 + position of suffix k*r in
 * the suffix array, k = 0..(n-1)/r. u may be t. Returns 0 or a negative error (u and I are then untouched). */
int kolmo_metal_bwt_aux(const uint8_t *t, uint8_t *u, int32_t n, int32_t r, int32_t *I);

/* Time the GPU spent on the last call (seconds, from the command buffers' GPU timestamps) and doubling rounds. */
double kolmo_metal_last_gpu_seconds(void);
int kolmo_metal_last_rounds(void);

#ifdef __cplusplus
}
#endif
#endif
