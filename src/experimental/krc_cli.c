#define _GNU_SOURCE
#include "krc.h"
#ifdef KRC_HAVE_METAL
#include "krc_gpu.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){
    int decode=0,threads=0,required=0,opt;size_t maximum=(size_t)1<<30;
    const char *backend="auto",*shader=NULL;
    while((opt=getopt(argc,argv,"db:T:s:GM:"))!=-1){
        if(opt=='d')decode=1;else if(opt=='b')backend=optarg;else if(opt=='T')threads=atoi(optarg);
        else if(opt=='s')shader=optarg;else if(opt=='G')required=1;
        else if(opt=='M'){char *end;unsigned long long mib=strtoull(optarg,&end,10);if(*end||!mib||mib>SIZE_MAX/(1<<20))return 1;maximum=(size_t)mib<<20;}else return 1;
    }
    if(argc-optind!=2||(strcmp(backend,"cpu")&&strcmp(backend,"metal")&&strcmp(backend,"hybrid")&&strcmp(backend,"auto"))){fprintf(stderr,"usage: krc [-d] [-b auto|cpu|metal|hybrid] [-T threads] [-G require GPU] [-M output MiB] input output\n");return 1;}
    FILE *f=fopen(argv[optind],"rb");if(!f){perror(argv[optind]);return 1;}long length;
    if(fseek(f,0,SEEK_END)||(length=ftell(f))<0||fseek(f,0,SEEK_SET)){fclose(f);return 1;}
    size_t n=(size_t)length;uint8_t *input=malloc(n?n:1);if(!input||fread(input,1,n,f)!=n){fclose(f);free(input);return 1;}fclose(f);
    int64_t original=decode?krc_original_size(input,n):(int64_t)n;
    if(original<0||(uint64_t)original>maximum){fprintf(stderr,"Invalid frame or output exceeds -M limit\n");free(input);return 1;}
    size_t cap=decode?(size_t)original:krc_bound(n);uint8_t *output=malloc(cap?cap:1);if(!output){free(input);return 1;}
    size_t gpu_buffers=0;double begin=now(),setup=0,device=0;int64_t result=-1;const char *used="cpu";
    if(decode)result=krc_decode_threads(input,n,output,cap,threads);
    else {
#ifdef KRC_HAVE_METAL
        krc_gpu *gpu=NULL;
        int automatic = !strcmp(backend,"auto");
        if (strcmp(backend,"cpu") && (!automatic || required || (n >= (8u<<20) && threads != 1))) {
            char error[1024]={0};
            gpu=krc_gpu_create(shader,error,sizeof error,&setup);
            if (!gpu) fprintf(stderr,"GPU unavailable: %s\n",error);
            else if (automatic) {
                size_t count=krc_plan_count(n), device_tiles=0;
                krc_plan *plans=count && count<=SIZE_MAX/sizeof(*plans) ? malloc(count*sizeof(*plans)) : NULL;
                if (plans || !count) {
                    int rc=krc_dispatch_plan(gpu,input,n,plans,count,threads,required,&device,&device_tiles);
                    result=rc ? rc : krc_encode_threads(input,n,plans,count,output,cap,threads);
                    if (result>=0 && device_tiles) used=device_tiles==count ? "metal" : "hybrid-auto";
                }
                free(plans);
            } else {
                result=krc_gpu_compress(gpu,input,n,output,cap,threads,!strcmp(backend,"hybrid")?50:0,&device);
                if(result>=0) used=backend;
            }
        }
        gpu_buffers=krc_gpu_buffer_bytes(gpu);krc_gpu_destroy(gpu);
#endif
        if(result<0){if(required&&strcmp(backend,"cpu")){fprintf(stderr,"Required GPU path failed\n");free(input);free(output);return 2;}result=krc_compress(input,n,output,cap,threads);}
    }
    double elapsed=now()-begin;
    if(result<0){fprintf(stderr,"Codec error %lld\n",(long long)result);free(input);free(output);return 1;}
    if(!decode){uint8_t *back=malloc(n?n:1);if(!back||krc_decompress(output,result,back,n)!=(int64_t)n||memcmp(back,input,n)){fprintf(stderr,"Round-trip verification failed\n");free(back);free(input);free(output);return 1;}free(back);}
    f=fopen(argv[optind+1],"wb");if(!f){perror(argv[optind+1]);free(input);free(output);return 1;}
    int bad=fwrite(output,1,(size_t)result,f)!=(size_t)result;if(fclose(f))bad=1;
    struct rusage usage;uint64_t rss=0;
    if(!getrusage(RUSAGE_SELF,&usage)&&usage.ru_maxrss>0){
#ifdef __APPLE__
        rss=(uint64_t)usage.ru_maxrss;
#else
        rss=(uint64_t)usage.ru_maxrss*1024;
#endif
    }
    fprintf(stderr,"process_peak_rss_bytes=%llu requested_metal_buffer_bytes=%zu\n",(unsigned long long)rss,gpu_buffers);
    fprintf(stderr,"backend=%s bytes=%zu output=%lld complete_seconds=%.9f setup_seconds=%.9f device_seconds=%.9f verified=1\n",used,n,(long long)result,elapsed,setup,device);
    free(input);free(output);return bad;
}
