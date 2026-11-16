/* Codec 4: Brotli (eco), window 2^24. vendor/brotli. */
static size_t brotli_bound(size_t n) { return BrotliEncoderMaxCompressedSize(n) ? BrotliEncoderMaxCompressedSize(n) : n + 1024; }
static size_t brotli_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; size_t o = cap;
    int ok = BrotliEncoderCompress(level, 24, BROTLI_MODE_GENERIC, n, in, &o, out);
    return ok ? o : 0;
}
static int brotli_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; size_t o = n; int r = BrotliDecoderDecompress(cn, in, &o, out); return (r == BROTLI_DECODER_RESULT_SUCCESS && o == n) ? 0 : -1;
}
static const kolmo_codec codec_brotli = { C_BROTLI, "brotli", brotli_bound, brotli_compress, brotli_decompress, NULL };
