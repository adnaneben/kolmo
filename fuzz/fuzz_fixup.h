/* Fuzz-only helpers: recompute the checksums that guard headers and sections, so that mutations reach the parsers
 * behind them (the frame header CRC, the KEXT trailer and section CRCs, the CRC and id of a KPF1 profile, the header
 * CRC of the table frame inside a DDP1 section). Block checksums (over decoded bytes) are left alone.
 * Needs kolmo_crc32c (public) and, for profiles, kolmo_xxh3 (include src/kolmo.c first). */
#ifndef KOLMO_FUZZ_FIXUP_H
#define KOLMO_FUZZ_FIXUP_H
#include <stdint.h>
#include <string.h>

static uint32_t fz_g32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t fz_g64(const uint8_t *p) { return (uint64_t)fz_g32(p) | (uint64_t)fz_g32(p + 4) << 32; }
static void fz_p32(uint8_t *p, uint32_t v) { for (int k = 0; k < 4; k++) p[k] = (uint8_t)(v >> (8 * k)); }

/* CZN1 header CRC (bytes 28..31: CRC-32C of bytes 0..27 and the block index) */
static void fz_fix_frame(uint8_t *s, size_t n) {
    if (n < 32 || memcmp(s, "CZN1", 4)) return;
    uint64_t nb = fz_g32(s + 20);
    if (32 + nb * 16 > n) return;
    uint32_t hc = kolmo_crc32c(0, s, 28); hc = kolmo_crc32c(hc, s + 32, (size_t)nb * 16);
    fz_p32(s + 28, hc);
}

/* KEXT trailer and every section CRC; a DDP1 section's nested table frame gets its header CRC first */
static void fz_fix_kext(uint8_t *s, size_t n) {
    if (n < 24) return;
    uint8_t *t = s + n - 24;
    if (memcmp(t, "KEXT", 4)) return;
    uint64_t count = fz_g32(t + 8), len = fz_g64(t + 12);
    if (len > n - 24) return;
    size_t p = n - 24 - (size_t)len, end = n - 24;
    for (uint64_t i = 0; i < count && end - p >= 16; i++) {
        uint64_t l = fz_g64(s + p + 4);
        if (l > end - p - 16) break;
        if (!memcmp(s + p, "DDP1", 4) && l >= 16) {
            uint64_t tl = fz_g64(s + p + 12 + 8);
            if (tl <= l - 16) fz_fix_frame(s + p + 12 + 16, (size_t)tl);
        }
        fz_p32(s + p + 12 + l, kolmo_crc32c(0, s + p, 12 + (size_t)l));
        p += 16 + (size_t)l;
    }
    fz_p32(t + 20, kolmo_crc32c(0, t, 20));
}

#ifdef KP_HDR
/* KPF1: id (bytes 8..11) = low 32 bits of XXH3-64 of the sections, then the trailing CRC-32C */
static void fz_fix_profile(uint8_t *s, size_t n) {
    if (n < KP_HDR + 4 || memcmp(s, "KPF1", 4)) return;
    fz_p32(s + 8, (uint32_t)kolmo_xxh3(s + KP_HDR, n - 4 - KP_HDR, KP_ID_SEED));
    fz_p32(s + n - 4, kolmo_crc32c(0, s, n - 4));
}
#endif
#endif
