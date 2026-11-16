/* Fast forward path of the integer node (KG_INT): the predictor choice and the residual pass of kg_encode_num.
 *
 * Same decisions and same bytes as the generic code in kgraph.inc, which stays as the reference and the control
 * build (-DKOLMO_FORCE_GENERIC_INT_FWD). Ported from the research tree of PR #7: P2-E2 (fused, specialised
 * forward passes), P2-E9 (the three predictor orders evaluated in one sweep) and P2-E30 (the 16-bit audio case,
 * which is the W = 2 instance here). What changes:
 *   - loops are generated per element width and signedness, so the load is a plain move;
 *   - the choice evaluates orders 0, 1 and 2 in the same sweep (production: one sweep per order and pass);
 *   - the residual pass skips the division when the common factor is 1, and takes the largest residual from the
 *     gcd pass in that case (production: a separate pass with a division per value).
 * Arithmetic is the generic code's, operation for operation (unsigned wrap-around, the same casts, the same
 * integer bit counts summed into the same doubles, the same strict comparisons in the same order). */

#define KGF_LD(p, W, SG) ((W) == 8 ? kgf_ld8(p)                                                               \
                        : (W) == 4 ? ((SG) ? (uint64_t)(int64_t)(int32_t)kgf_ld4(p) : (uint64_t)kgf_ld4(p))   \
                                   : ((SG) ? (uint64_t)(int64_t)(int16_t)kgf_ld2(p) : (uint64_t)kgf_ld2(p)))
static inline uint64_t kgf_ld8(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline uint32_t kgf_ld4(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t kgf_ld2(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline unsigned kgf_bits(uint64_t z) { return z ? 65u - (unsigned)__builtin_clzll(z) : 1u; }

/* Costs of orders 0, 1 and 2 for one signedness, over the same sample as kg_num_choose_n. */
#define KGF_COSTS(W, SG)                                                                                          \
static void kgf_costs_##W##_##SG(const uint8_t *a, size_t cnt, size_t runs, size_t rl, uint64_t cost[3], size_t tot[3]) { \
    const size_t step = (cnt - rl) / (runs > 1 ? runs - 1 : 1);                                                   \
    int64_t mn = INT64_MAX; uint64_t g0 = 0, g1 = 0, g2 = 0;                                                      \
    for (size_t r = 0; r < runs; r++) {                       /* pass 0: minimum (order 0), gcd (orders 1, 2) */  \
        const uint8_t *b = a + step * r * (size_t)(W); uint64_t v1 = 0, v2 = 0;                                   \
        for (size_t q = 0; q < rl; q++) {                                                                         \
            uint64_t v = KGF_LD(b + q * (W), W, SG);                                                              \
            if ((int64_t)v < mn) mn = (int64_t)v;                                                                 \
            if (q >= 1 && g1 != 1) { int64_t d = (int64_t)(v - v1); g1 = kg_gcd(d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, g1); } \
            if (q >= 2 && g2 != 1) { int64_t d = (int64_t)(v - (2 * v1 - v2)); g2 = kg_gcd(d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, g2); } \
            v2 = v1; v1 = v;                                                                                      \
        }                                                                                                         \
    }                                                                                                             \
    for (size_t r = 0; r < runs && g0 != 1; r++) {            /* gcd of v - min (order 0) */                      \
        const uint8_t *b = a + step * r * (size_t)(W);                                                            \
        for (size_t q = 0; q < rl && g0 != 1; q++) g0 = kg_gcd(KGF_LD(b + q * (W), W, SG) - (uint64_t)mn, g0);   \
    }                                                                                                             \
    uint64_t c0 = 0, c1 = 0, c2 = 0; size_t t0 = 0, t1 = 0, t2 = 0;                                               \
    for (size_t r = 0; r < runs; r++) {                       /* pass 1: summed bit lengths of the residuals */   \
        const uint8_t *b = a + step * r * (size_t)(W); uint64_t v1 = 0, v2 = 0;                                   \
        for (size_t q = 0; q < rl; q++) {                                                                         \
            uint64_t v = KGF_LD(b + q * (W), W, SG);                                                              \
            uint64_t z0 = v - (uint64_t)mn; if (g0 > 1) z0 /= g0;                                                 \
            c0 += kgf_bits(z0); t0++;                                                                             \
            if (q >= 1) { int64_t d = (int64_t)(v - v1); c1 += kgf_bits(kg_zz(g1 > 1 ? d / (int64_t)g1 : d)); t1++; } \
            if (q >= 2) { int64_t d = (int64_t)(v - (2 * v1 - v2)); c2 += kgf_bits(kg_zz(g2 > 1 ? d / (int64_t)g2 : d)); t2++; } \
            v2 = v1; v1 = v;                                                                                      \
        }                                                                                                         \
    }                                                                                                             \
    cost[0] = c0; cost[1] = c1; cost[2] = c2; tot[0] = t0; tot[1] = t1; tot[2] = t2;                              \
}
KGF_COSTS(2, 0) KGF_COSTS(2, 1) KGF_COSTS(4, 0) KGF_COSTS(4, 1) KGF_COSTS(8, 1)

/* Same result as the generic kg_num_choose_n (same sample, same candidates in the same order, strict <). */
static double kgf_num_choose_n(const uint8_t *a, size_t cnt, int w, int *bo, int *bs, size_t nruns) {
    size_t runs = cnt > 16384 ? nruns : 1, rl = cnt > 16384 ? 1024 : cnt; double bcost = 1e300;
    for (int sg = (w == 8); sg <= 1; sg++) {
        uint64_t cost[3]; size_t tot[3];
        if (w == 2) { if (sg) kgf_costs_2_1(a, cnt, runs, rl, cost, tot); else kgf_costs_2_0(a, cnt, runs, rl, cost, tot); }
        else if (w == 4) { if (sg) kgf_costs_4_1(a, cnt, runs, rl, cost, tot); else kgf_costs_4_0(a, cnt, runs, rl, cost, tot); }
        else kgf_costs_8_1(a, cnt, runs, rl, cost, tot);
        for (int ord = 0; ord <= 2; ord++) {
            double c = (double)cost[ord];
            if (tot[ord] && c / tot[ord] < bcost) { bcost = c / tot[ord]; *bo = ord; *bs = sg; }
        }
    }
    return bcost;
}

/* Bias, gcd and largest residual of the chosen predictor over the whole block (the generic code's first three
 * passes). With a gcd of 1 the largest residual comes from the gcd pass and the third pass is skipped. */
#define KGF_SCAN(W, SG)                                                                                           \
static void kgf_scan_##W##_##SG(const uint8_t *a, size_t cnt, int bo, uint64_t *pbias, uint64_t *pg, uint64_t *pmax) { \
    uint64_t bg = 0, bbias = 0, bmax = 0, m1 = 0;                                                                 \
    if (bo == 0) {                                                                                                \
        int64_t mn = INT64_MAX;                                                                                   \
        for (size_t q = 0; q < cnt; q++) { int64_t v = (int64_t)KGF_LD(a + q * (W), W, SG); if (v < mn) mn = v; } \
        bbias = (uint64_t)mn;                                                                                     \
        for (size_t q = 0; q < cnt; q++) { uint64_t r = KGF_LD(a + q * (W), W, SG) - bbias; if (r > m1) m1 = r; if (bg != 1) bg = kg_gcd(r, bg); } \
    } else {                                                                                                      \
        uint64_t v1 = 0, v2 = 0;                                                                                  \
        for (size_t q = 0; q < cnt; q++) {                                                                        \
            uint64_t v = KGF_LD(a + q * (W), W, SG);                                                              \
            if ((int)q >= bo) {                                                                                   \
                int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2));                                          \
                uint64_t z = kg_zz(d); if (z > m1) m1 = z;                                                        \
                if (bg != 1) bg = kg_gcd(d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, bg);                    \
            }                                                                                                     \
            v2 = v1; v1 = v;                                                                                      \
        }                                                                                                         \
    }                                                                                                             \
    if (!bg) bg = 1;                                                                                              \
    if (bg == 1) bmax = m1;                                                                                       \
    else {                                                                                                        \
        uint64_t v1 = 0, v2 = 0;                                                                                  \
        for (size_t q = 0; q < cnt; q++) {                                                                        \
            uint64_t v = KGF_LD(a + q * (W), W, SG), z;                                                           \
            if (bo == 0) z = (v - bbias) / bg;                                                                    \
            else if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                  \
            else z = kg_zz((int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)) / (int64_t)bg);                            \
            if (z > bmax) bmax = z;                                                                               \
            v2 = v1; v1 = v;                                                                                      \
        }                                                                                                         \
    }                                                                                                             \
    *pbias = bbias; *pg = bg; *pmax = bmax;                                                                       \
}

/* Residual planes (the generic code's last pass): ow planes of m bytes, or one stream when ow == 1. */
#define KGF_WRITE(W, SG)                                                                                          \
static void kgf_write_##W##_##SG(const uint8_t *a, size_t cnt, int bo, uint64_t bbias, uint64_t bg, int ow, uint8_t *r) { \
    const size_t m = cnt - (size_t)bo; uint64_t v1 = 0, v2 = 0;                                                   \
    for (size_t q = 0; q < (size_t)bo && q < cnt; q++) { uint64_t v = KGF_LD(a + q * (W), W, SG); v2 = v1; v1 = v; } \
    for (size_t q = (size_t)bo; q < cnt; q++) {                                                                   \
        uint64_t v = KGF_LD(a + q * (W), W, SG), z;                                                               \
        if (bo == 0) z = bg == 1 ? v - bbias : (v - bbias) / bg;                                                  \
        else { int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)); z = kg_zz(bg == 1 ? d : d / (int64_t)bg); } \
        const size_t i = q - (size_t)bo;                                                                          \
        switch (ow) {                                                                                             \
        case 1: r[i] = (uint8_t)z; break;                                                                         \
        case 2: r[i] = (uint8_t)z; r[m + i] = (uint8_t)(z >> 8); break;                                           \
        case 4: r[i] = (uint8_t)z; r[m + i] = (uint8_t)(z >> 8); r[2 * m + i] = (uint8_t)(z >> 16); r[3 * m + i] = (uint8_t)(z >> 24); break; \
        default: for (int b = 0; b < 8; b++) r[(size_t)b * m + i] = (uint8_t)(z >> (8 * b));                     \
        }                                                                                                         \
        v2 = v1; v1 = v;                                                                                          \
    }                                                                                                             \
}
KGF_SCAN(2, 0) KGF_SCAN(2, 1) KGF_SCAN(4, 0) KGF_SCAN(4, 1) KGF_SCAN(8, 0) KGF_SCAN(8, 1)
KGF_WRITE(2, 0) KGF_WRITE(2, 1) KGF_WRITE(4, 0) KGF_WRITE(4, 1) KGF_WRITE(8, 0) KGF_WRITE(8, 1)

static void kgf_scan(const uint8_t *a, size_t cnt, int w, int bs, int bo, uint64_t *pbias, uint64_t *pg, uint64_t *pmax) {
    if (w == 2) (bs ? kgf_scan_2_1 : kgf_scan_2_0)(a, cnt, bo, pbias, pg, pmax);
    else if (w == 4) (bs ? kgf_scan_4_1 : kgf_scan_4_0)(a, cnt, bo, pbias, pg, pmax);
    else (bs ? kgf_scan_8_1 : kgf_scan_8_0)(a, cnt, bo, pbias, pg, pmax);
}
static void kgf_write(const uint8_t *a, size_t cnt, int w, int bs, int bo, uint64_t bbias, uint64_t bg, int ow, uint8_t *r) {
    if (w == 2) (bs ? kgf_write_2_1 : kgf_write_2_0)(a, cnt, bo, bbias, bg, ow, r);
    else if (w == 4) (bs ? kgf_write_4_1 : kgf_write_4_0)(a, cnt, bo, bbias, bg, ow, r);
    else (bs ? kgf_write_8_1 : kgf_write_8_0)(a, cnt, bo, bbias, bg, ow, r);
}
