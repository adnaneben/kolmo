/* Codecs 1 and 2: LZ4 (rt) and LZ4-HC; one decoder for both. vendor/lz4. */
static size_t lz4_bound(size_t n) { return (size_t)LZ4_compressBound((int)n); }
static size_t lz4_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; int r = LZ4_compress_fast((const char *)in, (char *)out, (int)n, (int)cap, level > 0 ? level : 1); return r > 0 ? (size_t)r : 0;
}
static size_t lz4hc_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (!x->lz4hc_state) x->lz4hc_state = malloc(LZ4_sizeofStateHC());
    if (!x->lz4hc_state) return 0;
    int r = LZ4_compress_HC_extStateHC(x->lz4hc_state, (const char *)in, (char *)out, (int)n, (int)cap, level);
    return r > 0 ? (size_t)r : 0;
}
static int lz4_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; int r = LZ4_decompress_safe((const char *)in, (char *)out, (int)cn, (int)n); return r == (int)n ? 0 : -1;
}
static const kolmo_codec codec_lz4 = { C_LZ4, "lz4", lz4_bound, lz4_compress, lz4_decompress, NULL };
static const kolmo_codec codec_lz4hc = { C_LZ4HC, "lz4hc", lz4_bound, lz4hc_compress, lz4_decompress, NULL };
