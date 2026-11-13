/* P2-E2 fast type-first integer path (research only; nothing here is reachable from production).
 *
 * fi_forward: given the declared element width w and header length h, produces the KG_INT parameter bytes, the
 *   residual stream and the rest stream that production kg_encode_num would put in its program, byte for byte
 *   (checked against a production capture on every input by p2e2.c). The predictor/sign choice is production's
 *   kg_num_choose, unchanged. The passes are fused and specialised: no per-value division when the gcd is 1, the
 *   largest residual found in the gcd pass whenever the gcd turns out to be 1.
 * fi_inverse: the inverse of a KG_INT program, output identical to production kg_int_inv (also checked), with loops
 *   specialised by element width, output width, plane layout and order; anything else falls back to kg_int_inv.
 * Requires a little-endian host (checked at run time by p2e2.c). */

typedef struct { kbuf par, res, rest; size_t cnt, m; int w, bo, bs, ow, pln; uint64_t g; } fiprog;
static void fi_free(fiprog *P) { free(P->par.p); free(P->res.p); free(P->rest.p); memset(P, 0, sizeof *P); }

#define FI_LD(p, W, SG) ((W) == 8 ? fi_ld8(p) : (W) == 4 ? ((SG) ? (uint64_t)(int64_t)(int32_t)fi_ld4(p) : (uint64_t)fi_ld4(p)) \
                                              : ((SG) ? (uint64_t)(int64_t)(int16_t)fi_ld2(p) : (uint64_t)fi_ld2(p)))
static inline uint64_t fi_ld8(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline uint32_t fi_ld4(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t fi_ld2(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }


/* fi_choose: the same decision as production kg_num_choose (same samples, same order of evaluation, same costs; the
 * summed bit lengths are integers, so an integer accumulator gives identical comparisons), specialised by width and
 * sign, without a division when the sample gcd is 1. p2e2.c checks (order, sign) against kg_num_choose on every input. */
#define FI_CHOOSE_SG(W, SG)                                                                                           \
static void fi_choose_##W##_##SG(const uint8_t *a, size_t cnt, size_t runs, size_t rl, double *bcost, int *bo, int *bs) { \
    for (int ord = 0; ord <= 2; ord++) {                                                                              \
        uint64_t g = 0, cost = 0, tot = 0; int64_t mn = INT64_MAX;                                                    \
        for (int pass = 0; pass < 2; pass++) for (size_t r = 0; r < runs; r++) {                                      \
            const uint8_t *b = a + (cnt - rl) / (runs > 1 ? runs - 1 : 1) * r * (size_t)W; uint64_t v1 = 0, v2 = 0;   \
            if (!pass) {                                                                                              \
                if (ord == 0) for (size_t q = 0; q < rl; q++) { int64_t v = (int64_t)FI_LD(b + q * W, W, SG); if (v < mn) mn = v; } \
                else for (size_t q = 0; q < rl; q++) {                                                                \
                    uint64_t v = FI_LD(b + q * W, W, SG);                                                             \
                    if ((int)q >= ord) { int64_t d = (int64_t)(v - (ord == 1 ? v1 : 2 * v1 - v2));                    \
                        uint64_t r0 = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; if (g != 1) g = kg_gcd(r0, g); } \
                    v2 = v1; v1 = v;                                                                                  \
                }                                                                                                     \
                if (ord == 0 && r + 1 == runs) for (size_t r2 = 0; r2 < runs; r2++) {                                 \
                    const uint8_t *b2 = a + (cnt - rl) / (runs > 1 ? runs - 1 : 1) * r2 * (size_t)W;                  \
                    for (size_t q = 0; q < rl && g != 1; q++) g = kg_gcd(FI_LD(b2 + q * W, W, SG) - (uint64_t)mn, g); \
                }                                                                                                     \
                continue;                                                                                             \
            }                                                                                                         \
            uint64_t gg = g ? g : 1;                                                                                  \
            for (size_t q = 0; q < rl; q++) {                                                                         \
                uint64_t v = FI_LD(b + q * W, W, SG), z;                                                              \
                if (ord == 0) z = gg == 1 ? v - (uint64_t)mn : (v - (uint64_t)mn) / gg;                               \
                else if ((int)q < ord) { v2 = v1; v1 = v; continue; }                                                 \
                else { int64_t d = (int64_t)(v - (ord == 1 ? v1 : 2 * v1 - v2)); z = kg_zz(gg == 1 ? d : d / (int64_t)gg); } \
                cost += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; tot++;                                           \
                v2 = v1; v1 = v;                                                                                      \
            }                                                                                                         \
        }                                                                                                             \
        if (tot && (double)cost / (double)tot < *bcost) { *bcost = (double)cost / (double)tot; *bo = ord; *bs = SG; } \
    }                                                                                                                 \
}
FI_CHOOSE_SG(2, 0) FI_CHOOSE_SG(2, 1) FI_CHOOSE_SG(4, 0) FI_CHOOSE_SG(4, 1) FI_CHOOSE_SG(8, 1)
static double fi_choose(const uint8_t *a, size_t cnt, int w, int *bo, int *bs) {
    size_t runs = cnt > 16384 ? 16 : 1, rl = cnt > 16384 ? 1024 : cnt; double bcost = 1e300;
    if (w == 2) { fi_choose_2_0(a, cnt, runs, rl, &bcost, bo, bs); fi_choose_2_1(a, cnt, runs, rl, &bcost, bo, bs); }
    else if (w == 4) { fi_choose_4_0(a, cnt, runs, rl, &bcost, bo, bs); fi_choose_4_1(a, cnt, runs, rl, &bcost, bo, bs); }
    else fi_choose_8_1(a, cnt, runs, rl, &bcost, bo, bs);
    return bcost;
}

/* passes for one (W, SG): bias/gcd/max, then residual write. Mirrors kg_encode_num exactly. */
#define FI_PASSES(W, SG)                                                                                              \
static int fi_pass_##W##_##SG(const uint8_t *a, size_t cnt, int bo, uint64_t *pbias, uint64_t *pg, int *pow, kbuf *res) { \
    uint64_t bg = 0, bbias = 0, bmax = 0, zmax = 0;                                                                   \
    if (bo == 0) {                                                                                                    \
        int64_t mn = INT64_MAX;                                                                                       \
        for (size_t q = 0; q < cnt; q++) { int64_t v = (int64_t)FI_LD(a + q * W, W, SG); if (v < mn) mn = v; }        \
        bbias = (uint64_t)mn;                                                                                         \
        for (size_t q = 0; q < cnt; q++) {                                                                            \
            uint64_t r = FI_LD(a + q * W, W, SG) - bbias; if (r > zmax) zmax = r;                                     \
            if (bg != 1) bg = kg_gcd(r, bg);                                                                          \
        }                                                                                                             \
    } else {                                                                                                          \
        uint64_t v1 = 0, v2 = 0;                                                                                      \
        for (size_t q = 0; q < cnt; q++) {                                                                            \
            uint64_t v = FI_LD(a + q * W, W, SG);                                                                     \
            if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                           \
            int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2));                                                  \
            uint64_t r = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, z = kg_zz(d); if (z > zmax) zmax = z;       \
            if (bg != 1) bg = kg_gcd(r, bg);                                                                          \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    if (!bg) bg = 1;                                                                                                  \
    if (bg == 1) bmax = zmax;                                                                                         \
    else {                                                                                                            \
        uint64_t v1 = 0, v2 = 0;                                                                                      \
        for (size_t q = 0; q < cnt; q++) {                                                                            \
            uint64_t v = FI_LD(a + q * W, W, SG), z;                                                                  \
            if (bo == 0) z = (v - bbias) / bg;                                                                        \
            else if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                      \
            else z = kg_zz((int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)) / (int64_t)bg);                                \
            if (z > bmax) bmax = z;                                                                                   \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    int ow = kg_bw(bmax), pln = ow > 1; size_t m = cnt - (size_t)bo;                                                  \
    if (kb_res(res, m * (size_t)ow + 1)) return -1;                                                                   \
    uint8_t *r = res->p; uint64_t v1 = 0, v2 = 0;                                                                     \
    for (size_t q = 0; q < cnt; q++) {                                                                                \
        uint64_t v = FI_LD(a + q * W, W, SG), z;                                                                      \
        if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                               \
        if (bo == 0) z = bg == 1 ? v - bbias : (v - bbias) / bg;                                                      \
        else { int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)); z = kg_zz(bg == 1 ? d : d / (int64_t)bg); }   \
        size_t i = q - (size_t)bo;                                                                                    \
        if (!pln) r[i] = (uint8_t)z;                                                                                  \
        else if (ow == 2) { r[i] = (uint8_t)z; r[m + i] = (uint8_t)(z >> 8); }                                        \
        else if (ow == 4) { r[i] = (uint8_t)z; r[m + i] = (uint8_t)(z >> 8); r[2 * m + i] = (uint8_t)(z >> 16); r[3 * m + i] = (uint8_t)(z >> 24); } \
        else for (int b = 0; b < ow; b++) r[(size_t)b * m + i] = (uint8_t)(z >> (8 * b));                             \
        v2 = v1; v1 = v;                                                                                              \
    }                                                                                                                 \
    res->n = m * (size_t)ow; *pbias = bbias; *pg = bg; *pow = ow; return 0;                                           \
}
FI_PASSES(2, 0) FI_PASSES(2, 1) FI_PASSES(4, 0) FI_PASSES(4, 1) FI_PASSES(8, 0) FI_PASSES(8, 1)

/* pre-guard hook: when set, called after the predictor choice with the estimated residual bits; nonzero aborts */
typedef int (*fi_guard_fn)(void *ctx, double est_bits);
static int fi_forward_g(const uint8_t *src, size_t n, int w, size_t h, fiprog *P, fi_guard_fn gf, void *gctx);
static int fi_forward(const uint8_t *src, size_t n, int w, size_t h, fiprog *P) { return fi_forward_g(src, n, w, h, P, NULL, NULL); }
static int fi_forward_g(const uint8_t *src, size_t n, int w, size_t h, fiprog *P, fi_guard_fn gf, void *gctx) {
    memset(P, 0, sizeof *P);
    if (h > n || (n - h) / (size_t)w < 16 || (w != 2 && w != 4 && w != 8)) return -1;
    size_t cnt = (n - h) / (size_t)w; const uint8_t *a = src + h; int bo = 0, bs = 1, ow; uint64_t bias, g;
    double bits = fi_choose(a, cnt, w, &bo, &bs);
    if (gf && gf(gctx, bits * (double)cnt)) return 1;                       /* guard: not worth a typed graph */
    int rc = w == 2 ? (bs ? fi_pass_2_1 : fi_pass_2_0)(a, cnt, bo, &bias, &g, &ow, &P->res)
           : w == 4 ? (bs ? fi_pass_4_1 : fi_pass_4_0)(a, cnt, bo, &bias, &g, &ow, &P->res)
                    : (bs ? fi_pass_8_1 : fi_pass_8_0)(a, cnt, bo, &bias, &g, &ow, &P->res);
    if (rc) return -1;
    int pln = ow > 1;
    kbuf *par = &P->par;
    kb_byte(par, (uint8_t)w); kb_byte(par, (uint8_t)(bs | bo << 1 | pln << 3)); kb_byte(par, (uint8_t)ow);
    kb_uv(par, h); kb_uv(par, cnt); kb_uv(par, g); kb_uv(par, kg_zz((int64_t)bias));
    for (int q = 0; q < bo; q++) kb_uv(par, kg_zz((int64_t)kg_ld(a + (size_t)q * w, w, bs)));
    if (!par->p || kb_res(&P->rest, n - cnt * (size_t)w + 1)) return -1;
    kb_put(&P->rest, src, h); kb_put(&P->rest, a + cnt * (size_t)w, n - h - cnt * (size_t)w);
    P->cnt = cnt; P->m = cnt - (size_t)bo; P->w = w; P->bo = bo; P->bs = bs; P->ow = ow; P->pln = pln; P->g = g;
    return 0;
}

/* ---- inverse ---- */
#define FI_ST(o, v, W) do { if ((W) == 8) { uint64_t t_ = (v); memcpy(o, &t_, 8); } else if ((W) == 4) { uint32_t t_ = (uint32_t)(v); memcpy(o, &t_, 4); } \
                            else { uint16_t t_ = (uint16_t)(v); memcpy(o, &t_, 2); } } while (0)
#define FI_FIX(v, W, SG) ((W) == 8 ? (v) : (SG) ? (uint64_t)(int64_t)((W) == 4 ? (int64_t)(int32_t)(uint32_t)(v) : (int64_t)(int16_t)(uint16_t)(v)) \
                                              : ((v) & (((uint64_t)1 << (8 * (W))) - 1)))
#define FI_Z(r, i, m, OW) ((OW) == 1 ? (uint64_t)(r)[i] : (OW) == 2 ? (uint64_t)(r)[i] | (uint64_t)(r)[(m) + (i)] << 8 \
                         : (OW) == 4 ? (uint64_t)(r)[i] | (uint64_t)(r)[(m) + (i)] << 8 | (uint64_t)(r)[2 * (m) + (i)] << 16 | (uint64_t)(r)[3 * (m) + (i)] << 24 \
                         : fi_zpl(r, i, m, OW))
static inline uint64_t fi_zpl(const uint8_t *r, size_t i, size_t m, int ow) { uint64_t z = 0; for (int b = 0; b < ow; b++) z |= (uint64_t)r[(size_t)b * m + i] << (8 * b); return z; }
#define FI_INV(W, OW, SG)                                                                                             \
static void fi_inv_##W##_##OW##_##SG(const uint8_t *r, size_t m, int ord, uint64_t g, uint64_t bias, uint64_t v1, uint64_t v2, uint8_t *o) { \
    if (ord == 0) { if (g == 1) for (size_t i = 0; i < m; i++, o += W) { uint64_t v = bias + FI_Z(r, i, m, OW); FI_ST(o, FI_FIX(v, W, SG), W); } \
                    else for (size_t i = 0; i < m; i++, o += W) { uint64_t v = bias + FI_Z(r, i, m, OW) * g; FI_ST(o, FI_FIX(v, W, SG), W); } } \
    else if (ord == 1) { for (size_t i = 0; i < m; i++, o += W) { uint64_t d = (uint64_t)kg_unzz(FI_Z(r, i, m, OW)); if (g != 1) d *= g; \
                                                                   uint64_t v = FI_FIX(v1 + d, W, SG); FI_ST(o, v, W); v1 = v; } } \
    else { for (size_t i = 0; i < m; i++, o += W) { uint64_t d = (uint64_t)kg_unzz(FI_Z(r, i, m, OW)); if (g != 1) d *= g;      \
                                                     uint64_t v = FI_FIX(2 * v1 - v2 + d, W, SG); FI_ST(o, v, W); v2 = v1; v1 = v; } } \
}
#define FI_INV_W(W) FI_INV(W, 1, 0) FI_INV(W, 1, 1) FI_INV(W, 2, 0) FI_INV(W, 2, 1) FI_INV(W, 4, 0) FI_INV(W, 4, 1) FI_INV(W, 8, 0) FI_INV(W, 8, 1)
FI_INV_W(2) FI_INV_W(4) FI_INV_W(8)
typedef void (*fi_inv_fn)(const uint8_t *, size_t, int, uint64_t, uint64_t, uint64_t, uint64_t, uint8_t *);
static fi_inv_fn fi_pick(int w, int ow, int sg) {
    int oi = ow == 1 ? 0 : ow == 2 ? 1 : ow == 4 ? 2 : 3;
    static const fi_inv_fn T2[4][2] = {{fi_inv_2_1_0, fi_inv_2_1_1}, {fi_inv_2_2_0, fi_inv_2_2_1}, {fi_inv_2_4_0, fi_inv_2_4_1}, {fi_inv_2_8_0, fi_inv_2_8_1}};
    static const fi_inv_fn T4[4][2] = {{fi_inv_4_1_0, fi_inv_4_1_1}, {fi_inv_4_2_0, fi_inv_4_2_1}, {fi_inv_4_4_0, fi_inv_4_4_1}, {fi_inv_4_8_0, fi_inv_4_8_1}};
    static const fi_inv_fn T8[4][2] = {{fi_inv_8_1_0, fi_inv_8_1_1}, {fi_inv_8_2_0, fi_inv_8_2_1}, {fi_inv_8_4_0, fi_inv_8_4_1}, {fi_inv_8_8_0, fi_inv_8_8_1}};
    return w == 2 ? T2[oi][sg] : w == 4 ? T4[oi][sg] : w == 8 ? T8[oi][sg] : NULL;
}
/* same contract and validation as kg_int_inv; out must have room for cnt*w + rest */
static int fi_inverse(const uint8_t *par, size_t plen, const kbuf *res, const kbuf *rest, size_t maxs, kbuf *out) {
    size_t pi = 3; uint64_t h, cnt, g, bz, sd[2] = {0, 0};
    if (plen < 3) return -1;
    int w = par[0], fl = par[1], ow = par[2], ord = (fl >> 1) & 3, sg = fl & 1, pln = (fl >> 3) & 1;
    if ((w != 1 && w != 2 && w != 4 && w != 8) || (ow != 1 && ow != 2 && ow != 4 && ow != 8) || ord > 2 || (fl & ~15)) return -1;
    if (kg_getuv(par, plen, &pi, &h) || kg_getuv(par, plen, &pi, &cnt) || kg_getuv(par, plen, &pi, &g) || kg_getuv(par, plen, &pi, &bz) || g == 0) return -1;
    for (int q = 0; q < ord; q++) if (kg_getuv(par, plen, &pi, &sd[q])) return -1;
    if (pi != plen || h > rest->n || (uint64_t)ord > cnt || cnt > maxs / (unsigned)w || (cnt - (uint64_t)ord) * (unsigned)ow != res->n
        || cnt * (unsigned)w + rest->n > maxs) return -1;
    fi_inv_fn f = (pln || ow == 1) ? fi_pick(w, ow, sg) : NULL;
    if (!f) return kg_int_inv(par, plen, res, rest, maxs, out);            /* interleaved multi-byte or w = 1 */
    if (kb_res(out, (size_t)(cnt * w) + rest->n + 16)) return -1;
    if (h) memcpy(out->p, rest->p, (size_t)h);
    uint8_t *wp = out->p + h; uint64_t v1 = 0, v2 = 0, bias = (uint64_t)kg_unzz(bz); size_t m = (size_t)(cnt - (uint64_t)ord);
    for (int q = 0; q < ord; q++) {                                         /* seeds, through the same fix-up */
        uint64_t v = (uint64_t)kg_unzz(sd[q]);
        if (sg && w < 8) { int sh = 64 - 8 * w; v = (uint64_t)((int64_t)(v << sh) >> sh); } else if (w < 8) v &= ((uint64_t)1 << (8 * w)) - 1;
        for (int b = 0; b < w; b++) *wp++ = (uint8_t)(v >> (8 * b));
        v2 = v1; v1 = v;
    }
    f(res->p, m, ord, g, bias, v1, v2, wp); wp += m * (size_t)w;
    memcpy(wp, rest->p + h, rest->n - (size_t)h); out->n = (size_t)(cnt * w) + rest->n;
    return 0;
}
