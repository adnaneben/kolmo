#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "krc_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
struct krc_gpu {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> pipeline, reduce;
    id<MTLBuffer> input, output, trials;
    char name[256];
};
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
krc_gpu *krc_gpu_create(const char *path,char *error,size_t cap,double *setup) {
    double start=now();
    @autoreleasepool {
        krc_gpu *g=calloc(1,sizeof(*g));if(!g)return NULL;
        g->device=MTLCreateSystemDefaultDevice();
        if(!g->device){if(cap)snprintf(error,cap,"No accessible Metal device");free(g);return NULL;}
        NSError *e=nil;
        NSString *source=[NSString stringWithContentsOfFile:[NSString stringWithUTF8String:path] encoding:NSUTF8StringEncoding error:&e];
        id<MTLLibrary> library=source?[g->device newLibraryWithSource:source options:nil error:&e]:nil;
        id<MTLFunction> fn=[library newFunctionWithName:@"krc_trials"];
        BOOL simd=fn!=nil;
        if(!fn)fn=[library newFunctionWithName:@"krc_choose"];
        if(simd){id<MTLFunction> reducer=[library newFunctionWithName:@"krc_reduce"];g->reduce=reducer?[g->device newComputePipelineStateWithFunction:reducer error:&e]:nil;}
        g->pipeline=fn?[g->device newComputePipelineStateWithFunction:fn error:&e]:nil;
        g->queue=[g->device newCommandQueue];
        if(!g->pipeline||!g->queue||g->pipeline.maxTotalThreadsPerThreadgroup<32||(simd&&(!g->reduce||g->pipeline.threadExecutionWidth!=32))){
            if(cap)snprintf(error,cap,"%s",e?e.description.UTF8String:"Unsupported Metal pipeline");krc_gpu_destroy(g);return NULL;
        }
        snprintf(g->name,sizeof(g->name),"%s",g->device.name.UTF8String);
        if(setup)*setup=now()-start;return g;
    }
}
const char *krc_gpu_name(krc_gpu *g){return g?g->name:"unavailable";}
size_t krc_gpu_buffer_bytes(krc_gpu *g){return g?g->input.length+g->output.length+g->trials.length:0;}
int krc_gpu_plan(krc_gpu *g,const uint8_t *src,size_t n,krc_plan *plans,size_t count,double *seconds) {
    @autoreleasepool {
        size_t tiles=krc_plan_count(n);if(!g||!krc_bound(n)||count<tiles||(!src&&n)||(!plans&&tiles))return KRC_ERR_PLAN;
        if(seconds)*seconds=0;if(!tiles)return 0;
        if(tiles>SIZE_MAX/sizeof(krc_plan)||n>g->device.maxBufferLength||tiles*sizeof(krc_plan)>g->device.maxBufferLength)return KRC_ERR_ALLOC;
        if(!g->input||g->input.length<n)g->input=[g->device newBufferWithLength:n options:MTLResourceStorageModeShared];
        size_t output_bytes=tiles*sizeof(krc_plan);
        if(!g->output||g->output.length<output_bytes)g->output=[g->device newBufferWithLength:output_bytes options:MTLResourceStorageModeShared];
        if(!g->input||!g->output)return KRC_ERR_ALLOC;
        size_t trial_bytes=0;
        if(g->reduce){
            if(tiles>SIZE_MAX/(20*16)||tiles>UINT32_MAX/20)return KRC_ERR_ALLOC;
            trial_bytes=tiles*20*16;if(trial_bytes>g->device.maxBufferLength)return KRC_ERR_ALLOC;
            if(!g->trials||g->trials.length<trial_bytes)g->trials=[g->device newBufferWithLength:trial_bytes options:MTLResourceStorageModePrivate];
            if(!g->trials)return KRC_ERR_ALLOC;
        }
        memcpy(g->input.contents,src,n);
        id<MTLCommandBuffer> command=[g->queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
        if(!command||!encoder)return KRC_ERR_ALLOC;
        uint64_t words=n/4;
        [encoder setComputePipelineState:g->pipeline];
        [encoder setBuffer:g->input offset:0 atIndex:0];[encoder setBuffer:g->reduce?g->trials:g->output offset:0 atIndex:1];
        [encoder setBytes:&words length:sizeof(words) atIndex:2];
        [encoder dispatchThreadgroups:MTLSizeMake(g->reduce?tiles*20:tiles,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
        [encoder endEncoding];
        if(g->reduce){
            encoder=[command computeCommandEncoder];if(!encoder)return KRC_ERR_ALLOC;
            [encoder setComputePipelineState:g->reduce];
            [encoder setBuffer:g->trials offset:0 atIndex:0];[encoder setBuffer:g->output offset:0 atIndex:1];
            uint64_t tile_count=tiles;[encoder setBytes:&tile_count length:sizeof(tile_count) atIndex:2];
            NSUInteger group=g->reduce.maxTotalThreadsPerThreadgroup<256?g->reduce.maxTotalThreadsPerThreadgroup:256;
            [encoder dispatchThreads:MTLSizeMake(tiles,1,1) threadsPerThreadgroup:MTLSizeMake(group,1,1)];[encoder endEncoding];
        }
        [command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)return KRC_ERR_PLAN;
        if(seconds)*seconds=command.GPUEndTime-command.GPUStartTime;
        memcpy(plans,g->output.contents,output_bytes);return 0;
    }
}
void krc_gpu_destroy(krc_gpu *g) {
    if(!g)return;
    g->input=nil;g->output=nil;g->trials=nil;g->pipeline=nil;g->reduce=nil;g->queue=nil;g->device=nil;free(g);
}
