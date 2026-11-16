/* CRC32C with the SSE4.2 crc32 instruction (8 bytes per step). Reference and dispatch: arch/crc32c.c. */
#if defined(__SSE4_2__) || defined(KOLMO_X86_DISPATCH)
#include <nmmintrin.h>
#if !defined(__SSE4_2__)
KOLMO_TARGET_SSE42
#endif
static uint32_t crc32c_sse42(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = buf; uint64_t c = ~crc;
    while (len >= 8) { uint64_t v; memcpy(&v, p, 8); c = _mm_crc32_u64(c, v); p += 8; len -= 8; }
    while (len--) c = _mm_crc32_u8((uint32_t)c, *p++);
    return ~(uint32_t)c;
}
#endif
