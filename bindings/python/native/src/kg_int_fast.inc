/* Optimized KG_INT inverse, ported from the tested P2-E2 inverse only.
 * Wire semantics and encoder are unchanged. The generic inverse remains the fallback
 * for unsupported layouts and big-endian hosts. All length checks precede writes.
 * KOLMO_FORCE_GENERIC_INT_INV retains an exact same-build benchmark control.
 * Apache-2.0, as the containing project. */
#define KGI_ST(o, v, W) do { if ((W) == 8) { uint64_t t_ = (v); memcpy(o, &t_, 8); } else if ((W) == 4) { uint32_t t_ = (uint32_t)(v); memcpy(o, &t_, 4); } \
                            else { uint16_t t_ = (uint16_t)(v); memcpy(o, &t_, 2); } } while (0)
#define KGI_FIX(v, W, SG) ((W) == 8 ? (v) : (SG) ? (uint64_t)(int64_t)((W) == 4 ? (int64_t)(int32_t)(uint32_t)(v) : (int64_t)(int16_t)(uint16_t)(v)) \
                                              : ((v) & (((uint64_t)1 << (8 * (W))) - 1)))
#define KGI_Z(r, i, m, OW) ((OW) == 1 ? (uint64_t)(r)[i] : (OW) == 2 ? (uint64_t)(r)[i] | (uint64_t)(r)[(m) + (i)] << 8 \
                         : (OW) == 4 ? (uint64_t)(r)[i] | (uint64_t)(r)[(m) + (i)] << 8 | (uint64_t)(r)[2 * (m) + (i)] << 16 | (uint64_t)(r)[3 * (m) + (i)] << 24 \
                         : kgi_zpl(r, i, m, OW))
static inline uint64_t kgi_zpl(const uint8_t *r, size_t i, size_t m, int ow) { uint64_t z = 0; for (int b = 0; b < ow; b++) z |= (uint64_t)r[(size_t)b * m + i] << (8 * b); return z; }
#define KGI_INV(W, OW, SG)                                                                                             \
static void kgi_inv_##W##_##OW##_##SG(const uint8_t *r, size_t m, int ord, uint64_t g, uint64_t bias, uint64_t v1, uint64_t v2, uint8_t *o) { \
    if (ord == 0) { if (g == 1) for (size_t i = 0; i < m; i++, o += W) { uint64_t v = bias + KGI_Z(r, i, m, OW); KGI_ST(o, KGI_FIX(v, W, SG), W); } \
                    else for (size_t i = 0; i < m; i++, o += W) { uint64_t v = bias + KGI_Z(r, i, m, OW) * g; KGI_ST(o, KGI_FIX(v, W, SG), W); } } \
    else if (ord == 1) { for (size_t i = 0; i < m; i++, o += W) { uint64_t d = (uint64_t)kg_unzz(KGI_Z(r, i, m, OW)); if (g != 1) d *= g; \
                                                                   uint64_t v = KGI_FIX(v1 + d, W, SG); KGI_ST(o, v, W); v1 = v; } } \
    else { for (size_t i = 0; i < m; i++, o += W) { uint64_t d = (uint64_t)kg_unzz(KGI_Z(r, i, m, OW)); if (g != 1) d *= g;      \
                                                     uint64_t v = KGI_FIX(2 * v1 - v2 + d, W, SG); KGI_ST(o, v, W); v2 = v1; v1 = v; } } \
}
#define KGI_INV_W(W) KGI_INV(W, 1, 0) KGI_INV(W, 1, 1) KGI_INV(W, 2, 0) KGI_INV(W, 2, 1) KGI_INV(W, 4, 0) KGI_INV(W, 4, 1) KGI_INV(W, 8, 0) KGI_INV(W, 8, 1)
KGI_INV_W(2) KGI_INV_W(4) KGI_INV_W(8)
typedef void (*kgi_inv_fn)(const uint8_t *, size_t, int, uint64_t, uint64_t, uint64_t, uint64_t, uint8_t *);
static kgi_inv_fn kgi_pick(int w, int ow, int sg) {
    int oi = ow == 1 ? 0 : ow == 2 ? 1 : ow == 4 ? 2 : 3;
    static const kgi_inv_fn T2[4][2] = {{kgi_inv_2_1_0, kgi_inv_2_1_1}, {kgi_inv_2_2_0, kgi_inv_2_2_1}, {kgi_inv_2_4_0, kgi_inv_2_4_1}, {kgi_inv_2_8_0, kgi_inv_2_8_1}};
    static const kgi_inv_fn T4[4][2] = {{kgi_inv_4_1_0, kgi_inv_4_1_1}, {kgi_inv_4_2_0, kgi_inv_4_2_1}, {kgi_inv_4_4_0, kgi_inv_4_4_1}, {kgi_inv_4_8_0, kgi_inv_4_8_1}};
    static const kgi_inv_fn T8[4][2] = {{kgi_inv_8_1_0, kgi_inv_8_1_1}, {kgi_inv_8_2_0, kgi_inv_8_2_1}, {kgi_inv_8_4_0, kgi_inv_8_4_1}, {kgi_inv_8_8_0, kgi_inv_8_8_1}};
    return w == 2 ? T2[oi][sg] : w == 4 ? T4[oi][sg] : w == 8 ? T8[oi][sg] : NULL;
}
/* Byte-identical inverse specialization; reject overflow before allocation. */
static int kgi_inverse(const uint8_t *par, size_t plen, const kbuf *res, const kbuf *rest, size_t maxs, kbuf *out) {
    const uint16_t one = 1;
#if defined(KOLMO_FORCE_GENERIC_INT_INV)
    (void)one;
    return kg_int_inv(par, plen, res, rest, maxs, out);
#else
    if (*(const uint8_t *)&one != 1) return kg_int_inv(par, plen, res, rest, maxs, out);
#endif
    size_t pi = 3; uint64_t h, cnt, g, bz, sd[2] = {0, 0};
    if (plen < 3) return -1;
    int w = par[0], fl = par[1], ow = par[2], ord = (fl >> 1) & 3, sg = fl & 1, pln = (fl >> 3) & 1;
    if ((w != 1 && w != 2 && w != 4 && w != 8) || (ow != 1 && ow != 2 && ow != 4 && ow != 8) || ord > 2 || (fl & ~15)) return -1;
    if (kg_getuv(par, plen, &pi, &h) || kg_getuv(par, plen, &pi, &cnt) || kg_getuv(par, plen, &pi, &g) || kg_getuv(par, plen, &pi, &bz) || g == 0) return -1;
    for (int q = 0; q < ord; q++) if (kg_getuv(par, plen, &pi, &sd[q])) return -1;
    if (pi != plen || h > rest->n || (uint64_t)ord > cnt || rest->n > maxs
        || cnt > (maxs - rest->n) / (unsigned)w
        || (cnt - (uint64_t)ord) > SIZE_MAX / (unsigned)ow
        || (cnt - (uint64_t)ord) * (unsigned)ow != res->n
        || cnt * (unsigned)w + rest->n > SIZE_MAX - 16) return -1;
    kgi_inv_fn f = (pln || ow == 1) ? kgi_pick(w, ow, sg) : NULL;
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

#undef KGI_ST
#undef KGI_FIX
#undef KGI_Z
#undef KGI_INV
#undef KGI_INV_W
