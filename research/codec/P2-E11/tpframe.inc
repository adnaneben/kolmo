/* P2-E11: type-first pipelines that write GENUINE Kolmo CZN1 frames (research encoder only; decoding is the unchanged
 * production kolmo_decompress / `kolmo -d`). No src/ change, no new IDs: one block, version 7 (graph blocks), plan bytes
 * T_KG / 0 / width / header length, codec C_GRAPH, block checksum blk_sum(3, ...) — the same fields production writes
 * for a KG_INT block. Requires fastint9.inc (identity-checked fi_forward / fc_choose) and the production sources.
 *
 *   tp_build_prod: KG_INT program from fi_forward, serialized by production kg_serialize at (C_ZSTD, level): the same
 *                  payload production kg_encode_num would produce (checked in p2e11.c), in a production frame.
 *   tp_build_phuf: same program; residual leaf = C_HUF whose chunks are the concatenation of huf_compress_all() over
 *                  each byte plane (the C_HUF leaf format is a sequence of self-describing chunks of any length up to
 *                  128 KiB, read until the output is full, so plane-aligned chunks are an ordinary leaf); rest leaf
 *                  stored. Falls back to a stored residual leaf if Huffman fails.
 *   tp_fallback:   production frame from kolmo_compress (fast, transforms = 0: plain zstd-3 blocks). */
static size_t tp_wrap(const uint8_t *src, size_t n, int w, size_t h, const uint8_t *pay, size_t pn, uint8_t *dst) {
    uint8_t *e = dst + HDR, *p = dst + HDR + IDX;
    memcpy(dst, "CZN1", 4); dst[4] = 7; dst[5] = (uint8_t)KOLMO_FAST; dst[6] = dst[7] = dst[8] = dst[9] = 0; dst[10] = 0; dst[11] = 0;
    put64(dst + 12, n); put32(dst + 20, 1); put32(dst + 24, (uint32_t)n);
    p[0] = T_KG; p[1] = 0; p[2] = (uint8_t)w; p[3] = (uint8_t)(h < 255 ? h : 255); memcpy(p + 4, pay, pn);
    put32(e, (uint32_t)n); put32(e + 4, (uint32_t)(pn + 4)); put32(e + 8, blk_sum(3, src, n)); e[12] = C_GRAPH; e[13] = 0; e[14] = e[15] = 0;
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, IDX); put32(dst + 28, hc);
    return HDR + IDX + 4 + pn;
}
static size_t tp_fallback(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    kolmo_params p = {KOLMO_FAST, 0, 1, 0, 0}; int64_t r = kolmo_compress(src, n, dst, cap, &p); return r > 0 ? (size_t)r : 0;
}
/* production payload for the fi_forward program, via production kg_serialize */
static size_t tp_payload_prod(codec_ctx *x, fiprog *P, size_t n, int level, uint8_t *dst, size_t cap) {
    kgb g = {0}; size_t out = 0;
    if (kgb_new(&g) != 0) goto done;
    if (kgb_node(&g, KG_INT, (int[]){0}, 1, 2, &P->par) != 1) goto done;
    if (kb_put(&g.s[1], P->res.p, P->res.n) || kb_put(&g.s[2], P->rest.p, P->rest.n)) goto done;
    if (!P->res.n && kb_res(&g.s[1], 1)) goto done;
    out = kg_serialize(x, C_ZSTD, level, &g, n, dst, cap);
done:
    kgb_free(&g); return out;
}
/* plane-aligned Huffman residual leaf, stored rest leaf; mirrors kg_serialize's program/leaf syntax exactly */
static size_t tp_payload_phuf(fiprog *P, uint8_t *dst, size_t cap) {
    kbuf o = {0}; size_t out = 0, zc = P->res.n + P->res.n / 8 + 1024 * (size_t)(P->ow + 1);
    uint8_t *hb = malloc(zc); if (!hb) return 0;
    size_t z = 0; int cc = C_HUF, planes = P->pln ? P->ow : 1; size_t plen = P->pln ? P->m : P->res.n;
    for (int b = 0; b < planes && cc == C_HUF; b++) {
        size_t r = plen ? huf_compress_all(P->res.p + (size_t)b * plen, plen, hb + z, zc - z) : 0;
        if (plen && !r) cc = C_STORE; else z += r;
    }
    if (cc != C_HUF || z >= P->res.n) { cc = C_STORE; z = P->res.n; memcpy(hb, P->res.p, z); }
    if (kb_byte(&o, 'G') || kb_byte(&o, 1) || kb_uv(&o, 1)                                   /* header, one node */
        || kb_byte(&o, KG_INT) || kb_uv(&o, 1) || kb_uv(&o, 0) || kb_uv(&o, 2) || kb_uv(&o, P->par.n) || kb_put(&o, P->par.p, P->par.n)
        || kb_uv(&o, 2)                                                                         /* two leaves */
        || kb_uv(&o, 1) || kb_byte(&o, (uint8_t)cc) || kb_uv(&o, P->res.n) || kb_uv(&o, z) || kb_put(&o, hb, z)
        || kb_uv(&o, 2) || kb_byte(&o, C_STORE) || kb_uv(&o, P->rest.n) || kb_uv(&o, P->rest.n) || kb_put(&o, P->rest.p, P->rest.n)) goto done;
    if (o.n <= cap) { memcpy(dst, o.p, o.n); out = o.n; }
done:
    free(o.p); free(hb); return out;
}
