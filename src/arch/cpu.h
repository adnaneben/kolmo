/* CPU feature detection and the table of dispatched kernels (docs/LAYOUT.md, "arch").
 *
 * Every kernel has a portable C reference (`*_c`, in src/algo/) that is its specification; an ISA version may only
 * be faster, never write other bytes (tests/test_kernels.c compares them). The table is filled once, at first use,
 * from CPUID; `make NO_ASM=1` (-DKOLMO_NO_ASM) builds the C references only.
 *
 * KOLMO_CPU=c|sse2|sse41|avx2 in the environment lowers the level chosen at start-up (for tests and benchmarks; it
 * never raises it above what the CPU supports). */
#ifndef KOLMO_ARCH_CPU_H
#define KOLMO_ARCH_CPU_H
#include <stddef.h>
#include <stdint.h>

enum { KOLMO_ISA_C = 0, KOLMO_ISA_SSE2 = 1, KOLMO_ISA_SSE41 = 2, KOLMO_ISA_AVX2 = 3 };

typedef struct {
    /* IMG8 inverse: rows of W bytes, channel step C (1..4), m bytes (only whole rows are written); see predict.h */
    void (*med8_inv)(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);
} kolmo_kernels;

const kolmo_kernels *kolmo_kernels_get(void);          /* the table for this CPU (thread safe, cheap after the first call) */
int kolmo_cpu_level(void);                             /* the ISA level the table was filled for */
int kolmo_cpu_max_level(void);                         /* the highest level this CPU and build support */
/* the table for a given level (clamped to kolmo_cpu_max_level()); for tests and benchmarks */
void kolmo_kernels_for_level(int level, kolmo_kernels *k);
const char *kolmo_isa_name(int level);

#define KOLMO_K (kolmo_kernels_get())
#endif
