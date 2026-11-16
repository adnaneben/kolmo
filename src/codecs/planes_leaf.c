/* Codec 21: per-plane leaf (graph leaves; container version 11). Research ST-1 / P-01 (one leaf per byte plane, each
 * with its own codec), ST-5 (the same at hc), ST-8 (small messages), with the sparse guard of the leaf rule applied to
 * every plane.
 *
 * The leaf holds np equal byte planes (the plane-major residuals of an INT node, flag bit 3). Each plane is coded on
 * its own by the codec that codes it smallest: the high planes of small residuals are long zero runs that zstd or
 * LZMA2 code for almost nothing, the low planes are noise that Huffman or rANS code near their entropy.
 *
 * Payload: np u8 (2-8) | per plane: codec u8 | coded length uv | coded bytes. The leaf's raw length is a multiple of
 * np; plane b is bytes [b L, (b + 1) L) with L = raw length / np. A plane's codec is any leaf codec except 12
 * (graph), 15 (native plane leaf), 16, 17, 19, 20 and 21 itself.
 * Included by src/kolmo.c until the structure lane gives codecs their own units (src/codecs/codec.h). */

typedef size_t (*leaf_sel_fn)(codec_ctx *x, const void *ctx, const uint8_t *p, size_t n, uint8_t *out, size_t cap, int *codec);

static size_t planes_compress(codec_ctx *x, int np, const uint8_t *in, size_t n, uint8_t *out, size_t cap, leaf_sel_fn sel, const void *ctx) {
    if (np < 2 || np > 8 || n % (size_t)np || cap < 1) return 0;
    size_t L = n / (size_t)np, o = 1; out[0] = (uint8_t)np;
    for (int b = 0; b < np; b++) {
        if (cap - o < 12) return 0;
        size_t zc = L + L / 8 + 1024; uint8_t *t = malloc(zc); int cc = C_STORE;
        if (!t) return 0;
        size_t z = sel(x, ctx, in + (size_t)b * L, L, t, zc, &cc);
        if (!z || z > L) { cc = C_STORE; z = L; memcpy(t, in + (size_t)b * L, L); }
        uint8_t h[11]; size_t hn = 0; h[hn++] = (uint8_t)cc;
        for (uint64_t v = z;; v >>= 7) { h[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        if (cap - o < hn + z) { free(t); return 0; }
        memcpy(out + o, h, hn); memcpy(out + o + hn, t, z); o += hn + z; free(t);
    }
    return o;
}

static int leaf_sub_codec_ok(int c) {
    return c == C_STORE || c == C_LZ4 || c == C_LZ4HC || c == C_ZSTD || c == C_BROTLI || c == C_BSC || c == C_LZMA || c == C_CM ||
           c == C_HUF || c == C_RANS || c == C_HZ || c == C_ZRUN;
}

static int planes_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 1) return -1;
    int np = in[0]; size_t i = 1;
    if (np < 2 || np > 8 || n % (size_t)np) return -1;
    size_t L = n / (size_t)np;
    for (int b = 0; b < np; b++) {
        uint64_t z; if (i >= cn) return -1;
        int cc = in[i++];
        if (!leaf_sub_codec_ok(cc) || kgu_get(in, cn, &i, &z) || z > cn - i) return -1;
        if (codec_decompress(x, cc, in + i, (size_t)z, out + (size_t)b * L, L)) return -1;
        i += (size_t)z;
    }
    return i == cn ? 0 : -1;
}
