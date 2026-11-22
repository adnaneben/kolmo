/* Codec 3: Zstandard, with the optional dictionary of the context. vendor/zstd. */
#define KZ_WIDE_WLOG 23                 /* zwide: an 8 MiB window, so a whole text block of balanced is in reach */
#define KZ_WIDE_MIN ((size_t)4 << 20)   /* zwide: blocks above this size only (level 3's own 2 MiB window below it) */
#define KZ_WIDE_BLOCK ((uint64_t)6 << 20)   /* balanced, plain coding: largest block (files above it split evenly): within
                                         * 1.5x zstd -3's memory at one thread, smaller than zstd -3 on every text file */
static size_t zstd_bound(size_t n) { return ZSTD_compressBound(n); }
static size_t zstd_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (!x->zc) x->zc = ZSTD_createCCtx();
    if (!x->zc) return 0;
    if (x->zwide && !x->dict) {   /* balanced, large plain block: an 8 MiB window over the whole block */
        ZSTD_CCtx *c = x->zc; size_t r;
        ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
        if (ZSTD_isError(ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, level))
            || ZSTD_isError(ZSTD_CCtx_setParameter(c, ZSTD_c_windowLog, KZ_WIDE_WLOG))) r = (size_t)-1;
        else r = ZSTD_compress2(c, out, cap, in, n);
        ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
        return ZSTD_isError(r) ? 0 : r;
    }
    if (level == CZ_HC_TEXT_LEVEL && !x->dict) {   /* hc, word-coded text (level id 21): level 19 with a 128 MiB window, as zstd -19 --long=27 */
        ZSTD_CCtx *c = x->zc; size_t r;
        ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
        if (ZSTD_isError(ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, 19))
            || ZSTD_isError(ZSTD_CCtx_setParameter(c, ZSTD_c_windowLog, 27))) r = (size_t)-1;
        else r = ZSTD_compress2(c, out, cap, in, n);
        ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
        return ZSTD_isError(r) ? 0 : r;
    }
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
