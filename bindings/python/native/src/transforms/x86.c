/* x86 call/jump transform (E8/E9 with a rel32 whose top byte is 00 or FF): the low 24 bits become position-absolute,
 * so repeated calls to one target produce identical bytes. Encoded back to front and decoded front to back (as in
 * zpaq), so every decision sees the same bytes on both sides. */
/* x86 call/jump transform (zpaq order: encoder backward, decoder forward). The candidates (E8/E9 opcode with a
 * 0x00/0xFF top address byte four bytes on) are rare, so 16 positions are screened at a time with SIMD; after each
 * applied candidate the screen restarts behind it, because the rewritten bytes can create or remove candidates. */
static inline uint32_t e8_mask16(const uint8_t *b) {          /* bit k: position k is a candidate (b has 20 readable bytes) */
#if defined(KOLMO_USE_SSE2)
    __m128i v = _mm_loadu_si128((const __m128i *)b), w = _mm_loadu_si128((const __m128i *)(b + 4));
    __m128i c1 = _mm_cmpeq_epi8(_mm_and_si128(v, _mm_set1_epi8((char)0xFE)), _mm_set1_epi8((char)0xE8));
    __m128i c2 = _mm_or_si128(_mm_cmpeq_epi8(w, _mm_setzero_si128()), _mm_cmpeq_epi8(w, _mm_set1_epi8((char)0xFF)));
    return (uint32_t)_mm_movemask_epi8(_mm_and_si128(c1, c2));
#elif defined(KOLMO_USE_NEON) && defined(__aarch64__)
    uint8x16_t v = vld1q_u8(b), w = vld1q_u8(b + 4);
    uint8x16_t c = vandq_u8(vceqq_u8(vandq_u8(v, vdupq_n_u8(0xFE)), vdupq_n_u8(0xE8)), vorrq_u8(vceqq_u8(w, vdupq_n_u8(0)), vceqq_u8(w, vdupq_n_u8(0xFF))));
    uint64_t m4 = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(c), 4)), 0);
    uint32_t m = 0; while (m4) { int t = __builtin_ctzll(m4); m |= 1u << (t >> 2); m4 &= ~(0xFull << (t & ~3)); }
    return m;
#else
    uint32_t m = 0; for (int k = 0; k < 16; k++) m |= (uint32_t)((b[k] & 0xFE) == 0xE8 && (uint8_t)(b[k + 4] + 1) <= 1) << k;
    return m;
#endif
}
static void e8e9_fwd(uint8_t *b, size_t n) {
    if (n < 5) return;
    size_t i = n - 5 + 1;                                     /* positions i - 1 down to 0 remain */
    while (i > 0) {
        if (i >= 16) {
            uint32_t m = e8_mask16(b + i - 16);
            if (!m) { i -= 16; continue; }
            int t = 31 - __builtin_clz(m); size_t k = i - 16 + (size_t)t;
            uint32_t a = (uint32_t)(b[k + 1] | b[k + 2] << 8 | b[k + 3] << 16) + (uint32_t)k;
            b[k + 1] = (uint8_t)a; b[k + 2] = (uint8_t)(a >> 8); b[k + 3] = (uint8_t)(a >> 16);
            i = k; continue;
        }
        size_t k = --i;
        if ((b[k] & 0xFE) == 0xE8 && (uint8_t)(b[k + 4] + 1) <= 1) {
            uint32_t a = (uint32_t)(b[k + 1] | b[k + 2] << 8 | b[k + 3] << 16) + (uint32_t)k;
            b[k + 1] = (uint8_t)a; b[k + 2] = (uint8_t)(a >> 8); b[k + 3] = (uint8_t)(a >> 16);
        }
    }
}
static void e8e9_inv(uint8_t *b, size_t n) {
    if (n < 5) return;
    size_t i = 0;
    while (i + 5 <= n) {
        if (i + 20 <= n) {
            uint32_t m = e8_mask16(b + i);
            if (!m) { i += 16; continue; }
            i += (size_t)__builtin_ctz(m);
        } else if (!((b[i] & 0xFE) == 0xE8 && (uint8_t)(b[i + 4] + 1) <= 1)) { i++; continue; }
        uint32_t a = (uint32_t)(b[i + 1] | b[i + 2] << 8 | b[i + 3] << 16) - (uint32_t)i;
        b[i + 1] = (uint8_t)a; b[i + 2] = (uint8_t)(a >> 8); b[i + 3] = (uint8_t)(a >> 16);
        i++;
    }
}
