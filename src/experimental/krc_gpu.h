#ifndef KRC_GPU_H
#define KRC_GPU_H
#include "krc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct krc_gpu krc_gpu;
/* Contexts retain buffers; use one context per concurrent caller. */
krc_gpu *krc_gpu_create(const char *shader_path,char *error,size_t cap,double *setup_seconds);
const char *krc_gpu_name(krc_gpu *gpu);
size_t krc_gpu_buffer_bytes(krc_gpu *gpu);
int krc_gpu_plan(krc_gpu *gpu,const uint8_t *src,size_t n,krc_plan *plans,size_t count,double *device_seconds);
int krc_gpu_hybrid_plan(krc_gpu *gpu,const uint8_t *src,size_t n,krc_plan *plans,size_t count,int threads,int cpu_percent,double *device_seconds);
/* cpu_percent 0 uses GPU-only scoring; 1..99 combines CPU and GPU. */
int64_t krc_gpu_compress(krc_gpu *gpu,const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads,int cpu_percent,double *device_seconds);
int krc_dispatch_plan(krc_gpu *gpu,const uint8_t *src,size_t n,krc_plan *plans,size_t count,
                      int requested,int require_device,double *seconds,size_t *device_tiles);
void krc_gpu_destroy(krc_gpu *gpu);
#ifdef __cplusplus
}
#endif
#endif
