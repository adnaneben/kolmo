/* Huffman-only coding (zstd's HUF, no matches): for byte streams with a skewed distribution and no repeats, such as
 * the exponent plane of float arrays. Chunks of up to 128 KiB: mode u8 (0 stored, 1 one repeated byte, 2 Huffman),
 * raw length uv, then for mode 0 the bytes, mode 1 the byte, mode 2 coded length uv and the coded bytes. */
#include "common/huf.h"
static size_t huf_compress_all(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    U64 wk[HUF_WORKSPACE_SIZE_U64]; HUF_CREATE_STATIC_CTABLE(ct, HUF_SYMBOLVALUE_MAX); size_t o = 0;
    for (size_t i = 0; i < n;) {
        size_t m = n - i < HUF_BLOCKSIZE_MAX ? n - i : HUF_BLOCKSIZE_MAX;
        if (cap - o < m + 16) return 0;
        uint8_t *h = out + o; size_t hp = 1;
        for (uint64_t v = m;; v >>= 7) { h[hp++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        HUF_repeat rp = HUF_repeat_none; uint8_t tmp[10];
        size_t z = HUF_compress4X_repeat(h + hp + 4, cap - o - hp - 4, in + i, m, HUF_SYMBOLVALUE_MAX, HUF_TABLELOG_DEFAULT, wk, sizeof wk, ct, &rp, 0);
        if (HUF_isError(z)) return 0;
        if (z == 1) { h[0] = 1; h[hp] = in[i]; o += hp + 1; }                        /* one repeated byte */
        else if (z == 0 || z + 4 >= m) { h[0] = 0; memcpy(h + hp, in + i, m); o += hp + m; }
        else {
            size_t tn = 0; for (uint64_t v = z;; v >>= 7) { tmp[tn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
            memmove(h + hp + tn, h + hp + 4, z); memcpy(h + hp, tmp, tn); h[0] = 2; o += hp + tn + z;
        }
        i += m;
    }
    return o;
}
static int huf_decompress_all(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    HUF_CREATE_STATIC_DTABLEX2(dt, HUF_TABLELOG_MAX); U32 wk[HUF_DECOMPRESS_WORKSPACE_SIZE_U32]; size_t i = 0, o = 0;
    while (o < n) {
        uint64_t m, z; if (i >= cn) return -1;
        uint8_t mode = in[i++]; if (kgu_get(in, cn, &i, &m) || m == 0 || m > HUF_BLOCKSIZE_MAX || m > n - o) return -1;
        if (mode == 0) { if (m > cn - i) return -1; memcpy(out + o, in + i, m); i += m; }
        else if (mode == 1) { if (i >= cn) return -1; memset(out + o, in[i++], m); }
        else if (mode == 2) {
            if (kgu_get(in, cn, &i, &z) || z > cn - i) return -1;
            size_t r = HUF_decompress4X_hufOnly_wksp(dt, out + o, (size_t)m, in + i, (size_t)z, wk, sizeof wk, 0);
            if (HUF_isError(r) || r != m) return -1;
            i += z;
        } else return -1;
        o += m;
    }
    return i == cn ? 0 : -1;
}
static size_t huf_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; (void)level; return huf_compress_all(in, n, out, cap);
}
static int huf_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return huf_decompress_all(in, cn, out, n); }
static const kolmo_codec codec_huf = { C_HUF, "huf", NULL, huf_codec_compress, huf_codec_decompress, NULL };
