/* Independent-codec experiment: exact verified frames, randomized order, medians.
 * GPU timing includes copies/submission/wait/CPU encoding. Context setup is separate. */
#define _GNU_SOURCE
#include "krc.h"
#include "czn.h"
#include "zstd.h"
#include "lz4.h"
#ifdef KRC_HAVE_METAL
#include "krc_gpu.h"
#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static int cmp_double(const void *a,const void *b){double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);}
static uint64_t rng=0x20261107;
static unsigned random32(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return (unsigned)rng;}
typedef struct {const char *name;int kind,threads;} config;

int main(int argc,char **argv) {
    int repeats=5,threads=(int)sysconf(_SC_NPROCESSORS_ONLN),opt,require_gpu=0,percent=50;const char *shader="src/experimental/krc_simd.metal";
    while((opt=getopt(argc,argv,"r:T:s:Gp:"))!=-1){if(opt=='r')repeats=atoi(optarg);else if(opt=='T')threads=atoi(optarg);else if(opt=='s')shader=optarg;else if(opt=='G')require_gpu=1;else if(opt=='p')percent=atoi(optarg);else return 1;}
    if(repeats<3||repeats>31||threads<1||threads>256||percent<1||percent>99||optind==argc){fprintf(stderr,"usage: krcbench [-r 3..31] [-T workers] [-s shader] [-p CPU percent] [-G require GPU] FILE...\n");return 1;}
    config configs[8]={{"krc-cpu",0,1},{"krc-cpu",0,threads},{"kolmo-fast",2,1},{"kolmo-fast",2,threads},{"zstd-3",3,1},{"lz4",4,1}};
    int count=6;double setup=0;char hybrid_name[32];snprintf(hybrid_name,sizeof hybrid_name,"krc-hybrid-%d",percent);
#ifdef KRC_HAVE_METAL
    char error[1024]={0};krc_gpu *gpu=krc_gpu_create(shader,error,sizeof error,&setup);
    if(gpu){fprintf(stderr,"GPU device=%s setup_seconds=%.9f\n",krc_gpu_name(gpu),setup);configs[count++]=(config){"krc-metal",1,1};if(threads>=2)configs[count++]=(config){hybrid_name,5,threads};}
    else {fprintf(stderr,"GPU unavailable: %s; CPU path remains available\n",error);if(require_gpu)return 2;}
#else
    (void)shader;if(require_gpu){fprintf(stderr,"This build has no GPU backend\n");return 2;}
#endif
    ZSTD_CCtx *zc=ZSTD_createCCtx();ZSTD_DCtx *zd=ZSTD_createDCtx();if(!zc||!zd)return 1;
    puts("file,codec,cpu_threads,input_bytes,compressed_bytes,ratio,compress_mbps,decode_mbps,first_encode_ms,gpu_device_ms,gpu_setup_ms,repeats,verified");
    for(int a=optind;a<argc;a++) {
        FILE *f=fopen(argv[a],"rb");if(!f){perror(argv[a]);return 1;}
        long length;if(fseek(f,0,SEEK_END)||(length=ftell(f))<0||fseek(f,0,SEEK_SET)||length>INT_MAX){fclose(f);return 1;}
        size_t n=(size_t)length;uint8_t *input=malloc(n?n:1),*back=malloc(n?n:1);if(!input||!back||fread(input,1,n,f)!=n){fclose(f);return 1;}fclose(f);
        czn_params cp={CZN_FAST,0,1,0,1};size_t cap=czn_bound(n,&cp);
        size_t kb=krc_bound(n),zb=ZSTD_compressBound(n),lb=(size_t)LZ4_compressBound((int)n);
        if(cap<kb)cap=kb;if(cap<zb)cap=zb;if(cap<lb)cap=lb;
        uint8_t *out=malloc(cap),*reference=malloc(kb);size_t tiles=krc_plan_count(n);
        krc_plan *plans=malloc((tiles?tiles:1)*sizeof(*plans)),*cpu_plans=malloc((tiles?tiles:1)*sizeof(*plans));
        if(!out||!reference||!plans||!cpu_plans)return 1;
        int64_t reference_size=krc_compress(input,n,reference,kb,1);if(reference_size<0)return 1;
        if(krc_plan_cpu(input,n,cpu_plans,tiles,1))return 1;
        double enc[8][31],dec[8][31],device[8][31],first[8]={0};int runs[8]={0};int64_t sizes[8]={0};
        int jobs[8*31];for(int j=0;j<count*repeats;j++)jobs[j]=j%count;
        for(int j=count*repeats-1;j>0;j--){int k=(int)(random32()%(unsigned)(j+1)),t=jobs[j];jobs[j]=jobs[k];jobs[k]=t;}
        for(int job=0;job<count*repeats;job++) {
            int c=jobs[job],r=runs[c]++;config cfg=configs[c];double gpu_seconds=0,start=now();int64_t cn=-1;
            if(cfg.kind==0){int result=krc_plan_cpu(input,n,plans,tiles,cfg.threads);cn=result?result:krc_encode(input,n,plans,tiles,out,cap);}
#ifdef KRC_HAVE_METAL
            else if(cfg.kind==1){int result=krc_gpu_plan(gpu,input,n,plans,tiles,&gpu_seconds);cn=result?result:krc_encode(input,n,plans,tiles,out,cap);}
            else if(cfg.kind==5){int result=krc_gpu_hybrid_plan(gpu,input,n,plans,tiles,cfg.threads,percent,&gpu_seconds);cn=result?result:krc_encode(input,n,plans,tiles,out,cap);}
#endif
            else if(cfg.kind==2){cp.threads=cfg.threads;cn=czn_compress(input,n,out,cap,&cp);}
            else if(cfg.kind==3){size_t z=ZSTD_compressCCtx(zc,out,cap,input,n,3);cn=ZSTD_isError(z)?-1:(int64_t)z;}
            else if(cfg.kind==4)cn=LZ4_compress_default((const char *)input,(char *)out,(int)n,(int)cap);
            enc[c][r]=now()-start;device[c][r]=gpu_seconds;if(!r)first[c]=enc[c][r];
            if(cn<=0){fprintf(stderr,"Encode failed: %s %s (%lld)\n",argv[a],cfg.name,(long long)cn);return 1;}
            if(r&&sizes[c]!=cn){fprintf(stderr,"Unstable compressed size\n");return 1;}sizes[c]=cn;
            if((cfg.kind<=1||cfg.kind==5) && (cn!=reference_size||memcmp(out,reference,cn))){fprintf(stderr,"CPU/GPU frame disagreement: %s\n",argv[a]);return 1;}
            if((cfg.kind==1||cfg.kind==5) && memcmp(plans,cpu_plans,tiles*sizeof(*plans))){fprintf(stderr,"CPU/GPU plan disagreement\n");return 1;}
            for(size_t i=0;i<n;i++)back[i]=(uint8_t)~input[i];
            int64_t restored=-1;start=now();
            if(cfg.kind<=1||cfg.kind==5)restored=krc_decompress(out,cn,back,n);
            else if(cfg.kind==2)restored=czn_decompress(out,cn,back,n,cfg.threads);
            else if(cfg.kind==3){size_t d=ZSTD_decompressDCtx(zd,back,n,out,cn);restored=ZSTD_isError(d)?-1:(int64_t)d;}
            else if(cfg.kind==4)restored=LZ4_decompress_safe((const char *)out,(char *)back,(int)cn,(int)n);
            dec[c][r]=now()-start;
            if(restored!=(int64_t)n||memcmp(back,input,n)){fprintf(stderr,"Round trip failed: %s %s\n",argv[a],cfg.name);return 1;}
        }
        for(int c=0;c<count;c++) {
            qsort(enc[c],repeats,sizeof(double),cmp_double);qsort(dec[c],repeats,sizeof(double),cmp_double);qsort(device[c],repeats,sizeof(double),cmp_double);
            printf("%s,%s,%d,%zu,%lld,%.6f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,1\n",argv[a],configs[c].name,configs[c].threads,n,(long long)sizes[c],(double)n/sizes[c],n/enc[c][repeats/2]/1e6,n/dec[c][repeats/2]/1e6,first[c]*1000,device[c][repeats/2]*1000,(configs[c].kind==1||configs[c].kind==5)?setup*1000:0,repeats);
        }
        if(fflush(stdout))return 1;free(input);free(back);free(out);free(reference);free(plans);free(cpu_plans);
    }
    ZSTD_freeCCtx(zc);ZSTD_freeDCtx(zd);
#ifdef KRC_HAVE_METAL
    krc_gpu_destroy(gpu);
#endif
    return 0;
}
