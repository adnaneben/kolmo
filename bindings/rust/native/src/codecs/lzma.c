/* Codec 6: raw LZMA2 (max), liblzma. Payload: dict u32 | lc lp pb | raw LZMA2. Built without liblzma
 * (-DKOLMO_NO_LZMA): never written, refused when read. */
#ifndef KOLMO_NO_LZMA
static size_t lzma_bound(size_t n) { return lzma_stream_buffer_bound(n) + 64; }
static size_t lzma_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x;
    lzma_options_lzma opt; if (lzma_lzma_preset(&opt, 9 | LZMA_PRESET_EXTREME)) return 0;
    uint32_t d = 4096; while (d < n && d < (64u << 20)) d <<= 1; opt.dict_size = d;
    if (level >= 100) { opt.lc = (level / 100) % 10 - 1; opt.lp = (level / 10) % 10; opt.pb = level % 10; }
    lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
    if (cap < 8) return 0;
    memcpy(out, &d, 4); out[4] = (uint8_t)opt.lc; out[5] = (uint8_t)opt.lp; out[6] = (uint8_t)opt.pb; out[7] = 0;
    size_t pos = 8;
    if (lzma_raw_buffer_encode(f, NULL, in, n, out, &pos, cap) != LZMA_OK) return 0;
    return pos;
}
static int lzma_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x;
    if (cn < 8) return -1;
    uint32_t d; memcpy(&d, in, 4);
    if (d < 4096 || d > (64u << 20) || in[4] > 4 || in[5] > 4 || in[6] > 4 || in[4] + in[5] > 4) return -1;
    lzma_options_lzma opt; lzma_lzma_preset(&opt, 6); opt.dict_size = d; opt.lc = in[4]; opt.lp = in[5]; opt.pb = in[6];
    lzma_filter f[2] = {{LZMA_FILTER_LZMA2, &opt}, {LZMA_VLI_UNKNOWN, NULL}};
    size_t ip = 8, op = 0;
    if (lzma_raw_buffer_decode(f, NULL, in, &ip, cn, out, &op, n) != LZMA_OK) return -1;
    return (ip == cn && op == n) ? 0 : -1;
}
static const kolmo_codec codec_lzma = { C_LZMA, "lzma", lzma_bound, lzma_compress, lzma_decompress, NULL };
#else
static size_t lzma_bound(size_t n) { return n + n / 8 + 64; }
static const kolmo_codec codec_lzma = { C_LZMA, "lzma", lzma_bound, NULL, NULL, NULL };   /* block stored or another codec used */
#endif
