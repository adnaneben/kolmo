/* ARM64 branch transform (plan type T_A64, tflag 14, container version 11): in each 4-byte instruction word, BL's
 * 26-bit word offset and ADRP's 21-bit page offset become position-absolute, so repeated calls or address loads of
 * one target produce identical words. The mapping is the one of the ARM64 BCJ filter of XZ Utils (by Lasse Collin and
 * Jia Tan, 0BSD; written here from its description): BL always; ADRP only when the page offset lies within +-2^17
 * pages, and the result is kept in that range, so the mapping is a bijection on every 32-bit word.
 *
 * pc is the byte position of the word: phase (the position of b[0] modulo 4096, from the plan, so that ADRP's page
 * arithmetic follows the executable's pages) plus the offset in the block. Words are taken at offsets 0, 4, 8, ... of
 * the block; a tail of 1-3 bytes is left as it is. */
static inline uint32_t a64_map(uint32_t w, uint32_t pc, int enc) {
    if ((w >> 26) == 0x25) {                                    /* BL imm26 */
        uint32_t d = pc >> 2; if (!enc) d = 0u - d;
        return 0x94000000u | ((w + d) & 0x03FFFFFFu);
    }
    if ((w & 0x9F000000u) == 0x90000000u) {                     /* ADRP: immlo bits 29-30, immhi bits 5-23 */
        uint32_t src = ((w >> 29) & 3) | ((w >> 3) & 0x001FFFFCu);
        if ((src + 0x00020000u) & 0x001C0000u) return w;       /* outside +-2^17 pages: unchanged */
        uint32_t d = pc >> 12; if (!enc) d = 0u - d;
        uint32_t dst = src + d;
        w &= 0x9000001Fu; w |= (dst & 3) << 29; w |= (dst & 0x0003FFFCu) << 3; w |= (0u - (dst & 0x00020000u)) & 0x00E00000u;
    }
    return w;
}
static void a64_apply(uint8_t *b, size_t n, uint32_t phase, int enc) {
    for (size_t i = 0; i + 4 <= n; i += 4) {
        uint32_t w = (uint32_t)b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24;
        uint32_t v = a64_map(w, (uint32_t)i + phase, enc);
        if (v != w) { b[i] = (uint8_t)v; b[i + 1] = (uint8_t)(v >> 8); b[i + 2] = (uint8_t)(v >> 16); b[i + 3] = (uint8_t)(v >> 24); }
    }
}
static void a64_fwd(uint8_t *b, size_t n, uint32_t phase) { a64_apply(b, n, phase, 1); }
static void a64_inv(uint8_t *b, size_t n, uint32_t phase) { a64_apply(b, n, phase, 0); }
