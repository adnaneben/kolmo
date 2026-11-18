/* libFuzzer target for the block and leaf decoders rc3 added, called directly (no container, no checksum in the way):
 * codec 18 (Kolmo block sorting, LZP + BWT + QLFC in C), 19 (audio, kaud), 20 (video, the kvid inverse layout),
 * 21 (per-plane leaf), 22 (Huffman then zstd leaf), 23 (zero-run leaf), 25 (context mixing over parts), 26 (GR6+),
 * 17 (context mixing with the image model), 16 (the FASTQ family coder) and the graph block decoder with every
 * format-2 leaf allowed.
 *
 * Input: sel u8 | n (3 bytes, little-endian, the decoded length; capped at 1 MiB) | flags u8 | payload.
 *   sel % 12 picks the decoder (below); (sel & 0x7f) >= 120 picks decoder 12, codec 16 (FASTQ family). flags bit 0: 2 threads instead of 1 where the decoder takes threads.
 *   sel & 0x80: round-trip mode where the codec has an encoder here (16, 18, 19, 21, 22, 23, 26): the payload is
 *   encoded (n is then ignored), must decode back exactly (a mismatch traps: a real bug), then the valid stream is
 *   damaged at positions taken from the input and decoded again (any result is allowed, no crash or overread).
 * Built with KOLMO_FUZZ_NO_CM, the context-mixing decoders return at once (see src/codecs/cm.c), so codec 25 only
 * exercises its part table and codec 17 nothing; the default build runs them for real.
 * The whole library is included (one translation unit), so the static decoders are reachable. */
#include "../src/kolmo.c"

#define FZ_MAXN (1u << 20)

static size_t fz_sel_codec;   /* sub-codec used by the planes / zero-run round trips */
static size_t fz_leaf_sel(codec_ctx *x, const void *ctx, const uint8_t *p, size_t n, uint8_t *out, size_t cap, int *codec) {
    (void)ctx;
    static const int cs[] = { C_STORE, C_ZSTD, C_HUF, C_RANS, C_LZ4, C_BWT };
    int c = cs[fz_sel_codec % (sizeof cs / sizeof cs[0])];
    if (c == C_STORE) { if (n > cap) return 0; memcpy(out, p, n); *codec = C_STORE; return n; }
    if (codec_bound(c, n) > cap) return 0;
    size_t z = codec_compress(x, c, 3, p, n, out, cap);
    *codec = c; return z;
}

static int fz_decode(codec_ctx *x, int which, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    switch (which) {
    case 0: return codec_decompress(x, C_BWT, in, cn, out, n);
    case 1: return aud_decompress(in, cn, out, n);
    case 2: return kv_inverse(in, cn, out, n);
    case 3: return codec_decompress(x, C_PLANES, in, cn, out, n);
    case 4: return codec_decompress(x, C_HZ, in, cn, out, n);
    case 5: return codec_decompress(x, C_ZRUN, in, cn, out, n);
    case 6: return codec_decompress(x, C_CMP, in, cn, out, n);
    case 7: return gr6_decompress(in, cn, out, n);
    case 8: return codec_decompress(x, C_CMI, in, cn, out, n);
    case 9: return kg_decode(x, in, cn, out, n);
    case 10: return kolmo_bwt_decompress(in, cn, out, (int)n, x->threads);
    case 12: return kf_fastq_decompress(in, cn, out, n, x->threads);
    default: return codec_decompress(x, C_BSC5, in, cn, out, n);   /* codec 5 (rc2 frames), read through the C port */
    }
}

/* encode for the round trip; returns the stream length (0: the encoder declined) and the decoder to use */
static size_t fz_encode(codec_ctx *x, int which, const uint8_t *d, size_t n, uint8_t *e, size_t cap, uint8_t fl, int *dec) {
    *dec = which;
    switch (which) {
    case 0: case 10: *dec = 0; return n >= 2 && bwt18_bound(n) <= cap ? codec_compress(x, C_BWT, 0, d, n, e, cap) : 0;
    case 1: {                                                /* 44-byte head, PCM16 frames, a short tail */
        int ch = 1 + (fl >> 1 & 1); if (n < 46 + 4) return 0;
        size_t tail = fl >> 2 & 3; uint64_t frames = (n - 44 - tail) / (size_t)(2 * ch);
        if (frames == 0 || n > (64u << 10)) return 0;
        return aud_compress(fl >> 4 & 1, ch, d, n, 44, frames, e, cap);
    }
    case 3: { int np = 2 + (fl >> 1) % 7; size_t m = n / (size_t)np * (size_t)np; (void)m;
              if (n % (size_t)np || !n) return 0; fz_sel_codec = fl >> 4; *dec = 3; return planes_compress(x, np, d, n, e, cap, fz_leaf_sel, NULL); }
    case 4: { size_t hc = n + n / 1024 + 1024; uint8_t *h = malloc(hc); if (!h) return 0;
              size_t hl = huf_compress_all(d, n, h, hc); size_t z = hl ? hz_compress_huf(x, 3, h, hl, e, cap) : 0; free(h); return z; }
    case 5: fz_sel_codec = fl >> 4; return zrun_compress(x, d, n, e, cap, fz_leaf_sel, NULL);
    case 7: return n <= (256u << 10) ? gr6_compress(d, n, e, cap) : 0;
    case 12: {                                               /* FASTQ text; the coder declines anything else */
        size_t z = 0; uint8_t *q = kf_fastq_compress(d, n, 1, kf_chunk_for(n), KF_BCHUNK, &z);
        if (!q || z > cap) { free(q); return 0; }
        memcpy(e, q, z); free(q); return z;
    }
    default: return 0;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5) return 0;
    uint8_t sel = data[0], fl = data[4];
    size_t n = (size_t)data[1] | (size_t)data[2] << 8 | (size_t)data[3] << 16;
    if (n > FZ_MAXN) n = FZ_MAXN;
    data += 5; size -= 5;
    if (size > (2u << 20)) size = 2u << 20;
    int which = (sel & 0x7f) >= 120 ? 12 : (sel & 0x7f) % 12;
    codec_ctx x; memset(&x, 0, sizeof x);
    x.allow_nplane = 1; x.allow_f2 = 1; x.threads = (fl & 1) ? 2 : 1;
    if (!(sel & 0x80)) {
        uint8_t *o = malloc(n ? n : 1);
        if (o) fz_decode(&x, which, data, size, o, n);
        free(o); codec_ctx_free(&x); return 0;
    }
    /* round trip */
    size_t m = size > (512u << 10) ? (512u << 10) : size;
    size_t cap = 2 * m + 64 * 1024; uint8_t *e = malloc(cap), *o = malloc(m ? m : 1); int dec;
    if (e && o && m) {
        size_t z = fz_encode(&x, which, data, m, e, cap, fl, &dec);
        if (z) {
            if (fz_decode(&x, dec, e, z, o, m) || memcmp(o, data, m)) __builtin_trap();   /* lossy round trip */
            /* damaged valid streams: 16 of them, 2 for the decoders with a costly model setup (audio, GR6+, FASTQ) */
            size_t kmax = dec == 1 || dec == 7 || dec == 12 ? 4 : 32;
            for (size_t k = 0; k + 1 < size && k < kmax; k += 2) {
                e[(data[k] * 131u + k) % z] ^= (uint8_t)(data[k + 1] | 1);
                fz_decode(&x, dec, e, (fl & 0x40) ? z : (z > 1 ? 1 + data[k] * 257u % (z - 1) : z), o, m);
            }
        }
    }
    free(e); free(o); codec_ctx_free(&x); return 0;
}
