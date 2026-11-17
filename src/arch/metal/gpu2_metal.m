/* gpu2 kernels, host side (Metal). Apache-2.0, Kolmo contributors. See gpu2.h; kernels in gpu2.metal and kvid.metal.
 *
 * Memory: Apple GPUs share memory with the CPU, so a page-aligned host allocation (kg2_metal_alloc) is wrapped as a
 * Metal buffer without a copy (newBufferWithBytesNoCopy), once per allocation; any other pointer is copied in (and
 * out). Scratch buffers are kept between calls and only grow. One call at a time (a mutex). */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "gpu2.h"
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static const unsigned char gpu2_source[] = {
#include "gpu2_metal.inc"
};
static const unsigned char kvid2_source[] = {
#include "kvid_metal.inc"
};

enum { NSCR = 24 };
static struct {
    id<MTLDevice> dev;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> scan32, scan64, tbcount, tbstarts, tbfields, tbresid, hist, o1part, o1sum,
        knorm, kenc, ksizes, kpack, kdec, fscreen, fscore, kvid;
    id<MTLBuffer> scr[NSCR], logtab;
    int ok;
    double setup_s, gpu_s;
} G;
static pthread_once_t G_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t G_mu = PTHREAD_MUTEX_INITIALIZER;

/* registry of zero-copy allocations */
typedef struct { void *p; size_t n; __strong id<MTLBuffer> b; } kg2_alloc;
static kg2_alloc A_[256]; static int A_n; static pthread_mutex_t A_mu = PTHREAD_MUTEX_INITIALIZER;

static double now_s(void) {
    static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-9;
}

static id<MTLComputePipelineState> pipe_of(id<MTLLibrary> lib, const char *name) {
    id<MTLFunction> f = [lib newFunctionWithName:[NSString stringWithUTF8String:name]];
    if (!f) { fprintf(stderr, "gpu2: no function %s\n", name); return nil; }
    NSError *e = nil;
    id<MTLComputePipelineState> p = [G.dev newComputePipelineStateWithFunction:f error:&e];
    if (!p) fprintf(stderr, "gpu2: pipeline %s: %s\n", name, e.localizedDescription.UTF8String);
    return p;
}
static id<MTLLibrary> lib_of(const unsigned char *src, size_t n) {
    NSString *s = [[NSString alloc] initWithBytes:src length:n encoding:NSUTF8StringEncoding];
    NSError *e = nil;
    MTLCompileOptions *opt = [MTLCompileOptions new];
    opt.mathMode = MTLMathModeSafe;
    id<MTLLibrary> lib = s ? [G.dev newLibraryWithSource:s options:opt error:&e] : nil;
    if (!lib) fprintf(stderr, "gpu2: compile: %s\n", e.localizedDescription.UTF8String);
    return lib;
}

static void setup(void) {
    @autoreleasepool {
        double t0 = now_s();
        if (getenv("KOLMO_NO_METAL")) return;
        G.dev = MTLCreateSystemDefaultDevice();
        if (!G.dev) return;
        id<MTLLibrary> lib = lib_of(gpu2_source, sizeof gpu2_source), kl = lib_of(kvid2_source, sizeof kvid2_source);
        if (!lib || !kl) return;
        G.queue = [G.dev newCommandQueue];
        G.scan32 = pipe_of(lib, "scan_u32"); G.scan64 = pipe_of(lib, "scan_u64");
        G.tbcount = pipe_of(lib, "tb_count"); G.tbstarts = pipe_of(lib, "tb_starts"); G.tbfields = pipe_of(lib, "tb_fields");
        G.tbresid = pipe_of(lib, "tb_resid"); G.hist = pipe_of(lib, "hist_segs"); G.o1part = pipe_of(lib, "hist_o1_part");
        G.o1sum = pipe_of(lib, "hist_o1_sum"); G.knorm = pipe_of(lib, "krw_norm_k"); G.kenc = pipe_of(lib, "krw_enc");
        G.ksizes = pipe_of(lib, "krw_sizes"); G.kpack = pipe_of(lib, "krw_pack"); G.kdec = pipe_of(lib, "krw_dec");
        G.fscreen = pipe_of(lib, "fm_screen"); G.fscore = pipe_of(lib, "fm_score"); G.kvid = pipe_of(kl, "kvid_strips");
        uint32_t tab[1024]; kg2_log_table(tab);
        G.logtab = [G.dev newBufferWithBytes:tab length:sizeof tab options:MTLResourceStorageModeShared];
        G.ok = G.queue && G.scan32 && G.scan64 && G.tbcount && G.tbstarts && G.tbfields && G.tbresid && G.hist && G.o1part
            && G.o1sum && G.knorm && G.kenc && G.ksizes && G.kpack && G.kdec && G.fscreen && G.fscore && G.kvid && G.logtab
            && G.scan32.maxTotalThreadsPerThreadgroup >= 1024 && G.scan64.maxTotalThreadsPerThreadgroup >= 1024
            && G.hist.threadExecutionWidth == 32;
        G.setup_s = now_s() - t0;
    }
}
int kg2_metal_available(void) { pthread_once(&G_once, setup); return G.ok; }
double kg2_metal_setup_seconds(void) { return G.setup_s; }
double kg2_metal_last_gpu_seconds(void) { return G.gpu_s; }

void *kg2_metal_alloc(size_t n) {
    size_t pg = (size_t)getpagesize(), m = (n + pg - 1) / pg * pg;
    if (!m) m = pg;
    void *p = mmap(NULL, m, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) return NULL;
    pthread_mutex_lock(&A_mu);
    if (A_n < 256) { A_[A_n].p = p; A_[A_n].n = m; A_[A_n].b = nil; A_n++; }
    pthread_mutex_unlock(&A_mu);
    return p;
}
void kg2_metal_free(void *p, size_t n) {
    size_t pg = (size_t)getpagesize(), m = (n + pg - 1) / pg * pg;
    if (!m) m = pg;
    pthread_mutex_lock(&A_mu);
    for (int i = 0; i < A_n; i++) if (A_[i].p == p) { A_[i] = A_[--A_n]; A_[A_n].b = nil; break; }
    pthread_mutex_unlock(&A_mu);
    munmap(p, m);
}

/* a buffer and offset for [p, p + n): zero-copy inside a registered allocation, else a copy (cp = 1) */
typedef struct { __strong id<MTLBuffer> b; NSUInteger off; int cp; } bref;
static bref ref_of(const void *p, size_t n, int in) {
    bref r = {nil, 0, 0};
    pthread_mutex_lock(&A_mu);
    for (int i = 0; i < A_n; i++) {
        uint8_t *b = A_[i].p;
        if ((const uint8_t *)p >= b && (const uint8_t *)p + n <= b + A_[i].n) {
            if (!A_[i].b) A_[i].b = [G.dev newBufferWithBytesNoCopy:b length:A_[i].n options:MTLResourceStorageModeShared deallocator:nil];
            r.b = A_[i].b; r.off = (NSUInteger)((const uint8_t *)p - b);
            break;
        }
    }
    pthread_mutex_unlock(&A_mu);
    if (r.b) return r;
    r.cp = 1;
    size_t m = n ? n : 16;
    r.b = in ? [G.dev newBufferWithBytes:p length:m options:MTLResourceStorageModeShared]
             : [G.dev newBufferWithLength:m options:MTLResourceStorageModeShared];
    return r;
}
static void ref_done(bref *r, void *p, size_t n) { if (r->cp && n) memcpy(p, r->b.contents, n); r->b = nil; }

static id<MTLBuffer> scratch(int k, size_t n) {
    if (!n) n = 16;
    if (!G.scr[k] || G.scr[k].length < n) G.scr[k] = [G.dev newBufferWithLength:n options:MTLResourceStorageModeShared];
    return G.scr[k];
}
static void dispatch1(id<MTLComputeCommandEncoder> e, id<MTLComputePipelineState> p, NSUInteger threads, NSUInteger tg) {
    [e setComputePipelineState:p];
    if (tg > p.maxTotalThreadsPerThreadgroup) tg = p.maxTotalThreadsPerThreadgroup;
    [e dispatchThreads:MTLSizeMake(threads ? threads : 1, 1, 1) threadsPerThreadgroup:MTLSizeMake(tg, 1, 1)];
}
static int run(id<MTLCommandBuffer> cb) {
    [cb commit]; [cb waitUntilCompleted];
    if (cb.status != MTLCommandBufferStatusCompleted) { fprintf(stderr, "gpu2: command buffer failed: %s\n", cb.error.localizedDescription.UTF8String); return -1; }
    G.gpu_s += cb.GPUEndTime - cb.GPUStartTime;
    return 0;
}

/* ---------------- table front end ---------------- */
typedef struct { uint32_t n, rows, cols, mode, sep, tiles; } tb_args;
static int table_body(const uint8_t *d, size_t n, int mode, uint8_t sep, kg2_table *t) {
    G.gpu_s = 0;
    tb_args A = {(uint32_t)n, 0, t->cols, (uint32_t)mode, sep, (uint32_t)((n + KG2_LINE_TILE - 1) / KG2_LINE_TILE)};
    bref D = ref_of(d, n, 1);
    id<MTLBuffer> cnt = scratch(0, (size_t)A.tiles * 4), pre = scratch(1, (size_t)A.tiles * 4 + 4);
    id<MTLCommandBuffer> cb = [G.queue commandBuffer];
    id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
    [e setBuffer:D.b offset:D.off atIndex:0]; [e setBuffer:cnt offset:0 atIndex:1]; [e setBytes:&A length:sizeof A atIndex:2];
    [e setComputePipelineState:G.tbcount];
    [e dispatchThreadgroups:MTLSizeMake(A.tiles, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
    [e setBuffer:cnt offset:0 atIndex:0]; [e setBuffer:pre offset:0 atIndex:1]; [e setBytes:&A.tiles length:4 atIndex:2];
    dispatch1(e, G.scan32, 1024, 1024);
    [e endEncoding];
    if (run(cb)) return -1;
    uint32_t rows = ((uint32_t *)pre.contents)[A.tiles] + 1;
    if (rows > t->cap_rows) return -1;
    t->rows = A.rows = rows;
    size_t F = (size_t)rows * t->cols;
    bref LS = ref_of(t->line_start, (size_t)(rows + 1) * 4, 0), OFF = ref_of(t->off, F * 4, 0), LEN = ref_of(t->len, F * 2, 0),
         HS = ref_of(t->hash, F * 4, 0), VAL = ref_of(t->val, F * 8, 0), FMT = ref_of(t->fmt, F, 0),
         RF = ref_of(t->rowflag, rows, 0), FL = ref_of(t->fl, F, 0), RES = ref_of(t->res, F * 8, 0);
    cb = [G.queue commandBuffer]; e = [cb computeCommandEncoder];
    [e setBuffer:D.b offset:D.off atIndex:0]; [e setBuffer:pre offset:0 atIndex:1]; [e setBuffer:LS.b offset:LS.off atIndex:2];
    [e setBytes:&A length:sizeof A atIndex:3];
    [e setComputePipelineState:G.tbstarts];
    [e dispatchThreadgroups:MTLSizeMake(A.tiles, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
    [e setBuffer:LS.b offset:LS.off atIndex:1]; [e setBuffer:OFF.b offset:OFF.off atIndex:2]; [e setBuffer:LEN.b offset:LEN.off atIndex:3];
    [e setBuffer:HS.b offset:HS.off atIndex:4]; [e setBuffer:VAL.b offset:VAL.off atIndex:5]; [e setBuffer:FMT.b offset:FMT.off atIndex:6];
    [e setBuffer:RF.b offset:RF.off atIndex:7]; [e setBytes:&A length:sizeof A atIndex:8];
    dispatch1(e, G.tbfields, rows, 64);
    [e setBuffer:OFF.b offset:OFF.off atIndex:1]; [e setBuffer:LEN.b offset:LEN.off atIndex:2]; [e setBuffer:HS.b offset:HS.off atIndex:3];
    [e setBuffer:VAL.b offset:VAL.off atIndex:4]; [e setBuffer:FMT.b offset:FMT.off atIndex:5]; [e setBuffer:FL.b offset:FL.off atIndex:6];
    [e setBuffer:RES.b offset:RES.off atIndex:7]; [e setBytes:&A length:sizeof A atIndex:8];
    [e setComputePipelineState:G.tbresid];
    [e dispatchThreads:MTLSizeMake(rows, t->cols, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
    [e endEncoding];
    if (run(cb)) return -1;
    ref_done(&LS, t->line_start, (size_t)(rows + 1) * 4); ref_done(&OFF, t->off, F * 4); ref_done(&LEN, t->len, F * 2);
    ref_done(&HS, t->hash, F * 4); ref_done(&VAL, t->val, F * 8); ref_done(&FMT, t->fmt, F); ref_done(&RF, t->rowflag, rows);
    ref_done(&FL, t->fl, F); ref_done(&RES, t->res, F * 8);
    return 0;
}
int kg2_metal_table(const uint8_t *d, size_t n, int mode, uint8_t sep, kg2_table *t) {
    if (!kg2_metal_available() || !n || n >= 0xFFFFFFF0u || !t->cols) return -1;
    int rc;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool { rc = table_body(d, n, mode, sep, t); }
    pthread_mutex_unlock(&G_mu);
    return rc;
}

/* ---------------- histograms ---------------- */
static void enc_hist(id<MTLComputeCommandEncoder> e, bref *D, id<MTLBuffer> off, id<MTLBuffer> h, id<MTLBuffer> bits, uint32_t nseg) {
    [e setComputePipelineState:G.hist];
    [e setBuffer:D->b offset:D->off atIndex:0]; [e setBuffer:off offset:0 atIndex:1]; [e setBuffer:h offset:0 atIndex:2];
    [e setBuffer:bits offset:0 atIndex:3]; [e setBuffer:G.logtab offset:0 atIndex:4];
    [e dispatchThreadgroups:MTLSizeMake(nseg, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
}
int kg2_metal_hist_segs(const uint8_t *d, size_t n, const uint64_t *off, uint32_t nseg, uint32_t *h, uint64_t *bits) {
    if (!kg2_metal_available() || !nseg) return -1;
    int rc = -1;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool {
        G.gpu_s = 0;
        bref D = ref_of(d, n, 1), O = ref_of(off, (size_t)(nseg + 1) * 8, 1), H = ref_of(h, (size_t)nseg * 1024, 0),
             B = ref_of(bits, (size_t)nseg * 8, 0);
        id<MTLCommandBuffer> cb = [G.queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        bref Hh = H, Bb = B;
        [e setComputePipelineState:G.hist];
        [e setBuffer:D.b offset:D.off atIndex:0]; [e setBuffer:O.b offset:O.off atIndex:1]; [e setBuffer:Hh.b offset:Hh.off atIndex:2];
        [e setBuffer:Bb.b offset:Bb.off atIndex:3]; [e setBuffer:G.logtab offset:0 atIndex:4];
        [e dispatchThreadgroups:MTLSizeMake(nseg, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
        [e endEncoding];
        if (!run(cb)) { ref_done(&H, h, (size_t)nseg * 1024); ref_done(&B, bits, (size_t)nseg * 8); rc = 0; }
    }
    pthread_mutex_unlock(&G_mu);
    return rc;
}
int kg2_metal_hist_o1(const uint8_t *d, size_t n, uint32_t *h) {
    if (!kg2_metal_available()) return -1;
    int rc = -1;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool {
        G.gpu_s = 0;
        uint32_t Gc = 64; uint64_t nn = n;
        bref D = ref_of(d, n, 1), H = ref_of(h, 65536 * 4, 0);
        id<MTLBuffer> parts = scratch(2, (size_t)Gc * 65536 * 4);
        id<MTLCommandBuffer> cb = [G.queue commandBuffer];
        id<MTLBlitCommandEncoder> bl = [cb blitCommandEncoder];
        [bl fillBuffer:parts range:NSMakeRange(0, (size_t)Gc * 65536 * 4) value:0]; [bl endEncoding];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:G.o1part];
        [e setBuffer:D.b offset:D.off atIndex:0]; [e setBuffer:parts offset:0 atIndex:1]; [e setBytes:&nn length:8 atIndex:2];
        [e setBytes:&Gc length:4 atIndex:3];
        NSUInteger groups = n < (1u << 20) ? Gc : 1024;
        [e dispatchThreadgroups:MTLSizeMake(groups, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
        [e setBuffer:parts offset:0 atIndex:0]; [e setBuffer:H.b offset:H.off atIndex:1]; [e setBytes:&Gc length:4 atIndex:2];
        dispatch1(e, G.o1sum, 65536, 256);
        [e endEncoding];
        if (!run(cb)) { ref_done(&H, h, 65536 * 4); rc = 0; }
    }
    pthread_mutex_unlock(&G_mu);
    return rc;
}

/* ---------------- krw ---------------- */
typedef struct { uint32_t nseg, L, cap; } krw_args;
int kg2_metal_krw_encode(const uint8_t *d, size_t n, const uint64_t *off, uint32_t nseg, uint32_t L, uint8_t *out,
                         size_t cap, uint64_t *seg_out) {
    if (!kg2_metal_available() || !nseg || L < 32 || L > 1024 || L % 32) return -1;
    uint64_t maxseg = 0, tot = 0;
    for (uint32_t s = 0; s < nseg; s++) { uint64_t m = off[s + 1] - off[s]; if (m > maxseg) maxseg = m; tot += m; }
    if (maxseg >= (1u << 24) || cap < (uint64_t)nseg * krw_seg_header(L) + 2 * tot) return -1;
    int rc = -1;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool {
        G.gpu_s = 0;
        krw_args A = {nseg, L, (uint32_t)((maxseg + L - 1) / L + 2)};
        bref D = ref_of(d, n, 1), O = ref_of(off, (size_t)(nseg + 1) * 8, 1), OUT = ref_of(out, cap, 0);
        id<MTLBuffer> h = scratch(3, (size_t)nseg * 1024), bits = scratch(4, (size_t)nseg * 8), ft = scratch(5, (size_t)nseg * 512),
            sc = scratch(6, (size_t)nseg * A.cap * L * 2), st = scratch(7, (size_t)nseg * L * 4), cnt = scratch(8, (size_t)nseg * L * 4),
            lpre = scratch(9, (size_t)nseg * L * 4), ssz = scratch(10, (size_t)nseg * 8), sout = scratch(11, (size_t)(nseg + 1) * 8);
        id<MTLCommandBuffer> cb = [G.queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        enc_hist(e, &D, O.b, h, bits, nseg);
        [e setBuffer:O.b offset:O.off atIndex:1];
        [e setBuffer:h offset:0 atIndex:0]; [e setBuffer:ft offset:0 atIndex:2]; [e setBytes:&nseg length:4 atIndex:3];
        dispatch1(e, G.knorm, nseg, 64);
        [e setComputePipelineState:G.kenc];
        [e setBuffer:D.b offset:D.off atIndex:0]; [e setBuffer:ft offset:0 atIndex:2]; [e setBuffer:sc offset:0 atIndex:3];
        [e setBuffer:st offset:0 atIndex:4]; [e setBuffer:cnt offset:0 atIndex:5]; [e setBytes:&A length:sizeof A atIndex:6];
        [e dispatchThreadgroups:MTLSizeMake(nseg, 1, 1) threadsPerThreadgroup:MTLSizeMake(L, 1, 1)];
        [e setBuffer:cnt offset:0 atIndex:0]; [e setBuffer:lpre offset:0 atIndex:1]; [e setBuffer:ssz offset:0 atIndex:2];
        [e setBytes:&A length:sizeof A atIndex:3];
        dispatch1(e, G.ksizes, nseg, 64);
        [e setBuffer:ssz offset:0 atIndex:0]; [e setBuffer:sout offset:0 atIndex:1]; [e setBytes:&nseg length:4 atIndex:2];
        dispatch1(e, G.scan64, 1024, 1024);
        [e setComputePipelineState:G.kpack];
        [e setBuffer:ft offset:0 atIndex:0]; [e setBuffer:sc offset:0 atIndex:1]; [e setBuffer:st offset:0 atIndex:2];
        [e setBuffer:cnt offset:0 atIndex:3]; [e setBuffer:lpre offset:0 atIndex:4]; [e setBuffer:sout offset:0 atIndex:5];
        [e setBuffer:OUT.b offset:OUT.off atIndex:6]; [e setBytes:&A length:sizeof A atIndex:7];
        [e dispatchThreadgroups:MTLSizeMake(nseg, 1, 1) threadsPerThreadgroup:MTLSizeMake(L, 1, 1)];
        [e endEncoding];
        if (!run(cb)) {
            memcpy(seg_out, sout.contents, (size_t)(nseg + 1) * 8);
            ref_done(&OUT, out, seg_out[nseg]);
            rc = 0;
        }
    }
    pthread_mutex_unlock(&G_mu);
    return rc;
}
int kg2_metal_krw_decode(const uint8_t *code, size_t code_n, const uint64_t *seg_out, const uint64_t *off,
                         uint32_t nseg, uint32_t L, uint8_t *dst, size_t n) {
    if (!kg2_metal_available() || !nseg || L < 32 || L > 1024 || L % 32) return -1;
    int rc = -1;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool {
        G.gpu_s = 0;
        krw_args A = {nseg, L, 0};
        bref C = ref_of(code, code_n, 1), SO = ref_of(seg_out, (size_t)(nseg + 1) * 8, 1), O = ref_of(off, (size_t)(nseg + 1) * 8, 1),
             DST = ref_of(dst, n, 0);
        id<MTLCommandBuffer> cb = [G.queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:G.kdec];
        [e setBuffer:C.b offset:C.off atIndex:0]; [e setBuffer:SO.b offset:SO.off atIndex:1]; [e setBuffer:O.b offset:O.off atIndex:2];
        [e setBuffer:DST.b offset:DST.off atIndex:3]; [e setBytes:&A length:sizeof A atIndex:4];
        [e dispatchThreadgroups:MTLSizeMake(nseg, 1, 1) threadsPerThreadgroup:MTLSizeMake(L, 1, 1)];
        [e endEncoding];
        if (!run(cb)) { ref_done(&DST, dst, n); rc = 0; }
    }
    pthread_mutex_unlock(&G_mu);
    return rc;
}

/* ---------------- kvid strips, zero copy ---------------- */
typedef struct {
    uint32_t w[3], h[3], strips[3], pstart[3], strip_base[3], strips_per_frame, np, frames;
    uint64_t plane_res[3];
} kv_geom;
int kg2_metal_kvid(const uint8_t *file, size_t n, const uint64_t *frame_at, uint32_t frames, int np, const size_t *w,
                   const size_t *h, uint8_t *res, size_t res_n, uint8_t *flags, size_t flags_n) {
    if (!kg2_metal_available() || np < 1 || np > 3 || !frames) return -1;
    kv_geom g; memset(&g, 0, sizeof g);
    size_t fb = 0; uint32_t sp = 0;
    for (int p = 0; p < np; p++) {
        g.w[p] = (uint32_t)w[p]; g.h[p] = (uint32_t)h[p]; g.strips[p] = (uint32_t)((h[p] + 15) / 16);
        g.pstart[p] = (uint32_t)fb; g.strip_base[p] = sp; sp += g.strips[p]; fb += w[p] * h[p];
    }
    uint64_t acc = 0;
    for (int p = 0; p < np; p++) { g.plane_res[p] = acc; acc += (uint64_t)frames * w[p] * h[p]; }
    g.strips_per_frame = sp; g.np = (uint32_t)np; g.frames = frames;
    if (acc > res_n || (uint64_t)frames * sp > flags_n) return -1;
    int rc = -1;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool {
        G.gpu_s = 0;
        bref F = ref_of(file, n, 1), A = ref_of(frame_at, (size_t)frames * 8, 1), R = ref_of(res, (size_t)acc, 0),
             L = ref_of(flags, (size_t)frames * sp, 0);
        id<MTLCommandBuffer> cb = [G.queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setComputePipelineState:G.kvid];
        [e setBuffer:F.b offset:F.off atIndex:0]; [e setBuffer:A.b offset:A.off atIndex:1]; [e setBuffer:R.b offset:R.off atIndex:2];
        [e setBuffer:L.b offset:L.off atIndex:3]; [e setBytes:&g length:sizeof g atIndex:4];
        [e dispatchThreadgroups:MTLSizeMake((NSUInteger)frames * sp, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
        [e endEncoding];
        if (!run(cb)) { ref_done(&R, res, (size_t)acc); ref_done(&L, flags, (size_t)frames * sp); rc = 0; }
    }
    pthread_mutex_unlock(&G_mu);
    return rc;
}

/* ---------------- formula search ---------------- */
typedef struct { uint32_t R, rows, lim, nc; } fm_args;
static int formula_run(int score, const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t K, uint32_t R,
                       uint32_t rows, uint32_t lim, const kg2_cand *cand, uint32_t nc, const kg2_score *bound, kg2_score *out) {
    if (!kg2_metal_available() || !nc) return -1;
    int rc = -1;
    pthread_mutex_lock(&G_mu);
    @autoreleasepool {
        G.gpu_s = 0;
        fm_args A = {R, rows, lim, nc};
        size_t F = (size_t)K * R;
        bref Vb = ref_of(V, F * 8, 1), Db = ref_of(D, F, 1), Ob = ref_of(OK, F, 1), Cb = ref_of(cand, (size_t)nc * 8, 1),
             Out = ref_of(out, (size_t)nc * 8, 0);
        bref Bb = {nil, 0, 0};
        if (score) Bb = ref_of(bound, (size_t)K * 8, 1);
        id<MTLCommandBuffer> cb = [G.queue commandBuffer];
        id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
        [e setBuffer:Vb.b offset:Vb.off atIndex:0]; [e setBuffer:Db.b offset:Db.off atIndex:1]; [e setBuffer:Ob.b offset:Ob.off atIndex:2];
        [e setBuffer:Cb.b offset:Cb.off atIndex:3]; [e setBuffer:Out.b offset:Out.off atIndex:4]; [e setBytes:&A length:sizeof A atIndex:5];
        if (score) [e setBuffer:Bb.b offset:Bb.off atIndex:6];
        dispatch1(e, score ? G.fscore : G.fscreen, nc, 64);
        [e endEncoding];
        if (!run(cb)) { ref_done(&Out, out, (size_t)nc * 8); rc = 0; }
    }
    pthread_mutex_unlock(&G_mu);
    return rc;
}
int kg2_metal_formula_screen(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t K, uint32_t R,
                             uint32_t rows, const kg2_cand *cand, uint32_t nc, kg2_score *out) {
    return formula_run(0, V, D, OK, K, R, rows, 0, cand, nc, NULL, out);
}
int kg2_metal_formula_score(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t K, uint32_t R,
                            uint32_t rows, uint32_t lim, const kg2_cand *cand, uint32_t nc, const kg2_score *bound,
                            kg2_score *out) {
    return formula_run(1, V, D, OK, K, R, rows, lim, cand, nc, bound, out);
}
