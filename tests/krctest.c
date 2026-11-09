#undef NDEBUG
#include "krc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t state=0x123456789abcdefULL;
static uint32_t random32(void){state^=state<<13;state^=state>>7;state^=state<<17;return (uint32_t)state;}
static void put32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t checksum(const uint8_t *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int b=0;b<8;b++)c=(c>>1)^(0x82f63b78u&(0u-(c&1)));}return ~c;}
static void repair(uint8_t *frame,size_t n){put32(frame+24,checksum(frame+32,n-32));put32(frame+28,checksum(frame,28));}
static unsigned modes;
static void check(const uint8_t *src,size_t n) {
    size_t count=krc_plan_count(n),cap=krc_bound(n);assert(cap);
    krc_plan *p=calloc(count?count:1,sizeof(*p)),*q=calloc(count?count:1,sizeof(*q));
    uint8_t *frame=malloc(cap),*parallel=malloc(cap),*back=malloc(n+32),*bad=malloc(cap);assert(p&&q&&frame&&parallel&&back&&bad);
    assert(!krc_plan_cpu(src,n,p,count,1));assert(!krc_plan_cpu(src,n,q,count,10));assert(!memcmp(p,q,count*sizeof(*p)));
    for(size_t i=0;i<count;i++)modes|=1u<<p[i].mode;
    int64_t cn=krc_encode(src,n,p,count,frame,cap);assert(cn>=32&&(size_t)cn<=cap);assert(krc_original_size(frame,cn)==(int64_t)n);
    assert(krc_compress(src,n,parallel,cap,4)==cn&&!memcmp(frame,parallel,cn));
    memset(back,0xA5,n+32);assert(krc_decompress(frame,cn,back,n)==(int64_t)n&&!memcmp(src,back,n));
    for(size_t i=n;i<n+32;i++)assert(back[i]==0xA5);
    if(n)assert(krc_decompress(frame,cn,back,n-1)==KRC_ERR_CAPACITY);
    assert(krc_compress(src,n,parallel,cap-1,1)==KRC_ERR_CAPACITY);
    for(int i=0;i<80;i++) {
        memcpy(bad,frame,cn);size_t at=random32()%(size_t)cn;bad[at]^=1u<<(random32()%8);
        assert(krc_decompress(bad,cn,back,n)<0);
        size_t truncated=random32()%(size_t)cn;assert(krc_decompress(frame,truncated,back,n)<0);
    }
    if(count){
        memcpy(bad,frame,cn);bad[32]=9;repair(bad,cn);assert(krc_decompress(bad,cn,back,n)<0);
        memcpy(bad,frame,cn);bad[35]=1;repair(bad,cn);assert(krc_decompress(bad,cn,back,n)<0);
        memcpy(q,p,count*sizeof(*p));q[0].mode=9;assert(krc_encode(src,n,q,count,parallel,cap)==KRC_ERR_PLAN);
    }
    free(p);free(q);free(frame);free(parallel);free(back);free(bad);
}
int main(void) {
    uint8_t *data=malloc(65539);assert(data);
    const size_t edges[]={0,1,2,3,4,7,8,31,32,63,255,256,257,1023,1024,1025,2047,2048,2049,65539};
    for(unsigned pattern=0;pattern<7;pattern++) {
        uint32_t walk=100000;
        for(size_t i=0;i<65536;i+=4){uint32_t index=(uint32_t)(i/4),v;
            if(pattern==0)v=0;
            else if(pattern==1)v=random32();
            else if(pattern==2)v=random32()%16;
            else if(pattern==3){walk+=(random32()&1)?1u:~0u;v=walk;}
            else if(pattern==4)v=index*index;
            else if(pattern==5)v=0x3f800000u+(random32()%1024);
            else v=index%257?index*17:random32();
            put32(data+i,v);
        }
        data[65536]=1;data[65537]=2;data[65538]=3;
        for(size_t e=0;e<sizeof edges/sizeof *edges;e++)check(data,edges[e]);
    }
    for(int it=0;it<100;it++){size_t n=random32()%4096;for(size_t i=0;i<n;i++)data[i]=(uint8_t)random32();check(data,n);}
    uint8_t small[128];int64_t cn=krc_compress((const uint8_t *)"123456789",9,small,sizeof small,1);assert(cn>0&&get32(small+20)==0xe3069283u);
    assert(!krc_bound(SIZE_MAX));
    assert((modes&31)==31);free(data);
    printf("KRC TESTS PASSED (240 cases, all predictor modes, tails, corruption, truncation, CPU thread agreement)\n");return 0;
}
