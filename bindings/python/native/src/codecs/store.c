/* Codec 0: stored bytes. */
static int store_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; if (cn != n) return -1; memcpy(out, in, n); return 0;
}
static const kolmo_codec codec_store = { C_STORE, "store", NULL, NULL, store_decompress, NULL };
