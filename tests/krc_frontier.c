#include "krc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned state=17;
static uint32_t next(void) {state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void) {
    static const size_t ns[]={0,1,3,4,1024,1048576,1048577,1048578,1048579,1268724,5213463};
    for(unsigned j=0;j<sizeof(ns)/sizeof(ns[0]);++j) {
        size_t n=ns[j],cap=krc_bound(n),tiles=krc_plan_count(n);
        unsigned char *src=malloc(n+1),*dst=malloc(cap),*ref=malloc(cap),*out=malloc(n+1);
        krc_plan *plans=malloc((tiles+1)*sizeof(*plans));
        assert(src&&dst&&ref&&out&&plans);
        for(size_t i=0;i<n;++i)
            src[i]=i/8192%3==0?next():i/8192%3==1?(i>>7):0;
        assert(!krc_plan_cpu(src,n,plans,tiles,4));
        int64_t expected=krc_encode(src,n,plans,tiles,ref,cap);
        assert(expected>0);
        for(unsigned w=1;w<=10;w+=3) {
            int64_t v=krc_encode_threads(src,n,plans,tiles,dst,cap,w);
            assert(v==expected && !memcmp(dst,ref,(size_t)v));
            assert(krc_decode_threads(dst,(size_t)v,out,n,w)==(int64_t)n && !memcmp(src,out,n));
        }
        if(n>1048576) {
            dst[42]^=128;
            assert(krc_decode_threads(dst,(size_t)expected,out,n,4)<0);
            assert(krc_decode_threads(ref,(size_t)expected-1,out,n,4)<0);
            plans[0].mode=122;
            assert(krc_encode_threads(src,n,plans,tiles,dst,cap,4)<0);
        }
        free(plans);free(src);free(dst);free(ref);free(out);
    }
    puts("All region layouts produced exact KRC1 bytes and rejected damaged payloads.");
}
