/* Codec 23: zero-run leaf (graph leaves; container version 11). Research P2-05.
 *
 * Sparse residuals (most bytes zero, the rest scattered) are split by src/algo/zerorun.c into the lengths of the zero
 * runs and the non-zero bytes; each of the two streams is coded by the codec that codes it smallest.
 *
 * Payload: literal count k uv | runs: codec u8 | raw length uv | coded length uv | coded bytes |
 *          literals: codec u8 | coded length uv | coded bytes (raw length k).
 * The sub-codecs are leaf codecs other than 12, 15, 16, 17, 19, 20, 21 and 23.
 * Part of the single translation unit built from src/kolmo.c; its codec table is at the end of the file. */
#include "../algo/zerorun.c"

static size_t zrun_put_sub(codec_ctx *x, leaf_sel_fn sel, const void *ctx, const uint8_t *p, size_t m, int with_raw, uint8_t *out, size_t cap) {
    size_t zc = m + m / 8 + 1024; uint8_t *t = malloc(zc); int cc = C_STORE; size_t z = 0;
    if (!t) return 0;
    if (m) z = sel(x, ctx, p, m, t, zc, &cc);
    if (!z || z > m) { cc = C_STORE; z = m; if (m) memcpy(t, p, m); }
    if (cc == C_ZRUN || cc == C_PLANES) { free(t); return 0; }
    uint8_t h[21]; size_t hn = 0; h[hn++] = (uint8_t)cc;
    if (with_raw) for (uint64_t v = m;; v >>= 7) { h[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
    for (uint64_t v = z;; v >>= 7) { h[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
    if (cap < hn + z) { free(t); return 0; }
    memcpy(out, h, hn); memcpy(out + hn, t, z); free(t); return hn + z;
}

static size_t zrun_compress(codec_ctx *x, const uint8_t *in, size_t n, uint8_t *out, size_t cap, leaf_sel_fn sel, const void *ctx) {
    size_t k = zr_count_nonzero(in, n), rcap = (k + 1) * 10, rl, ll, o = 0;
    uint8_t *runs = malloc(rcap), *lits = malloc(k ? k : 1);
    if (!runs || !lits || zr_split(in, n, runs, rcap, &rl, lits, k ? k : 1, &ll)) { free(runs); free(lits); return 0; }
    for (uint64_t v = k;; v >>= 7) { if (o >= cap) goto fail; out[o++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
    size_t a = zrun_put_sub(x, sel, ctx, runs, rl, 1, out + o, cap - o); if (!a) goto fail; o += a;
    size_t b = zrun_put_sub(x, sel, ctx, lits, ll, 0, out + o, cap - o); if (!b) goto fail; o += b;
    free(runs); free(lits); return o;
fail:
    free(runs); free(lits); return 0;
}

static int zrun_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0; uint64_t k, rl, rz, lz; int rc = -1;
    if (kgu_get(in, cn, &i, &k) || k > n || i >= cn) return -1;
    int c1 = in[i++];
    if (!leaf_sub_codec_ok(c1) || c1 == C_ZRUN || kgu_get(in, cn, &i, &rl) || rl > (k + 1) * 10 || kgu_get(in, cn, &i, &rz) || rz > cn - i) return -1;
    uint8_t *runs = malloc(rl ? (size_t)rl : 1), *lits = malloc(k ? (size_t)k : 1);
    if (!runs || !lits || codec_decompress(x, c1, in + i, (size_t)rz, runs, (size_t)rl)) goto out;
    i += (size_t)rz; if (i >= cn) goto out;
    int c2 = in[i++];
    if (!leaf_sub_codec_ok(c2) || c2 == C_ZRUN || kgu_get(in, cn, &i, &lz) || lz != cn - i) goto out;
    if (codec_decompress(x, c2, in + i, (size_t)lz, lits, (size_t)k)) goto out;
    rc = zr_join(runs, (size_t)rl, lits, (size_t)k, out, n);
out:
    free(runs); free(lits); return rc;
}

static int zrun_dec_tab(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { return x->allow_f2 ? zrun_decompress(x, in, cn, out, n) : -1; }
static const kolmo_codec codec_zrun = { C_ZRUN, "zrun", NULL, NULL, zrun_dec_tab, NULL };   /* written by the graph leaves */
