/* Codec 5: BWT pipeline (LZP + BWT + QLFC) of libbsc, vendor/libbsc. */
static void bsc_once(void) { bsc_init(LIBBSC_FEATURE_FASTMODE); }
static pthread_once_t bsc_init_once = PTHREAD_ONCE_INIT;
static size_t bwt_bound(size_t n) { return n + LIBBSC_HEADER_SIZE + 64; }
static size_t bwt_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; (void)level;               /* payload: XXH32 of the bsc stream | bsc stream (checked before decoding) */
    pthread_once(&bsc_init_once, bsc_once);
    if (n > 0x7FFFFFF0 || cap < n + LIBBSC_HEADER_SIZE + 4) return 0;
    int r = bsc_compress(in, out + 4, (int)n, LIBBSC_DEFAULT_LZPHASHSIZE, LIBBSC_DEFAULT_LZPMINLEN, LIBBSC_BLOCKSORTER_BWT,
                         LIBBSC_CODER_QLFC_STATIC, LIBBSC_FEATURE_FASTMODE);
    if (r <= 0) return 0;
    uint32_t h = (uint32_t)XXH64(out + 4, (size_t)r, 0x62736331); memcpy(out, &h, 4);
    return (size_t)r + 4;
}
static int bwt_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x;
    pthread_once(&bsc_init_once, bsc_once);
    if (cn < 4 + LIBBSC_HEADER_SIZE || n > 0x7FFFFFF0 || cn > 0x7FFFFFF0) return -1;
    uint32_t h; memcpy(&h, in, 4); if (h != (uint32_t)XXH64(in + 4, cn - 4, 0x62736331)) return -1;
    int bs, ds; if (bsc_block_info(in + 4, LIBBSC_HEADER_SIZE, &bs, &ds, LIBBSC_FEATURE_FASTMODE) != LIBBSC_NO_ERROR) return -1;
    if ((size_t)bs != cn - 4 || (size_t)ds != n) return -1;
    return bsc_decompress(in + 4, (int)(cn - 4), out, (int)n, LIBBSC_FEATURE_FASTMODE) == LIBBSC_NO_ERROR ? 0 : -1;
}
static const kolmo_codec codec_bwt = { C_BSC, "bwt", bwt_bound, bwt_compress, bwt_decompress, NULL };
