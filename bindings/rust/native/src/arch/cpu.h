/* CPU feature detection and the target attributes of the dispatched kernels (x86-64: CPUID at first use). */
/* ------------------------------------------------------------------ runtime ISA dispatch (x86-64)
 * Release builds and the Python wheels compile for baseline x86-64 (no -mavx2 / -msse4.2), so the vector paths below
 * are compiled for their ISA with a target attribute and chosen at first use by CPUID (__builtin_cpu_supports).
 * When the build already targets the ISA (-mavx2, -march=native) the test folds to a constant and the generic code is
 * dead. -DKOLMO_NO_DISPATCH restores compile-time selection only. Every path writes the same bytes. */
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(KOLMO_NO_DISPATCH)
#define KOLMO_X86_DISPATCH 1
#include <immintrin.h>
#if !defined(__AVX2__)
static int kolmo_have_avx2(void) {
    static int v = -1; int r = __atomic_load_n(&v, __ATOMIC_RELAXED);
    if (r < 0) { __builtin_cpu_init(); r = __builtin_cpu_supports("avx2") ? 1 : 0; __atomic_store_n(&v, r, __ATOMIC_RELAXED); }
    return r;
}
#endif
#if !defined(__SSE4_2__)
static int kolmo_have_sse42(void) {
    static int v = -1; int r = __atomic_load_n(&v, __ATOMIC_RELAXED);
    if (r < 0) { __builtin_cpu_init(); r = __builtin_cpu_supports("sse4.2") ? 1 : 0; __atomic_store_n(&v, r, __ATOMIC_RELAXED); }
    return r;
}
#endif
#define KOLMO_TARGET_AVX2 __attribute__((target("avx2")))
#define KOLMO_TARGET_SSE42 __attribute__((target("sse4.2")))
#endif
#if defined(__AVX2__)
#define kolmo_have_avx2() 1
#endif
#if defined(__SSE4_2__)
#define kolmo_have_sse42() 1
#endif
#if defined(__AVX2__) || defined(KOLMO_X86_DISPATCH)
#define KOLMO_FMT_AVX2 1                /* the AVX2 decimal formatters are compiled (and chosen by kolmo_have_avx2) */
#endif
#if !defined(KOLMO_TARGET_AVX2)
#define KOLMO_TARGET_AVX2
#endif

/* Host byte order for the integer-inverse loops (transforms/graph.c and the arch/ kernels): the loops store whole
 * words, so they are used on little-endian hosts only. -DKOLMO_FORCE_GENERIC_INT_INV sends every program to the
 * reference inverse (kg_int_inv). */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define FI_HOST_LE 0
#else
#define FI_HOST_LE 1
#endif
#if defined(KOLMO_FORCE_GENERIC_INT_INV)
#define FI_USE 0
#else
#define FI_USE FI_HOST_LE
#endif
