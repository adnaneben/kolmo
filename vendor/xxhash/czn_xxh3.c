/* XXH3-64 for czn block checksums, compiled apart from zstd's bundled XXH64. */
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#undef XXH_NAMESPACE
#define XXH_NAMESPACE CZNX_
#include "xxhash.h"
unsigned long long czn_xxh3(const void *p, size_t n, unsigned long long seed) { return XXH3_64bits_withSeed(p, n, seed); }
