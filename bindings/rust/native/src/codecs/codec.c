/* The codec registry: every block codec by identifier, and the three calls the container and the graph leaves use.
 * Identifiers 8-11 are the opt-in research codecs of the frontier build (coded at block level in core/), 12 the graph
 * block (transforms/graph.c) and 16 the FASTQ family coder (codecs/fastq.c): their tables carry a name only. 26 (GR6+,
 * codecs/gr6.c) has an encoder here but is decoded at block level. Identifiers 24, 27-29 and 32 are Kolmo's LZ, LZMA-
 * class and Huffman coders (codecs/native.c). The retired library ids (1-4, 6, 13, 22) have no entry. */
static const kolmo_codec *const kolmo_codecs[] = {
    [C_STORE] = &codec_store, [C_BSC5] = &codec_bwt5, [C_CM] = &codec_cm, [C_GRAPH] = &codec_graph,
    [C_RANS] = &codec_rans, [C_NPLANE] = &codec_nplane, [C_FQ] = &codec_fastq, [C_CMI] = &codec_cm_image, [C_BWT] = &codec_bwt,
    [C_AUD] = &codec_audio, [C_VID] = &codec_video, [C_PLANES] = &codec_planes, [C_ZRUN] = &codec_zrun,
    [C_GR6] = &codec_gr6,
    [C_CMP] = &codec_cm_par,
    [C_KLZMA] = &codec_klzma, [C_KHUF] = &codec_khuf, [C_KLZ] = &codec_klz, [C_KLZH] = &codec_klzh,
    [C_KIMG] = &codec_kimg, [C_KLZH2] = &codec_klzh2,
};
static const kolmo_codec codec_graph = { C_GRAPH, "graph", NULL, NULL, NULL, NULL };
static inline const kolmo_codec *codec_get(int codec) {
    return codec >= 0 && (size_t)codec < sizeof kolmo_codecs / sizeof kolmo_codecs[0] ? kolmo_codecs[codec] : NULL;
}
/* bound and compress take a role or an id: a role is coded by its native coder (codecs/native.c) */
static size_t codec_bound(int codec, size_t n) {
    const kolmo_codec *c = codec_get(codec_native_id(codec));
    return c && c->bound ? c->bound(n) : n;
}
static size_t codec_compress(codec_ctx *x, int codec, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    { int nc = codec_native_id(codec); if (nc != codec) { level = native_level(codec, level); codec = nc; } }
    const kolmo_codec *c = codec_get(codec);
    return c && c->compress ? c->compress(x, level, in, n, out, cap) : 0;
}
/* decompress takes a wire id only */
static int codec_decompress(codec_ctx *x, int codec, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (x && x->deny_cm && (codec == C_CM || codec == C_CMI || codec == C_CMP)) return -1;
    const kolmo_codec *c = codec_get(codec);
    return c && c->decompress ? c->decompress(x, in, cn, out, n) : -1;
}
static void codec_ctx_free(codec_ctx *x) { (void)x; }   /* the native coders keep no state in the context */

static int np_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n);
