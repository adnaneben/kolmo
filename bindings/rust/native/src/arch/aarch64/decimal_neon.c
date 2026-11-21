/* NEON decimal formatter for fixed slots: 2 values per step for |v| < 1e8 and S < 8. Reference: fmt_dec_swar
 * (algo/decimal.c); dispatch: arch/decimal.c. Returns the number of values written. */
#if defined(__ARM_NEON) && defined(__aarch64__)
static uint8_t FMT_SHA[2][9 * 8][16] __attribute__((aligned(16))), FMT_ORA[2][9 * 8][16] __attribute__((aligned(16)));
__attribute__((constructor)) static void fmt_luts_neon_init(void) {
    for (int ng = 0; ng < 2; ng++) for (int D = 1; D <= 8; D++) for (int S = 0; S < 8 && S < D; S++) {
        uint8_t *m = FMT_SHA[ng][D * 8 + S], *d = FMT_ORA[ng][D * 8 + S]; memset(m, 0xFF, 16); memset(d, 0, 16);
        int o = ng; if (ng) d[0] = '-';
        for (int i = 0; i < D - S; i++) m[o++] = (uint8_t)(8 - D + i);
        if (S) { d[o] = '.'; o++; for (int i = 0; i < S; i++) m[o++] = (uint8_t)(8 - S + i); }
    }
}
#endif
#if defined(__ARM_NEON) && defined(__aarch64__)
static int fmt_slots_neon(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0;
    if (S < 8) {                                     /* 2 values per step: digits by 32/16-bit lane arithmetic, table lookup */
        for (; i + 2 <= n; i += 2) {
            int64_t x0 = v[i], x1 = v[i + 1];
            uint64_t a0 = x0 < 0 ? -(uint64_t)x0 : (uint64_t)x0, a1 = x1 < 0 ? -(uint64_t)x1 : (uint64_t)x1;
            if ((a0 | a1) > 99999999u && (a0 > 99999999u || a1 > 99999999u)) {
                for (int j = 0; j < 2; j++) { int d = ds[i + j]; uint8_t *q = o + (size_t)FMT_STRIDE * (i + j); lv[i + j] = (uint8_t)(fmt_dec_swar(q, v[i + j], S) - d - (d == S && d) - q); }
                continue;
            }
            uint32x2_t a32 = vcreate_u32(a0 | (a1 << 32));
            uint32x2_t hi = vmovn_u64(vshrq_n_u64(vmull_u32(a32, vdup_n_u32(0xD1B71759u)), 45));
            uint32x2_t lo = vmls_u32(a32, hi, vdup_n_u32(10000));
            uint32x4_t mg = vcombine_u32(vzip1_u32(hi, lo), vzip2_u32(hi, lo));      /* [hi0 lo0 hi1 lo1] */
            uint32x4_t top = vshrq_n_u32(vmulq_n_u32(mg, 10486), 20);
            uint32x4_t bot = vmlsq_n_u32(mg, top, 100);
            uint16x8_t hd = vreinterpretq_u16_u32(vorrq_u32(top, vshlq_n_u32(bot, 16)));
            uint16x8_t tn = vshrq_n_u16(vmulq_n_u16(hd, 103), 10);
            uint16x8_t un = vmlsq_n_u16(hd, tn, 10);
            uint8x16_t dig = vaddq_u8(vreinterpretq_u8_u16(vorrq_u16(tn, vshlq_n_u16(un, 8))), vdupq_n_u8('0'));
            uint64_t aa[2] = {a0, a1}; int64_t xx[2] = {x0, x1};
            for (int j = 0; j < 2; j++) {
                uint64_t a = aa[j];
                int D = 1 + (a > 9) + (a > 99) + (a > 999) + (a > 9999) + (a > 99999) + (a > 999999) + (a > 9999999);
                D = D < S + 1 ? S + 1 : D; int ng = xx[j] < 0;
                uint8x16_t dj = j ? vextq_u8(dig, dig, 8) : dig;
                uint8x16_t r = vorrq_u8(vqtbl1q_u8(dj, vld1q_u8(FMT_SHA[ng][D * 8 + S])), vld1q_u8(FMT_ORA[ng][D * 8 + S]));
                vst1q_u8(o + (size_t)FMT_STRIDE * (i + j), r);
                int d = ds[i + j];
                lv[i + j] = (uint8_t)(ng + D + (S > 0) - d - (d == S && d));
            }
        }
    }
    return i;
}
#endif
