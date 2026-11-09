#include "frontier_runs.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}

size_t frontier_runs_encode(const uint8_t *s,size_t n,uint8_t *out,size_t cap) {
    if(n<1024 || n>UINT32_MAX || cap<8) return 0;
    unsigned repeated=0;
    for(size_t i=0;i<256;++i) {
        size_t p=(n-8)/256*i;
        repeated+=s[p]==s[p+1] && s[p]==s[p+3] && s[p]==s[p+7];
    }
    if(repeated<12) return 0;
    put32(out,(uint32_t)n);
    size_t p=0,literal=0,w=4;
    while(n-p>=16) {
        uint64_t a,b;
        memcpy(&a,s+p,8);memcpy(&b,s+p+8,8);
        if(a!=b || a!=UINT64_C(0x0101010101010101)*s[p]) {p+=8;continue;}
        size_t begin=p,stop=p+16;
        while(begin>literal && s[begin-1]==s[p]) --begin;
        while(stop<n && s[stop]==s[p]) ++stop;
        size_t count=begin-literal;
        if(cap-w<9 || count>cap-w-9) return 0;
        put32(out+w,(uint32_t)count);w+=4;
        memcpy(out+w,s+literal,count);w+=count;
        put32(out+w,(uint32_t)(stop-begin));out[w+4]=s[p];w+=5;
        literal=p=stop;
    }
    if(cap-w<4 || n-literal>cap-w-4) return 0;
    put32(out+w,(uint32_t)(n-literal));w+=4;
    memcpy(out+w,s+literal,n-literal);w+=n-literal;
    return w<n?w:0;
}

int frontier_runs_decode(const uint8_t *src,size_t n,uint8_t *out,size_t raw) {
    if(n<8 || get32(src)!=raw) return -1;
    size_t p=4,w=0;
    for(;;) {
        if(n-p<4) return -1;
        size_t len=get32(src+p);p+=4;
        if(len>raw-w || len>n-p) return -1;
        memcpy(out+w,src+p,len);w+=len;p+=len;
        if(p==n) return w==raw?0:-1;
        if(n-p<5) return -1;
        size_t run=get32(src+p);p+=4;
        if(run<16 || run>raw-w) return -1;
        memset(out+w,src[p++],run);w+=run;
    }
}
