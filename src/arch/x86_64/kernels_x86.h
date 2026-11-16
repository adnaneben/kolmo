/* x86-64 kernels: declarations for the dispatch table (src/arch/cpu.c). */
#ifndef KOLMO_ARCH_X86_KERNELS_H
#define KOLMO_ARCH_X86_KERNELS_H
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
void kolmo_med8_inv_sse2(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);
void kolmo_med8_inv_sse41(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);
#endif
