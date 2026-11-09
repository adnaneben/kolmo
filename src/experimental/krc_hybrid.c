#include "krc_gpu.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
typedef struct {krc_gpu *gpu;const uint8_t *src;size_t n,count;krc_plan *plans;double seconds;int status;} gpu_job;
static void *gpu_worker(void *arg){gpu_job *j=arg;j->status=krc_gpu_plan(j->gpu,j->src,j->n,j->plans,j->count,&j->seconds);return NULL;}
int krc_gpu_hybrid_plan(krc_gpu *gpu,const uint8_t *src,size_t n,krc_plan *plans,size_t count,int threads,int percent,double *seconds){
    size_t tiles=krc_plan_count(n);
    if(!gpu||!krc_bound(n)||count<tiles||(!src&&n)||(!plans&&tiles)||percent<1||percent>99)return KRC_ERR_PLAN;
    if(threads<=0){long cores=sysconf(_SC_NPROCESSORS_ONLN);threads=cores>0?(cores<256?(int)cores:256):1;}
    if(threads<2)return KRC_ERR_PLAN;if(threads>256)threads=256;
    if(!tiles){if(seconds)*seconds=0;return 0;}
    size_t cpu_tiles=tiles*(unsigned)percent/100,cpu_bytes=cpu_tiles*1024;
    gpu_job j={gpu,src+cpu_bytes,n-cpu_bytes,tiles-cpu_tiles,plans+cpu_tiles,0,0};pthread_t thread;
    if(pthread_create(&thread,NULL,gpu_worker,&j))return KRC_ERR_ALLOC;
    int result=krc_plan_cpu(src,cpu_bytes,plans,cpu_tiles,threads-1);
    pthread_join(thread,NULL);if(seconds)*seconds=j.seconds;return result?result:j.status;
}
int64_t krc_gpu_compress(krc_gpu *gpu,const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads,int cpu_percent,double *seconds){
    size_t bound=krc_bound(n),tiles=krc_plan_count(n);
    if(!bound||cap<bound)return KRC_ERR_CAPACITY;if(tiles>SIZE_MAX/sizeof(krc_plan))return KRC_ERR_ALLOC;
    krc_plan *plans=tiles?malloc(tiles*sizeof(*plans)):NULL;if(tiles&&!plans)return KRC_ERR_ALLOC;
    int result=cpu_percent?krc_gpu_hybrid_plan(gpu,src,n,plans,tiles,threads,cpu_percent,seconds):krc_gpu_plan(gpu,src,n,plans,tiles,seconds);
    int64_t encoded=result?result:krc_encode(src,n,plans,tiles,dst,cap);free(plans);return encoded;
}
