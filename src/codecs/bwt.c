/* Codec 5: the BWT pipeline (LZP + BWT + QLFC) in libbsc 3.4.0's block format, written and read by Kolmo's C port
   (codecs/bwt_stream.c, algo/lzp.c, algo/qlfc.c, algo/sais/; byte-identical to libbsc with these parameters).
   Payload: XXH32 of the block | block (the hash is checked before decoding). */
static size_t bwt_bound(size_t n) { return n + KOLMO_BSC_HEADER_SIZE + 64; }
static size_t bwt_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)level;
    if (n > 0x7FFFFFF0 || cap < n + KOLMO_BSC_HEADER_SIZE + 4) return 0;
    int r = kolmo_bsc_compress(in, out + 4, (int)n, 15, 72, KOLMO_QLFC_STATIC, x->threads);   /* libbsc's defaults: LZP 15/72 */
    if (r <= 0) return 0;
    uint32_t h = (uint32_t)XXH64(out + 4, (size_t)r, 0x62736331); memcpy(out, &h, 4);
    return (size_t)r + 4;
}
static int bwt_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 4 + KOLMO_BSC_HEADER_SIZE || n > 0x7FFFFFF0 || cn > 0x7FFFFFF0) return -1;
    uint32_t h; memcpy(&h, in, 4); if (h != (uint32_t)XXH64(in + 4, cn - 4, 0x62736331)) return -1;
    int bs, ds; if (kolmo_bsc_block_info(in + 4, KOLMO_BSC_HEADER_SIZE, &bs, &ds) != 0) return -1;
    if ((size_t)bs != cn - 4 || (size_t)ds != n) return -1;
    return kolmo_bsc_decompress(in + 4, (int)(cn - 4), out, (int)n, x->threads) == 0 ? 0 : -1;
}
static const kolmo_codec codec_bwt = { C_BSC, "bwt", bwt_bound, bwt_compress, bwt_decompress, NULL };
