// GPU memory bandwidth calibration on this Mac: copy of 64-bit keys + 32-bit values, coalesced and scattered
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
static const char *src =
"#include <metal_stdlib>\nusing namespace metal;\n"
"kernel void cp(device const ulong *k [[buffer(0)]], device const uint *v [[buffer(1)]], device ulong *ko [[buffer(2)]], device uint *vo [[buffer(3)]], uint i [[thread_position_in_grid]]) { ko[i] = k[i]; vo[i] = v[i]; }\n"
"kernel void sc(device const ulong *k [[buffer(0)]], device const uint *v [[buffer(1)]], device ulong *ko [[buffer(2)]], device uint *vo [[buffer(3)]], device const uint *p [[buffer(4)]], uint i [[thread_position_in_grid]]) { uint d = p[i]; ko[d] = k[i]; vo[d] = v[i]; }\n"
"kernel void rd(device const ulong *k [[buffer(0)]], device atomic_uint *o [[buffer(1)]], uint i [[thread_position_in_grid]]) { ulong x = k[i]; if (x == 12345) atomic_fetch_add_explicit(o, 1u, memory_order_relaxed); }\n";
int main(void) {
  @autoreleasepool {
    id<MTLDevice> d = MTLCreateSystemDefaultDevice();
    printf("%s, recommended working set %.1f GB\n", d.name.UTF8String, d.recommendedMaxWorkingSetSize / 1e9);
    NSError *e = nil;
    id<MTLLibrary> L = [d newLibraryWithSource:[NSString stringWithUTF8String:src] options:nil error:&e];
    id<MTLComputePipelineState> cp = [d newComputePipelineStateWithFunction:[L newFunctionWithName:@"cp"] error:&e];
    id<MTLComputePipelineState> sc = [d newComputePipelineStateWithFunction:[L newFunctionWithName:@"sc"] error:&e];
    id<MTLComputePipelineState> rd = [d newComputePipelineStateWithFunction:[L newFunctionWithName:@"rd"] error:&e];
    uint32_t n = 10u << 20;
    id<MTLBuffer> k = [d newBufferWithLength:8ul*n options:MTLResourceStorageModePrivate], v = [d newBufferWithLength:4ul*n options:MTLResourceStorageModePrivate];
    id<MTLBuffer> ko = [d newBufferWithLength:8ul*n options:MTLResourceStorageModePrivate], vo = [d newBufferWithLength:4ul*n options:MTLResourceStorageModePrivate];
    id<MTLBuffer> p = [d newBufferWithLength:4ul*n options:MTLResourceStorageModeShared], o = [d newBufferWithLength:16 options:MTLResourceStorageModeShared];
    uint32_t *pp = p.contents; for (uint32_t i = 0; i < n; i++) pp[i] = i;
    uint64_t s = 1; for (uint32_t i = n - 1; i > 0; i--) { s = s * 6364136223846793005ull + 1; uint32_t j = (uint32_t)((s >> 33) % (i + 1)); uint32_t t = pp[i]; pp[i] = pp[j]; pp[j] = t; }
    // partially local permutation: blocks of 4096 permuted within 256 buckets (like a radix scatter)
    id<MTLCommandQueue> q = [d newCommandQueue];
    for (int kind = 0; kind < 3; kind++) {
      double best = 1e9;
      for (int it = 0; it < 5; it++) {
        id<MTLCommandBuffer> cb = [q commandBuffer]; id<MTLComputeCommandEncoder> en = [cb computeCommandEncoder];
        [en setComputePipelineState:kind == 0 ? cp : kind == 1 ? sc : rd];
        [en setBuffer:k offset:0 atIndex:0];
        if (kind == 2) [en setBuffer:o offset:0 atIndex:1]; else { [en setBuffer:v offset:0 atIndex:1]; [en setBuffer:ko offset:0 atIndex:2]; [en setBuffer:vo offset:0 atIndex:3]; [en setBuffer:p offset:0 atIndex:4]; }
        [en dispatchThreads:MTLSizeMake(n,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
        [en endEncoding]; [cb commit]; [cb waitUntilCompleted];
        double t = cb.GPUEndTime - cb.GPUStartTime; if (t < best) best = t;
      }
      double bytes = kind == 2 ? 8.0 * n : 24.0 * n;
      printf("%s: %.2f ms, %.1f GB/s\n", kind == 0 ? "copy 12B pairs" : kind == 1 ? "random scatter 12B pairs" : "read 8B keys", best * 1e3, bytes / best / 1e9);
    }
  }
  return 0;
}
