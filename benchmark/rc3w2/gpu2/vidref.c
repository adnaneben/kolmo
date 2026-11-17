/* kvid's strip prediction on the CPU (NEON on Apple silicon), the same loop as encode_plane() in
 * src/experimental/kvid.c, for frames [f0, f1) of a y4m held in memory, laid out as the GPU kernel's output (residuals
 * plane-major over the frames of the call, one flag per strip, frame-major). Apache-2.0, Kolmo contributors. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "vidref.h"
#include "../../../src/arch/kernels.h"

uint64_t kolmo_kvid_row_neon(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t y, uint8_t *res);
enum { STRIP = 16 };
static inline int med(int a, int b, int c) { int mx = a > b ? a : b, mn = a < b ? a : b; return c >= mx ? mn : c <= mn ? mx : a + b - c; }
static inline int predict(int k, const uint8_t *cur, const uint8_t *prev, size_t w, size_t x, size_t y) {
    size_t i = y * w + x;
    if (k == 1) return prev[i];
    int a, b, c;
    if (k == 0) { a = x ? cur[i - 1] : (y ? cur[i - w] : 0); b = y ? cur[i - w] : a; c = (x && y) ? cur[i - w - 1] : b; return med(a, b, c); }
#define D(j) ((int)(int8_t)(uint8_t)(cur[j] - prev[j]))
    a = x ? D(i - 1) : (y ? D(i - w) : 0); b = y ? D(i - w) : a; c = (x && y) ? D(i - w - 1) : b;
#undef D
    return prev[i] + med(a, b, c);
}
static inline uint8_t zz(int r) { int8_t s = (int8_t)(uint8_t)r; return (uint8_t)(((uint8_t)s << 1) ^ (uint8_t)(s >> 7)); }

/* strips [s0, s1) of one plane */
static void strips(const uint8_t *cur, const uint8_t *prev, size_t w, size_t h, size_t s0, size_t s1, uint8_t *res, uint8_t *flags) {
    for (size_t s = s0; s < s1; s++) {
        size_t y0 = s * STRIP, y1 = y0 + STRIP < h ? y0 + STRIP : h; int best = 0; uint64_t bc = UINT64_MAX;
#if defined(__aarch64__)
        for (int k = 0; k < (prev ? 3 : 1); k++) {
            uint64_t c = 0;
            for (size_t y = y0; y < y1; y++) {
                if (y && w >= 17) {
                    int8_t r = (int8_t)(uint8_t)(cur[y * w] - predict(k, cur, prev, w, 0, y)); c += (uint64_t)(r < 0 ? -r : r);
                    c += kolmo_kvid_row_neon(k, cur, prev, w, y, NULL);
                    continue;
                }
                for (size_t x = 0; x < w; x++) { int8_t r = (int8_t)(uint8_t)(cur[y * w + x] - predict(k, cur, prev, w, x, y)); c += (uint64_t)(r < 0 ? -r : r); }
            }
            if (c < bc) { bc = c; best = k; }
        }
        flags[s] = (uint8_t)best;
        for (size_t y = y0; y < y1; y++) {
            if (y && w >= 17) { res[y * w] = zz(cur[y * w] - predict(best, cur, prev, w, 0, y)); kolmo_kvid_row_neon(best, cur, prev, w, y, res); continue; }
            for (size_t x = 0; x < w; x++) res[y * w + x] = zz(cur[y * w + x] - predict(best, cur, prev, w, x, y));
        }
#else
        flags[s] = (uint8_t)kolmo_kernels_get()->vid_strip_enc(cur, prev, w, y0, y1, res);
        (void)best; (void)bc;
#endif
    }
}

void vid_geom(vid_t *V) {
    size_t acc = 0, sp = 0, fb = 0;
    for (int p = 0; p < V->np; p++) { V->pstart[p] = fb; fb += V->w[p] * V->h[p]; V->strip_base[p] = sp; sp += (V->h[p] + STRIP - 1) / STRIP; }
    V->strips_per_frame = sp; V->frame_samples = fb;
    for (int p = 0; p < V->np; p++) { V->plane_res[p] = acc; acc += (size_t)V->frames * V->w[p] * V->h[p]; }
    V->res_bytes = acc;
}

/* job j of a call over frames [0, frames) of V (frame_at relative to file): (frame, plane, strip band of 8 strips) */
size_t vid_jobs(const vid_t *V) {
    size_t j = 0;
    for (int p = 0; p < V->np; p++) j += ((V->h[p] + STRIP - 1) / STRIP + 7) / 8;
    return j * V->frames;
}
void vid_job(const vid_t *V, const uint8_t *file, const uint64_t *frame_at, size_t job, uint8_t *res, uint8_t *flags) {
    size_t per = 0, band[3];
    for (int p = 0; p < V->np; p++) { band[p] = ((V->h[p] + STRIP - 1) / STRIP + 7) / 8; per += band[p]; }
    size_t f = job / per, r = job % per; int p = 0;
    while (r >= band[p]) { r -= band[p]; p++; }
    size_t ns = (V->h[p] + STRIP - 1) / STRIP, s0 = r * 8, s1 = s0 + 8 < ns ? s0 + 8 : ns;
    const uint8_t *cur = file + frame_at[f] + V->pstart[p], *prev = f ? file + frame_at[f - 1] + V->pstart[p] : NULL;
    strips(cur, prev, V->w[p], V->h[p], s0, s1, res + V->plane_res[p] + f * V->w[p] * V->h[p],
           flags + f * V->strips_per_frame + V->strip_base[p]);
}
