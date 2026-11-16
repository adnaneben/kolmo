/* Codec 20: video (kvid front end, container version 11). 8-bit YUV4MPEG2 files (C420*, C422, C444, Cmono).
 *
 * Ported from the research tool src/experimental/kvid.c (Apache-2.0, Kolmo contributors), which stays as the
 * standalone tool. Each plane of each frame is cut into strips of 16 rows; each strip is predicted by whichever of
 * three predictors leaves the smallest residuals: 0 LOCO-I median (MED) of the left, upper and upper-left samples of
 * the same frame; 1 the same sample of the previous frame; 2 MED applied to the difference from the previous frame.
 * Residuals (mod 256, zigzag) are grouped by plane over all frames.
 *
 * Layout (kv_transform): u32 header length, the header bytes (stream header and every FRAME line), one predictor byte
 * per strip (frame-major), then the residuals plane by plane. The whole file is one block of codec 20 whose payload is
 * a Kolmo frame of the layout (src/kolmo.c: vid_compress_frame, vid_decode_frame; SPEC.md "Codec 20").
 * File-scope names carry the prefix kv_ (the codecs are still included into src/kolmo.c). */
enum { KV_STRIP = 16, KV_MAXP = 3 };
typedef struct { size_t w[KV_MAXP], h[KV_MAXP]; int np; size_t frame_bytes, frames, hdr_len; } kv_layout_t;


/* Parses the stream header and checks that the file is whole frames. Header bytes (the stream header and every
 * FRAME line) are returned in *hdr, in order; the sample bytes stay where they are. */
/* Plane kv_geometry from the stream header line [h, e) (e points at its newline). */
static int kv_geometry(const uint8_t *h, const uint8_t *e, kv_layout_t *L) {
    if (e - h < 10 || memcmp(h, "YUV4MPEG2 ", 10)) return -1;
    size_t W = 0, H = 0; char cs[32] = "420";
    for (const uint8_t *p = h + 9; p + 1 < e; p++) {
        if (*p != ' ') continue;
        if (p[1] == 'W') W = strtoul((const char *)p + 2, NULL, 10);
        else if (p[1] == 'H') H = strtoul((const char *)p + 2, NULL, 10);
        else if (p[1] == 'C') { size_t k = 0; while (p + 2 + k < e && p[2 + k] != ' ' && k < 31) { cs[k] = (char)p[2 + k]; k++; } cs[k] = 0; }
    }
    if (!W || !H || W > 16384 || H > 16384) return -1;
    size_t cw, ch;
    if (!strcmp(cs, "420") || !strcmp(cs, "420jpeg") || !strcmp(cs, "420mpeg2") || !strcmp(cs, "420paldv")) { cw = (W + 1) / 2; ch = (H + 1) / 2; L->np = 3; }
    else if (!strcmp(cs, "422")) { cw = (W + 1) / 2; ch = H; L->np = 3; }
    else if (!strcmp(cs, "444")) { cw = W; ch = H; L->np = 3; }
    else if (!strcmp(cs, "mono")) { cw = ch = 0; L->np = 1; }
    else return -1;                                              /* other layouts and >8-bit samples: not handled */
    L->w[0] = W; L->h[0] = H; for (int p = 1; p < L->np; p++) { L->w[p] = cw; L->h[p] = ch; }
    L->frame_bytes = 0; for (int p = 0; p < L->np; p++) L->frame_bytes += L->w[p] * L->h[p];
    return 0;
}

static int kv_parse(const uint8_t *s, size_t n, kv_layout_t *L, uint8_t **hdr) {
    const uint8_t *e = memchr(s, '\n', n < 4096 ? n : 4096);
    if (!e || kv_geometry(s, e, L)) return -1;
    size_t cap = (size_t)(e - s) + 1 + 64, hn = (size_t)(e - s) + 1; uint8_t *hb = malloc(cap); if (!hb) return -1;
    memcpy(hb, s, hn); size_t i = hn; L->frames = 0;
    while (i < n) {
        const uint8_t *f = memchr(s + i, '\n', n - i < 1024 ? n - i : 1024);
        if (!f || n - i < 5 || memcmp(s + i, "FRAME", 5)) { free(hb); return -1; }
        size_t fl = (size_t)(f - (s + i)) + 1;
        if (n - i - fl < L->frame_bytes) { free(hb); return -1; }
        if (hn + fl > cap) { cap = 2 * cap + fl; uint8_t *t = realloc(hb, cap); if (!t) { free(hb); return -1; } hb = t; }
        memcpy(hb + hn, s + i, fl); hn += fl; i += fl + L->frame_bytes; L->frames++;
    }
    if (!L->frames) { free(hb); return -1; }
    L->hdr_len = hn; *hdr = hb; return 0;
}

static inline int kv_med(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b;
    return c >= mx ? mn : c <= mn ? mx : a + b - c;
}
/* Prediction of sample (x, y) under predictor k; cur holds the samples decoded so far, prev the previous frame. */
static inline int kv_predict(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t x, size_t y) {
    size_t i = y * w + x;
    if (k == 1) return prev[i];
    int a, b, c;
    if (k == 0) {
        a = x ? cur[i - 1] : (y ? cur[i - w] : 0); b = y ? cur[i - w] : a; c = (x && y) ? cur[i - w - 1] : b;
        return kv_med(a, b, c);
    }
#define D(j) ((int)(int8_t)(uint8_t)(cur[j] - prev[j]))
    a = x ? D(i - 1) : (y ? D(i - w) : 0); b = y ? D(i - w) : a; c = (x && y) ? D(i - w - 1) : b;
#undef D
    return prev[i] + kv_med(a, b, c);
}
static inline uint8_t kv_zz(int r) { int8_t s = (int8_t)(uint8_t)r; return (uint8_t)(((uint8_t)s << 1) ^ (uint8_t)(s >> 7)); }
static inline int kv_unzz(uint8_t z) { return (int)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }

static void kv_encode_plane(const uint8_t *cur, const uint8_t *prev, size_t w, size_t h, uint8_t *res, uint8_t *flags) {
    for (size_t y0 = 0, s = 0; y0 < h; y0 += KV_STRIP, s++) {
        size_t y1 = y0 + KV_STRIP < h ? y0 + KV_STRIP : h; int best = 0; uint64_t bc = UINT64_MAX;
        for (int k = 0; k < (prev ? 3 : 1); k++) {
            uint64_t c = 0;
            for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++) {
                int8_t r = (int8_t)(uint8_t)(cur[y * w + x] - kv_predict(k, cur, prev, w, x, y)); c += (uint64_t)(r < 0 ? -r : r);
            }
            if (c < bc) { bc = c; best = k; }
        }
        flags[s] = (uint8_t)best;
        for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++)
            res[y * w + x] = kv_zz(cur[y * w + x] - kv_predict(best, cur, prev, w, x, y));
    }
}
static int kv_decode_plane(uint8_t *cur, const uint8_t *prev, size_t w, size_t h, const uint8_t *res, const uint8_t *flags) {
    for (size_t y0 = 0, s = 0; y0 < h; y0 += KV_STRIP, s++) {
        int k = flags[s]; if (k > 2 || (k && !prev)) return -1;
        size_t y1 = y0 + KV_STRIP < h ? y0 + KV_STRIP : h;
        for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++)
            cur[y * w + x] = (uint8_t)(kv_predict(k, cur, prev, w, x, y) + kv_unzz(res[y * w + x]));
    }
    return 0;
}
static size_t kv_strips(size_t h) { return (h + KV_STRIP - 1) / KV_STRIP; }

/* Layout: u32 header length, header bytes, flags (one per strip, frame-major), then residuals plane by plane. */
static uint8_t *kv_transform(const uint8_t *s, size_t n, size_t *outn) {
    kv_layout_t L; uint8_t *hdr;
    if (kv_parse(s, n, &L, &hdr)) return NULL;
    size_t nflags = 0; for (int p = 0; p < L.np; p++) nflags += kv_strips(L.h[p]); nflags *= L.frames;
    size_t tot = 4 + L.hdr_len + nflags + L.frames * L.frame_bytes;
    uint8_t *o = malloc(tot); if (!o) { free(hdr); return NULL; }
    { uint32_t hv = (uint32_t)L.hdr_len; for (int q = 0; q < 4; q++) o[q] = (uint8_t)(hv >> (8 * q)); }; memcpy(o + 4, hdr, L.hdr_len);
    uint8_t *fl = o + 4 + L.hdr_len, *rs = fl + nflags;
    size_t plane_off[KV_MAXP], acc = 0; for (int p = 0; p < L.np; p++) { plane_off[p] = acc; acc += L.frames * L.w[p] * L.h[p]; }
    const uint8_t *prev = NULL; size_t i = (size_t)((const uint8_t *)memchr(s, '\n', n) - s) + 1;
    for (size_t f = 0; f < L.frames; f++) {
        i = (size_t)((const uint8_t *)memchr(s + i, '\n', n - i) - s) + 1;
        const uint8_t *cur = s + i, *pp = prev;
        for (int p = 0; p < L.np; p++) {
            size_t ps = L.w[p] * L.h[p];
            kv_encode_plane(cur, pp, L.w[p], L.h[p], rs + plane_off[p] + f * ps, fl); fl += kv_strips(L.h[p]);
            cur += ps; if (pp) pp += ps;
        }
        prev = s + i; i += L.frame_bytes;
    }
    free(hdr); *outn = tot; return o;
}
static int kv_inverse(const uint8_t *t, size_t tn, uint8_t *out, size_t n) {
    if (tn < 4) return -1;
    size_t hl = (size_t)t[0] | (size_t)t[1] << 8 | (size_t)t[2] << 16 | (size_t)t[3] << 24; if (hl > tn - 4) return -1;
    kv_layout_t L;
    const uint8_t *h = t + 4, *e = memchr(h, '\n', hl);          /* the header bytes alone give the layout */
    if (!e || kv_geometry(h, e, &L)) return -1;
    size_t frames = 0; for (const uint8_t *q = e + 1; q < h + hl; ) { const uint8_t *f = memchr(q, '\n', (size_t)(h + hl - q)); if (!f) return -1; frames++; q = f + 1; }
    L.frames = frames;
    size_t nflags = 0; for (int p = 0; p < L.np; p++) nflags += kv_strips(L.h[p]); nflags *= L.frames;
    if (tn != 4 + hl + nflags + L.frames * L.frame_bytes || n != hl + L.frames * L.frame_bytes) return -1;
    const uint8_t *fl = t + 4 + hl, *rs = fl + nflags;
    size_t plane_off[KV_MAXP], acc = 0; for (int p = 0; p < L.np; p++) { plane_off[p] = acc; acc += L.frames * L.w[p] * L.h[p]; }
    size_t o = (size_t)(e - h) + 1, hi = o; memcpy(out, h, o);
    const uint8_t *prev = NULL;
    for (size_t f = 0; f < L.frames; f++) {
        const uint8_t *fe = memchr(h + hi, '\n', hl - hi); size_t fl_len = (size_t)(fe - (h + hi)) + 1;
        memcpy(out + o, h + hi, fl_len); o += fl_len; hi += fl_len;
        uint8_t *cur = out + o; const uint8_t *pp = prev;
        for (int p = 0; p < L.np; p++) {
            size_t ps = L.w[p] * L.h[p];
            if (kv_decode_plane(cur, pp, L.w[p], L.h[p], rs + plane_off[p] + f * ps, fl)) return -1;
            fl += kv_strips(L.h[p]); cur += ps; if (pp) pp += ps;
        }
        prev = out + o; o += L.frame_bytes;
    }
    return o == n ? 0 : -1;
}

