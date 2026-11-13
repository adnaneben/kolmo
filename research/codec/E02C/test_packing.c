#include "packing.h"
#include <stdio.h>
static uint64_t rng=3101;
static uint64_t rnd(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
int main(void){size_t cases=0,mutations=0,truncations=0;uint64_t in[513],out[513];ec_buf b={0};
    const size_t sizes[]={0,1,2,15,16,31,32,127,128,129,255,256,257,513};
    for(size_t z=0;z<sizeof(sizes)/sizeof(*sizes);z++)for(unsigned kind=0;kind<8;kind++){
        size_t n=sizes[z];for(size_t i=0;i<n;i++){uint64_t x=rnd();
            in[i]=kind==0?0:kind==1?UINT64_MAX:kind==2?i:kind==3?x&511:kind==4?(i%127?x&7:x):kind==5?(UINT64_C(1)<<(i%64)):kind==6?(i%2?UINT64_MAX:0):x;}
        for(unsigned method=1;method<=4;method++){
            if(ec_pack(in,n,method,&b)||ec_unpack(b.p,b.n,n,method,out)||memcmp(in,out,n*8)){fprintf(stderr,"roundtrip failed\n");return 1;}cases++;
            for(size_t k=0;k<b.n;k++){(void)ec_unpack(b.p,k,n,method,out);truncations++;}
            for(unsigned j=0;j<16&&b.n;j++){size_t k=rnd()%b.n;uint8_t old=b.p[k];b.p[k]^=(uint8_t)(1+rnd()%255);(void)ec_unpack(b.p,b.n,n,method,out);b.p[k]=old;mutations++;}
        }
    }ec_free(&b);printf("packing: %zu roundtrips, %zu truncated parser probes, %zu mutation probes passed\n",cases,truncations,mutations);return 0;
}
