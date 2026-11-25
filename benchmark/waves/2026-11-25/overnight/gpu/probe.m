#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>

int main(void) {
    @autoreleasepool {
        NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
        printf("MTLCopyAllDevices count=%lu\n", (unsigned long)devices.count);
        for (id<MTLDevice> device in devices)
            printf("enumerated=%s\n", device.name.UTF8String);
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            puts("MTLCreateSystemDefaultDevice=nil");
            return 2;
        }
        printf("default=%s unified_memory=%d max_threads=%lu\n",
               device.name.UTF8String, device.hasUnifiedMemory,
               (unsigned long)device.maxThreadsPerThreadgroup.width);
        NSError *error = nil;
        NSString *source = @"#include <metal_stdlib>\nusing namespace metal;\n"
                            "kernel void echo(device uint *p [[buffer(0)]], "
                            "uint i [[thread_position_in_grid]]) {p[i] = 73 + i;}";
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (!library) {
            fprintf(stderr, "runtime library: %s\n", error.localizedDescription.UTF8String);
            return 3;
        }
        id<MTLFunction> function = [library newFunctionWithName:@"echo"];
        id<MTLComputePipelineState> pipeline = [device newComputePipelineStateWithFunction:function
                                                                                    error:&error];
        if (!pipeline) {
            fprintf(stderr, "pipeline: %s\n", error.localizedDescription.UTF8String);
            return 4;
        }
        id<MTLBuffer> buffer = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
        id<MTLCommandQueue> queue = [device newCommandQueue];
        if (!buffer || !queue) return 5;
        id<MTLCommandBuffer> command = [queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:pipeline];
        [encoder setBuffer:buffer offset:0 atIndex:0];
        [encoder dispatchThreads:MTLSizeMake(4, 1, 1) threadsPerThreadgroup:MTLSizeMake(4, 1, 1)];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted) {
            fprintf(stderr, "submit: %s\n", command.error.localizedDescription.UTF8String);
            return 6;
        }
        const uint32_t *v = buffer.contents;
        printf("echo=%u,%u,%u,%u\n", v[0], v[1], v[2], v[3]);
        return !(v[0] == 73 && v[1] == 74 && v[2] == 75 && v[3] == 76);
    }
}
