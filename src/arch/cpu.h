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
    /* byte planes of w-byte words, one channel (T_SHUF); see algo/planes.h */
    void (*planes_split)(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out);
    void (*planes_merge)(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out);
    /* context-mixing mixer: dot products and weight updates of 2 or 4 weight sets; see algo/mixer.h */
    void (*cm_dot)(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]);
    void (*cm_train)(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift);
    /* video strips (kvid, Y4M): predictor choice and residuals, and their inverse; see algo/video.h */
    int (*vid_strip_enc)(const uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, uint8_t *res);
    void (*vid_strip_dec)(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res);
    /* IMG16 forward prediction and the detector's stride search; see algo/predict.h */
    void (*img16_fwd)(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi);
    uint64_t (*sad16)(const uint16_t *x, const uint16_t *y, size_t n);
} kolmo_kernels;

const kolmo_kernels *kolmo_kernels_get(void);          /* the table for this CPU (thread safe, cheap after the first call) */
int kolmo_cpu_level(void);                             /* the ISA level the table was filled for */
int kolmo_cpu_max_level(void);                         /* the highest level this CPU and build support */
/* the table for a given level (clamped to kolmo_cpu_max_level()); for tests and benchmarks */
void kolmo_kernels_for_level(int level, kolmo_kernels *k);
const char *kolmo_isa_name(int level);

#define KOLMO_K (kolmo_kernels_get())
#endif
