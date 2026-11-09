#undef NDEBUG
#include "krc_gpu.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t state=0x9e3779b97f4a7c15ULL;
static uint32_t next(void){state^=state<<13;state^=state>>7;state^=state<<17;return (uint32_t)state;}
static void put32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
int main(int argc,char **argv){
    const char *shader=argc>1?argv[1]:"src/experimental/krc_simd.metal";char error[1024]={0};double setup=0;
    krc_gpu *gpu=krc_gpu_create(shader,error,sizeof error,&setup);
    if(!gpu){fprintf(stderr,"GPU unavailable: %s\n",error);return 77;}
    const size_t lengths[]={0,1,3,4,5,1023,1024,1025,4099,65539};
    uint8_t input[65539];unsigned cases=0;
    for(unsigned pattern=0;pattern<7;pattern++){
        uint32_t walk=123456;
        for(size_t i=0;i<65536;i+=4){uint32_t x=(uint32_t)(i/4),v;
            if(pattern==0)v=0;else if(pattern==1)v=next();else if(pattern==2)v=next()%16;
            else if(pattern==3){walk+=(next()&1)?1u:~0u;v=walk;}
            else if(pattern==4)v=x*x;else if(pattern==5)v=0x3f800000u+next()%1024;
            else v=x%257?x*17:next();put32(input+i,v);
        }
        input[65536]=1;input[65537]=2;input[65538]=3;
        for(size_t e=0;e<sizeof lengths/sizeof *lengths;e++){
            size_t n=lengths[e],tiles=krc_plan_count(n),cap=krc_bound(n);
            krc_plan *cpu=calloc(tiles?tiles:1,sizeof(*cpu)),*device=calloc(tiles?tiles:1,sizeof(*device));
            uint8_t *reference=malloc(cap),*output=malloc(cap),*back=malloc(n+1);assert(cpu&&device&&reference&&output&&back);
            assert(!krc_plan_cpu(input,n,cpu,tiles,1));int64_t cn=krc_encode(input,n,cpu,tiles,reference,cap);assert(cn>0);
            for(int percent=0;percent<=75;percent+=25){double seconds=0;
                int result=percent?krc_gpu_hybrid_plan(gpu,input,n,device,tiles,10,percent,&seconds):krc_gpu_plan(gpu,input,n,device,tiles,&seconds);
                assert(!result&&!memcmp(cpu,device,tiles*sizeof(*cpu)));
                assert(krc_encode(input,n,device,tiles,output,cap)==cn&&!memcmp(output,reference,cn));
                assert(krc_decompress(output,cn,back,n)==(int64_t)n&&!memcmp(back,input,n));cases++;
            }
            assert(krc_gpu_compress(gpu,input,n,output,cap,10,50,NULL)==cn&&!memcmp(output,reference,cn));
            free(cpu);free(device);free(reference);free(output);free(back);
        }
    }
    printf("GPU TESTS PASSED (%u exact CPU/GPU/hybrid plan, frame and round-trip cases; %s; setup %.3f ms)\n",cases,krc_gpu_name(gpu),setup*1000);
    krc_gpu_destroy(gpu);return 0;
}
