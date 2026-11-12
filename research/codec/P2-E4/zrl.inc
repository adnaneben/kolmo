/* P2-E4 zero-run residual leaf (research framing 0xB0; never a production codec id). Input: a KG_INT residual stream
 * in production layout (m values of ow bytes, byte planes when pln). Output blob:
 *   0xB0, uv(nnz), stream R (zero-run length before each nonzero, LEB128 bytes), then ow streams V_b (byte b of
 *   each nonzero value). Each stream: codec byte (C_HUF or C_STORE), uv(raw length), uv(coded length), data.
 * Trailing zeros are implied by m. Decoding rebuilds the residual stream in the same production layout. */
static int zrl_put(codec_ctx *x, kbuf *o, const uint8_t *p, size_t n) {
    size_t cap = n + n / 8 + 1024; if (kb_res(o, cap + 24)) return -1;
    uint8_t *tmp = malloc(cap); if (!tmp) return -1;
    size_t z = n >= 64 ? codec_compress(x, C_HUF, 0, p, n, tmp, cap) : 0; int cc = C_HUF;
    if (!z || z >= n) { cc = C_STORE; z = n; if (n) memcpy(tmp, p, n); }
    int rc = kb_byte(o, (uint8_t)cc) || kb_uv(o, n) || kb_uv(o, z) || kb_put(o, tmp, z);
    free(tmp); return rc ? -1 : 0;
}
static inline uint64_t zrl_get(const uint8_t *r, size_t i, size_t m, int ow, int pln) {
    uint64_t z = 0; for (int b = 0; b < ow; b++) z |= (uint64_t)(pln ? r[(size_t)b * m + i] : r[i * (size_t)ow + b]) << (8 * b); return z;
}
static int zrl_encode(codec_ctx *x, const uint8_t *r, size_t m, int ow, int pln, kbuf *blob) {
    size_t nnz = 0; for (size_t i = 0; i < m; i++) nnz += zrl_get(r, i, m, ow, pln) != 0;
    kbuf R = {0}; uint8_t *V = malloc(nnz * (size_t)ow + 1); int rc = -1; if (!V) return -1;
    if (kb_res(&R, nnz * 2 + 16)) goto done;
    { size_t run = 0, k = 0;
      for (size_t i = 0; i < m; i++) { uint64_t z = zrl_get(r, i, m, ow, pln);
          if (!z) { run++; continue; }
          if (kb_uv(&R, run)) goto done; run = 0;
          for (int b = 0; b < ow; b++) V[(size_t)b * nnz + k] = (uint8_t)(z >> (8 * b)); k++; } }
    blob->n = 0;
    if (kb_byte(blob, 0xB0) || kb_uv(blob, nnz) || zrl_put(x, blob, R.p, R.n)) goto done;
    for (int b = 0; b < ow; b++) if (zrl_put(x, blob, V + (size_t)b * nnz, nnz)) goto done;
    rc = 0;
done:
    free(R.p); free(V); return rc;
}
static int zrl_take(codec_ctx *x, const uint8_t *p, size_t n, size_t *i, size_t maxraw, uint8_t **out, size_t *on) {
    uint64_t rl, zl; if (*i >= n) return -1; int cc = p[(*i)++];
    if (kg_getuv(p, n, i, &rl) || kg_getuv(p, n, i, &zl) || zl > n - *i || rl > maxraw || (cc != C_HUF && cc != C_STORE)) return -1;
    uint8_t *d = malloc(rl + 16); if (!d) return -1;
    if (codec_decompress(x, cc, p + *i, (size_t)zl, d, (size_t)rl)) { free(d); return -1; }
    *i += zl; *out = d; *on = (size_t)rl; return 0;
}
static int zrl_decode(codec_ctx *x, const uint8_t *p, size_t n, size_t m, int ow, int pln, kbuf *res) {
    size_t i = 1; uint64_t nnz; uint8_t *R = NULL, *V[8] = {0}; size_t rn = 0, vn; int rc = -1;
    if (n < 2 || p[0] != 0xB0 || kg_getuv(p, n, &i, &nnz) || nnz > m || ow < 1 || ow > 8) return -1;
    if (zrl_take(x, p, n, &i, nnz * 10 + 16, &R, &rn)) goto done;
    for (int b = 0; b < ow; b++) if (zrl_take(x, p, n, &i, (size_t)nnz, &V[b], &vn) || vn != nnz) goto done;
    if (i != n || kb_res(res, m * (size_t)ow + 16)) goto done;
    memset(res->p, 0, m * (size_t)ow); res->n = m * (size_t)ow;
    { size_t ri = 0, pos = 0;
      for (size_t k = 0; k < nnz; k++) { uint64_t run; if (kg_getuv(R, rn, &ri, &run) || run > m - pos || pos + run >= m) goto done;
          pos += run;
          for (int b = 0; b < ow; b++) { uint8_t v = V[b][k]; if (pln) res->p[(size_t)b * m + pos] = v; else res->p[pos * (size_t)ow + b] = v; }
          pos++; }
      if (ri != rn) goto done; }
    rc = 0;
done:
    free(R); for (int b = 0; b < 8; b++) free(V[b]); return rc;
}

/* ---- fused variants (same 0xB0 bytes as zrl_encode; decoder writes output values directly) ---- */
static int zrl_encode_fast(codec_ctx *x, const uint8_t *r, size_t m, int ow, int pln, kbuf *blob) {
    if (!pln && ow != 1) return zrl_encode(x, r, m, ow, pln, blob);
    size_t rcap = m / 4 + 64; uint8_t *V = malloc((size_t)ow * (m + 1)), *R = malloc(rcap); size_t nnz = 0, rn = 0, run = 0; int rc = -1;
    if (!V || !R) goto done;
    for (size_t i = 0; i < m; i++) {
        uint8_t any = r[i]; for (int b = 1; b < ow; b++) any |= r[(size_t)b * m + i];
        if (!any) { run++; continue; }
        if (rn + 10 > rcap) { rcap = rcap * 2 + 64; uint8_t *t = realloc(R, rcap); if (!t) goto done; R = t; }
        { uint8_t *q = R + rn; uint64_t v = run; while (v >= 128) { *q++ = (uint8_t)(v | 128); v >>= 7; } *q++ = (uint8_t)v; rn = (size_t)(q - R); }
        run = 0;
        for (int b = 0; b < ow; b++) V[(size_t)b * m + nnz] = r[(size_t)b * m + i];
        nnz++;
    }
    blob->n = 0;
    if (kb_byte(blob, 0xB0) || kb_uv(blob, nnz) || zrl_put(x, blob, R, rn)) goto done;
    for (int b = 0; b < ow; b++) if (zrl_put(x, blob, V + (size_t)b * m, nnz)) goto done;
    rc = 0;
done:
    free(V); free(R); return rc;
}
/* parse the 0xB0 blob and invert the KG_INT program directly into out (cnt*w + rest bytes); same validation
 * contract as kg_int_inv for the parameters. Returns 0 on success. */
static int zrl_inverse_fused(codec_ctx *x, const uint8_t *par, size_t plen, const uint8_t *p, size_t n, const kbuf *rest,
                             size_t maxs, uint8_t *out, size_t outcap) {
    size_t pi = 3; uint64_t h, cnt, g, bz, sd[2] = {0, 0};
    if (plen < 3) return -1;
    int w = par[0], fl = par[1], ow = par[2], ord = (fl >> 1) & 3, sg = fl & 1, pln = (fl >> 3) & 1;
    if ((w != 2 && w != 4 && w != 8) || (ow != 1 && ow != 2 && ow != 4 && ow != 8) || ord > 2 || (fl & ~15) || (!pln && ow != 1)) return -1;
    if (kg_getuv(par, plen, &pi, &h) || kg_getuv(par, plen, &pi, &cnt) || kg_getuv(par, plen, &pi, &g) || kg_getuv(par, plen, &pi, &bz) || g == 0) return -1;
    for (int q = 0; q < ord; q++) if (kg_getuv(par, plen, &pi, &sd[q])) return -1;
    if (pi != plen || h > rest->n || (uint64_t)ord > cnt || cnt > maxs / (unsigned)w || cnt * (unsigned)w + rest->n > maxs
        || cnt * (unsigned)w + rest->n > outcap) return -1;
    size_t m = (size_t)(cnt - (uint64_t)ord), i = 1; uint64_t nnz; uint8_t *R = NULL, *V[8] = {0}; size_t rn = 0, vn; int rc = -1;
    if (n < 2 || p[0] != 0xB0 || kg_getuv(p, n, &i, &nnz) || nnz > m) return -1;
    if (zrl_take(x, p, n, &i, nnz * 10 + 16, &R, &rn)) goto done;
    for (int b = 0; b < ow; b++) if (zrl_take(x, p, n, &i, (size_t)nnz, &V[b], &vn) || vn != nnz) goto done;
    if (i != n) goto done;
    if (h) memcpy(out, rest->p, (size_t)h);
    {   uint8_t *wp = out + h; uint64_t v1 = 0, v2 = 0, bias = (uint64_t)kg_unzz(bz); const int sh = 64 - 8 * w;
        const uint64_t mask = w < 8 ? ((uint64_t)1 << (8 * w)) - 1 : ~(uint64_t)0;
#define ZFIX(v) (w == 8 ? (v) : sg ? (uint64_t)((int64_t)((v) << sh) >> sh) : ((v) & mask))
#define ZST(v) do { if (w == 2) { uint16_t t_ = (uint16_t)(v); memcpy(wp, &t_, 2); } else if (w == 4) { uint32_t t_ = (uint32_t)(v); memcpy(wp, &t_, 4); } else { uint64_t t_ = (v); memcpy(wp, &t_, 8); } wp += w; } while (0)
        for (int q = 0; q < ord; q++) { uint64_t v = ZFIX((uint64_t)kg_unzz(sd[q])); ZST(v); v2 = v1; v1 = v; }
        size_t ri = 0, pos = 0;
        for (size_t k = 0; k <= nnz; k++) {
            uint64_t run;
            if (k < nnz) { if (kg_getuv(R, rn, &ri, &run) || run > m - pos - 1) goto done; } else run = m - pos;
            if (ord == 0) { uint64_t v = ZFIX(bias); for (uint64_t t = 0; t < run; t++) ZST(v); }
            else if (ord == 1) { for (uint64_t t = 0; t < run; t++) ZST(v1); v2 = v1; }
            else for (uint64_t t = 0; t < run; t++) { uint64_t v = ZFIX(2 * v1 - v2); ZST(v); v2 = v1; v1 = v; }
            pos += run; if (k == nnz) break;
            uint64_t z = 0; for (int b = 0; b < ow; b++) z |= (uint64_t)V[b][k] << (8 * b);
            uint64_t v;
            if (ord == 0) v = ZFIX(bias + z * g);
            else { uint64_t d = (uint64_t)kg_unzz(z) * g; v = ZFIX((ord == 1 ? v1 : 2 * v1 - v2) + d); }
            ZST(v); v2 = v1; v1 = v; pos++;
        }
#undef ZFIX
#undef ZST
        if (ri != rn || pos != m) goto done;
        memcpy(wp, rest->p + h, rest->n - (size_t)h);
    }
    rc = 0;
done:
    free(R); for (int b = 0; b < 8; b++) free(V[b]); return rc;
}
