#undef NDEBUG
#include "kbc.h"
#include "krc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t rng=20261107;
static uint32_t random32(void) { rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return (uint32_t)rng; }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void wr32(uint8_t *p,uint32_t x) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(i*8)); }
static uint32_t crc(const uint8_t *p,size_t n) {
    uint32_t c=~0u;
    for(size_t i=0;i<n;i++) { c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^(0x82f63b78u&(0u-(c&1))); }
    return ~c;
}
static void repair(uint8_t *p,size_t n) { wr32(p+24,crc(p+32,n-32));wr32(p+28,crc(p,28)); }
static unsigned modes, cases;
static void check(const uint8_t *input,size_t n) {
    size_t cap=kbc_bound(n);assert(cap);
    uint8_t *a=malloc(cap),*b=malloc(cap),*back=malloc(n+32),*bad=malloc(cap);assert(a&&b&&back&&bad);
    int64_t cn=kbc_compress(input,n,a,cap,1);assert(cn>=32&&(size_t)cn<=cap);
    assert(kbc_original_size(a,cn)==(int64_t)n);
    assert(kbc_compress(input,n,b,cap,10)==cn&&!memcmp(a,b,cn));
    for(int t=1;t<=10;t+=9) {
        memset(back,0xa5,n+32);
        assert(kbc_decompress(a,cn,back,n,t)==(int64_t)n&&!memcmp(input,back,n));
        for(size_t i=n;i<n+32;i++)assert(back[i]==0xa5);
    }
    assert(kbc_compress(input,n,b,cap-1,1)==KRC_ERR_CAPACITY);
    if(n)assert(kbc_decompress(a,cn,back,n-1,1)==KRC_ERR_CAPACITY);
    unsigned count=rd32(a+16);
    for(unsigned i=0;i<count;i++)modes|=1u<<a[32+12*i];
    for(unsigned i=0;i<12;i++) {
        memcpy(bad,a,cn);bad[random32()%(size_t)cn]^=(uint8_t)(1u<<(random32()%8));
        assert(kbc_decompress(bad,cn,back,n,4)<0);
        assert(kbc_decompress(a,random32()%(size_t)cn,back,n,4)<0);
    }
    if(count) {
        memcpy(bad,a,cn);bad[32]=3;repair(bad,cn);assert(kbc_decompress(bad,cn,back,n,4)<0);
        memcpy(bad,a,cn);bad[33]=1;repair(bad,cn);assert(kbc_decompress(bad,cn,back,n,4)<0);
        memcpy(bad,a,cn);wr32(bad+40,UINT32_MAX);repair(bad,cn);assert(kbc_decompress(bad,cn,back,n,4)<0);
        /* Reach the codec after repairing payload CRC, retaining original CRC. */
        if(cn>32+12*count) {
            memcpy(bad,a,cn);bad[32+12*count]^=0x80;repair(bad,cn);
            assert(kbc_decompress(bad,cn,back,n,4)<0);
        }
    }
    cases++;free(a);free(b);free(back);free(bad);
}
int main(void) {
    size_t max=2*KBC_BLOCK_BYTES+17;uint8_t *data=malloc(max);assert(data);
    const char *text="The independent byte model predicts the next symbol using bounded contexts. ";
    const size_t edges[]={0,1,2,3,4,7,31,32,255,1023,1024,1025,4096,65539,KBC_BLOCK_BYTES-1,KBC_BLOCK_BYTES,KBC_BLOCK_BYTES+1,2*KBC_BLOCK_BYTES+17};
    for(unsigned pattern=0;pattern<4;pattern++) {
        for(size_t i=0;i<max;i++)data[i]=pattern==0?0:pattern==1?(uint8_t)random32():pattern==2?(uint8_t)text[i%strlen(text)]:(uint8_t)((i/4)>>(8*(i%4)));
        for(size_t e=0;e<sizeof(edges)/sizeof(*edges);e++)check(data,edges[e]);
    }
    for(unsigned i=0;i<25;i++) { size_t n=random32()%2048;for(size_t j=0;j<n;j++)data[j]=(uint8_t)random32();check(data,n); }
    uint8_t frame[128];int64_t cn=kbc_compress((const uint8_t*)"123456789",9,frame,sizeof frame,1);
    assert(cn>0&&rd32(frame+20)==0xe3069283u);
    assert(kbc_bound(SIZE_MAX)==0&&kbc_compress(NULL,1,frame,sizeof frame,1)<0);
    assert(kbc_decompress(NULL,0,NULL,0,1)<0);
    assert(modes==7);free(data);
    printf("KBC TESTS PASSED (%u cases; all storage modes, block boundaries, CPU agreement, guarded output, repaired-CRC corruption)\n",cases);
    return 0;
}
