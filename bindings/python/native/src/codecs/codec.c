/* The codec registry: every block codec by identifier, and the three calls the container and the graph leaves use.
 * Identifiers 8-11 are the opt-in research codecs of the frontier build (coded at block level in core/), 12 the graph
 * block (transforms/graph.c) and 16 the FASTQ family coder (codecs/fastq.c): their tables carry a name only. 26 (GR6+,
 * codecs/gr6.c) has an encoder here but is decoded at block level. Identifiers 18, 24 and 25 are reserved (NULL). */
static const kolmo_codec *const kolmo_codecs[] = {
    [C_STORE] = &codec_store, [C_LZ4] = &codec_lz4, [C_LZ4HC] = &codec_lz4hc, [C_ZSTD] = &codec_zstd, [C_BROTLI] = &codec_brotli,
    [C_BSC5] = &codec_bwt5, [C_LZMA] = &codec_lzma, [C_CM] = &codec_cm, [C_GRAPH] = &codec_graph, [C_HUF] = &codec_huf,
    [C_RANS] = &codec_rans, [C_NPLANE] = &codec_nplane, [C_FQ] = &codec_fastq, [C_CMI] = &codec_cm_image, [C_BWT] = &codec_bwt,
    [C_AUD] = &codec_audio, [C_VID] = &codec_video, [C_PLANES] = &codec_planes, [C_HZ] = &codec_hz, [C_ZRUN] = &codec_zrun,
    [C_GR6] = &codec_gr6,
};
static const kolmo_codec codec_graph = { C_GRAPH, "graph", NULL, NULL, NULL, NULL };
static inline const kolmo_codec *codec_get(int codec) {
    return codec >= 0 && (size_t)codec < sizeof kolmo_codecs / sizeof kolmo_codecs[0] ? kolmo_codecs[codec] : NULL;
}
static size_t codec_bound(int codec, size_t n) {
    const kolmo_codec *c = codec_get(codec);
    return c && c->bound ? c->bound(n) : n;
}
static size_t codec_compress(codec_ctx *x, int codec, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    if (x->prof) { int h = 0; size_t r = kp_codec_compress(x->prof, (void **)&x->zc, codec, level, in, n, out, cap, &h); if (h) return r; }
    const kolmo_codec *c = codec_get(codec);
    return c && c->compress ? c->compress(x, level, in, n, out, cap) : 0;
}
static int codec_decompress(codec_ctx *x, int codec, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (x->prof) { int h = 0; int r = kp_codec_decompress(x->prof, (void **)&x->zd, codec, in, cn, out, n, &h); if (h) return r; }
    const kolmo_codec *c = codec_get(codec);
    return c && c->decompress ? c->decompress(x, in, cn, out, n) : -1;
}
static void codec_ctx_free(codec_ctx *x) { ZSTD_freeCCtx(x->zc); ZSTD_freeDCtx(x->zd); free(x->lz4hc_state); }

static int np_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n);
