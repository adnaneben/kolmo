/* P2-E9: predictor choice with all three orders evaluated in the same sweep (research only). Same samples, same
 * per-order arithmetic (bias, gcd with its own early exit, zigzag, bit-length sums) and the same comparison order as
 * production kg_num_choose, so the decision AND the returned cost are identical; checked against production on every
 * input and by a randomized differential test (p2e9.c). Requires fastint.inc (FI_LD). */
#define FC_SG(W, SG)                                                                                                  \
static void fc_##W##_##SG(const uint8_t *a, size_t cnt, size_t runs, size_t rl, double *bcost, int *bo, int *bs) {    \
    uint64_t g0 = 0, g1 = 0, g2 = 0, c0 = 0, c1 = 0, c2 = 0, t0 = 0, t1 = 0, t2 = 0; int64_t mn = INT64_MAX;          \
    const size_t step = (cnt - rl) / (runs > 1 ? runs - 1 : 1);                                                       \
    for (size_t r = 0; r < runs; r++) {                         /* sweep 1: min (order 0), gcds (orders 1, 2) */       \
        const uint8_t *b = a + step * r * (size_t)W; uint64_t v1 = 0, v2 = 0;                                         \
        for (size_t q = 0; q < rl; q++) {                                                                             \
            uint64_t v = FI_LD(b + q * W, W, SG); if ((int64_t)v < mn) mn = (int64_t)v;                               \
            if (q >= 1) { int64_t d = (int64_t)(v - v1); uint64_t r0 = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; if (g1 != 1) g1 = kg_gcd(r0, g1); } \
            if (q >= 2) { int64_t d = (int64_t)(v - (2 * v1 - v2)); uint64_t r0 = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; if (g2 != 1) g2 = kg_gcd(r0, g2); } \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    for (size_t r2 = 0; r2 < runs && g0 != 1; r2++) {            /* order-0 gcd of v - min, production's order */      \
        const uint8_t *b2 = a + step * r2 * (size_t)W;                                                                \
        for (size_t q = 0; q < rl && g0 != 1; q++) g0 = kg_gcd(FI_LD(b2 + q * W, W, SG) - (uint64_t)mn, g0);           \
    }                                                                                                                 \
    uint64_t G0 = g0 ? g0 : 1, G1 = g1 ? g1 : 1, G2 = g2 ? g2 : 1;                                                    \
    for (size_t r = 0; r < runs; r++) {                         /* sweep 2: bit-length costs for all three orders */   \
        const uint8_t *b = a + step * r * (size_t)W; uint64_t v1 = 0, v2 = 0;                                         \
        for (size_t q = 0; q < rl; q++) {                                                                             \
            uint64_t v = FI_LD(b + q * W, W, SG), z;                                                                  \
            z = G0 == 1 ? v - (uint64_t)mn : (v - (uint64_t)mn) / G0; c0 += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; t0++; \
            if (q >= 1) { int64_t d = (int64_t)(v - v1); z = kg_zz(G1 == 1 ? d : d / (int64_t)G1); c1 += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; t1++; } \
            if (q >= 2) { int64_t d = (int64_t)(v - (2 * v1 - v2)); z = kg_zz(G2 == 1 ? d : d / (int64_t)G2); c2 += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; t2++; } \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    if (t0 && (double)c0 / (double)t0 < *bcost) { *bcost = (double)c0 / (double)t0; *bo = 0; *bs = SG; }              \
    if (t1 && (double)c1 / (double)t1 < *bcost) { *bcost = (double)c1 / (double)t1; *bo = 1; *bs = SG; }              \
    if (t2 && (double)c2 / (double)t2 < *bcost) { *bcost = (double)c2 / (double)t2; *bo = 2; *bs = SG; }              \
}
FC_SG(2, 0) FC_SG(2, 1) FC_SG(4, 0) FC_SG(4, 1) FC_SG(8, 1)
static double fc_choose(const uint8_t *a, size_t cnt, int w, int *bo, int *bs) {
    size_t runs = cnt > 16384 ? 16 : 1, rl = cnt > 16384 ? 1024 : cnt; double bcost = 1e300;
    if (w == 2) { fc_2_0(a, cnt, runs, rl, &bcost, bo, bs); fc_2_1(a, cnt, runs, rl, &bcost, bo, bs); }
    else if (w == 4) { fc_4_0(a, cnt, runs, rl, &bcost, bo, bs); fc_4_1(a, cnt, runs, rl, &bcost, bo, bs); }
    else fc_8_1(a, cnt, runs, rl, &bcost, bo, bs);
    return bcost;
}
