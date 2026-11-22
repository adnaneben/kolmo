/* Codec 22: Huffman then zstd (graph leaves; container version 11).
 *
 * The leaf is first coded by the Huffman-only coder (codec 13, chunks of up to 128 KiB), and the Huffman stream is
 * then coded by zstd. zstd finds repeats that survive the Huffman code (the same chunk table, runs of a constant
 * chunk, repeated code words at the same bit phase); research S1 "H->Z" on PR #7.
 *
 * Payload: Huffman length uv | zstd frame of the Huffman stream (single frame, no dictionary).
 * Part of the single translation unit built from src/kolmo.c; its codec table is at the end of the file. */

/* code an already Huffman-coded stream (huf, hl bytes) with zstd at `level`; returns the payload length or 0 */
static size_t hz_compress_huf(codec_ctx *x, int level, const uint8_t *huf, size_t hl, uint8_t *out, size_t cap) {
    uint8_t h[10]; size_t hn = 0;
    for (uint64_t v = hl;; v >>= 7) { h[hn++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
    if (cap <= hn) return 0;
    if (!x->zc) x->zc = ZSTD_createCCtx();
    if (!x->zc) return 0;
    size_t r = ZSTD_compressCCtx(x->zc, out + hn, cap - hn, huf, hl, level);
    if (ZSTD_isError(r)) return 0;
    memcpy(out, h, hn); return hn + r;
}

static int hz_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0; uint64_t hl;
    /* a Huffman stream of n bytes is at most n plus a few bytes per 128 KiB chunk */
    if (kgu_get(in, cn, &i, &hl) || hl == 0 || hl > n + n / 1024 + 64) return -1;
    uint8_t *t = malloc((size_t)hl); if (!t) return -1;
    if (!x->zd) x->zd = ZSTD_createDCtx();
    size_t r = x->zd ? ZSTD_decompressDCtx(x->zd, t, (size_t)hl, in + i, cn - i) : (size_t)-1;
    int rc = !x->zd || ZSTD_isError(r) || r != hl ? -1 : huf_decompress_all(t, (size_t)hl, out, n);
    free(t); return rc;
}

/* encoder rule: Huffman streams up to 64 KiB take zstd level 19 (or the leaf's higher level) directly; larger ones
 * zstd at the `probe` level over the Huffman stream first (cheap: the leaf's own zstd level up to 9, else 1); the given level only when that probe finds
 * repeats (at least 1% smaller than the Huffman stream) and comes within 5% of `best` (the leaf's current smallest
 * coding; 0 = none). Returns the smaller payload written to out, or 0.
 * `fast` (balanced, zstd at the fast levels): every stream is probed at level 1 first, and level HZ_FAST_LEVEL follows
 * only when the probe is already no larger than `best`. Level 19 cost 30-60 ms per 64 KiB stream and ran on every
 * leaf: balanced compressed DEC64 float arrays 9x slower than rc2 (benchmark/rc3w2/regress/RESULTS.md); level 5 keeps
 * 67% of the format-2 gain on them at 1.36x rc2's instructions, level 9 76% at 1.46x. */
#ifndef HZ_FAST_LEVEL
#define HZ_FAST_LEVEL 5
#endif
static size_t hz_try(codec_ctx *x, int probe, int level, const uint8_t *huf, size_t hl, uint8_t *out, size_t cap, size_t best, int fast) {
    if (fast) {
        size_t q1 = hz_compress_huf(x, 1, huf, hl, out, cap);
        if (!q1 || (uint64_t)q1 * 100 > (uint64_t)hl * 99 || (best && q1 > best)) return q1;
        uint8_t *u = malloc(cap); size_t qz = u ? hz_compress_huf(x, HZ_FAST_LEVEL, huf, hl, u, cap) : 0;
        if (qz && qz < q1) { memcpy(out, u, qz); q1 = qz; }
        free(u); return q1;
    }
    if (hl <= (64u << 10)) return hz_compress_huf(x, level > 19 ? level : 19, huf, hl, out, cap);   /* small: the strong level at once */
    size_t q1 = hz_compress_huf(x, probe, huf, hl, out, cap);
    if (!q1 || level <= probe || (uint64_t)q1 * 100 > (uint64_t)hl * 99 || (best && (uint64_t)q1 * 100 >= (uint64_t)best * 105)) return q1;
    uint8_t *u = malloc(cap); size_t qz = u ? hz_compress_huf(x, level, huf, hl, u, cap) : 0;
    if (qz && qz < q1) { memcpy(out, u, qz); q1 = qz; }
    free(u); return q1;
}

static int hz_dec_tab(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { return x->allow_f2 ? hz_decompress(x, in, cn, out, n) : -1; }
static const kolmo_codec codec_hz = { C_HZ, "hz", NULL, NULL, hz_dec_tab, NULL };   /* written by the graph leaves */
