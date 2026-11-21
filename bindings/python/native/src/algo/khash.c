/* khash64: Kolmo's 64-bit block hash. Copyright 2026 Adnane Aqartit and contributors, Apache-2.0. See khash.h. */
#include "khash.h"
#include <string.h>

static inline uint64_t kx_r64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }   /* little-endian hosts */
static inline uint64_t kx_r32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t kx_fold(uint64_t a, uint64_t b) { unsigned __int128 r = (unsigned __int128)a * b; return (uint64_t)r ^ (uint64_t)(r >> 64); }
static inline uint64_t kx_aval(uint64_t h) {   /* final avalanche: xorshift-multiply, twice */
    h ^= h >> 32; h *= 0xD6E8FEB86659FD93ull; h ^= h >> 32; h *= 0xD6E8FEB86659FD93ull; h ^= h >> 32; return h;
}
static inline uint64_t kx_mix64(uint64_t z) {   /* SplitMix64's output function (Steele, Lea, Flood 2014) */
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31);
}
#define KX_G 0x9E3779B97F4A7C15ull
/* the 16 key words of seed 0, precomputed (kx_mix64(seed + (i + 1) * KX_G)) */
static const uint64_t kx_key0[16] = {
    0xE220A8397B1DCDAFull, 0x6E789E6AA1B965F4ull, 0x06C45D188009454Full, 0xF88BB8A8724C81ECull,
    0x1B39896A51A8749Bull, 0x53CB9F0C747EA2EAull, 0x2C829ABE1F4532E1ull, 0xC584133AC916AB3Cull,
    0x3EE5789041C98AC3ull, 0xF3B8488C368CB0A6ull, 0x657EECDD3CB13D09ull, 0xC2D326E0055BDEF6ull,
    0x8621A03FE0BBDB7Bull, 0x8E1F7555983AA92Full, 0xB54E0F1600CC4D19ull, 0x84BB3F97971D80ABull};

/* stripe t of a 1 KiB block: lane i uses key word (i + t) mod 16, so equal words in different stripes meet different keys
 * (with one key per lane, swapping two stripes' words of a lane would collide) */
#if defined(__GNUC__) && !defined(KOLMO_KHASH_SCALAR)
/* the same stripe with the compilers' generic vector types (GCC and Clang): two 128-bit halves on SSE2/NEON, one
 * 256-bit register with AVX2; the 32x32->64 product of the low and high halves of each lane maps to (v)pmuludq / umull */
typedef uint64_t kx_v4 __attribute__((vector_size(32), aligned(8)));
static inline void kx_stripe(uint64_t acc[8], const uint8_t *q, const uint64_t *K) {   /* K: 8 contiguous key words */
    const kx_v4 lo = {0xFFFFFFFFull, 0xFFFFFFFFull, 0xFFFFFFFFull, 0xFFFFFFFFull};
    kx_v4 a0, a1, v0, v1, k0, k1;   /* loads by memcpy: no vector-typed arguments or returns (ABI-neutral) */
    memcpy(&a0, acc, 32); memcpy(&a1, acc + 4, 32); memcpy(&v0, q, 32); memcpy(&v1, q + 32, 32); memcpy(&k0, K, 32); memcpy(&k1, K + 4, 32);
    kx_v4 d0 = v0 ^ k0, d1 = v1 ^ k1;
    a0 += (d0 & lo) * (d0 >> 32) + v0; a1 += (d1 & lo) * (d1 >> 32) + v1;
    memcpy(acc, &a0, 32); memcpy(acc + 4, &a1, 32);
}
#else
static inline void kx_stripe(uint64_t acc[8], const uint8_t *q, const uint64_t *K) {   /* K: 8 contiguous key words */
    for (int i = 0; i < 8; i++) { uint64_t v = kx_r64(q + 8 * i), d = v ^ K[i]; acc[i] += (uint64_t)(uint32_t)d * (uint32_t)(d >> 32) + v; }
}
#endif
uint64_t kolmo_khash64(const void *vp, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)vp; const uint64_t *K0 = kx_key0;
    const uint64_t L = (uint64_t)n * KX_G, S = seed * 0xD6E8FEB86659FD93ull;
    if (n <= 16) {
        uint64_t a, b;
        if (n >= 8) { a = kx_r64(p); b = kx_r64(p + n - 8); }
        else if (n >= 4) { a = kx_r32(p); b = kx_r32(p + n - 4); }
        else if (n) { a = (uint64_t)p[0] | (uint64_t)p[n >> 1] << 8 | (uint64_t)p[n - 1] << 16; b = 0; }
        else { a = b = 0; }
        return kx_aval(kx_fold(a ^ K0[0] ^ S, b ^ K0[1] ^ L) ^ seed ^ n);
    }
    if (n <= 64) {
        uint64_t h = kx_fold(kx_r64(p) ^ K0[0] ^ S, kx_r64(p + 8) ^ K0[1]) + kx_fold(kx_r64(p + n - 16) ^ K0[2], kx_r64(p + n - 8) ^ K0[3] ^ L);
        if (n > 32) h += kx_fold(kx_r64(p + 16) ^ K0[4], kx_r64(p + 24) ^ K0[5] ^ S) + kx_fold(kx_r64(p + n - 32) ^ K0[6], kx_r64(p + n - 24) ^ K0[7]);
        return kx_aval(h ^ seed ^ n);
    }
    uint64_t K[24], acc[8];   /* the 16 key words, then the first 8 again: rotation t is K + t, contiguous */
    for (int i = 0; i < 16; i++) K[i] = K0[i] + ((i & 1) ? -S : S);   /* the seed moves every key word */
    for (int i = 0; i < 8; i++) K[16 + i] = K[i];
    for (int i = 0; i < 8; i++) acc[i] = K[8 + i] ^ K[i];
    size_t ns = (n - 1) / 64, s = 0;   /* full stripes before the tail stripe (the last 64 bytes, overlapping) */
    for (; s + 16 <= ns; s += 16) {   /* 1 KiB, then a scramble that keeps high bits flowing into the next products */
        for (size_t t = 0; t < 16; t++) kx_stripe(acc, p + (s + t) * 64, K + t);
        for (int i = 0; i < 8; i++) { uint64_t a = acc[i] ^ (acc[i] >> 47) ^ K[8 + i]; acc[i] = a * 0x9E3779B1ull; }
    }
    for (; s < ns; s++) kx_stripe(acc, p + s * 64, K + (s & 15));
    kx_stripe(acc, p + n - 64, K + (ns & 15));   /* the tail: the next rotation, unused by the stripes since the last scramble */
    uint64_t h = L ^ seed;
    for (int i = 0; i < 8; i += 2) h += kx_fold(acc[i] ^ K[8 + i], acc[i + 1] ^ K[9 + i]);
    return kx_aval(h);
}
