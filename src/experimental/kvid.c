/* kvid: research prototype of a lossless video front end for Kolmo. Apache-2.0, Kolmo contributors.
 *
 * Input: a YUV4MPEG2 (.y4m) file with 8-bit samples (C420*, C422, C444 or Cmono). Each plane of each frame is cut
 * into strips of 16 rows; each strip is predicted by whichever of three predictors leaves the smallest residuals:
 *   0  LOCO-I median (MED) of the left, upper and upper-left samples of the same frame (intra);
 *   1  the same sample of the previous frame (temporal);
 *   2  MED applied to the difference from the previous frame (temporal, then spatial).
 * Residuals (mod 256, zigzag) are grouped by plane over all frames and the whole layout is compressed by the Kolmo
 * library in the chosen mode. Frame: "KVD1", u64 input size, then one Kolmo frame. Not a Kolmo frame; `kolmo` never
 * writes it. One thread.
 *
 *   kvid [-m rt|balanced|eco|hc|ratio|x] [-T threads] in.y4m out   compress (default: ratio, one thread)
 *   kvid -d in out                                        decompress
 *   kvid -b [-m ...] in.y4m                               in-memory timing (fastest of at least 3 runs), JSON line
 * The prediction runs on -T threads (frames are independent at encode time), or on the GPU with --gpu (macOS builds
 * with Metal; src/arch/metal/kvid.metal), with the same bytes in every case. */
#include "kolmo.h"
#include "arch/kernels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#ifdef KVID_GPU
#include "arch/metal/metal.h"
#endif
static int kv_threads = 1, kv_gpu = 0;
static double kv_transform_s = 1e30;                             /* fastest transform seen (-b reports it) */

enum { STRIP = 16, MAXP = 3 };
/* NEON kernels (src/arch/aarch64/kvid_neon.c, and the median band of image_neon.c in the library); KVID_FORCE_C=1 in
 * the environment keeps the C code below, which is their reference */
#if defined(__aarch64__) && defined(__ARM_NEON) && !defined(KOLMO_NO_ASM)
#define KVID_NEON 1
uint64_t kolmo_kvid_row_neon(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t y, uint8_t *res);
void kolmo_kvid_add_neon(uint8_t *cur, const uint8_t *prev, const uint8_t *res, size_t n);
int kolmo_med_band16_neon(const uint8_t *r, size_t W, uint8_t *p, size_t y0);
static int kvid_c;
#endif
typedef struct { size_t w[MAXP], h[MAXP]; int np; size_t frame_bytes, frames, hdr_len; } layout_t;

static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t get64(const uint8_t *p) { return (uint64_t)get32(p) | (uint64_t)get32(p + 4) << 32; }

/* Parses the stream header and checks that the file is whole frames. Header bytes (the stream header and every
 * FRAME line) are returned in *hdr, in order; the sample bytes stay where they are. */
/* Plane geometry from the stream header line [h, e) (e points at its newline). */
static int geometry(const uint8_t *h, const uint8_t *e, layout_t *L) {
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

static int parse(const uint8_t *s, size_t n, layout_t *L, uint8_t **hdr) {
    const uint8_t *e = memchr(s, '\n', n < 4096 ? n : 4096);
    if (!e || geometry(s, e, L)) return -1;
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

#ifdef KVID_NEON   /* the NEON strip paths below use the scalar predictor for the first column and the tail */
static inline int med(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b;
    return c >= mx ? mn : c <= mn ? mx : a + b - c;
}
static inline int predict(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t x, size_t y) {
    size_t i = y * w + x;
    if (k == 1) return prev[i];
    int a, b, c;
    if (k == 0) {
        a = x ? cur[i - 1] : (y ? cur[i - w] : 0); b = y ? cur[i - w] : a; c = (x && y) ? cur[i - w - 1] : b;
        return med(a, b, c);
    }
#define D(j) ((int)(int8_t)(uint8_t)(cur[j] - prev[j]))
    a = x ? D(i - 1) : (y ? D(i - w) : 0); b = y ? D(i - w) : a; c = (x && y) ? D(i - w - 1) : b;
#undef D
    return prev[i] + med(a, b, c);
}
static inline uint8_t zz(int r) { int8_t s = (int8_t)(uint8_t)r; return (uint8_t)(((uint8_t)s << 1) ^ (uint8_t)(s >> 7)); }
static inline int unzz(uint8_t z) { return (int)((z >> 1) ^ (uint8_t)-(int)(z & 1)); }
#endif
/* Strip choice and residuals: src/algo/video.c (the reference, the prediction described above) and its SIMD kernels
 * in src/arch. */
static void encode_plane(const uint8_t *cur, const uint8_t *prev, size_t w, size_t h, uint8_t *res, uint8_t *flags) {
#ifdef KVID_NEON
    for (size_t y0 = 0, s = 0; y0 < h; y0 += STRIP, s++) {
        size_t y1 = y0 + STRIP < h ? y0 + STRIP : h; int best = 0; uint64_t bc = UINT64_MAX;
        for (int k = 0; k < (prev ? 3 : 1); k++) {
            uint64_t c = 0;
            for (size_t y = y0; y < y1; y++) {
#ifdef KVID_NEON
                if (!kvid_c && y && w >= 17) {
                    int8_t r = (int8_t)(uint8_t)(cur[y * w] - predict(k, cur, prev, w, 0, y)); c += (uint64_t)(r < 0 ? -r : r);
                    c += kolmo_kvid_row_neon(k, cur, prev, w, y, NULL);
                    continue;
                }
#endif
                for (size_t x = 0; x < w; x++) {
                    int8_t r = (int8_t)(uint8_t)(cur[y * w + x] - predict(k, cur, prev, w, x, y)); c += (uint64_t)(r < 0 ? -r : r);
                }
            }
            if (c < bc) { bc = c; best = k; }
        }
        flags[s] = (uint8_t)best;
        for (size_t y = y0; y < y1; y++) {
#ifdef KVID_NEON
            if (!kvid_c && y && w >= 17) { res[y * w] = zz(cur[y * w] - predict(best, cur, prev, w, 0, y)); kolmo_kvid_row_neon(best, cur, prev, w, y, res); continue; }
#endif
            for (size_t x = 0; x < w; x++) res[y * w + x] = zz(cur[y * w + x] - predict(best, cur, prev, w, x, y));
        }
    }
#else
    const kolmo_kernels *K = kolmo_kernels_get();
    for (size_t y0 = 0, s = 0; y0 < h; y0 += STRIP, s++)
        flags[s] = (uint8_t)K->vid_strip_enc(cur, prev, w, y0, y0 + STRIP < h ? y0 + STRIP : h, res);
#endif
}
static int decode_plane(uint8_t *cur, const uint8_t *prev, size_t w, size_t h, const uint8_t *res, const uint8_t *flags) {
    const kolmo_kernels *K = kolmo_kernels_get();
    for (size_t y0 = 0, s = 0; y0 < h; y0 += STRIP, s++) {
        int k = flags[s]; if (k > 2 || (k && !prev)) return -1;
#ifdef KVID_NEON
        size_t y1 = y0 + STRIP < h ? y0 + STRIP : h;
#ifdef KVID_NEON
        if (!kvid_c && k == 1) { kolmo_kvid_add_neon(cur + y0 * w, prev + y0 * w, res + y0 * w, (y1 - y0) * w); continue; }
        if (!kvid_c && k == 0 && y1 - y0 == 16 && kolmo_med_band16_neon(res, w, cur, y0)) continue;
#endif
        for (size_t y = y0; y < y1; y++) for (size_t x = 0; x < w; x++)
            cur[y * w + x] = (uint8_t)(predict(k, cur, prev, w, x, y) + unzz(res[y * w + x]));
#else
        K->vid_strip_dec(cur, prev, w, y0, y0 + STRIP < h ? y0 + STRIP : h, k, res);
#endif
    }
    return 0;
}
static size_t strips(size_t h) { return (h + STRIP - 1) / STRIP; }

/* Frames are independent at encode time (the previous frame is the input's): workers take whole frames. */
static double now(void);
typedef struct {
    const uint8_t *s; const layout_t *L; const uint64_t *at; uint8_t *rs, *fl; const size_t *plane_off; size_t fpf;
    atomic_size_t next;
} kv_job;
static void *kv_worker(void *arg) {
    kv_job *J = arg; const layout_t *L = J->L;
    for (size_t f; (f = atomic_fetch_add(&J->next, 1)) < L->frames; ) {
        const uint8_t *cur = J->s + J->at[f], *pp = f ? J->s + J->at[f - 1] : NULL;
        uint8_t *fl = J->fl + f * J->fpf;
        for (int p = 0; p < L->np; p++) {
            size_t ps = L->w[p] * L->h[p];
            encode_plane(cur, pp, L->w[p], L->h[p], J->rs + J->plane_off[p] + f * ps, fl); fl += strips(L->h[p]);
            cur += ps; if (pp) pp += ps;
        }
    }
    return NULL;
}

/* Layout: u32 header length, header bytes, flags (one per strip, frame-major), then residuals plane by plane. */
static uint8_t *transform(const uint8_t *s, size_t n, size_t *outn) {
    layout_t L; uint8_t *hdr;
    if (parse(s, n, &L, &hdr)) return NULL;
    size_t nflags = 0; for (int p = 0; p < L.np; p++) nflags += strips(L.h[p]); nflags *= L.frames;
    size_t tot = 4 + L.hdr_len + nflags + L.frames * L.frame_bytes;
    uint8_t *o = malloc(tot); if (!o) { free(hdr); return NULL; }
    put32(o, (uint32_t)L.hdr_len); memcpy(o + 4, hdr, L.hdr_len);
    uint8_t *fl = o + 4 + L.hdr_len, *rs = fl + nflags;
    size_t plane_off[MAXP], acc = 0; for (int p = 0; p < L.np; p++) { plane_off[p] = acc; acc += L.frames * L.w[p] * L.h[p]; }
    uint64_t *at = malloc(L.frames * sizeof(uint64_t)); if (!at) { free(o); free(hdr); return NULL; }
    size_t i = (size_t)((const uint8_t *)memchr(s, '\n', n) - s) + 1;
    for (size_t f = 0; f < L.frames; f++) {
        i = (size_t)((const uint8_t *)memchr(s + i, '\n', n - i) - s) + 1;
        at[f] = i; i += L.frame_bytes;
    }
    double t0 = now();
    int done = 0;
#ifdef KVID_GPU
    if (kv_gpu && L.frames <= UINT32_MAX)
        done = !kolmo_metal_kvid(s, n, at, (uint32_t)L.frames, L.np, L.w, L.h, rs, fl);
#endif
    if (!done) {
        kv_job J = {s, &L, at, rs, fl, plane_off, nflags / L.frames, 0};
        int T = kv_threads > 1 ? kv_threads : 1; pthread_t th[64]; int started = 0;
        if (T > 64) T = 64;
        for (int t = 1; t < T && (size_t)t < L.frames; t++) if (!pthread_create(&th[started], NULL, kv_worker, &J)) started++;
        kv_worker(&J);
        for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
    }
    double e = now() - t0; if (e < kv_transform_s) kv_transform_s = e;
    free(at); free(hdr); *outn = tot; return o;
}
static int inverse(const uint8_t *t, size_t tn, uint8_t *out, size_t n) {
    if (tn < 4) return -1;
    size_t hl = get32(t); if (hl > tn - 4) return -1;
    layout_t L;
    const uint8_t *h = t + 4, *e = memchr(h, '\n', hl);          /* the header bytes alone give the layout */
    if (!e || geometry(h, e, &L)) return -1;
    size_t frames = 0; for (const uint8_t *q = e + 1; q < h + hl; ) { const uint8_t *f = memchr(q, '\n', (size_t)(h + hl - q)); if (!f) return -1; frames++; q = f + 1; }
    L.frames = frames;
    size_t nflags = 0; for (int p = 0; p < L.np; p++) nflags += strips(L.h[p]); nflags *= L.frames;
    if (tn != 4 + hl + nflags + L.frames * L.frame_bytes || n != hl + L.frames * L.frame_bytes) return -1;
    const uint8_t *fl = t + 4 + hl, *rs = fl + nflags;
    size_t plane_off[MAXP], acc = 0; for (int p = 0; p < L.np; p++) { plane_off[p] = acc; acc += L.frames * L.w[p] * L.h[p]; }
    size_t o = (size_t)(e - h) + 1, hi = o; memcpy(out, h, o);
    const uint8_t *prev = NULL;
    for (size_t f = 0; f < L.frames; f++) {
        const uint8_t *fe = memchr(h + hi, '\n', hl - hi); size_t fl_len = (size_t)(fe - (h + hi)) + 1;
        memcpy(out + o, h + hi, fl_len); o += fl_len; hi += fl_len;
        uint8_t *cur = out + o; const uint8_t *pp = prev;
        for (int p = 0; p < L.np; p++) {
            size_t ps = L.w[p] * L.h[p];
            if (decode_plane(cur, pp, L.w[p], L.h[p], rs + plane_off[p] + f * ps, fl)) return -1;
            fl += strips(L.h[p]); cur += ps; if (pp) pp += ps;
        }
        prev = out + o; o += L.frame_bytes;
    }
    return o == n ? 0 : -1;
}

static uint8_t *slurp(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); long l = ftell(f); rewind(f);
    uint8_t *b = malloc(l > 0 ? (size_t)l : 1);
    if (!b || fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; }
    fclose(f); *n = (size_t)l; return b;
}
static int spit(const char *path, const uint8_t *b, size_t n) {
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return -1; }
    int bad = fwrite(b, 1, n, f) != n; if (fclose(f)) bad = 1; if (bad) perror(path); return bad ? -1 : 0;
}
static int mode_of(const char *m, kolmo_params *P) {
    memset(P, 0, sizeof *P); P->threads = 1; P->transforms = 1;
    if (!strcmp(m, "rt")) P->mode = KOLMO_RT; else if (!strcmp(m, "balanced")) P->mode = KOLMO_FAST;
    else if (!strcmp(m, "eco")) P->mode = KOLMO_ECO; else if (!strcmp(m, "hc")) P->mode = KOLMO_HC;
    else if (!strcmp(m, "ratio")) P->mode = KOLMO_MAX; else if (!strcmp(m, "x")) P->mode = KOLMO_ULTRA; else return -1;
    return 0;
}
static int64_t enc(const uint8_t *s, size_t n, uint8_t **out, const kolmo_params *P) {
    size_t tn; uint8_t *t = transform(s, n, &tn); if (!t) return -1;
    size_t cap = kolmo_bound(tn, P) + 12; uint8_t *o = malloc(cap); if (!o) { free(t); return -1; }
    memcpy(o, "KVD1", 4); put64(o + 4, n);
    int64_t z = kolmo_compress(t, tn, o + 12, cap - 12, P); free(t);
    if (z <= 0) { free(o); return -1; }
    *out = o; return z + 12;
}
static int dec_threads = 1;
static int dec(const uint8_t *c, size_t cn, uint8_t *out, size_t n) {
    if (cn < 12 || memcmp(c, "KVD1", 4) || get64(c + 4) != n) return -1;
    int64_t tn = kolmo_original_size(c + 12, cn - 12); if (tn <= 0) return -1;
    uint8_t *t = malloc((size_t)tn); if (!t) return -1;
    int r = kolmo_decompress(c + 12, cn - 12, t, (size_t)tn, dec_threads) == tn ? inverse(t, (size_t)tn, out, n) : -1;
    free(t); return r;
}
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

int main(int argc, char **argv) {
#ifdef KVID_NEON
    kvid_c = getenv("KVID_FORCE_C") && atoi(getenv("KVID_FORCE_C"));
#endif
    const char *m = "ratio"; int a = 1, bench = 0, d = 0, threads = 1;
    for (; a < argc && argv[a][0] == '-' && argv[a][1]; a++) {
        if (!strcmp(argv[a], "-d")) d = 1; else if (!strcmp(argv[a], "-b")) bench = 1;
        else if (!strcmp(argv[a], "-m") && a + 1 < argc) m = argv[++a];
        else if (!strcmp(argv[a], "-T") && a + 1 < argc) threads = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--gpu")) kv_gpu = 1;
        else { fprintf(stderr, "kvid: unknown option %s\n", argv[a]); return 2; }
    }
    kolmo_params P; if (mode_of(m, &P)) { fprintf(stderr, "kvid: unknown mode %s\n", m); return 2; }
    P.threads = threads > 0 ? threads : 1; dec_threads = P.threads; kv_threads = P.threads;
    if (d) {
        if (argc - a != 2) return 2;
        size_t cn; uint8_t *c = slurp(argv[a], &cn); if (!c || cn < 12) return 1;
        size_t n = (size_t)get64(c + 4); uint8_t *o = malloc(n ? n : 1);
        if (!o || dec(c, cn, o, n)) { fprintf(stderr, "kvid: corrupt input\n"); return 1; }
        return spit(argv[a + 1], o, n) ? 1 : 0;
    }
    if (argc - a != (bench ? 1 : 2)) { fprintf(stderr, "usage: kvid [-m mode] in.y4m out | -d in out | -b [-m mode] in.y4m\n"); return 2; }
    size_t n; uint8_t *s = slurp(argv[a], &n); if (!s) return 1;
    uint8_t *c = NULL, *back = malloc(n ? n : 1); if (!back) return 1;
    double ce = 1e30, de = 1e30, t0 = now(); int64_t z = -1; int k = 0;
    do { free(c); c = NULL; double st = now(); z = enc(s, n, &c, &P); double e = now() - st; if (e < ce) ce = e; k++; }
    while (bench && z > 0 && (k < 3 || now() - t0 < 1.0));
    if (z <= 0) { fprintf(stderr, "kvid: not a supported 8-bit y4m file, or compression failed\n"); return 1; }
    t0 = now(); k = 0;
    do { double st = now(); if (dec(c, (size_t)z, back, n)) { fprintf(stderr, "kvid: verification failed\n"); return 1; } double e = now() - st; if (e < de) de = e; k++; }
    while (bench && (k < 3 || now() - t0 < 1.0));
    if (memcmp(s, back, n)) { fprintf(stderr, "kvid: round trip differs\n"); return 1; }
    double gk = 0;
#ifdef KVID_GPU
    if (kv_gpu) gk = kolmo_metal_kvid_gpu_seconds();
#endif
    if (bench) { printf("{\"mode\": \"%s\", \"input\": %zu, \"compressed\": %lld, \"compress_mbs\": %.3f, \"decompress_mbs\": %.3f, \"transform_ms\": %.2f, \"gpu\": %d, \"gpu_kernel_ms\": %.2f}\n", m, n, (long long)z, n / ce / 1e6, n / de / 1e6, kv_transform_s * 1e3, kv_gpu, gk * 1e3); return 0; }
    return spit(argv[a + 1], c, (size_t)z) ? 1 : 0;
}
