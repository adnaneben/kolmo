/* Byte-plane shuffle of fixed-width arrays (T_SHUF), with optional delta. */
static inline uint64_t ldw(const uint8_t *p, int w) {
    switch (w) { case 1: return p[0]; case 2: { uint16_t v; memcpy(&v, p, 2); return v; }
                 case 4: { uint32_t v; memcpy(&v, p, 4); return v; } default: { uint64_t v; memcpy(&v, p, 8); return v; } }
}
static inline void stw(uint8_t *p, uint64_t v, int w) {
    switch (w) { case 1: p[0] = (uint8_t)v; break; case 2: { uint16_t x = (uint16_t)v; memcpy(p, &x, 2); break; }
                 case 4: { uint32_t x = (uint32_t)v; memcpy(p, &x, 4); break; } default: memcpy(p, &v, 8); }
}
#define SB 4096
static void shuffle_fwd(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out) {
#ifdef KOLMO_FRONTIER
    if (w == 2 && frontier_shuffle16(in, (size_t)rows, (unsigned)c, delta, out, 0)) return;
#endif
#if KOLMO_NEON
#if KOLMO_NEON
    if (!kolmo_force_c && kolmo_shuffle_fwd_neon(in, rows, w, c, delta, out)) return;
#endif
#else
    if (c == 1) { KOLMO_K->planes_split(in, (size_t)rows, (unsigned)w, delta, out); return; }   /* algo/planes.c, SIMD in src/arch */
#endif
    uint64_t prev[64] = {0}, tmp[SB]; uint64_t mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r0 = 0; r0 < rows; r0 += SB) {
        int64_t nb = rows - r0 < SB ? rows - r0 : SB;
        for (int ch = 0; ch < c; ch++) {
            uint64_t pv = prev[ch]; const uint8_t *p = in + (r0 * c + ch) * w;
            for (int64_t r = 0; r < nb; r++, p += (int64_t)c * w) { uint64_t v = ldw(p, w); tmp[r] = delta ? ((v - pv) & mask) : v; pv = v; }
            prev[ch] = pv;
            for (int b = 0; b < w; b++) { uint8_t *o = out + ((int64_t)b * c + ch) * rows + r0; for (int64_t r = 0; r < nb; r++) o[r] = (uint8_t)(tmp[r] >> (8 * b)); }
        }
    }
}
static void shuffle_inv(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out) {
#ifdef KOLMO_FRONTIER
    if (w == 2 && frontier_shuffle16(in, (size_t)rows, (unsigned)c, delta, out, 1)) return;
#endif
#if KOLMO_NEON
#if KOLMO_NEON
    if (!kolmo_force_c && kolmo_shuffle_inv_neon(in, rows, w, c, delta, out)) return;
#endif
#else
    if (c == 1) { KOLMO_K->planes_merge(in, (size_t)rows, (unsigned)w, delta, out); return; }
#endif
    uint64_t prev[64] = {0}, tmp[SB]; uint64_t mask = w == 8 ? ~0ull : ((1ull << (8 * w)) - 1);
    for (int64_t r0 = 0; r0 < rows; r0 += SB) {
        int64_t nb = rows - r0 < SB ? rows - r0 : SB;
        for (int ch = 0; ch < c; ch++) {
            memset(tmp, 0, nb * 8);
            for (int b = 0; b < w; b++) { const uint8_t *q = in + ((int64_t)b * c + ch) * rows + r0; for (int64_t r = 0; r < nb; r++) tmp[r] |= (uint64_t)q[r] << (8 * b); }
            uint64_t pv = prev[ch]; uint8_t *p = out + (r0 * c + ch) * w;
            for (int64_t r = 0; r < nb; r++, p += (int64_t)c * w) { uint64_t v = delta ? ((pv + tmp[r]) & mask) : tmp[r]; stw(p, v, w); pv = v; }
            prev[ch] = pv;
        }
    }
}
