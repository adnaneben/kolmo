#include "frontier_lag.h"
#include <limits.h>
#include <string.h>

enum { CHUNK = 256, SHORTLIST = 8, PROBES = 48, CHECK = 384 };
static void put32(uint8_t *p,uint32_t x) {
    for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(x>>(8*i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static unsigned distance(uint8_t a,uint8_t b) {
    return a>b?(unsigned)a-b:(unsigned)b-a;
}
static uint32_t random_word(uint32_t *state) {
    uint32_t v=*state;
    v^=v<<13; v^=v>>17; v^=v<<5;
    return *state=v;
}

typedef struct { uint32_t lag, cost; } Choice;
static void add(Choice *list,uint32_t lag,uint32_t cost) {
    if (cost>=list[SHORTLIST-1].cost) return;
    unsigned j=SHORTLIST-1;
    while (j && cost<list[j-1].cost) { list[j]=list[j-1]; --j; }
    list[j]=(Choice){lag,cost};
}

static uint32_t score(const uint8_t *src,const size_t *pos,unsigned count,uint32_t lag) {
    unsigned result=0;
    for (unsigned i=0;i<count;++i) result+=distance(src[pos[i]],src[pos[i]-lag]);
    return result;
}

static Choice confirm(const uint8_t *src,const size_t *pos,const Choice *list) {
    Choice best={0,UINT_MAX};
    for (unsigned i=0;i<SHORTLIST;++i) if (list[i].lag) {
        unsigned cost=score(src,pos,CHECK,list[i].lag);
        if (cost<best.cost) best=(Choice){list[i].lag,cost};
    }
    return best;
}

int frontier_lag_probe(const uint8_t *src,size_t n,frontier_lags *out) {
    if (n<65536) return 0;
    size_t probe=n<4u*1024*1024?n:4u*1024*1024;
    uint32_t maxlag=(uint32_t)(probe/3);
    size_t pos[CHECK];
    uint32_t state=0x71b53d19;
    for (unsigned i=0;i<CHECK;++i)
        pos[i]=maxlag+random_word(&state)%(probe-maxlag);
    Choice near={1,UINT_MAX};
    for (uint32_t lag=1;lag<=8;++lag) {
        unsigned cost=score(src,pos,CHECK,lag);
        if(cost<near.cost) near=(Choice){lag,cost};
    }
    /* Reject streams without appreciable local byte correlation, before an
     * expensive period search. Validation uses a disjoint set of offsets. */
    if (near.cost>CHECK*36) return 0;
    Choice row[SHORTLIST],frame[SHORTLIST];
    for (unsigned i=0;i<SHORTLIST;++i) row[i]=frame[i]=(Choice){0,UINT_MAX};
    for (uint32_t lag=16;lag<=4096;++lag) add(row,lag,score(src,pos,PROBES,lag));
    for (uint32_t lag=4160;lag<=maxlag;lag+=64) add(frame,lag,score(src,pos,PROBES,lag));
    for (unsigned i=0;i<CHECK;++i) pos[i]=maxlag+random_word(&state)%(probe-maxlag);
    near.cost=score(src,pos,CHECK,near.lag);
    Choice a=confirm(src,pos,row), b=confirm(src,pos,frame);
    if (a.cost>near.cost*17/10 || a.cost>CHECK*30) return 0;
    out->near=near.lag;out->stride=a.lag;
    out->period=b.cost<near.cost*13/10 && b.cost<CHECK*20?b.lag:0;
    return a.cost<CHECK*19 || out->period;
}

/* Introduce two equal-length trials for neighboring rows around a candidate
 * period. Offsets crossing the beginning of a block are never referenced. */
static unsigned candidates(const frontier_lags *s,uint32_t *lag) {
    unsigned n=0;
    lag[n++]=s->near;
    lag[n++]=s->stride;
    if (s->period) {
        lag[n++]=s->period;
        if (s->period>s->stride) lag[n++]=s->period-s->stride;
        lag[n++]=s->period+s->stride;
        lag[n++]=s->period-s->near; lag[n++]=s->period+s->near;
        if (s->period>2*s->near) {lag[n++]=s->period-2*s->near;lag[n++]=s->period+2*s->near;}
    }
    return n;
}

size_t frontier_lag_encode(const uint8_t *src,size_t n,const frontier_lags *s,uint8_t *dst,size_t cap) {
    size_t chunks=n/CHUNK+(n%CHUNK!=0);
    if (n<CHUNK || cap<n || cap-n<12+chunks) return 0;
    uint32_t lag[10]; unsigned modes=candidates(s,lag);
    put32(dst,s->near);put32(dst+4,s->stride);put32(dst+8,s->period);
    uint8_t *tags=dst+12,*body=tags+chunks;
    for (size_t chunk=0;chunk<chunks;++chunk) {
        size_t st=chunk*CHUNK, len=n-st<CHUNK?n-st:CHUNK;
        unsigned pick=modes, best=UINT_MAX;
        for (unsigned m=0;m<modes;++m) if (st>=lag[m]) {
            unsigned cost=0;
            for (size_t k=3;k<len;k+=11) cost+=distance(src[st+k],src[st+k-lag[m]]);
            if (cost<best) {best=cost;pick=m;}
        }
        tags[chunk]=(uint8_t)pick;
        if(pick==modes) {memcpy(body+st,src+st,len);continue;}
        const uint8_t *prev=src+st-lag[pick];
        for (size_t i=0;i<len;++i) body[st+i]=src[st+i]-prev[i];
    }
    return n+12+chunks;
}

int frontier_lag_decode(const uint8_t *src,size_t n,uint8_t *dst,size_t raw) {
    size_t chunks=raw/CHUNK+(raw%CHUNK!=0);
    if (raw<CHUNK || n<raw || n-raw!=12+chunks) return -1;
    frontier_lags s={get32(src),get32(src+4),get32(src+8)};
    if(!s.near || s.near>8 || s.stride<16 || s.stride>4096 ||
        (s.period && (s.period<4160 || s.period>(4u<<20)))) return -1;
    uint32_t lag[10]; unsigned modes=candidates(&s,lag);
    const uint8_t *tags=src+12,*body=tags+chunks;
    for(size_t j=0;j<chunks;++j) {
        unsigned m=tags[j];
        size_t st=j*CHUNK,len=raw-st<CHUNK?raw-st:CHUNK;
        if (m>modes) return -1;
        if (m==modes) {memcpy(dst+st,body+st,len);continue;}
        if (lag[m]>st) return -1;
        for(size_t i=0;i<len;++i) dst[st+i]=body[st+i]+dst[st+i-lag[m]];
    }
    return 0;
}
