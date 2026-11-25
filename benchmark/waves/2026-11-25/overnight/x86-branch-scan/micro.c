#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#define e8_mask16 baseline_mask
#define e8e9_fwd baseline_fwd
#define e8e9_inv baseline_inv
#include BASELINE
#undef e8_mask16
#undef e8e9_fwd
#undef e8e9_inv
#include CANDIDATE
static uint64_t ticks(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec; }
static uint32_t state=2026112521u;
static uint32_t next(void) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void edge_cases(void) {
    uint8_t src[288],a[288],b[288];
    for (int mode=0;mode<3;mode++) for (size_t n=0;n<257;n++) for (int off=0;off<16;off++) {
        for (int i=0;i<288;i++) {
            uint32_t v=next(); src[i]=mode==0?(uint8_t)v:mode==1?(const uint8_t[]){0xe8,0xe9,0,255,0xe8,0,0xe9,255}[v%8]:(uint8_t)(v%7?0:0xe8);
        }
        memcpy(a,src,sizeof a);memcpy(b,src,sizeof b);
        baseline_fwd(a+off,n);e8e9_fwd(b+off,n);assert(!memcmp(a,b,sizeof a));
        e8e9_inv(a+off,n);baseline_inv(b+off,n);assert(!memcmp(a,src,sizeof a)&&!memcmp(b,src,sizeof b));
    }
}
int main(int argc,char **argv) {
    assert(argc==3);edge_cases();
    FILE *f=fopen(argv[2],"rb");assert(f&&!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0&&!fseek(f,0,SEEK_SET));
    uint8_t *src=malloc(n),*work=malloc(n),*reference=malloc(n);assert(src&&work&&reference&&fread(src,1,n,f)==(size_t)n);fclose(f);
    memcpy(reference,src,n);baseline_fwd(reference,n);uint64_t changed=0;
    for(long i=0;i<n;i++) changed+=src[i]!=reference[i];
    int loops=(int)((16u<<20)/(size_t)n)+1;
    for (int rep=0;rep<10;rep++) {
        int arms[]={0,1,2};for(int j=2;j>0;j--){int z=next()%(j+1),v=arms[j];arms[j]=arms[z];arms[z]=v;}
        for(int j=0;j<3;j++) {
            int arm=arms[j];uint64_t en=0,de=0;
            void (*forward)(uint8_t*,size_t)=arm==2?e8e9_fwd:baseline_fwd;
            void (*inverse)(uint8_t*,size_t)=arm==2?e8e9_inv:baseline_inv;
            for(int k=0;k<loops;k++){
                memcpy(work,src,n);uint64_t t=ticks();forward(work,n);en+=ticks()-t;
                assert(!memcmp(work,reference,n));t=ticks();inverse(work,n);de+=ticks()-t;assert(!memcmp(work,src,n));
            }
            printf("%s,%d,%d,%d,%ld,%llu,%d,%llu,%llu\n",argv[1],rep,j,arm,n,(unsigned long long)changed,loops,(unsigned long long)en,(unsigned long long)de);
        }
    }
    free(reference);free(src);free(work);return 0;
}
