/* Planner-sized work on the GPU against one CPU core: byte histogram + order-0 entropy of a buffer, and the bare
 * cost of a command-buffer round trip. Build: clang -O2 -fobjc-arc hist_bench.m -framework Metal -framework Foundation
 * usage: hist_bench FILE   (uses slices of 64 KiB, 1 MiB, 16 MiB of FILE) */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *src =
"#include <metal_stdlib>\nusing namespace metal;\n"
"kernel void hist(device const uchar *p [[buffer(0)]], device atomic_uint *h [[buffer(1)]], constant uint &n [[buffer(2)]],\n"
"                 uint tid [[thread_position_in_threadgroup]], uint tg [[threadgroup_position_in_grid]], uint tgs [[threadgroups_per_grid]]) {\n"
"  threadgroup atomic_uint l[256]; atomic_store_explicit(&l[tid], 0u, memory_order_relaxed);\n"
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"  for (uint i = tg * 256 + tid; i < n; i += tgs * 256) atomic_fetch_add_explicit(&l[p[i]], 1u, memory_order_relaxed);\n"
"  threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"  uint c = atomic_load_explicit(&l[tid], memory_order_relaxed); if (c) atomic_fetch_add_explicit(&h[tid], c, memory_order_relaxed);\n"
"}\n"
"kernel void nop(device uint *h [[buffer(0)]], uint i [[thread_position_in_grid]]) { if (i == 0) h[0] = 1; }\n";

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

static double entropy(const uint32_t *h, size_t n) {
    double e = 0; for (int i = 0; i < 256; i++) if (h[i]) { double p = (double)h[i] / n; e -= p * log2(p); } return e;
}

int main(int argc, char **argv) {
    @autoreleasepool {
        FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); size_t N = (size_t)ftell(f); rewind(f);
        uint8_t *buf = malloc(N); if (fread(buf, 1, N, f) != N) return 1; fclose(f);
        double t0 = now();
        id<MTLDevice> d = MTLCreateSystemDefaultDevice();
        NSError *e = nil;
        id<MTLLibrary> L = [d newLibraryWithSource:[NSString stringWithUTF8String:src] options:nil error:&e];
        id<MTLComputePipelineState> hp = [d newComputePipelineStateWithFunction:[L newFunctionWithName:@"hist"] error:&e];
        id<MTLComputePipelineState> np = [d newComputePipelineStateWithFunction:[L newFunctionWithName:@"nop"] error:&e];
        id<MTLCommandQueue> q = [d newCommandQueue];
        printf("Metal set-up (device, compile from source, pipelines): %.1f ms\n", (now() - t0) * 1e3);
        id<MTLBuffer> H = [d newBufferWithLength:1024 options:MTLResourceStorageModeShared];
        double rt = 1e9;
        for (int k = 0; k < 50; k++) {
            double s = now();
            id<MTLCommandBuffer> cb = [q commandBuffer]; id<MTLComputeCommandEncoder> en = [cb computeCommandEncoder];
            [en setComputePipelineState:np]; [en setBuffer:H offset:0 atIndex:0];
            [en dispatchThreads:MTLSizeMake(32, 1, 1) threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
            [en endEncoding]; [cb commit]; [cb waitUntilCompleted];
            double t = now() - s; if (t < rt) rt = t;
        }
        printf("empty command buffer round trip: %.3f ms (best of 50)\n", rt * 1e3);
        size_t sizes[3] = {64u << 10, 1u << 20, 16u << 20};
        for (int si = 0; si < 3; si++) {
            size_t n = sizes[si] < N ? sizes[si] : N;
            double cpu = 1e9, gpu = 1e9, gpuc = 1e9; uint32_t hc[256]; double ec = 0, eg = 0;
            for (int k = 0; k < 20; k++) {
                double s = now();
                uint32_t h4[4][256]; memset(h4, 0, sizeof h4);
                size_t i = 0;
                for (; i + 4 <= n; i += 4) { h4[0][buf[i]]++; h4[1][buf[i + 1]]++; h4[2][buf[i + 2]]++; h4[3][buf[i + 3]]++; }
                for (; i < n; i++) h4[0][buf[i]]++;
                for (int b = 0; b < 256; b++) hc[b] = h4[0][b] + h4[1][b] + h4[2][b] + h4[3][b];
                ec = entropy(hc, n);
                double t = now() - s; if (t < cpu) cpu = t;
            }
            for (int k = 0; k < 20; k++) {
                double s = now();
                id<MTLBuffer> B = [d newBufferWithBytes:buf length:n options:MTLResourceStorageModeShared];
                memset(H.contents, 0, 1024);
                id<MTLCommandBuffer> cb = [q commandBuffer]; id<MTLComputeCommandEncoder> en = [cb computeCommandEncoder];
                [en setComputePipelineState:hp]; [en setBuffer:B offset:0 atIndex:0]; [en setBuffer:H offset:0 atIndex:1];
                uint32_t nn = (uint32_t)n; [en setBytes:&nn length:4 atIndex:2];
                NSUInteger groups = (n + 256 * 64 - 1) / (256 * 64); if (groups < 1) groups = 1;
                [en dispatchThreadgroups:MTLSizeMake(groups, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
                [en endEncoding]; [cb commit]; [cb waitUntilCompleted];
                eg = entropy(H.contents, n);
                double t = now() - s; if (t < gpu) { gpu = t; gpuc = cb.GPUEndTime - cb.GPUStartTime; }
            }
            printf("histogram + entropy of %8zu bytes: CPU one core %.3f ms | GPU %.3f ms end to end (kernel %.3f ms) | same %d\n",
                   n, cpu * 1e3, gpu * 1e3, gpuc * 1e3, fabs(ec - eg) < 1e-12);
        }
    }
    return 0;
}
