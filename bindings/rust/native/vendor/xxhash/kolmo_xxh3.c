/* XXH3-64 for kolmo block checksums, compiled apart from zstd's bundled XXH64.
 * x86-64 builds for baseline ISA (release builds, wheels) also compile xxHash's AVX2 kernels with a target attribute
 * and use them for long inputs when CPUID reports AVX2, as upstream's xxh_x86dispatch.c does. Same hash on every path
 * (xxHash's kernels are bit-exact by design); -DKOLMO_NO_DISPATCH keeps compile-time selection only. */
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(__AVX2__) && !defined(KOLMO_NO_DISPATCH)
#define KOLMO_XXH_DISPATCH 1
#define XXH_DISPATCH_AVX2 1
#define XXH_TARGET_AVX2 __attribute__((target("avx2")))
#include <immintrin.h>
#endif
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#undef XXH_NAMESPACE
#define XXH_NAMESPACE KOLMOX_
#include "xxhash.h"
#if defined(KOLMO_XXH_DISPATCH)
XXH_NO_INLINE XXH_TARGET_AVX2 XXH64_hash_t kolmo_xxh3_long_avx2(const void *p, size_t n, XXH64_hash_t seed) {
    if (seed == 0) return XXH3_hashLong_64b_internal(p, n, XXH3_kSecret, sizeof(XXH3_kSecret), XXH3_accumulate_avx2, XXH3_scrambleAcc_avx2);
    XXH_ALIGN(64) xxh_u8 secret[XXH_SECRET_DEFAULT_SIZE];   /* the AVX2 kernel stores 32-byte aligned */
    XXH3_initCustomSecret_avx2(secret, seed);
    return XXH3_hashLong_64b_internal(p, n, secret, sizeof(secret), XXH3_accumulate_avx2, XXH3_scrambleAcc_avx2);
}
static int kolmo_xxh_have_avx2(void) {
    static int v = -1; int r = __atomic_load_n(&v, __ATOMIC_RELAXED);
    if (r < 0) { __builtin_cpu_init(); r = __builtin_cpu_supports("avx2") ? 1 : 0; __atomic_store_n(&v, r, __ATOMIC_RELAXED); }
    return r;
}
#endif
unsigned long long kolmo_xxh3(const void *p, size_t n, unsigned long long seed) {
#if defined(KOLMO_XXH_DISPATCH)
    if (n > XXH3_MIDSIZE_MAX && kolmo_xxh_have_avx2()) return kolmo_xxh3_long_avx2(p, n, seed);
#endif
    return XXH3_64bits_withSeed(p, n, seed);
}
