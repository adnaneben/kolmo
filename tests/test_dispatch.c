/* Runtime ISA dispatch: the CRC32C entry point must give the reference result (a bitwise CRC32C) on every path, for
 * every length (through the long-input kernels) and alignment. The block hash kolmo_khash64 (src/algo/khash.c; it
 * replaced XXH3-64 with container version 12) has no hand-written kernels: its value must not depend on the
 * alignment of its input (compared with an aligned copy), for every length and seed; its pinned values are checked by
 * tests/test_native.c. Reports x86 CPU capabilities when available. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kolmo.h"
#include "algo/khash.h"

static uint32_t crc_ref(uint32_t crc, const uint8_t *p, size_t n) {
    uint32_t c = ~crc;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0x82F63B78u : c >> 1; }
    return ~c;
}
int main(void) {
    enum { N = 1 << 17 };
    uint8_t *buf = malloc(N + 64), *al = malloc(N + 64); uint64_t x = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < N + 64; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; buf[i] = (uint8_t)x; }
    const unsigned long long seeds[] = {0, 1, 0x637a6e31ull, 0x637a6e44ull, 0xFFFFFFFFFFFFFFFFull};
    long bad = 0, n_checks = 0;
    for (size_t len = 0; len <= N; len = len < 1100 ? len + 1 : len * 5 / 4 + 7)
        for (int off = 0; off < 33; off += (len < 1100 ? 16 : 1)) {
            const uint8_t *p = buf + off;
            if (kolmo_crc32c(0, p, len) != crc_ref(0, p, len)) { if (bad++ < 5) printf("crc32c differs: len %zu off %d\n", len, off); }
            if (kolmo_crc32c(0x12345678u, p, len) != crc_ref(0x12345678u, p, len)) bad++;
            memcpy(al, p, len);
            for (unsigned s = 0; s < sizeof seeds / sizeof *seeds; s++)
                if (kolmo_khash64(p, len, seeds[s]) != kolmo_khash64(al, len, seeds[s])) {
                    if (bad++ < 5) printf("khash64 depends on alignment: len %zu off %d seed %llx\n", len, off, seeds[s]);
                }
            n_checks += 7;
        }
#if defined(KOLMO_NO_ASM)
    printf("test_dispatch: %ld checks, %ld mismatches (NO_ASM=1)\n", n_checks, bad);
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init();
    printf("test_dispatch: %ld checks, %ld mismatches (cpu: sse4.2 %d, avx2 %d)\n", n_checks, bad,
           !!__builtin_cpu_supports("sse4.2"), !!__builtin_cpu_supports("avx2"));
#else
    printf("test_dispatch: %ld checks, %ld mismatches (non-x86 host)\n", n_checks, bad);
#endif
    free(buf); free(al);
    return bad != 0;
}
