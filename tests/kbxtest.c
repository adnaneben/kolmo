#undef NDEBUG
#include "kbx.h"
#include "kbc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t state=20261108;
static uint32_t rnd(void){state^=state<<13;state^=state>>7;state^=state<<17;return (uint32_t)state;}
static uint32_t checksum(const uint8_t *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int b=0;b<8;b++)c=(c>>1)^(0x82f63b78u&(0u-(c&1)));}return ~c;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void repair(uint8_t *p,size_t n){put32(p+24,checksum(p+32,n-32));put32(p+28,checksum(p,28));}
static unsigned cases,mode_mask;
static void test(const uint8_t *src,size_t n){
 size_t cap=kbx_bound(n);assert(cap==kbc_bound(n));
 uint8_t *old=malloc(cap),*a=malloc(cap),*b=malloc(cap),*back=malloc(n+32),*bad=malloc(cap);assert(old&&a&&b&&back&&bad);
 int64_t baseline=kbc_compress(src,n,old,cap,1);assert(baseline>0);
 int64_t strong=0;
 for(int p=0;p<3;p++){
  int64_t z=kbx_compress(src,n,a,cap,1,p);assert(z>=32&&(size_t)z<=cap);
  assert(kbx_original_size(a,z)==(int64_t)n);
  assert(kbx_compress(src,n,b,cap,10,p)==z&&!memcmp(a,b,z));
  if(!p){assert(z==baseline&&!memcmp(a,old,z));assert(kbc_decompress(a,z,back,n,1)==(int64_t)n&&!memcmp(src,back,n));}
  if(p==1)strong=z;
  if(p==2)assert(z<=baseline&&z<=strong);
  memset(back,0xa5,n+32);
  assert(kbx_decompress(a,z,back,n,10)==(int64_t)n&&!memcmp(src,back,n));
  for(size_t i=n;i<n+32;i++)assert(back[i]==0xa5);
  if(n)assert(kbx_decompress(a,z,back,n-1,1)==KRC_ERR_CAPACITY);
  assert(kbx_compress(src,n,b,cap-1,1,p)==KRC_ERR_CAPACITY);
  unsigned count=(unsigned)(n/KBX_BLOCK_BYTES+(n%KBX_BLOCK_BYTES!=0));
  for(unsigned i=0;i<count;i++)mode_mask|=1u<<a[32+12*i];
  if(count){
   memcpy(bad,a,z);bad[32]=4;repair(bad,z);assert(kbx_decompress(bad,z,back,n,4)<0);
   memcpy(bad,a,z);bad[33]=1;repair(bad,z);assert(kbx_decompress(bad,z,back,n,4)<0);
   memcpy(bad,a,z);bad[32+12*count]^=0x80;repair(bad,z);assert(kbx_decompress(bad,z,back,n,4)<0);
  }
  for(int k=0;k<8;k++){
   memcpy(bad,a,z);bad[rnd()%(size_t)z]^=(uint8_t)(1u<<(rnd()%8));
   assert(kbx_decompress(bad,z,back,n,1)<0);
   assert(kbx_decompress(a,rnd()%(size_t)z,back,n,1)<0);
  }
 }
 assert(kbx_decompress(old,baseline,back,n,1)==(int64_t)n&&!memcmp(src,back,n));
 cases++;free(old);free(a);free(b);free(back);free(bad);
}
int main(void){
 const size_t sizes[]={0,1,3,4,7,255,1025,4096,262143,262144,262145,524305};
 size_t max=524305;uint8_t *src=malloc(max);assert(src);
 const char *text="This independently tested pattern includes repeated words, records and byte contexts. ";
 for(unsigned pat=0;pat<4;pat++){
  for(size_t i=0;i<max;i++)src[i]=pat==0?0:pat==1?(uint8_t)rnd():pat==2?(uint8_t)text[i%strlen(text)]:(uint8_t)((i/4)>>(8*(i%4)));
  for(unsigned k=0;k<sizeof sizes/sizeof *sizes;k++)test(src,sizes[k]);
 }
 for(unsigned i=0;i<25;i++){size_t n=rnd()%2048;for(size_t j=0;j<n;j++)src[j]=(uint8_t)rnd();test(src,n);}
 uint8_t out[64];assert(kbx_compress(NULL,0,out,sizeof out,1,3)==KRC_ERR_PLAN);
 assert(!kbx_bound(SIZE_MAX));assert(mode_mask==15);free(src);
 printf("KBX TESTS PASSED (%u cases x 3 profiles; exact KBC1 frames; stronger-model round trips; smallest-payload invariant; CPU agreement; corruption and bounds)\n",cases);
 return 0;
}
