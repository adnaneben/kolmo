/* Format 2 (container version 11): codecs 19-23.
 *   - zero-run split and join (src/algo/zerorun.c) on edge cases and random streams, and rejection of bad splits;
 *   - leaf codecs 21 (per-plane), 22 (Huffman then zstd) and 23 (zero runs): round trips through codec_decompress,
 *     refusal without version 11, and damaged payloads (no crash; a decode that succeeds is caught by the block
 *     checksum at container level);
 *   - whole frames of integer arrays (graph leaves), a 16-bit PCM WAV (codec 19) and an 8-bit Y4M video (codec 20) in
 *     every mode: version 11, determinism, round trip, range reads, file API, damaged frames;
 *   - a version-11 frame whose header says version 10 is refused. */
#include "../src/kolmo.c"
#include <stdio.h>
#include <fcntl.h>

static uint64_t st = 0x9e3779b97f4a7c15ULL;
static uint64_t rv(void) { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return st; }
#define FAIL(...) do { fprintf(stderr, "test_format2: " __VA_ARGS__); fprintf(stderr, " (line %d)\n", __LINE__); return 1; } while (0)

static int zr_case(const uint8_t *p, size_t n) {
    size_t k = zr_count_nonzero(p, n), rl, ll; uint8_t *r = malloc((k + 1) * 10), *l = malloc(k + 1), *o = malloc(n + 1);
    int bad = !r || !l || !o || zr_split(p, n, r, (k + 1) * 10, &rl, l, k + 1, &ll) || ll != k || zr_join(r, rl, l, ll, o, n) || (n && memcmp(o, p, n));
    if (!bad && rl) { if (!zr_join(r, rl - 1, l, ll, o, n)) bad = 1; }            /* truncated runs */
    if (!bad && n && !zr_join(r, rl, l, ll, o, n - 1)) bad = 1;                       /* wrong length */
    if (!bad && k) { uint8_t c = l[0]; l[0] = 0; if (!zr_join(r, rl, l, ll, o, n)) bad = 1; l[0] = c; }   /* zero literal */
    free(r); free(l); free(o); return bad;
}

typedef size_t (*enc_fn)(codec_ctx *, const uint8_t *, size_t, uint8_t *, size_t);
static size_t enc_planes(codec_ctx *x, const uint8_t *p, size_t n, uint8_t *o, size_t c) { kgsel S = {C_ZSTD, 19, 1}; return planes_compress(x, 4, p, n, o, c, kg_sel_sub, &S); }
static size_t enc_zrun(codec_ctx *x, const uint8_t *p, size_t n, uint8_t *o, size_t c) { kgsel S = {C_LZMA, 9, 0}; return zrun_compress(x, p, n, o, c, kg_sel_sub, &S); }
static size_t enc_hz(codec_ctx *x, const uint8_t *p, size_t n, uint8_t *o, size_t c) {
    uint8_t *h = malloc(c); size_t hl = h ? huf_compress_all(p, n, h, c) : 0, z = hl ? hz_compress_huf(x, 19, h, hl, o, c) : 0; free(h); return z;
}

static int leaf_case(int codec, enc_fn enc, const uint8_t *p, size_t n) {
    codec_ctx x = {0}; size_t cap = n + n / 8 + 4096; uint8_t *c = malloc(cap), *o = malloc(n + 1), *d = malloc(cap); int rc = 1;
    if (!c || !o || !d) goto out;
    size_t z = enc(&x, p, n, c, cap);
    if (!z) { fprintf(stderr, "codec %d: no output\n", codec); goto out; }
    if (!codec_decompress(&x, codec, c, z, o, n)) { fprintf(stderr, "codec %d: accepted without version 11\n", codec); goto out; }
    x.allow_f2 = 1;
    if (codec_decompress(&x, codec, c, z, o, n) || memcmp(o, p, n)) { fprintf(stderr, "codec %d: round trip\n", codec); goto out; }
    for (int t = 0; t < 200; t++) {                             /* damage: no crash, no read or write out of bounds */
        memcpy(d, c, z); size_t q = (size_t)(rv() % z); d[q] ^= (uint8_t)(1 + rv() % 255);
        (void)codec_decompress(&x, codec, d, t & 1 ? z : (size_t)(rv() % z), o, n);
    }
    rc = 0;
out:
    codec_ctx_free(&x); free(c); free(o); free(d); return rc;
}

static size_t mk_npy(uint8_t *p, int w, size_t count) {
    char d[128]; int len = snprintf(d, sizeof d, "{'descr': '<i%d', 'fortran_order': False, 'shape': (%zu,), }", w, count);
    memset(p, ' ', 128); memcpy(p, "\x93NUMPY\1\0", 8); p[8] = 118; p[9] = 0; memcpy(p + 10, d, (size_t)len); p[127] = '\n'; return 128;
}
static size_t mk_wav(uint8_t *p, int ch, size_t frames, int odd) {
    size_t data = frames * 2 * (size_t)ch + (size_t)odd, o = 0;
    memcpy(p, "RIFF", 4); put32(p + 4, (uint32_t)(36 + data + 16)); memcpy(p + 8, "WAVEfmt ", 8); put32(p + 16, 16);
    p[20] = 1; p[21] = 0; p[22] = (uint8_t)ch; p[23] = 0; put32(p + 24, 44100); put32(p + 28, 44100 * 2 * (uint32_t)ch);
    p[32] = (uint8_t)(2 * ch); p[33] = 0; p[34] = 16; p[35] = 0; memcpy(p + 36, "data", 4); put32(p + 40, (uint32_t)data); o = 44;
    double a = 0, b = 0;
    for (size_t i = 0; i < frames; i++) for (int c = 0; c < ch; c++) {
        a += 0.031 + 0.0007 * c; b += 0.0013;
        int v = (int)(9000 * sin(a) + 3000 * sin(b * (c + 1)) + (int)(rv() % 64) - 32);
        p[o++] = (uint8_t)v; p[o++] = (uint8_t)(v >> 8);
    }
    if (odd) p[o++] = 0x55;
    memcpy(p + o, "LIST\x08\0\0\0INFOtest", 16); o += 16;
    return o;
}
static size_t mk_y4m(uint8_t *p, int W, int H, int frames) {
    size_t o = (size_t)sprintf((char *)p, "YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C420jpeg\n", W, H);
    for (int f = 0; f < frames; f++) {
        memcpy(p + o, "FRAME\n", 6); o += 6;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) p[o++] = (uint8_t)(((x + f * 2) * 3 + y * 5) / 4 + (rv() & 3));
        for (int k = 0; k < 2; k++) for (int y = 0; y < (H + 1) / 2; y++) for (int x = 0; x < (W + 1) / 2; x++) p[o++] = (uint8_t)(128 + k * 7 + ((x + y + f) & 15));
    }
    return o;
}

/* compress in a mode twice (deterministic), check the version, decode, range read, damage */
static int frame_case(const char *what, const uint8_t *s, size_t n, kolmo_mode m, int want_ver, int want_codec) {
    kolmo_params p = {m, 0, 2, 0, 1}; size_t cap = kolmo_bound(n, &p); uint8_t *f = malloc(cap), *g = malloc(cap), *o = malloc(n + 1);
    if (!f || !g || !o) FAIL("alloc");
    int64_t z = kolmo_compress(s, n, f, cap, &p); p.threads = 1; int64_t z2 = kolmo_compress(s, n, g, cap, &p);
    if (z <= 0 || z != z2 || memcmp(f, g, (size_t)z)) FAIL("%s mode %d: not deterministic across threads", what, m);
    if (want_ver && f[4] != want_ver) FAIL("%s mode %d: version %d, want %d", what, m, f[4], want_ver);
    if (want_codec >= 0) { int has = 0; for (uint32_t i = 0; i < get32(f + 20); i++) has |= f[HDR + i * IDX + 12] == want_codec; if (!has) FAIL("%s mode %d: no codec %d block", what, m, want_codec); }
    if (kolmo_decompress(f, (size_t)z, o, n, 3) != (int64_t)n || memcmp(o, s, n)) FAIL("%s mode %d: round trip", what, m);
    uint8_t rr[100]; uint64_t off = n / 3;
    if (kolmo_read_range(f, (size_t)z, off, 100, rr, 2) != 100 || memcmp(rr, s + off, 100)) FAIL("%s mode %d: range read", what, m);
    if (f[4] == 11) { memcpy(g, f, (size_t)z); g[4] = 10; uint32_t hc = kolmo_crc32c(0, g, 28); hc = kolmo_crc32c(hc, g + HDR, (size_t)get32(g + 20) * IDX); put32(g + 28, hc);
        if (kolmo_decompress(g, (size_t)z, o, n, 1) >= 0) FAIL("%s mode %d: version 11 content read as version 10", what, m); }
    for (int t = 0; t < 40; t++) {
        memcpy(g, f, (size_t)z); size_t q = HDR + (size_t)(rv() % (uint64_t)(z - HDR)); g[q] ^= (uint8_t)(1 + rv() % 255);
        int64_t got = kolmo_decompress(g, (size_t)z, o, n, 1);
        if (got >= 0 && (got != (int64_t)n || memcmp(o, s, n))) FAIL("%s mode %d: damaged frame decoded to other bytes", what, m);
    }
    free(f); free(g); free(o); return 0;
}

static int file_case(const char *what, const uint8_t *s, size_t n, kolmo_mode m) {
    char a[] = "/tmp/kf2aXXXXXX", b[] = "/tmp/kf2bXXXXXX", c[] = "/tmp/kf2cXXXXXX";
    int fa = mkstemp(a), fb = mkstemp(b), fc = mkstemp(c); kolmo_params p = {m, 0, 2, 0, 1}; int rc = 1;
    if (fa < 0 || fb < 0 || fc < 0 || write(fa, s, n) != (ssize_t)n) goto out;
    int64_t z = kolmo_compress_file(fa, fb, n, &p, NULL, 0);
    if (z <= 0 || kolmo_decompress_file(fb, fc, (size_t)z, 2, NULL, 0) != (int64_t)n) { fprintf(stderr, "%s: file API\n", what); goto out; }
    uint8_t *o = malloc(n); if (!o || pread(fc, o, n, 0) != (ssize_t)n || memcmp(o, s, n)) { free(o); fprintf(stderr, "%s: file API bytes\n", what); goto out; }
    free(o); rc = 0;
out:
    close(fa); close(fb); close(fc); unlink(a); unlink(b); unlink(c); return rc;
}

int main(void) {
    /* zero-run split */
    { uint8_t e[1]; if (zr_case(e, 0)) FAIL("zr empty"); }
    for (int t = 0; t < 300; t++) {
        size_t n = (size_t)(rv() % 5000); uint8_t *p = malloc(n + 1); int dens = (int)(rv() % 100);
        for (size_t i = 0; i < n; i++) p[i] = (int)(rv() % 100) < dens ? (uint8_t)(1 + rv() % 255) : 0;
        if (t == 1) memset(p, 0, n);
        if (zr_case(p, n)) FAIL("zr random %d n=%zu", t, n);
        free(p);
    }
    { size_t n = 1 << 20; uint8_t *p = calloc(n, 1); p[n - 1] = 9; if (zr_case(p, n)) FAIL("zr long run"); free(p); }
    /* leaf codecs on residual-like planes: sparse high planes, a constant plane, noisy low plane */
    size_t L = 70000, n = 4 * L; uint8_t *p = malloc(n);
    for (size_t i = 0; i < L; i++) { p[i] = (uint8_t)(rv() % 23); p[L + i] = i % 97 == 0 ? (uint8_t)(rv() % 4) : 0; p[2 * L + i] = 0; p[3 * L + i] = i % 5000 == 0; }
    if (leaf_case(C_PLANES, enc_planes, p, n)) return 1;
    if (leaf_case(C_ZRUN, enc_zrun, p + L, 3 * L)) return 1;
    if (leaf_case(C_HZ, enc_hz, p, n)) return 1;
    free(p);
    /* frames: integer arrays whose leaves take codecs 21-23 */
    size_t cnt = 60000; uint8_t *s = malloc(128 + cnt * 8);
    size_t h = mk_npy(s, 8, cnt); int64_t v = 1700000000000LL;
    for (size_t i = 0; i < cnt; i++) { v += (int64_t)(rv() % 7 == 0 ? 1000 : rv() % 5); memcpy(s + h + i * 8, &v, 8); }
    for (kolmo_mode m = KOLMO_RT; m <= KOLMO_MAX; m++) if (frame_case("i64 npy", s, h + cnt * 8, m, 0, -1)) return 1;
    h = mk_npy(s, 4, cnt);
    for (size_t i = 0; i < cnt; i++) { int32_t y = 1950 + (int32_t)(i / 70) % 70; memcpy(s + h + i * 4, &y, 4); }
    int v11 = 0;
    for (kolmo_mode m = KOLMO_FAST; m <= KOLMO_MAX; m++) {
        if (frame_case("year npy", s, h + cnt * 4, m, 0, C_GRAPH)) return 1;
        kolmo_params q = {m, 0, 1, 0, 1}; size_t cap = kolmo_bound(h + cnt * 4, &q); uint8_t *f = malloc(cap);
        if (kolmo_compress(s, h + cnt * 4, f, cap, &q) > 0 && f[4] == 11) v11++;
        free(f);
    }
    if (!v11) FAIL("year npy: no mode wrote a format-2 leaf");
    free(s);
    /* audio: mono and stereo, odd data chunk with a trailing chunk; small blocks so blocks start inside samples */
    uint8_t *w = malloc(44 + 2 * 2 * 300000 + 64);
    for (int ch = 1; ch <= 2; ch++) {
        size_t wn = mk_wav(w, ch, 150000 + 77 * (size_t)ch, ch == 1);
        for (kolmo_mode m = KOLMO_FAST; m <= KOLMO_MAX; m += 3) if (frame_case("wav", w, wn, m, 11, C_AUD)) return 1;
        kolmo_params p = {KOLMO_FAST, 0, 3, 65537, 1}; size_t cap = kolmo_bound(wn, &p); uint8_t *f = malloc(cap), *o = malloc(wn);
        int64_t z = kolmo_compress(w, wn, f, cap, &p);
        if (z <= 0 || kolmo_decompress(f, (size_t)z, o, wn, 3) != (int64_t)wn || memcmp(o, w, wn)) FAIL("wav, odd blocks");
        int aud = 0; for (uint32_t i = 0; i < get32(f + 20); i++) aud += f[HDR + i * IDX + 12] == C_AUD;
        if (aud < 2) FAIL("wav, odd blocks: %d audio blocks", aud);
        free(f); free(o);
        if (file_case("wav", w, wn, KOLMO_HC)) return 1;
    }
    /* a WAV that is not 16-bit PCM: no audio block */
    { size_t wn = mk_wav(w, 1, 50000, 0); w[34] = 24; kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(wn, &p); uint8_t *f = malloc(cap);
      int64_t z = kolmo_compress(w, wn, f, cap, &p); if (z <= 0) FAIL("24-bit wav");
      for (uint32_t i = 0; i < get32(f + 20); i++) if (f[HDR + i * IDX + 12] == C_AUD) FAIL("24-bit wav coded as audio");
      free(f); }
    free(w);
    /* video */
    uint8_t *y = malloc(200000);
    size_t yn = mk_y4m(y, 64, 48, 30);
    for (kolmo_mode m = KOLMO_FAST; m <= KOLMO_MAX; m++) if (frame_case("y4m", y, yn, m, 11, C_VID)) return 1;
    if (file_case("y4m", y, yn, KOLMO_FAST)) return 1;
    {   /* the layout on threads: the same bytes for 1-8 threads; the wavefront inverse on 1-8 threads; a bad predictor
         * byte (3, or the previous frame at frame 0) in any frame fails the inverse on every thread count, never hangs */
        size_t t1n, tn; uint8_t *t1 = kv_transform(y, yn, &t1n, 1), *o = malloc(yn);
        if (!t1 || !o) FAIL("kv_transform");
        for (int th = 2; th <= 8; th++) { uint8_t *t = kv_transform(y, yn, &tn, th); if (!t || tn != t1n || memcmp(t, t1, tn)) FAIL("kv_transform: %d threads differ", th); free(t); }
        for (int th = 1; th <= 8; th++) if (kv_inverse(t1, t1n, o, yn, th) || memcmp(o, y, yn)) FAIL("kv_inverse: %d threads", th);
        size_t hl = (size_t)t1[0] | (size_t)t1[1] << 8, nfl = 3 + 2 * 2;   /* 48 rows: 3 luma strips, 2 per chroma plane */
        for (int c = 0; c < 6; c++) {
            uint8_t *t = malloc(t1n); memcpy(t, t1, t1n);
            size_t fr = c < 3 ? (size_t)(29 - 13 * c) : (size_t)(c - 3) * 11, at = 4 + hl + fr * nfl + (size_t)c % nfl;
            t[at] = fr == 0 && c % 2 ? 1 : 3;
            for (int th = 1; th <= 4; th += 3) if (!kv_inverse(t, t1n, o, yn, th)) FAIL("kv_inverse: bad predictor accepted (frame %zu, %d threads)", fr, th);
            free(t);
        }
        free(t1); free(o);
    }
    {   /* a video frame whose nested frame is again a video frame is refused (no recursion) */
        kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(yn, &p); uint8_t *f = malloc(cap), *g = malloc(cap + 64), *o = malloc(yn);
        int64_t z = kolmo_compress(y, yn, f, cap, &p);
        if (z <= 0 || f[HDR + 12] != C_VID) FAIL("y4m: no video frame");
        size_t gz = HDR + IDX + 4 + (size_t)z; memcpy(g, f, HDR + IDX); put32(g + HDR + 4, (uint32_t)(4 + z)); memset(g + HDR + IDX, 0, 4);
        memcpy(g + HDR + IDX + 4, f, (size_t)z); uint32_t hc = kolmo_crc32c(0, g, 28); hc = kolmo_crc32c(hc, g + HDR, IDX); put32(g + 28, hc);
        if (kolmo_decompress(g, gz, o, yn, 1) >= 0) FAIL("nested video frame accepted");
        free(f); free(g); free(o);
    }
    { kolmo_params p = {KOLMO_RT, 0, 1, 0, 1}; size_t cap = kolmo_bound(yn, &p); uint8_t *f = malloc(cap);
      int64_t z = kolmo_compress(y, yn, f, cap, &p); if (z <= 0 || f[HDR + 12] == C_VID) FAIL("y4m at rt took the video route"); free(f); }
    y[yn - 1000] = 0; memcpy(y + 50000, "FRAMX", 5);                  /* not whole frames: no video route, still a round trip */
    if (frame_case("bad y4m", y, yn, KOLMO_FAST, 0, -1)) return 1;
    free(y);
    printf("FORMAT2 zero-run, leaves 21-23, integer frames, audio (codec 19), video (codec 20) PASSED\n");
    return 0;
}
