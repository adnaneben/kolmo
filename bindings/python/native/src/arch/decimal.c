/* Dispatch of the decimal formatters (arch/x86_64/decimal_avx2.c, arch/aarch64/decimal_neon.c); the scalar loop is
 * the reference (fmt_dec_swar, algo/decimal.c). */
/* A run of n values of one numeric column at scale S, trailing zeros cut per value (ds), written back to back at o
 * with their lengths in lv. Same bytes as fmt_dec_swar. x86 with AVX2: 4 values per step for |v| < 1e8 and S < 8
 * (digits by vector multiplies, decimal point placed by a byte shuffle); other values and CPUs use fmt_dec_swar.
 * Needs 40 bytes of room past the end. */
static void fmt_slots(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0;
#if defined(KOLMO_USE_NEON) && defined(__aarch64__)
    i = fmt_slots_neon(o, v, ds, n, S, lv);
#endif
#if defined(KOLMO_FMT_AVX2)
    if (S < 8 && kolmo_have_avx2()) i = fmt_slots_avx2(o, v, ds, n, S, lv);
#endif
    for (; i < n; i++) { int d = ds[i]; uint8_t *q = o + (size_t)FMT_STRIDE * i; lv[i] = (uint8_t)(fmt_dec_swar(q, v[i], S) - d - (d == S && d) - q); }
}
static uint8_t *fmt_run(uint8_t *o, const int64_t *v, const uint8_t *ds, int n, int S, uint8_t *lv) {
    int i = 0;
#if defined(KOLMO_FMT_AVX2)
    if (S < 8 && kolmo_have_avx2()) i = fmt_run_avx2(&o, v, ds, n, S, lv);
#endif
    for (; i < n; i++) { int d = ds[i]; uint8_t *e = fmt_dec_swar(o, v[i], S) - d - (d == S && d); lv[i] = (uint8_t)(e - o); o = e; }
    return o;
}
