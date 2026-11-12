/* Native plane-context residual leaf: graph leaf codec C_NPLANE (15), written only in container version 9 and only
 * when the caller opts in (KOLMO_T_NATIVE_LEAF at balanced). Proposal and evidence:
 * docs/proposals/native-residual-leaf.md. Uses only Kolmo's own Huffman / rANS coders and the rANS table format.
 *
 * Payload (raw length rl known from the leaf header):
 *   fmt u8 = 1 | np u8 (1..8, rl % np == 0; plane length L = rl / np) | plane[0..np-1]
 *   plane := tag u8 | body
 *     'c' value u8                       L copies of value
 *     's' L raw bytes
 *     'h' len uv | existing Huffman stream (huf_compress_all)
 *     'r' len uv | existing rANS stream (rans_compress_all)
 *     'l' len uv | lanes context stream (previous-byte context in 4 independent lanes; L >= 8)
 *     'p' len uv | cross-plane context stream (context = same index in plane k-1; k >= 1)
 * Context stream: mask u16 LE (buckets 0..8 present; others zero; nonzero) | per present bucket a 256-entry frequency
 *   table in the rANS table format (freq uv, or 0 then (further absent symbols - 1) u8; sum 2^16; a single symbol has
 *   2^16) | z uv (multiple of 4, >= 32, == rest of the body) | 4 initial 64-bit states (LE) | 32-bit LE words read
 *   forwards. Bucket of a context byte c: 0 if c == 0, else its bit length (1..8). Each state ends at 2^31.
 *   Lanes: L4 = L / 4; lane j = positions j*L4 .. j*L4+L4-1, decoded in lockstep (state j); then the tail positions
 *   4*L4 + j with state j. Context resets to bucket 0 at position 0 and at each lane start; otherwise previous byte.
 *   Cross-plane: position i uses state i mod 4, in order.
 * Maximum expansion: rl + 2 + np (each plane can fall back to stored). Decoder scratch: one 64 KiB slot table per
 * present bucket (at most 9), allocated per call. */
#define NP_CK 9
static void codec_ctx_free(codec_ctx *x);
#define NP_MAXP 8
static inline int np_ctx(uint8_t c) { return c ? 32 - __builtin_clz((unsigned)c) : 0; }
static size_t np_bound(size_t n) { return n + 2 + NP_MAXP + 16; }
static size_t np_uvlen(uint64_t v) { size_t k = 1; while (v >= 128) { v >>= 7; k++; } return k; }
static uint8_t *np_putuv(uint8_t *h, uint64_t v) { for (;; v >>= 7) { *h++ = (uint8_t)(v | (v >= 128 ? 128 : 0)); if (v < 128) break; } return h; }

/* context of position pos: lanes (cv == NULL) or cross-plane (cv = plane k-1) */
static inline int np_ctx_at(const uint8_t *src, const uint8_t *cv, size_t pos, size_t L4) {
    if (cv) return np_ctx(cv[pos]);
    if (pos == 0 || (L4 && pos < 4 * L4 && pos % L4 == 0)) return 0;
    return np_ctx(src[pos - 1]);
}
typedef struct { uint32_t cnt[NP_CK][256], f[NP_CK][256]; r64sym es[NP_CK][256]; uint32_t *words; size_t cw; } np_enc;

/* one context stream for a plane of m bytes; returns its length or 0 (does not apply / does not fit) */
static size_t np_ctx_encode(np_enc *E, const uint8_t *src, const uint8_t *cv, size_t m, uint8_t *out, size_t cap) {
    if (m < 8 || m + 64 > E->cw) return 0;
    size_t L4 = cv ? 0 : m / 4; uint32_t tot[NP_CK] = {0}; unsigned mask = 0;
    memset(E->cnt, 0, sizeof E->cnt);
    for (size_t q = 0; q < m; q++) { int c = np_ctx_at(src, cv, q, L4); E->cnt[c][src[q]]++; tot[c]++; }
    for (int c = 0; c < NP_CK; c++) {
        if (!tot[c]) continue;
        mask |= 1u << c; int used = 0, only = 0; for (int s = 0; s < 256; s++) if (E->cnt[c][s]) { used++; only = s; }
        if (used == 1) { memset(E->f[c], 0, sizeof E->f[c]); E->f[c][only] = RANS_M; }
        else if (rans_norm(E->cnt[c], tot[c], E->f[c])) return 0;
        uint32_t st = 0; for (int s = 0; s < 256; s++) { if (E->f[c][s]) r64sym_init(&E->es[c][s], st, E->f[c][s]); st += E->f[c][s]; }
    }
    uint32_t *e = E->words + E->cw; uint64_t X[4] = {R64_L, R64_L, R64_L, R64_L};
    for (size_t v = m; v-- > 0;) {                            /* reverse decode order */
        size_t pos; int sti;
        if (cv) { pos = v; sti = (int)(v & 3); }
        else if (v < 4 * L4) { pos = (v & 3) * L4 + (v >> 2); sti = (int)(v & 3); }
        else { pos = v; sti = (int)(v - 4 * L4); }
        const r64sym *y = &E->es[np_ctx_at(src, cv, pos, L4)][src[pos]]; uint64_t xmax = ((R64_L >> RANS_SB) << 32) * y->freq;
        if (X[sti] >= xmax) { *--e = (uint32_t)X[sti]; X[sti] >>= 32; }
        X[sti] = X[sti] + y->bias + (r64_mulhi(X[sti], y->rcp) >> y->shift) * y->cmpl;
    }
    for (int k = 3; k >= 0; k--) { *--e = (uint32_t)(X[k] >> 32); *--e = (uint32_t)X[k]; }
    size_t zw = (size_t)(E->words + E->cw - e), z = zw * 4;
    if (cap < 2 + (size_t)NP_CK * 3 * 256 + 16 + z) return 0;
    uint8_t *h = out; *h++ = (uint8_t)mask; *h++ = (uint8_t)(mask >> 8);
    for (int c = 0; c < NP_CK; c++) {
        if (!(mask >> c & 1)) continue;
        for (int k = 0; k < 256;) {
            if (E->f[c][k]) { h = np_putuv(h, E->f[c][k]); k++; continue; }
            int r = 1; while (k + r < 256 && !E->f[c][k + r]) r++;
            *h++ = 0; *h++ = (uint8_t)(r - 1); k += r;
        }
    }
    h = np_putuv(h, z);
    for (size_t i = 0; i < zw; i++) { uint32_t w = e[i]; h[4 * i] = (uint8_t)w; h[4 * i + 1] = (uint8_t)(w >> 8); h[4 * i + 2] = (uint8_t)(w >> 16); h[4 * i + 3] = (uint8_t)(w >> 24); }
    return (size_t)(h - out) + z;
}

typedef struct { uint32_t f[NP_CK][256], c[NP_CK][257]; uint8_t have[NP_CK]; uint8_t *sym; } np_dec;
static int np_ctx_decode(np_dec *D, const uint8_t *in, size_t cn, uint8_t *w, const uint8_t *cv, size_t m) {
    size_t i = 2; uint64_t v, z;
    if (cn < 2 || m < 8) return -1;
    unsigned mask = in[0] | (unsigned)in[1] << 8; if (!mask || mask >> NP_CK) return -1;
    for (int b = 0; b < NP_CK; b++) {
        D->have[b] = (uint8_t)(mask >> b & 1);
        if (!D->have[b]) continue;
        D->c[b][0] = 0;
        for (int k = 0; k < 256;) {
            if (kgu_get(in, cn, &i, &v) || v > RANS_M) return -1;
            if (v) { D->f[b][k] = (uint32_t)v; D->c[b][k + 1] = D->c[b][k] + D->f[b][k]; if (D->c[b][k + 1] > RANS_M) return -1; k++; continue; }
            if (i >= cn) return -1;
            int r = 1 + in[i++]; if (k + r > 256) return -1;
            for (int q = 0; q < r; q++) { D->f[b][k] = 0; D->c[b][k + 1] = D->c[b][k]; k++; }
        }
        if (D->c[b][256] != RANS_M) return -1;
        uint8_t *sy = D->sym + (size_t)b * RANS_M;
        for (int k = 0; k < 256; k++) if (D->f[b][k]) memset(sy + D->c[b][k], k, D->f[b][k]);
    }
    if (kgu_get(in, cn, &i, &z) || z < 32 || z % 4 || z != cn - i) return -1;
    const uint8_t *p = in + i, *pe = p + z; uint64_t X[4];
    for (int k = 0; k < 4; k++) { X[k] = (uint64_t)r64_rd(p) | (uint64_t)r64_rd(p + 4) << 32; p += 8; if (X[k] < R64_L) return -1; }
#define NP_GET(S, C, POS) do { int c_ = (C); if (!D->have[c_]) return -1; uint32_t sl = (uint32_t)X[S] & (RANS_M - 1); \
        uint8_t k_ = D->sym[(size_t)c_ * RANS_M + sl]; X[S] = (uint64_t)D->f[c_][k_] * (X[S] >> RANS_SB) + sl - D->c[c_][k_]; \
        if (X[S] < R64_L) { if (p >= pe) return -1; X[S] = (X[S] << 32) | r64_rd(p); p += 4; } w[POS] = k_; } while (0)
    if (cv) {
        size_t q = 0;
        for (; q + 3 < m; q += 4) { NP_GET(0, np_ctx(cv[q]), q); NP_GET(1, np_ctx(cv[q + 1]), q + 1); NP_GET(2, np_ctx(cv[q + 2]), q + 2); NP_GET(3, np_ctx(cv[q + 3]), q + 3); }
        for (int s = 0; q < m; q++, s++) NP_GET(s, np_ctx(cv[q]), q);
    } else {
        size_t L4 = m / 4; int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        for (size_t t = 0; t < L4; t++) {
            NP_GET(0, c0, t); c0 = np_ctx(w[t]);
            NP_GET(1, c1, L4 + t); c1 = np_ctx(w[L4 + t]);
            NP_GET(2, c2, 2 * L4 + t); c2 = np_ctx(w[2 * L4 + t]);
            NP_GET(3, c3, 3 * L4 + t); c3 = np_ctx(w[3 * L4 + t]);
        }
        for (size_t q = 4 * L4; q < m; q++) NP_GET((int)(q - 4 * L4), np_ctx(w[q - 1]), q);
    }
#undef NP_GET
    return p == pe && X[0] == R64_L && X[1] == R64_L && X[2] == R64_L && X[3] == R64_L ? 0 : -1;
}

/* encoder: per plane the smallest of constant, stored, Huffman, rANS, lanes context, cross-plane context.
 * Returns the payload length, or 0 when it does not apply or does not fit. */
static size_t np_compress(codec_ctx *x, const uint8_t *s, size_t n, int np, uint8_t *out, size_t cap) {
    if (np < 1 || np > NP_MAXP || !n || n % (size_t)np || cap < np_bound(n)) return 0;
    size_t L = n / (size_t)np, o = 2, ret = 0;
    np_enc *E = malloc(sizeof *E); uint8_t *cand = malloc(L + L / 8 + 4096), *best = malloc(L + L / 8 + 4096);
    if (!E || !cand || !best) goto done;
    E->cw = L + 64; E->words = malloc(E->cw * 4); if (!E->words) goto done;
    out[0] = 1; out[1] = (uint8_t)np;
    for (int k = 0; k < np; k++) {
        const uint8_t *p = s + (size_t)k * L; int con = 1;
        for (size_t i = 1; i < L; i++) if (p[i] != p[0]) { con = 0; break; }
        if (con) { if (o + 2 > cap) goto done; out[o++] = 'c'; out[o++] = p[0]; continue; }
        char bt = 's'; size_t bl = L, bcost = 1 + L, cc = L + L / 8 + 4096;
        size_t h = codec_compress(x, C_HUF, 0, p, L, cand, cc);
        if (h && 1 + np_uvlen(h) + h < bcost) { memcpy(best, cand, h); bt = 'h'; bl = h; bcost = 1 + np_uvlen(h) + h; }
        size_t r = codec_compress(x, C_RANS, 0, p, L, cand, cc);
        if (r && 1 + np_uvlen(r) + r < bcost) { memcpy(best, cand, r); bt = 'r'; bl = r; bcost = 1 + np_uvlen(r) + r; }
        size_t l = np_ctx_encode(E, p, NULL, L, cand, cc);
        if (l && 1 + np_uvlen(l) + l < bcost) { memcpy(best, cand, l); bt = 'l'; bl = l; bcost = 1 + np_uvlen(l) + l; }
        if (k > 0) {
            size_t c2 = np_ctx_encode(E, p, p - L, L, cand, cc);
            if (c2 && 1 + np_uvlen(c2) + c2 < bcost) { memcpy(best, cand, c2); bt = 'p'; bl = c2; bcost = 1 + np_uvlen(c2) + c2; }
        }
        if (o + bcost > cap) goto done;
        out[o++] = (uint8_t)bt;
        if (bt == 's') { memcpy(out + o, p, L); o += L; }
        else { o = (size_t)(np_putuv(out + o, bl) - out); memcpy(out + o, best, bl); o += bl; }
    }
    ret = o;
done:
    if (E) free(E->words);
    free(E); free(cand); free(best); return ret;
}

static int np_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    if (cn < 2 || in[0] != 1 || in[1] < 1 || in[1] > NP_MAXP || !n || n % in[1]) return -1;
    int np = in[1]; size_t L = n / (size_t)np, i = 2; int ret = -1;
    np_dec *D = NULL;
    for (int k = 0; k < np; k++) {
        uint8_t *o = out + (size_t)k * L; uint64_t len;
        if (i >= cn) goto done;
        int tag = in[i++];
        if (tag == 'c') { if (i >= cn) goto done; memset(o, in[i++], L); continue; }
        if (tag == 's') { if (cn - i < L) goto done; memcpy(o, in + i, L); i += L; continue; }
        if (kgu_get(in, cn, &i, &len) || len > cn - i) goto done;
        if (tag == 'h') { if (codec_decompress(x, C_HUF, in + i, (size_t)len, o, L)) goto done; }
        else if (tag == 'r') { if (codec_decompress(x, C_RANS, in + i, (size_t)len, o, L)) goto done; }
        else if (tag == 'l' || (tag == 'p' && k > 0)) {
            if (!D) { D = malloc(sizeof *D); if (!D) goto done; D->sym = malloc((size_t)NP_CK * RANS_M); if (!D->sym) goto done; }
            if (np_ctx_decode(D, in + i, (size_t)len, o, tag == 'p' ? o - L : NULL, L)) goto done;
        } else goto done;
        i += (size_t)len;
    }
    ret = i == cn ? 0 : -1;
done:
    if (D) free(D->sym);
    free(D); return ret;
}

/* test and fuzz entry points (tests/kolmotest.c, fuzz/fuzz_nplane.c) */
size_t np_encode_pub(const uint8_t *s, size_t n, int np, uint8_t *out, size_t cap) {
    codec_ctx x = {0}; size_t r = np_compress(&x, s, n, np, out, cap); codec_ctx_free(&x); return r;
}
int np_decode_pub(const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    codec_ctx x = {0}; x.allow_nplane = 1; int r = np_decompress(&x, in, cn, out, n); codec_ctx_free(&x); return r;
}
size_t np_bound_pub(size_t n) { return np_bound(n); }
