/* Runtime ISA dispatch: the CRC32C and XXH3 entry points must give the reference result on every path, for every
 * length (through the long-input kernels), alignment and seed. The references here are scalar: a bitwise CRC32C and
 * xxHash compiled with XXH_VECTOR=XXH_SCALAR in its own namespace. Reports which paths this CPU takes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kolmo.h"
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#define XXH_VECTOR 0                      /* XXH_SCALAR */
#undef XXH_NAMESPACE
#define XXH_NAMESPACE KOLMOREF_
#include "../vendor/xxhash/xxhash.h"
unsigned long long kolmo_xxh3(const void *p, size_t n, unsigned long long seed);

static uint32_t crc_ref(uint32_t crc, const uint8_t *p, size_t n) {
    uint32_t c = ~crc;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0x82F63B78u : c >> 1; }
    return ~c;
}
int main(void) {
    enum { N = 1 << 17 };
    uint8_t *buf = malloc(N + 64); uint64_t x = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < N + 64; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; buf[i] = (uint8_t)x; }
    const unsigned long long seeds[] = {0, 1, 0x637a6e31ull, 0x637a6e44ull, 0xFFFFFFFFFFFFFFFFull};
    long bad = 0, n_checks = 0;
    for (size_t len = 0; len <= N; len = len < 1100 ? len + 1 : len * 5 / 4 + 7)
        for (int off = 0; off < 33; off += (len < 1100 ? 16 : 1)) {
            const uint8_t *p = buf + off;
            if (kolmo_crc32c(0, p, len) != crc_ref(0, p, len)) { if (bad++ < 5) printf("crc32c differs: len %zu off %d\n", len, off); }
            if (kolmo_crc32c(0x12345678u, p, len) != crc_ref(0x12345678u, p, len)) bad++;
            for (unsigned s = 0; s < sizeof seeds / sizeof *seeds; s++)
                if (kolmo_xxh3(p, len, seeds[s]) != KOLMOREF_XXH3_64bits_withSeed(p, len, seeds[s])) {
                    if (bad++ < 5) printf("xxh3 differs: len %zu off %d seed %llx\n", len, off, seeds[s]);
                }
            n_checks += 7;
        }
    __builtin_cpu_init();
    printf("test_dispatch: %ld checks, %ld mismatches (cpu: sse4.2 %d, avx2 %d)\n", n_checks, bad,
           !!__builtin_cpu_supports("sse4.2"), !!__builtin_cpu_supports("avx2"));
    free(buf);
    return bad != 0;
}
