/* Suffix array and BWT on the GPU (Metal), host side. Apache-2.0, Kolmo contributors.
 *
 * Algorithm: prefix doubling (Manber and Myers; the GPU formulation of Deo and Keely, and libcubwt's use of a radix
 * sort over (rank, rank at +h) pairs). No libcubwt code is used; only its C API is mirrored in cubwt_metal.c.
 *   1. Sort all suffixes by their first eight bytes (zero padded) with a stable LSD radix sort of 64-bit keys.
 *   2. Each group of equal keys gets the SA position of its first member as its rank (isa[i]). Groups of one are done.
 *   3. While some groups have two or more members ("active"), sort the active suffixes i by (isa[i], isa[i+h]) and
 *      split the groups again; h doubles each round (8, 16, 32, ...). Past the end of the text the second part of the
 *      key is below every rank and decreases with i + h, so a suffix that ends first sorts first, as with libsais'
 *      implicit sentinel; that also orders the zero padding of step 1 correctly.
 * Radix passes whose digit is the same in every key are skipped (the OR and AND of the keys are read back with the
 * count of active suffixes, once per round). Memory: about 44 bytes per input byte while sorting.
 * The result is the unique suffix array, so it equals libsais' bit for bit; tests/test_gpu_sa.c checks that. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "metal.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const unsigned char sa_source[] = {
#include "sa_metal.inc"
};

enum { TG = 256, TILE = 4096, STILE = 2048 };   /* threads per group; scan tile; sort tile (as in sa.metal) */

static struct {
    id<MTLDevice> dev;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> hist, scatter, sreduce, spart, sdown, init, apply, heads, rank, compact, keys, total, bwt;
    int ok;
    double gpu_seconds; int rounds;
} M;
static pthread_once_t M_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t M_mu = PTHREAD_MUTEX_INITIALIZER;

static id<MTLComputePipelineState> pipe_of(id<MTLLibrary> lib, const char *name, NSUInteger need) {
    id<MTLFunction> f = [lib newFunctionWithName:[NSString stringWithUTF8String:name]];
    if (!f) return nil;
    NSError *e = nil;
    id<MTLComputePipelineState> p = [M.dev newComputePipelineStateWithFunction:f error:&e];
    if (!p || p.maxTotalThreadsPerThreadgroup < need || p.threadExecutionWidth != 32) return nil;
    return p;
}

static void setup(void) {
    @autoreleasepool {
        if (getenv("KOLMO_NO_METAL")) return;
        M.dev = MTLCreateSystemDefaultDevice();
        if (!M.dev) return;
        NSString *src = [[NSString alloc] initWithBytes:sa_source length:sizeof(sa_source) encoding:NSUTF8StringEncoding];
        NSError *e = nil;
        MTLCompileOptions *opt = [MTLCompileOptions new];
        opt.mathMode = MTLMathModeSafe;
        id<MTLLibrary> lib = src ? [M.dev newLibraryWithSource:src options:opt error:&e] : nil;
        if (!lib) return;
        M.queue = [M.dev newCommandQueue];
        M.hist = pipe_of(lib, "rs_hist", TG); M.scatter = pipe_of(lib, "rs_scatter", TG);
        M.sreduce = pipe_of(lib, "scan_reduce", TG); M.spart = pipe_of(lib, "scan_partials", 1024); M.sdown = pipe_of(lib, "scan_down", TG);
        M.init = pipe_of(lib, "sa_init_keys", TG); M.apply = pipe_of(lib, "sa_apply", TG); M.heads = pipe_of(lib, "sa_heads", TG);
        M.rank = pipe_of(lib, "sa_rank", TG); M.compact = pipe_of(lib, "sa_compact", TG); M.keys = pipe_of(lib, "sa_keys", TG);
        M.total = pipe_of(lib, "copy_total", 1); M.bwt = pipe_of(lib, "sa_bwt", TG);
        M.ok = M.queue && M.hist && M.scatter && M.sreduce && M.spart && M.sdown && M.init && M.apply && M.heads && M.rank
            && M.compact && M.keys && M.total && M.bwt;
    }
}

int kolmo_metal_available(void) { pthread_once(&M_once, setup); return M.ok; }
size_t kolmo_metal_sa_bytes(size_t n) { return 44 * n + 2 * n + (n / STILE + 2) * 4 * 260 + (1u << 16); }
double kolmo_metal_last_gpu_seconds(void) { return M.gpu_seconds; }
size_t kolmo_metal_budget(void) { return kolmo_metal_available() ? (size_t)(M.dev.recommendedMaxWorkingSetSize / 2) : 0; }
int kolmo_metal_last_rounds(void) { return M.rounds; }

static inline uint32_t tiles_of(uint32_t n) { return (n + TILE - 1) / TILE; }
static inline uint32_t stiles_of(uint32_t n) { return (n + STILE - 1) / STILE; }

static void grid(id<MTLComputeCommandEncoder> e, uint32_t threads) {   /* dispatch after the pipeline and buffers are set */
    [e dispatchThreadgroups:MTLSizeMake((threads + TG - 1) / TG, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
}
static void u32(id<MTLComputeCommandEncoder> e, uint32_t v, int at) { [e setBytes:&v length:4 atIndex:at]; }
static void buf(id<MTLComputeCommandEncoder> e, id<MTLBuffer> b, size_t off, int at) { [e setBuffer:b offset:off atIndex:at]; }

/* exclusive scan of n u32 at (a, aoff) into (out, ooff); part needs tiles_of(n) + 1 entries; the total lands in
 * part[tiles_of(n)] */
static void scan(id<MTLComputeCommandEncoder> e, id<MTLBuffer> a, size_t aoff, id<MTLBuffer> out, size_t ooff, id<MTLBuffer> part, uint32_t n) {
    uint32_t t = tiles_of(n);
    [e setComputePipelineState:M.sreduce];
    buf(e, a, aoff, 0); buf(e, part, 0, 1); u32(e, n, 2);
    [e dispatchThreadgroups:MTLSizeMake(t, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
    [e setComputePipelineState:M.spart];
    buf(e, part, 0, 0); u32(e, t, 1);
    [e dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1024, 1, 1)];
    [e setComputePipelineState:M.sdown];
    buf(e, a, aoff, 0); buf(e, out, ooff, 1); buf(e, part, 0, 2); u32(e, n, 3);
    [e dispatchThreadgroups:MTLSizeMake(t, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
}

/* stable LSD sort of m (key, value) pairs in K[*cur], V[*cur], on the bytes where the keys differ (vary) */
static void sort_pairs(id<MTLComputeCommandEncoder> e, id<MTLBuffer> __strong *K, id<MTLBuffer> __strong *V, int *cur, uint32_t m,
                       uint64_t vary, id<MTLBuffer> counts, id<MTLBuffer> part) {
    uint32_t t = stiles_of(m);
    for (uint32_t d = 0; d < 8; d++) {
        if (!((vary >> (8 * d)) & 255)) continue;
        uint32_t shift = 8 * d;
        [e setComputePipelineState:M.hist];
        buf(e, K[*cur], 0, 0); buf(e, counts, 0, 1); u32(e, m, 2); u32(e, shift, 3); u32(e, t, 4);
        [e dispatchThreadgroups:MTLSizeMake(t, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
        scan(e, counts, 0, counts, 0, part, 256 * t);
        [e setComputePipelineState:M.scatter];
        buf(e, K[*cur], 0, 0); buf(e, V[*cur], 0, 1); buf(e, K[*cur ^ 1], 0, 2); buf(e, V[*cur ^ 1], 0, 3);
        buf(e, counts, 0, 4); u32(e, m, 5); u32(e, shift, 6); u32(e, t, 7);
        [e dispatchThreadgroups:MTLSizeMake(t, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
        *cur ^= 1;
    }
}

static double last_cb;
static int run(id<MTLCommandBuffer> cb) {
    [cb commit]; [cb waitUntilCompleted];
    last_cb = cb.GPUEndTime - cb.GPUStartTime;
    if (cb.status != MTLCommandBufferStatusCompleted) {
        if (getenv("KOLMO_METAL_DEBUG")) fprintf(stderr, "metal: %s\n", cb.error.description.UTF8String);
        return -1;
    }
    M.gpu_seconds += cb.GPUEndTime - cb.GPUStartTime;
    return 0;
}

static id<MTLBuffer> nb(size_t bytes, int shared) {
    return [M.dev newBufferWithLength:bytes ? bytes : 16 options:shared ? MTLResourceStorageModeShared : MTLResourceStorageModePrivate];
}

/* Sorts the suffixes of t; on success the suffix array and the ranks (isa) are left in *SA and *ISA (shared). */
static int suffix_sort(const uint8_t *t, uint32_t n, id<MTLBuffer> *SAo, id<MTLBuffer> *ISAo, id<MTLBuffer> *To) {
    M.gpu_seconds = 0; M.rounds = 0;
    id<MTLBuffer> T = nb((size_t)n + 16, 1), SA = nb(4 * (size_t)n, 1), ISA = nb(4 * (size_t)n, 1);
    id<MTLBuffer> K[2] = {nb(8 * (size_t)n, 0), nb(8 * (size_t)n, 0)}, V[2] = {nb(4 * (size_t)n, 0), nb(4 * (size_t)n, 0)};
    id<MTLBuffer> P[2] = {nb(4 * (size_t)n, 0), nb(4 * (size_t)n, 0)}, KEEP = nb(4 * (size_t)n, 0);
    uint32_t tmax = tiles_of(n), smax = stiles_of(n);
    id<MTLBuffer> counts = nb(4 * (size_t)256 * smax + 64, 0), partA = nb(4 * ((size_t)256 * smax / TILE + 2) + 64, 0);
    id<MTLBuffer> partB = nb(4 * ((size_t)tmax + 2), 0), partC = nb(4 * ((size_t)tmax + 2), 0);
    id<MTLBuffer> orand = nb(16, 1), res = nb(16, 1);
    if (!T || !SA || !ISA || !K[0] || !K[1] || !V[0] || !V[1] || !P[0] || !P[1] || !KEEP || !counts || !partA || !partB || !partC || !orand || !res)
        return -2;
    memcpy(T.contents, t, n); memset((uint8_t *)T.contents + n, 0, 16);
    uint32_t *ob = orand.contents; ob[0] = ob[1] = 0; ob[2] = ob[3] = ~0u;

    id<MTLCommandBuffer> cb = [M.queue commandBuffer];
    id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
    [e setComputePipelineState:M.init];
    buf(e, T, 0, 0); buf(e, K[0], 0, 1); buf(e, V[0], 0, 2); buf(e, orand, 0, 3); buf(e, P[0], 0, 4); u32(e, n, 5);
    grid(e, n);
    [e endEncoding];
    if (run(cb)) return -7;

    uint32_t m = n, h = 8, sorted_h = 0; int pc = 0;     /* sorted_h: the h of the keys this round sorts (0: first bytes) */
    for (;;) {
        uint64_t vary = ((uint64_t)ob[1] << 32 | ob[0]) ^ ((uint64_t)ob[3] << 32 | ob[2]);
        ob[0] = ob[1] = 0; ob[2] = ob[3] = ~0u;
        int cur = 0;
        cb = [M.queue commandBuffer]; e = [cb computeCommandEncoder];
        sort_pairs(e, K, V, &cur, m, vary, counts, partA);
        id<MTLBuffer> Kc = K[cur], Vc = V[cur], Ko = K[cur ^ 1], Vo = V[cur ^ 1];
        size_t eoff = 4 * (size_t)n;                     /* F at Ko + 0, E at Ko + 4n, H in Vo */
        [e setComputePipelineState:M.apply];
        buf(e, Kc, 0, 0); buf(e, Vc, 0, 1); buf(e, P[pc], 0, 2); buf(e, SA, 0, 3); buf(e, Ko, 0, 4); u32(e, m, 5);
        grid(e, m);
        scan(e, Ko, 0, Ko, eoff, partB, m);
        [e setComputePipelineState:M.heads];
        buf(e, Ko, 0, 0); buf(e, Ko, eoff, 1); buf(e, P[pc], 0, 2); buf(e, Vo, 0, 3); u32(e, m, 4);
        grid(e, m);
        [e setComputePipelineState:M.rank];
        buf(e, Ko, 0, 0); buf(e, Ko, eoff, 1); buf(e, Vo, 0, 2); buf(e, Vc, 0, 3); buf(e, ISA, 0, 4); buf(e, KEEP, 0, 5); u32(e, m, 6);
        grid(e, m);
        scan(e, KEEP, 0, Ko, 0, partC, m);
        [e setComputePipelineState:M.compact];
        buf(e, KEEP, 0, 0); buf(e, Ko, 0, 1); buf(e, P[pc], 0, 2); buf(e, P[pc ^ 1], 0, 3); u32(e, m, 4);
        grid(e, m);
        [e setComputePipelineState:M.total];
        buf(e, partC, 0, 0); buf(e, res, 0, 1); u32(e, tiles_of(m), 2);
        [e dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        /* keys of the next round into K[0], V[0] (the sorted pairs are no longer needed) */
        [e setComputePipelineState:M.keys];
        buf(e, P[pc ^ 1], 0, 0); buf(e, SA, 0, 1); buf(e, ISA, 0, 2); buf(e, K[0], 0, 3); buf(e, V[0], 0, 4);
        uint32_t sbits = 0; while (sbits < 32 && (((uint64_t)n - 1 + h) >> sbits)) sbits++;
        buf(e, orand, 0, 5); buf(e, res, 0, 6); u32(e, n, 7); u32(e, h, 8); u32(e, sbits, 9);
        grid(e, m);
        [e endEncoding];
        if (run(cb)) return -7;
        M.rounds++;
        pc ^= 1;
        m = ((uint32_t *)res.contents)[0];
        if (getenv("KOLMO_METAL_DEBUG")) fprintf(stderr, "round %d: vary %016llx -> m %u (h %u) %.2f ms\n", M.rounds, (unsigned long long)vary, m, h, last_cb * 1e3);
        if (!m) break;
        if (sorted_h >= n) return -7;                    /* cannot happen: every suffix is apart once h >= n */
        sorted_h = h; h *= 2;
    }
    *SAo = SA; *ISAo = ISA; *To = T;
    return 0;
}

int kolmo_metal_sa(const uint8_t *t, int32_t *sa, int32_t n) {
    if (n < 2 || n > (1 << 30) || !kolmo_metal_available()) return -1;
    pthread_mutex_lock(&M_mu);
    int r;
    @autoreleasepool {
        id<MTLBuffer> SA = nil, ISA = nil, T = nil;
        r = suffix_sort(t, (uint32_t)n, &SA, &ISA, &T);
        if (!r) memcpy(sa, SA.contents, 4 * (size_t)n);
    }
    pthread_mutex_unlock(&M_mu);
    return r;
}

int kolmo_metal_bwt_aux(const uint8_t *t, uint8_t *u, int32_t n, int32_t r, int32_t *I) {
    if (n < 2 || n > (1 << 30) || r < 2 || (r & (r - 1)) || !kolmo_metal_available()) return -1;
    pthread_mutex_lock(&M_mu);
    int ret;
    @autoreleasepool {
        id<MTLBuffer> SA = nil, ISA = nil, T = nil;
        ret = suffix_sort(t, (uint32_t)n, &SA, &ISA, &T);
        if (!ret) {
            const uint32_t *isa = ISA.contents;
            uint32_t p0 = isa[0];
            id<MTLBuffer> U = nb((size_t)n, 1);
            if (!U) ret = -2;
            else {
                id<MTLCommandBuffer> cb = [M.queue commandBuffer];
                id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
                [e setComputePipelineState:M.bwt];
                buf(e, T, 0, 0); buf(e, SA, 0, 1); buf(e, U, 0, 2); u32(e, (uint32_t)n, 3); u32(e, p0, 4);
                grid(e, (uint32_t)n);
                [e endEncoding];
                ret = run(cb) ? -7 : 0;
                if (!ret) {
                    for (int32_t k = 0; k <= (n - 1) / r; k++) I[k] = (int32_t)isa[(size_t)k * r] + 1;
                    memcpy(u, U.contents, (size_t)n);
                }
            }
        }
    }
    pthread_mutex_unlock(&M_mu);
    return ret;
}

#ifdef KOLMO_METAL_BENCH
/* kernel timing on the first eight bytes of t: 8 x hist, 8 x scan of the counts, 8 x scatter, each in its own
 * command buffer */
void kolmo_metal_bench_sort(const uint8_t *t, uint32_t n) {
    if (!kolmo_metal_available()) return;
    @autoreleasepool {
        id<MTLBuffer> T = nb((size_t)n + 16, 1), K[2] = {nb(8 * (size_t)n, 0), nb(8 * (size_t)n, 0)}, V[2] = {nb(4 * (size_t)n, 0), nb(4 * (size_t)n, 0)};
        id<MTLBuffer> P = nb(4 * (size_t)n, 0), orand = nb(16, 1);
        uint32_t smax = stiles_of(n);
        id<MTLBuffer> counts = nb(4 * (size_t)256 * smax + 64, 0), part = nb(4 * ((size_t)256 * smax / TILE + 2) + 64, 0);
        memcpy(T.contents, t, n);
        id<MTLCommandBuffer> cb = [M.queue commandBuffer]; id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:M.init];
        buf(e, T, 0, 0); buf(e, K[0], 0, 1); buf(e, V[0], 0, 2); buf(e, orand, 0, 3); buf(e, P, 0, 4); u32(e, n, 5);
        grid(e, n); [e endEncoding]; run(cb);
        double th = 0, ts = 0, tc = 0;
        for (uint32_t d = 0; d < 8; d++) {
            uint32_t shift = 8 * d, tl = smax;
            cb = [M.queue commandBuffer]; e = [cb computeCommandEncoder];
            [e setComputePipelineState:M.hist];
            buf(e, K[0], 0, 0); buf(e, counts, 0, 1); u32(e, n, 2); u32(e, shift, 3); u32(e, tl, 4);
            [e dispatchThreadgroups:MTLSizeMake(tl, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
            [e endEncoding]; run(cb); th += last_cb;
            cb = [M.queue commandBuffer]; e = [cb computeCommandEncoder];
            scan(e, counts, 0, counts, 0, part, 256 * tl);
            [e endEncoding]; run(cb); tc += last_cb;
            cb = [M.queue commandBuffer]; e = [cb computeCommandEncoder];
            [e setComputePipelineState:M.scatter];
            buf(e, K[0], 0, 0); buf(e, V[0], 0, 1); buf(e, K[1], 0, 2); buf(e, V[1], 0, 3);
            buf(e, counts, 0, 4); u32(e, n, 5); u32(e, shift, 6); u32(e, tl, 7);
            [e dispatchThreadgroups:MTLSizeMake(tl, 1, 1) threadsPerThreadgroup:MTLSizeMake(TG, 1, 1)];
            [e endEncoding]; run(cb); ts += last_cb;
        }
        fprintf(stderr, "n=%u: 8 x hist %.2f ms, 8 x scan %.2f ms, 8 x scatter %.2f ms\n", n, th * 1e3, tc * 1e3, ts * 1e3);
    }
}
#endif
