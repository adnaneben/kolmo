/* CRC32C (Castagnoli) of the container header and index: the portable reference (slicing by 8) and the choice of
 * kernel. x86-64: SSE4.2 when the build targets it, else chosen at run time by CPUID (arch/x86_64/crc32c_sse42.c).
 * Arm: the CRC32 extension when the build targets it (arch/aarch64/crc32c_arm.c). Same value on every path. */
#if defined(__SSE4_2__)
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) { return crc32c_sse42(crc, buf, len); }
#elif defined(__ARM_FEATURE_CRC32)
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) { return crc32c_arm(crc, buf, len); }
#else
static uint32_t crc_tab[8][256];
static pthread_once_t crc_once = PTHREAD_ONCE_INIT;
static void crc_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = c & 1 ? (c >> 1) ^ 0x82F63B78u : c >> 1;
        crc_tab[0][i] = c;
    }
    for (int t = 1; t < 8; t++)
        for (int i = 0; i < 256; i++) crc_tab[t][i] = (crc_tab[t - 1][i] >> 8) ^ crc_tab[0][crc_tab[t - 1][i] & 0xFF];
}
uint32_t kolmo_crc32c(uint32_t crc, const void *buf, size_t len) {
#if defined(KOLMO_X86_DISPATCH)
    if (kolmo_have_sse42()) return crc32c_sse42(crc, buf, len);
#endif
    pthread_once(&crc_once, crc_init);
    const uint8_t *p = buf; uint32_t c = ~crc;
    while (len >= 8) {
        uint64_t v; memcpy(&v, p, 8); v ^= c;
        c = crc_tab[7][v & 0xFF] ^ crc_tab[6][(v >> 8) & 0xFF] ^ crc_tab[5][(v >> 16) & 0xFF] ^ crc_tab[4][(v >> 24) & 0xFF] ^
            crc_tab[3][(v >> 32) & 0xFF] ^ crc_tab[2][(v >> 40) & 0xFF] ^ crc_tab[1][(v >> 48) & 0xFF] ^ crc_tab[0][v >> 56];
        p += 8; len -= 8;
    }
    while (len--) c = (c >> 8) ^ crc_tab[0][(c ^ *p++) & 0xFF];
    return ~c;
}
#endif
