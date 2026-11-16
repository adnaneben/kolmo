/* Codec 3: Zstandard, with the optional dictionary of the context. vendor/zstd. */
static size_t zstd_bound(size_t n) { return ZSTD_compressBound(n); }
static size_t zstd_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (!x->zc) x->zc = ZSTD_createCCtx();
    if (!x->zc) return 0;
    size_t r = x->dict ? ZSTD_compress_usingDict(x->zc, out, cap, in, n, x->dict, x->dictlen, level)
                       : ZSTD_compressCCtx(x->zc, out, cap, in, n, level);
    return ZSTD_isError(r) ? 0 : r;
}
static int zstd_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (!x->zd) x->zd = ZSTD_createDCtx();
    if (!x->zd) return -1;
    size_t r = x->dict ? ZSTD_decompress_usingDict(x->zd, out, n, in, cn, x->dict, x->dictlen)
                       : ZSTD_decompressDCtx(x->zd, out, n, in, cn);
    return (!ZSTD_isError(r) && r == n) ? 0 : -1;
}
static const kolmo_codec codec_zstd = { C_ZSTD, "zstd", zstd_bound, zstd_compress, zstd_decompress, NULL };
