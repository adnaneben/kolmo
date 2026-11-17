/* kvid's strip prediction on the GPU. Apache-2.0, Kolmo contributors.
 *
 * The C reference is encode_plane() in src/experimental/kvid.c: every strip of 16 rows of every plane of every frame
 * picks the predictor (0 MED, 1 previous frame, 2 MED of the difference to the previous frame) with the smallest sum
 * of |residual| (the first on ties), then writes the zigzag residuals. One threadgroup per strip. At encode time every
 * sample is known, so strips and frames are independent. */
#include <metal_stdlib>
using namespace metal;

#define STRIP 16u
#define TG 256u

struct kv_geom {
    uint w[3], h[3], strips[3];          /* per plane: width, height, strips (h + 15) / 16 */
    uint pstart[3];                      /* offset of plane p inside a frame's samples */
    uint strip_base[3];                  /* first strip of plane p in a frame's flag list */
    uint strips_per_frame, np, frames;
    ulong plane_res[3];                  /* offset of plane p's residuals (all frames) in the output */
};

static inline int med(int a, int b, int c) {
    int mx = max(a, b), mn = min(a, b);
    return c >= mx ? mn : c <= mn ? mx : a + b - c;
}
static inline int predict(uint k, device const uchar *cur, device const uchar *prev, uint w, uint x, uint y) {
    uint i = y * w + x;
    if (k == 1) return prev[i];
    int a, b, c;
    if (k == 0) {
        a = x ? cur[i - 1] : (y ? cur[i - w] : 0); b = y ? cur[i - w] : a; c = (x && y) ? cur[i - w - 1] : b;
        return med(a, b, c);
    }
#define D(j) ((int)(char)(uchar)(cur[j] - prev[j]))
    a = x ? D(i - 1) : (y ? D(i - w) : 0); b = y ? D(i - w) : a; c = (x && y) ? D(i - w - 1) : b;
#undef D
    return prev[i] + med(a, b, c);
}
static inline uchar zz(int r) { char s = (char)(uchar)r; return (uchar)(((uchar)s << 1) ^ (uchar)(s >> 7)); }

kernel void kvid_strips(device const uchar *file [[buffer(0)]], device const ulong *frame_at [[buffer(1)]],
                        device uchar *res [[buffer(2)]], device uchar *flags [[buffer(3)]], constant kv_geom &g [[buffer(4)]],
                        uint tid [[thread_position_in_threadgroup]], uint tg [[threadgroup_position_in_grid]],
                        uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint part[TG / 32][3];
    threadgroup uint best_k;
    uint f = tg / g.strips_per_frame, r = tg % g.strips_per_frame, p = 0;
    while (p + 1 < g.np && r >= g.strip_base[p + 1]) p++;
    uint s = r - g.strip_base[p], w = g.w[p], h = g.h[p];
    uint y0 = s * STRIP, y1 = min(y0 + STRIP, h), count = (y1 - y0) * w;
    device const uchar *cur = file + frame_at[f] + g.pstart[p];
    device const uchar *prev = f ? file + frame_at[f - 1] + g.pstart[p] : cur;
    uint nk = f ? 3u : 1u;
    uint c[3] = {0, 0, 0};
    for (uint j = tid; j < count; j += TG) {
        uint y = y0 + j / w, x = j % w, i = y * w + x;
        for (uint k = 0; k < nk; k++) {
            char rr = (char)(uchar)(cur[i] - predict(k, cur, prev, w, x, y));
            c[k] += (uint)abs((int)rr);
        }
    }
    for (uint k = 0; k < 3; k++) { uint t = simd_sum(c[k]); if (lane == 0) part[sg][k] = t; }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (tid == 0) {
        uint bk = 0, bc = 0xFFFFFFFFu;
        for (uint k = 0; k < nk; k++) {
            uint t = 0; for (uint q = 0; q < TG / 32; q++) t += part[q][k];
            if (t < bc) { bc = t; bk = k; }
        }
        best_k = bk;
        flags[(ulong)f * g.strips_per_frame + r] = (uchar)bk;
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint k = best_k;
    device uchar *out = res + g.plane_res[p] + (ulong)f * w * h;
    for (uint j = tid; j < count; j += TG) {
        uint y = y0 + j / w, x = j % w, i = y * w + x;
        out[i] = zz(cur[i] - predict(k, cur, prev, w, x, y));
    }
}
