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
 * a Kolmo frame of the layout (core/compress.c: vid_compress_frame, core/decode.c: vid_decode_frame; SPEC.md "Video frames").
 * File-scope names carry the prefix kv_ (src/kolmo.c includes every part into one translation unit). */
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

/* strip prediction and its inverse: algo/video.c (C reference) and the SIMD kernels of src/arch, through KOLMO_K */
static size_t kv_strips(size_t h) { return (h + KV_STRIP - 1) / KV_STRIP; }

/* Layout: u32 header length, header bytes, flags (one per strip, frame-major), then residuals plane by plane.
 *
 * Threads (same bytes for every thread count). The encoder predicts from the source samples only, so the frames are
 * independent: kolmo_par_for codes them in parallel. The decoder needs frame f - 1 for frames whose strips use the
 * previous frame, but only the rows that strip covers (and the row above it, already done): frame f decodes strip s
 * of a plane once frame f - 1 has finished that strip, so frames decode as a wavefront, each a few strips behind the
 * one before (kv_dec_frame). Frames are handed out in order (kolmo_par_for), so the frame a worker waits for has
 * always been started by another worker or by itself: no deadlock with any number of threads.
 * Measured: benchmark/rc3w2/regress/RESULTS.md. */
typedef struct {
    kv_layout_t L; const uint8_t *src; uint8_t *out;                        /* encoder: src; decoder: out */
    uint8_t *fl0, *rs0; const uint8_t *cfl0, *crs0;                         /* flags and residuals, to write or to read */
    size_t *foff; size_t nfl, plane_off[KV_MAXP];                           /* per frame: sample offset; flags per frame */
    size_t *prog; int err;                                                  /* decoder: strips finished per frame */
} kv_par;

static void kv_enc_frame(void *ctx, int f, int worker) {
    kv_par *P = ctx; (void)worker; const kv_layout_t *L = &P->L;
    const uint8_t *cur = P->src + P->foff[f], *pp = f ? P->src + P->foff[f - 1] : NULL; uint8_t *fl = P->fl0 + (size_t)f * P->nfl;
    for (int p = 0; p < L->np; p++) {
        size_t ps = L->w[p] * L->h[p], w = L->w[p], h = L->h[p]; uint8_t *res = P->rs0 + P->plane_off[p] + (size_t)f * ps;
        for (size_t y0 = 0, st = 0; y0 < h; y0 += KV_STRIP, st++)       /* KOLMO_K: the SIMD strip kernels (same bytes) */
            fl[st] = (uint8_t)KOLMO_K->vid_strip_enc(cur, pp, w, y0, y0 + KV_STRIP < h ? y0 + KV_STRIP : h, res);
        fl += kv_strips(h); cur += ps; if (pp) pp += ps;
    }
}

static uint8_t *kv_transform(const uint8_t *s, size_t n, size_t *outn, int threads) {
    kv_layout_t L; uint8_t *hdr;
    if (kv_parse(s, n, &L, &hdr)) return NULL;
    size_t nfl = 0; for (int p = 0; p < L.np; p++) nfl += kv_strips(L.h[p]);
    size_t nflags = nfl * L.frames;
    size_t tot = 4 + L.hdr_len + nflags + L.frames * L.frame_bytes;
    uint8_t *o = malloc(tot); size_t *foff = malloc(L.frames * sizeof *foff);
    if (!o || !foff || L.frames > INT32_MAX) { free(o); free(foff); free(hdr); return NULL; }
    { uint32_t hv = (uint32_t)L.hdr_len; for (int q = 0; q < 4; q++) o[q] = (uint8_t)(hv >> (8 * q)); }; memcpy(o + 4, hdr, L.hdr_len);
    kv_par P = {0}; P.L = L; P.src = s; P.fl0 = o + 4 + L.hdr_len; P.rs0 = P.fl0 + nflags; P.foff = foff; P.nfl = nfl;
    size_t acc = 0; for (int p = 0; p < L.np; p++) { P.plane_off[p] = acc; acc += L.frames * L.w[p] * L.h[p]; }
    size_t i = (size_t)((const uint8_t *)memchr(s, '\n', n) - s) + 1;
    for (size_t f = 0; f < L.frames; f++) { i = (size_t)((const uint8_t *)memchr(s + i, '\n', n - i) - s) + 1; foff[f] = i; i += L.frame_bytes; }
    kolmo_par_for(threads, (int)L.frames, kv_enc_frame, &P);
    free(foff); free(hdr); *outn = tot; return o;
}

static void kv_dec_frame(void *ctx, int f, int worker) {
    kv_par *P = ctx; (void)worker; const kv_layout_t *L = &P->L;
    uint8_t *cur = P->out + P->foff[f]; const uint8_t *pp = f ? P->out + P->foff[f - 1] : NULL;
    const uint8_t *fl = P->cfl0 + (size_t)f * P->nfl; size_t done = 0; int bad = 0;
    for (int p = 0; p < L->np; p++) {
        size_t ps = L->w[p] * L->h[p], w = L->w[p], h = L->h[p]; const uint8_t *res = P->crs0 + P->plane_off[p] + (size_t)f * ps;
        for (size_t y0 = 0, st = 0; y0 < h; y0 += KV_STRIP, st++) {
            int k = fl[st];
            if (k > 2 || (k && !pp)) bad = 1;
            if (!bad) {
                if (k) while (__atomic_load_n(&P->prog[f - 1], __ATOMIC_ACQUIRE) <= done) sched_yield();   /* frame f - 1 has this strip */
                KOLMO_K->vid_strip_dec(cur, pp, w, y0, y0 + KV_STRIP < h ? y0 + KV_STRIP : h, k, res);
            }
            __atomic_store_n(&P->prog[f], bad ? SIZE_MAX : ++done, __ATOMIC_RELEASE);
        }
        fl += kv_strips(h); cur += ps; if (pp) pp += ps;
    }
    if (bad) __atomic_store_n(&P->err, 1, __ATOMIC_RELAXED);
}

static int kv_inverse(const uint8_t *t, size_t tn, uint8_t *out, size_t n, int threads) {
    if (tn < 4) return -1;
    size_t hl = (size_t)t[0] | (size_t)t[1] << 8 | (size_t)t[2] << 16 | (size_t)t[3] << 24; if (hl > tn - 4) return -1;
    kv_layout_t L;
    const uint8_t *h = t + 4, *e = memchr(h, '\n', hl);          /* the header bytes alone give the layout */
    if (!e || kv_geometry(h, e, &L)) return -1;
    size_t frames = 0; for (const uint8_t *q = e + 1; q < h + hl; ) { const uint8_t *f = memchr(q, '\n', (size_t)(h + hl - q)); if (!f) return -1; frames++; q = f + 1; }
    L.frames = frames;
    size_t nfl = 0; for (int p = 0; p < L.np; p++) nfl += kv_strips(L.h[p]);
    size_t nflags = nfl * L.frames;
    if (!frames || frames > INT32_MAX || tn != 4 + hl + nflags + L.frames * L.frame_bytes || n != hl + L.frames * L.frame_bytes) return -1;
    kv_par P = {0}; P.L = L; P.out = out; P.cfl0 = t + 4 + hl; P.crs0 = P.cfl0 + nflags; P.nfl = nfl;
    size_t acc = 0; for (int p = 0; p < L.np; p++) { P.plane_off[p] = acc; acc += L.frames * L.w[p] * L.h[p]; }
    P.foff = malloc(frames * sizeof *P.foff); P.prog = calloc(frames, sizeof *P.prog);
    if (!P.foff || !P.prog) { free(P.foff); free(P.prog); return -1; }
    size_t o = (size_t)(e - h) + 1, hi = o; memcpy(out, h, o);
    for (size_t f = 0; f < frames; f++) {                       /* FRAME lines in place; sample offsets of each frame */
        const uint8_t *fe = memchr(h + hi, '\n', hl - hi); size_t fl_len = (size_t)(fe - (h + hi)) + 1;
        memcpy(out + o, h + hi, fl_len); o += fl_len; hi += fl_len; P.foff[f] = o; o += L.frame_bytes;
    }
    if (o == n) kolmo_par_for(threads, (int)frames, kv_dec_frame, &P);
    free(P.foff); free(P.prog);
    return o == n && !P.err ? 0 : -1;
}


/* a whole frame, coded by core/compress.c and decoded by core/decode.c (never a graph leaf): a name only */
static const kolmo_codec codec_video = { C_VID, "video", NULL, NULL, NULL, NULL };
