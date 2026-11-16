/* Static order-0 rANS (graph leaves): near-entropy coding of skewed bytes, where Huffman's whole-bit code lengths
 * lose (an exponent plane at 2.6 bits per byte). Chunks of up to 1 MiB: raw length uv, frequency table, coded length uv, then the bytes: a 32-bit state (4 bytes, little-endian) and the
 * renormalisation bytes, read forwards. Table: per symbol its frequency uv; a 0 is followed by the count of further
 * absent symbols uv. Frequencies are scaled to 2^16. */
#define RANS_SB 16
#define RANS_M (1u << RANS_SB)
#define RANS_L (1u << 23)
#define RANS_CHUNK (1u << 20)
static int rans_norm(const uint32_t *cnt, size_t n, uint32_t *f) {   /* scale counts to sum RANS_M, present symbols >= 1 */
    uint64_t tot = 0; int used = 0;
    for (int s = 0; s < 256; s++) { f[s] = cnt[s] ? (uint32_t)(((uint64_t)cnt[s] * RANS_M) / n) : 0; if (cnt[s] && !f[s]) f[s] = 1; tot += f[s]; used += cnt[s] != 0; }
    if (used < 2) return -1;
    while (tot != RANS_M) {                                   /* adjust the largest (or the cheapest to change) */
        int b = -1; double bc = 1e300;
        for (int s = 0; s < 256; s++) {
            if (!cnt[s]) continue;
            if (tot > RANS_M && f[s] <= 1) continue;
            double c = tot > RANS_M ? (double)cnt[s] * log2((double)f[s] / (f[s] - 1)) : -(double)cnt[s] * log2((double)(f[s] + 1) / f[s]);
            if (c < bc) { bc = c; b = s; }
        }
        if (b < 0) return -1;
        if (tot > RANS_M) { uint64_t d = tot - RANS_M, room = f[b] - 1, k = d < room ? d : room; if (k > (cnt[b] / 64) + 1) k = (cnt[b] / 64) + 1; if (k > room) k = room; f[b] -= (uint32_t)k; tot -= k; }
        else { uint64_t d = RANS_M - tot, k = d < (cnt[b] / 64) + 1 ? d : (cnt[b] / 64) + 1; f[b] += (uint32_t)k; tot += k; }
    }
    return 0;
}
/* rans64 (ryg): 64-bit state, 32-bit renormalisation words, reciprocal encoding; four interleaved states */
#define R64_L (1ull << 31)
typedef struct { uint64_t rcp; uint32_t freq, bias, cmpl, shift; } r64sym;
static void r64sym_init(r64sym *e, uint32_t start, uint32_t f) {
    e->freq = f; e->cmpl = RANS_M - f;
    if (f < 2) { e->rcp = ~0ull; e->shift = 0; e->bias = start + RANS_M - 1; }
    else {
        uint32_t sh = 0; while (f > (1u << sh)) sh++;
        uint64_t x0 = f - 1, x1 = 1ull << (sh + 31), t1 = x1 / f; x0 += (x1 % f) << 32; uint64_t t0 = x0 / f;
        e->rcp = t0 + (t1 << 32); e->shift = sh - 1; e->bias = start;
    }
}
static inline uint64_t r64_mulhi(uint64_t a, uint64_t b) { return (uint64_t)(((unsigned __int128)a * b) >> 64); }
/* single != 0: a chunk of one byte value gets that symbol at frequency RANS_M (research P-18 / P2-20 on PR #7). The
 * state update is then the identity, the chunk codes as its four initial states (32 bytes), and the decoder below has
 * always accepted it (frequencies up to RANS_M, sum RANS_M, final states R64_L). Used for plane-aligned leaves only;
 * whole leaves keep rejecting such chunks (the other codecs code them in a few bytes). */
static size_t rans_compress_all_x(const uint8_t *in, size_t n, uint8_t *out, size_t cap, int single) {
    size_t o = 0, cw = (n < RANS_CHUNK ? n : RANS_CHUNK) + 64; uint32_t *tmp = malloc(cw * 4); if (!tmp) return 0;   /* 32-bit words, filled backwards (at most one per symbol) */
    for (size_t i = 0; i < n;) {
        size_t m = n - i < RANS_CHUNK ? n - i : RANS_CHUNK; const uint8_t *src = in + i;
        uint32_t cnt[256], f[256], c = 0; r64sym es[256]; kg_hist(src, m, cnt);
        if (single && cnt[src[0]] == m) { memset(f, 0, sizeof f); f[src[0]] = RANS_M; }
        else if (rans_norm(cnt, m, f)) { free(tmp); return 0; }    /* one symbol: leave it to the other codecs */
        for (int k = 0; k < 256; k++) { if (f[k]) r64sym_init(&es[k], c, f[k]); c += f[k]; }
        uint32_t *e = tmp + cw; uint64_t x0 = R64_L, x1 = R64_L;   /* states for even and odd positions, back to front */
#define R64_PUT(X, Q) do { const r64sym *y = &es[src[Q]]; uint64_t xmax = ((R64_L >> RANS_SB) << 32) * y->freq; \
            if (X >= xmax) { *--e = (uint32_t)X; X >>= 32; } X = X + y->bias + (r64_mulhi(X, y->rcp) >> y->shift) * y->cmpl; } while (0)
        uint64_t x2 = R64_L, x3 = R64_L; size_t q = m;            /* four states (position mod 4); at most one word per symbol */
        while (q & 3) { q--; switch (q & 3) { case 0: R64_PUT(x0, q); break; case 1: R64_PUT(x1, q); break; default: R64_PUT(x2, q); } }
        while (q) { q -= 4; R64_PUT(x3, q + 3); R64_PUT(x2, q + 2); R64_PUT(x1, q + 1); R64_PUT(x0, q); }
#undef R64_PUT
        uint64_t x[4] = {x0, x1, x2, x3};
        if (e - tmp < 8) { free(tmp); return 0; }
        for (int k = 3; k >= 0; k--) { *--e = (uint32_t)(x[k] >> 32); *--e = (uint32_t)x[k]; }   /* x0 lo, x0 hi, x1 lo, ... */
        size_t zw = (size_t)(tmp + cw - e), z = zw * 4;
        if (cap - o < 16 + 256 * 3 + z) { free(tmp); return 0; }
        uint8_t *h = out + o;
        for (uint64_t v = m;; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (int k = 0; k < 256;) {
            if (f[k]) { for (uint64_t v = f[k];; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; } k++; continue; }
            int r = 1; while (k + r < 256 && !f[k + r]) r++;
            *h++ = 0; *h++ = (uint8_t)(r - 1); k += r;              /* r - 1 < 256 */
        }
        for (uint64_t v = z;; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; }
        for (size_t q = 0; q < zw; q++) { uint32_t w = e[q]; h[4 * q] = (uint8_t)w; h[4 * q + 1] = (uint8_t)(w >> 8); h[4 * q + 2] = (uint8_t)(w >> 16); h[4 * q + 3] = (uint8_t)(w >> 24); }
        o = (size_t)(h - out) + z; i += m;
    }
    free(tmp); return o;
}
static size_t rans_compress_all(const uint8_t *in, size_t n, uint8_t *out, size_t cap) { return rans_compress_all_x(in, n, out, cap, 0); }
static inline uint32_t r64_rd(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int rans_decompress_all(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    size_t i = 0, o = 0; uint8_t *sym = malloc(RANS_M); if (!sym) return -1;     /* slot -> symbol: 64 KiB, stays in cache */
    while (o < n) {
        uint64_t m, v, z; uint32_t f[256], c[257]; c[0] = 0;
        if (kgu_get(in, cn, &i, &m) || m == 0 || m > RANS_CHUNK || m > n - o) goto bad;
        for (int k = 0; k < 256;) {
            if (kgu_get(in, cn, &i, &v) || v > RANS_M) goto bad;
            if (v) { f[k] = (uint32_t)v; c[k + 1] = c[k] + f[k]; if (c[k + 1] > RANS_M) goto bad; k++; continue; }
            if (i >= cn) goto bad;
            int r = 1 + in[i++]; if (k + r > 256) goto bad;
            for (int q = 0; q < r; q++) { f[k] = 0; c[k + 1] = c[k]; k++; }
        }
        if (c[256] != RANS_M || kgu_get(in, cn, &i, &z) || z < 32 || z % 4 || z > cn - i) goto bad;
        /* one symbol at frequency RANS_M (a constant plane of a plane-aligned leaf): the state update below is the
         * identity and reads no word, so the loop accepts exactly the chunks whose body is the four states, all
         * R64_L, and writes m copies of the symbol. Same acceptance and output, as a fill (research R4, PR #7). */
        int one = -1; for (int k = 0; k < 256; k++) if (f[k] == RANS_M) one = k;
        if (one >= 0) {
            if (z != 32) goto bad;
            for (int k = 0; k < 4; k++) if (((uint64_t)r64_rd(in + i + 8 * k) | (uint64_t)r64_rd(in + i + 8 * k + 4) << 32) != R64_L) goto bad;
            memset(out + o, one, m); i += z; o += m; continue;
        }
        for (int k = 0; k < 256; k++) if (f[k]) memset(sym + c[k], k, f[k]);
        const uint8_t *p = in + i, *pe = p + z; uint64_t x[4];
        for (int k = 0; k < 4; k++) { x[k] = (uint64_t)r64_rd(p) | (uint64_t)r64_rd(p + 4) << 32; p += 8; if (x[k] < R64_L) goto bad; }
        uint8_t *w = out + o; uint64_t x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
#define R64_GET(X, Q) do { uint32_t sl = (uint32_t)X & (RANS_M - 1); uint8_t k = sym[sl]; w[Q] = k; \
            X = (uint64_t)f[k] * (X >> RANS_SB) + sl - c[k]; if (X < R64_L) { if (p >= pe) goto bad; X = (X << 32) | r64_rd(p); p += 4; } } while (0)
        size_t q = 0;
        for (; q + 3 < m; q += 4) { R64_GET(x0, q); R64_GET(x1, q + 1); R64_GET(x2, q + 2); R64_GET(x3, q + 3); }
        if (q < m) R64_GET(x0, q);
        if (q + 1 < m) R64_GET(x1, q + 1);
        if (q + 2 < m) R64_GET(x2, q + 2);
#undef R64_GET
        if (p != pe || x0 != R64_L || x1 != R64_L || x2 != R64_L || x3 != R64_L) goto bad;   /* back where the encoder started */
        i += z; o += m;
    }
    free(sym); return i == cn ? 0 : -1;
bad: free(sym); return -1;
}
static size_t rans_codec_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; (void)level; return rans_compress_all(in, n, out, cap);
}
static int rans_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { (void)x; return rans_decompress_all(in, cn, out, n); }
static const kolmo_codec codec_rans = { C_RANS, "rans", NULL, rans_codec_compress, rans_codec_decompress, NULL };
