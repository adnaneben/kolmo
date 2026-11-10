#include "krc_gpu.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static unsigned seed=0x272315;
static unsigned next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}

int main(int argc,char **argv) {
    if (argc!=3) return 2;
    FILE *f=fopen(argv[2],"rb");
    if(!f || fseek(f,0,SEEK_END))return 1;
    long end=ftell(f);
    if(end<0 || fseek(f,0,SEEK_SET))return 1;
    size_t n=(size_t)end,tiles=krc_plan_count(n),bound=krc_bound(n);
    uint8_t *src=malloc(n),*out=malloc(bound),*control=malloc(bound),*back=malloc(n);
    krc_plan *plans=malloc(tiles*sizeof(*plans)),*base=malloc(tiles*sizeof(*base));
    if(!src||!out||!control||!back||!plans||!base||fread(src,1,n,f)!=n)return 1;
    fclose(f);
    if(krc_plan_cpu(src,n,base,tiles,1))return 1;
    int64_t bytes=krc_encode(src,n,base,tiles,control,bound);
    if(bytes<=0)return 1;
    char error[512]={0};double setup;
    krc_gpu *gpu=krc_gpu_create(argv[1],error,sizeof(error),&setup);
    if(!gpu){fprintf(stderr,"%s\n",error);return 1;}
    fprintf(stderr,"%s setup %.6fs\n",krc_gpu_name(gpu),setup);
    const char *methods[]={"legacy-cpu1","legacy-cpu10","legacy-metal","legacy-split50","parallel-cpu10",
                           "parallel-metal","parallel-split50","adaptive"};
    puts("codec,repeat,input_bytes,frame_bytes,encode_s,decode_s,device_s,device_tiles,all_tiles");
    for(unsigned rep=0;rep<8;++rep) {
        unsigned order[8];for(unsigned i=0;i<8;++i)order[i]=i;
        for(unsigned i=7;i;--i){unsigned j=next()%(i+1),v=order[i];order[i]=order[j];order[j]=v;}
        for(unsigned j=0;j<8;++j) {
            unsigned m=order[j];double dev=0;size_t done=0;int rc;
            double start=now();
            if(m==0 || m==1 || m==4) rc=krc_plan_cpu(src,n,plans,tiles,m?10:1);
            else if(m==2 || m==5){rc=krc_gpu_plan(gpu,src,n,plans,tiles,&dev);done=tiles;}
            else if(m==3 || m==6){rc=krc_gpu_hybrid_plan(gpu,src,n,plans,tiles,10,50,&dev);done=tiles-tiles/2;}
            else rc=krc_dispatch_plan(gpu,src,n,plans,tiles,10,0,&dev,&done);
            int64_t used=rc?rc:m<4?krc_encode(src,n,plans,tiles,out,bound):krc_encode_threads(src,n,plans,tiles,out,bound,10);
            double enc=now()-start;
            if(used!=bytes || memcmp(out,control,(size_t)bytes) || memcmp(plans,base,tiles*sizeof(*plans))) {
                fprintf(stderr,"Plan/frame mismatch %s %" PRId64 "\n",methods[m],used);return 1;
            }
            memset(back,42,n);
            start=now();
            int64_t restored=m<4?krc_decompress(out,(size_t)bytes,back,n):krc_decode_threads(out,(size_t)bytes,back,n,10);
            double dec=now()-start;
            if(restored!=(int64_t)n||memcmp(src,back,n))return 1;
            printf("%s,%u,%zu,%" PRId64 ",%.9f,%.9f,%.9f,%zu,%zu\n",methods[m],rep,n,bytes,enc,dec,dev,done,tiles);
            fflush(stdout);
        }
    }
    krc_gpu_destroy(gpu);free(plans);free(base);free(src);free(out);free(back);free(control);
}
