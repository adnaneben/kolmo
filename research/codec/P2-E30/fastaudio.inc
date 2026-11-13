/* P2-E30 audio levers (research only; include after P2-E13's p2e13.c).
 * 1) fx_pass_2_1_o: an order-1/2, signed, 16-bit forward pass producing exactly what fi_pass_2_1 produces (checked
 *    by verify mode on corpora + random arrays), restructured into branch-free loops the compiler vectorises:
 *    zigzag deltas into a uint32 scratch with a running max, then the gcd on |d| with early exit, then the plane
 *    write. Any case it does not cover (gcd != 1, other widths/orders) falls back to fi_pass_2_1 itself.
 * 2) fx_rep_score: a near-free long-range repetition detector (hash 16-byte windows every 64 bytes of the first half;
 *    probe 32 windows of the second half at all 64 alignments); returns the share of probes found earlier. */
static int fx_pass_2_1_o(const uint8_t *a, size_t cnt, int bo, uint64_t *pbias, uint64_t *pg, int *pow, kbuf *res) {
    if (cnt < 3 || (bo != 1 && bo != 2)) return 1;
    size_t m = cnt - (size_t)bo; uint32_t *t = malloc(m * sizeof *t); if (!t) return -1;
    uint32_t zmax = 0; int16_t s0, s1; memcpy(&s0, a, 2); memcpy(&s1, a + 2, 2); int32_t v2 = s0, v1 = s1;
    if (bo == 1) { int32_t prev = s0;
        for (size_t i = 0; i < m; i++) { int16_t sv; memcpy(&sv, a + 2 * (i + 1), 2); int32_t v = sv, d = v - prev; prev = v;
            uint32_t z = ((uint32_t)d << 1) ^ (uint32_t)(d >> 31); t[i] = z; zmax = z > zmax ? z : zmax; } }
    else {
        for (size_t i = 0; i < m; i++) { int16_t sv; memcpy(&sv, a + 2 * (i + 2), 2); int32_t v = sv, d = v - (2 * v1 - v2); v2 = v1; v1 = v;
            uint32_t z = ((uint32_t)d << 1) ^ (uint32_t)(d >> 31); t[i] = z; zmax = z > zmax ? z : zmax; } }
    uint64_t g = 0;
    for (size_t i = 0; i < m && g != 1; i++) g = kg_gcd((t[i] + 1) >> 1, g);
    if (g > 1) { free(t); return 1; }                        /* rare: let the original pass handle gcd scaling */
    int ow = kg_bw(zmax);
    if (kb_res(res, m * (size_t)ow + 1)) { free(t); return -1; }
    uint8_t *r = res->p;
    if (ow == 1) for (size_t i = 0; i < m; i++) r[i] = (uint8_t)t[i];
    else if (ow == 2) { uint8_t *r1 = r + m; for (size_t i = 0; i < m; i++) { r[i] = (uint8_t)t[i]; r1[i] = (uint8_t)(t[i] >> 8); } }
    else { for (size_t i = 0; i < m; i++) { r[i] = (uint8_t)t[i]; r[m + i] = (uint8_t)(t[i] >> 8); r[2 * m + i] = (uint8_t)(t[i] >> 16); r[3 * m + i] = (uint8_t)(t[i] >> 24); } }
    res->n = m * (size_t)ow; *pbias = 0; *pg = g ? g : 1; *pow = ow; free(t); return 0;
}
/* single-pass variant: proves gcd == 1 on a prefix (a prefix gcd of 1 implies a total gcd of 1), then writes the two
 * byte planes directly; ow 1 keeps plane 0 only; ow 4 (|delta| > 32767) falls back to the scratch version above. */
static int fx_pass_2_1_o_1p(const uint8_t *a, size_t cnt, int bo, uint64_t *pbias, uint64_t *pg, int *pow, kbuf *res) {
    if (cnt < 3 || (bo != 1 && bo != 2)) return 1;
    size_t m = cnt - (size_t)bo; int16_t s0, s1; memcpy(&s0, a, 2); memcpy(&s1, a + 2, 2);
    uint64_t g = 0; { int32_t q2 = s0, q1 = s1, prev = s0;
        for (size_t i = 0; i < m && g != 1; i++) { int16_t sv; memcpy(&sv, a + 2 * (i + (size_t)bo), 2); int32_t v = sv;
            int32_t d = bo == 1 ? v - prev : v - (2 * q1 - q2); prev = v; q2 = q1; q1 = v; g = kg_gcd((uint64_t)(d < 0 ? -(int64_t)d : d), g); } }
    if (g != 1) return fx_pass_2_1_o(a, cnt, bo, pbias, pg, pow, res);   /* gcd 0 or > 1: scratch path decides */
    if (kb_res(res, m * 2 + 1)) return -1;
    uint8_t *r0 = res->p, *r1 = res->p + m; uint32_t zmax = 0;
    if (bo == 1) { int32_t prev = s0;
        for (size_t i = 0; i < m; i++) { int16_t sv; memcpy(&sv, a + 2 * (i + 1), 2); int32_t v = sv, d = v - prev; prev = v;
            uint32_t z = ((uint32_t)d << 1) ^ (uint32_t)(d >> 31); r0[i] = (uint8_t)z; r1[i] = (uint8_t)(z >> 8); zmax = z > zmax ? z : zmax; } }
    else { int32_t v2 = s0, v1 = s1;
        for (size_t i = 0; i < m; i++) { int16_t sv; memcpy(&sv, a + 2 * (i + 2), 2); int32_t v = sv, d = v - (2 * v1 - v2); v2 = v1; v1 = v;
            uint32_t z = ((uint32_t)d << 1) ^ (uint32_t)(d >> 31); r0[i] = (uint8_t)z; r1[i] = (uint8_t)(z >> 8); zmax = z > zmax ? z : zmax; } }
    if (zmax >= 65536) { res->n = 0; return fx_pass_2_1_o(a, cnt, bo, pbias, pg, pow, res); }
    int ow = kg_bw(zmax); res->n = m * (size_t)ow; *pbias = 0; *pg = 1; *pow = ow; return 0;
}
/* fi_forward_g with the fast pass substituted where it applies; identical output by construction + verify mode */
static int fx_forward_g(const uint8_t *src, size_t n, int w, size_t h, fiprog *P, fi_guard_fn gf, void *gctx) {
    memset(P, 0, sizeof *P);
    if (h > n || (n - h) / (size_t)w < 16 || (w != 2 && w != 4 && w != 8)) return -1;
    size_t cnt = (n - h) / (size_t)w; const uint8_t *a = src + h; int bo = 0, bs = 1, ow; uint64_t bias, g;
    double bits = fc_choose(a, cnt, w, &bo, &bs);
    if (gf && gf(gctx, bits * (double)cnt)) return 1;
    int rc = 1;
    if (w == 2 && bs == 1 && (bo == 1 || bo == 2)) rc = fx_pass_2_1_o_1p(a, cnt, bo, &bias, &g, &ow, &P->res);
    if (rc == 1) { P->res.n = 0;
        rc = w == 2 ? (bs ? fi_pass_2_1 : fi_pass_2_0)(a, cnt, bo, &bias, &g, &ow, &P->res)
           : w == 4 ? (bs ? fi_pass_4_1 : fi_pass_4_0)(a, cnt, bo, &bias, &g, &ow, &P->res)
                    : (bs ? fi_pass_8_1 : fi_pass_8_0)(a, cnt, bo, &bias, &g, &ow, &P->res); }
    if (rc) return -1;
    int pln = ow > 1; kbuf *par = &P->par;
    kb_byte(par, (uint8_t)w); kb_byte(par, (uint8_t)(bs | bo << 1 | pln << 3)); kb_byte(par, (uint8_t)ow);
    kb_uv(par, h); kb_uv(par, cnt); kb_uv(par, g); kb_uv(par, kg_zz((int64_t)bias));
    for (int q = 0; q < bo; q++) kb_uv(par, kg_zz((int64_t)kg_ld(a + (size_t)q * w, w, bs)));
    if (!par->p || kb_res(&P->rest, n - cnt * (size_t)w + 1)) return -1;
    kb_put(&P->rest, src, h); kb_put(&P->rest, a + cnt * (size_t)w, n - h - cnt * (size_t)w);
    P->cnt = cnt; P->m = cnt - (size_t)bo; P->w = w; P->bo = bo; P->bs = bs; P->ow = ow; P->pln = pln; P->g = g;
    return 0;
}
static double fx_rep_score(const uint8_t *b, size_t n) {
    enum { K = 16, PROBES = 32, HB = 16 };
    if (n < 8192) return 0.0;
    size_t half = n / 2, slots = (size_t)1 << HB, STRIDE = 64; while (half / STRIDE > slots / 2) STRIDE *= 2;   /* load <= 50% */
    uint64_t *tab = calloc(slots, sizeof *tab); if (!tab) return 0.0;
    for (size_t i = 0; i + K <= half; i += STRIDE) { uint64_t x = XXH64(b + i, K, 0) | 1; size_t s = (size_t)(x >> (64 - HB));
        while (tab[s] && tab[s] != x) s = (s + 1) & (slots - 1); tab[s] = x; }
    int hit = 0;
    for (int j = 0; j < PROBES; j++) {
        size_t q = half + ((size_t)j * (n - half - STRIDE - K)) / PROBES;
        for (size_t o = 0; o < STRIDE; o++) { uint64_t x = XXH64(b + q + o, K, 0) | 1; size_t s = (size_t)(x >> (64 - HB));
            while (tab[s] && tab[s] != x) s = (s + 1) & (slots - 1);
            if (tab[s] == x) { hit++; break; } }
    }
    free(tab); return (double)hit / PROBES;
}
