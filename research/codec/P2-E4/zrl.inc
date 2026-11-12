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
