/* kvid's strip prediction on the GPU, host side. Apache-2.0, Kolmo contributors. See kvid.metal. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "metal.h"
#include <pthread.h>
#include <string.h>

static const unsigned char kvid_source[] = {
#include "kvid_metal.inc"
};

typedef struct {
    uint32_t w[3], h[3], strips[3], pstart[3], strip_base[3], strips_per_frame, np, frames;
    uint64_t plane_res[3];
} kv_geom;

static struct { id<MTLDevice> dev; id<MTLCommandQueue> queue; id<MTLComputePipelineState> strips; int ok; double gpu_s; } K;
static pthread_once_t K_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t K_mu = PTHREAD_MUTEX_INITIALIZER;

static void setup(void) {
    @autoreleasepool {
        if (getenv("KOLMO_NO_METAL")) return;
        K.dev = MTLCreateSystemDefaultDevice();
        if (!K.dev) return;
        NSString *src = [[NSString alloc] initWithBytes:kvid_source length:sizeof(kvid_source) encoding:NSUTF8StringEncoding];
        NSError *e = nil;
        id<MTLLibrary> lib = src ? [K.dev newLibraryWithSource:src options:nil error:&e] : nil;
        id<MTLFunction> f = [lib newFunctionWithName:@"kvid_strips"];
        K.strips = f ? [K.dev newComputePipelineStateWithFunction:f error:&e] : nil;
        K.queue = [K.dev newCommandQueue];
        K.ok = K.strips && K.queue && K.strips.maxTotalThreadsPerThreadgroup >= 256 && K.strips.threadExecutionWidth == 32;
    }
}

double kolmo_metal_kvid_gpu_seconds(void) { return K.gpu_s; }

int kolmo_metal_kvid(const uint8_t *file, size_t n, const uint64_t *frame_at, uint32_t frames, int np,
                     const size_t *w, const size_t *h, uint8_t *res, uint8_t *flags) {
    pthread_once(&K_once, setup);
    if (!K.ok || np < 1 || np > 3 || !frames) return -1;
    kv_geom g; memset(&g, 0, sizeof g);
    size_t fb = 0; uint32_t sp = 0;
    for (int p = 0; p < np; p++) {
        if (w[p] > 16384 || h[p] > 16384) return -1;
        g.w[p] = (uint32_t)w[p]; g.h[p] = (uint32_t)h[p]; g.strips[p] = (uint32_t)((h[p] + 15) / 16);
        g.pstart[p] = (uint32_t)fb; g.strip_base[p] = sp; sp += g.strips[p]; fb += w[p] * h[p];
    }
    uint64_t acc = 0;
    for (int p = 0; p < np; p++) { g.plane_res[p] = acc; acc += (uint64_t)frames * w[p] * h[p]; }
    g.strips_per_frame = sp; g.np = (uint32_t)np; g.frames = frames;
    if ((uint64_t)frames * sp > UINT32_MAX) return -1;
    int rc = -1;
    pthread_mutex_lock(&K_mu);
    @autoreleasepool {
        id<MTLBuffer> F = [K.dev newBufferWithBytes:file length:n options:MTLResourceStorageModeShared];
        id<MTLBuffer> A = [K.dev newBufferWithBytes:frame_at length:frames * sizeof(uint64_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> R = [K.dev newBufferWithLength:(size_t)acc options:MTLResourceStorageModeShared];
        id<MTLBuffer> L = [K.dev newBufferWithLength:(size_t)frames * sp options:MTLResourceStorageModeShared];
        if (F && A && R && L) {
            id<MTLCommandBuffer> cb = [K.queue commandBuffer];
            id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
            [e setComputePipelineState:K.strips];
            [e setBuffer:F offset:0 atIndex:0]; [e setBuffer:A offset:0 atIndex:1]; [e setBuffer:R offset:0 atIndex:2];
            [e setBuffer:L offset:0 atIndex:3]; [e setBytes:&g length:sizeof g atIndex:4];
            [e dispatchThreadgroups:MTLSizeMake((NSUInteger)frames * sp, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
            [e endEncoding];
            [cb commit]; [cb waitUntilCompleted];
            if (cb.status == MTLCommandBufferStatusCompleted) {
                K.gpu_s = cb.GPUEndTime - cb.GPUStartTime;
                memcpy(res, R.contents, (size_t)acc); memcpy(flags, L.contents, (size_t)frames * sp);
                rc = 0;
            }
        }
    }
    pthread_mutex_unlock(&K_mu);
    return rc;
}
