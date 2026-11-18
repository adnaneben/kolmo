/* AVX2: word transform decoder (C reference: the loop of word_decode in transforms/word.c; the NEON version is
 * arch/aarch64/word_neon.c, same contract). Decmap lane, rc3 wave 3.
 *
 * The reference scans for the next special byte (a code prefix, or the capital marker in front of one) after every
 * token, with a byte table lookup per input byte when a prefix is >= 32. Here 64 input bytes are classified at once:
 * membership in the special set is a 256-bit bitmap looked up with two in-lane byte shuffles (low nibble -> bit row,
 * high nibble -> bit), compares give the capital markers, and the tokens of the window are taken from the two 64-bit
 * masks with count-trailing-zeros. Literal runs move in 16-byte pieces and words in three 16-byte pieces from their
 * 64-byte slots; bytes written past a token are overwritten by the next one, as in the reference. It decodes whole
 * tokens while 80 bytes of input and 2048 bytes of output room remain and stops at a token boundary (or in front of a
 * token the reference rejects), where the C loop goes on. Same bytes; tests/test_arch.c compares it with the C loop
 * (kolmo_force_c). Chosen by CPUID on baseline x86-64 builds. */
#if defined(KOLMO_FMT_AVX2) && !defined(KOLMO_NO_ASM)
#define KOLMO_WD_AVX2 1
#include <immintrin.h>
/* bytes of v that are in the set: lo[n] has bit h set when byte 16 h + n (h < 8) is special, hi[n] the same for
 * h >= 8; bit[h] = 1 << (h & 7) */
KOLMO_TARGET_AVX2 static inline __m256i wd_member(__m256i v, __m256i lo, __m256i hi, __m256i bit) {
    const __m256i m8f = _mm256_set1_epi8((char)0x8F), m80 = _mm256_set1_epi8((char)0x80), m0f = _mm256_set1_epi8(0x0F);
    __m256i rows = _mm256_or_si256(_mm256_shuffle_epi8(lo, _mm256_and_si256(v, m8f)),                  /* v < 128 */
                                   _mm256_shuffle_epi8(hi, _mm256_and_si256(_mm256_xor_si256(v, m80), m8f)));   /* v >= 128 */
    __m256i b = _mm256_shuffle_epi8(bit, _mm256_and_si256(_mm256_srli_epi16(v, 4), m0f));
    return _mm256_cmpeq_epi8(_mm256_and_si256(rows, b), b);
}
KOLMO_TARGET_AVX2 static void kolmo_wd_dec_avx2(const uint8_t **pp, const uint8_t *end, uint8_t **po, uint8_t *oe,
                                                const uint8_t *arena, const uint8_t *wl, uint32_t nw, const int16_t *pidx,
                                                uint8_t capb, const uint8_t *special) {
    const uint8_t *p = *pp; uint8_t *o = *po;
    uint8_t tl[16] = {0}, th[16] = {0}, tb[16];
    for (int b = 0; b < 256; b++) if (special[b]) { if (b < 128) tl[b & 15] |= (uint8_t)(1u << (b >> 4)); else th[b & 15] |= (uint8_t)(1u << ((b >> 4) - 8)); }
    for (int h = 0; h < 16; h++) tb[h] = (uint8_t)(1u << (h & 7));
    const __m256i lo = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)tl));
    const __m256i hi = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)th));
    const __m256i bit = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)tb));
    const __m256i vc = _mm256_set1_epi8((char)capb);
    const __m128i capclr[2] = {_mm_set1_epi8((char)0xFF), _mm_setr_epi8((char)0xDF, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)};
    /* per 64-byte window at most 64 literal bytes plus 32 words of at most 48 bytes (each written as 48) */
    while (end - p >= 80 && oe - o >= 2048) {
        __m256i v0 = _mm256_loadu_si256((const __m256i *)p), v1 = _mm256_loadu_si256((const __m256i *)(p + 32));
        const uint64_t S = (uint32_t)_mm256_movemask_epi8(wd_member(v0, lo, hi, bit)) | (uint64_t)(uint32_t)_mm256_movemask_epi8(wd_member(v1, lo, hi, bit)) << 32;
        const uint64_t C = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v0, vc)) | (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v1, vc)) << 32;
        const uint64_t P = S & ~C;
        uint64_t rest = S; unsigned c = 0;
        for (;;) {
            if (!rest) {                                         /* no token left in the window: the rest is literal */
                for (unsigned k = c; k < 64; k += 16) _mm_storeu_si128((__m128i *)(o + (k - c)), _mm_loadu_si128((const __m128i *)(p + k)));
                o += 64 - c; p += 64;
                break;
            }
            unsigned t = (unsigned)__builtin_ctzll(rest);
            for (unsigned k = c; k < t; k += 16) _mm_storeu_si128((__m128i *)(o + (k - c)), _mm_loadu_si128((const __m128i *)(p + k)));
            o += t - c;
            unsigned cf = (unsigned)(C >> t) & 1, e = t + cf + 2;
            if (e > 64) { p += t; break; }                       /* the token crosses the window: start one at it */
            if (cf && !((P >> (t + 1)) & 1)) { *pp = p + t; *po = o; return; }   /* not a prefix: the C loop rejects it */
            uint32_t code = (uint32_t)pidx[p[t + cf]] << 8 | p[t + cf + 1];
            if (code >= nw) { *pp = p + t; *po = o; return; }
            const uint8_t *src = arena + (size_t)code * 64;
            _mm_storeu_si128((__m128i *)o, _mm_and_si128(_mm_loadu_si128((const __m128i *)src), capclr[cf]));
            _mm_storeu_si128((__m128i *)(o + 16), _mm_loadu_si128((const __m128i *)(src + 16)));
            _mm_storeu_si128((__m128i *)(o + 32), _mm_loadu_si128((const __m128i *)(src + 32)));
            o += wl[code];
            c = e;
            if (c >= 64) { p += 64; break; }
            rest = S & (~0ull << c);
        }
    }
    *pp = p; *po = o;
}
#endif
