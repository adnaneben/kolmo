/* CRC32C with the Armv8 CRC32 extension (8 bytes per step). Reference and dispatch: arch/crc32c.c. */
#if defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
static uint32_t crc32c_arm(uint32_t crc, const void *buf, size_t len) {
    const uint8_t *p = buf; uint32_t c = ~crc;
    while (len >= 8) { uint64_t v; memcpy(&v, p, 8); c = __crc32cd(c, v); p += 8; len -= 8; }
    while (len--) c = __crc32cb(c, *p++);
    return ~c;
}
#endif
