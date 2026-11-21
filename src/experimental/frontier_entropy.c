/* Independently implemented integer entropy coder. For the underlying
 * asymmetric numeral system, see Jarek Duda, arXiv:1311.2540. */
#include "frontier_entropy.h"
#include <stdlib.h>
#include <string.h>

enum { BITS=12, TOTAL=1<<BITS, LOWER=1<<23, GROUPS=8, LANES=4 };
typedef struct { uint16_t base, count; uint64_t inverse; } Symbol;
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void put32(uint8_t *p,uint32_t v) {
    for(unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(8*i));
}
static unsigned group_count(unsigned model) {
    if(model==FRONTIER_PLAIN) return 1;
    if(model==FRONTIER_BYTES) return 4;
    return model==FRONTIER_QUALITY?8:0;
}
static void contexts(uint8_t *lookup,unsigned model) {
    for(unsigned c=0;c<256;++c) {
        if(model==FRONTIER_BYTES) lookup[c]=(uint8_t)(c>>6);
        else if(model==FRONTIER_PLAIN) lookup[c]=0;
        else if(c<39) lookup[c]=0;
        else if(c<49) lookup[c]=1;
        else if(c<57) lookup[c]=2;
        else if(c<63) lookup[c]=3;
        else if(c<68) lookup[c]=4;
        else if(c<71) lookup[c]=5;
        else if(c<74) lookup[c]=6;
        else lookup[c]=7;
    }
}

static void normalize(const uint32_t *hist, Symbol *model) {
    uint64_t total=0;
    unsigned largest=0;
    for(unsigned c=0;c<256;++c) {
        total+=hist[c];
        if(hist[c]>hist[largest]) largest=c;
    }
    if(!total) return;
    unsigned assigned=0;
    for(unsigned c=0;c<256;++c) {
        if(!hist[c]) continue;
        uint16_t freq=(uint16_t)(((uint64_t)hist[c]*TOTAL)/total);
        model[c].count=freq?freq:1;
        assigned+=model[c].count;
    }
    if(assigned<TOTAL) model[largest].count+=(uint16_t)(TOTAL-assigned);
    while(assigned>TOTAL) {
        unsigned best=0;
        for(unsigned c=1;c<256;++c) if(model[c].count>model[best].count) best=c;
        unsigned removable=model[best].count-1;
        unsigned take=assigned-TOTAL<removable?assigned-TOTAL:removable;
        model[best].count-=(uint16_t)take;assigned-=take;
    }
    unsigned offset=0;
    for(unsigned c=0;c<256;++c) if(model[c].count) {
        model[c].base=(uint16_t)offset;
        offset+=model[c].count;
        /* Ceiling reciprocal, corrected by at most one for a 32-bit state. */
        model[c].inverse=((UINT64_C(1)<<32)+model[c].count-1)/model[c].count;
    }
}

size_t frontier_entropy_encode(const uint8_t *src,size_t n,unsigned mode,uint8_t *dst,size_t cap) {
    unsigned groups=group_count(mode);
    if(!groups || n<256 || n>UINT32_MAX || cap<24) return 0;
    uint32_t hist[GROUPS][256]={{0}};
    uint8_t groups_for[256];contexts(groups_for,mode);
    unsigned prev=0;
    for(size_t i=0;i<n;++i) {
        unsigned c=src[i];++hist[groups_for[prev]][c];prev=c;
    }
    Symbol models[GROUPS][256]={0};
    put32(dst,(uint32_t)n);dst[4]=(uint8_t)mode;
    size_t head=5;
    for(unsigned g=0;g<groups;++g) {
        normalize(hist[g],models[g]);
        if(cap-head<2) return 0;
        size_t index=head;head+=2;
        unsigned used=0;
        for(unsigned c=0;c<256;++c) if(models[g][c].count) {
            if(cap-head<3) return 0;
            ++used;dst[head++]=(uint8_t)c;
            dst[head++]=(uint8_t)models[g][c].count;
            dst[head++]=(uint8_t)(models[g][c].count>>8);
        }
        dst[index]=(uint8_t)used;dst[index+1]=(uint8_t)(used>>8);
    }
    if(cap-head<4*LANES) return 0;
    uint32_t state[LANES]={LOWER,LOWER,LOWER,LOWER};
    uint8_t *at=dst+cap,*limit=dst+head+4*LANES;
    for(size_t i=n;i--;) {
        unsigned g=groups_for[i?src[i-1]:0];
        const Symbol *s=&models[g][src[i]];
        uint32_t x=state[i%LANES];
        uint32_t stop=(uint32_t)((LOWER>>BITS)*256)*s->count;
        while(x>=stop) {
            if(at==limit) return 0;
            *--at=(uint8_t)x;x>>=8;
        }
        uint32_t q=(uint32_t)((x*s->inverse)>>32);
        if((uint64_t)q*s->count>x) --q;
        state[i%LANES]=(q<<BITS)+(x-q*s->count)+s->base;
    }
    for(unsigned i=0;i<LANES;++i) put32(dst+head+4*i,state[i]);
    size_t bytes=(size_t)((dst+cap)-at);
    size_t length=head+4*LANES+bytes;
    if(length>=n) return 0;
    memmove(limit,at,bytes);
    return length;
}

int frontier_entropy_decode(const uint8_t *src,size_t n,uint8_t *dst,size_t expected) {
    if(n<5 || get32(src)!=expected || !expected) return -1;
    unsigned mode=src[4],groups=group_count(mode);
    if(!groups) return -1;
    uint8_t *lookup=malloc(groups*TOTAL);
    if(!lookup) return -1;
    uint16_t freq[GROUPS][256]={{0}},base[GROUPS][256]={{0}};
    unsigned available=0;
    size_t pos=5;
    int result=-1;
    for(unsigned g=0;g<groups;++g) {
        if(n-pos<2) goto done;
        unsigned used=src[pos]|(unsigned)src[pos+1]<<8;pos+=2;
        if(used>256 || used>(n-pos)/3) goto done;
        unsigned sum=0;
        int last=-1;
        for(unsigned j=0;j<used;++j) {
            unsigned c=src[pos],f=src[pos+1]|(unsigned)src[pos+2]<<8;pos+=3;
            if((int)c<=last || !f || f>TOTAL-sum) goto done;
            freq[g][c]=(uint16_t)f;base[g][c]=(uint16_t)sum;
            memset(lookup+g*TOTAL+sum,(int)c,f);
            sum+=f;last=(int)c;
        }
        if(used && sum!=TOTAL) goto done;
        if(used) available|=1u<<g;
    }
    if(n-pos<4*LANES) goto done;
    uint32_t state[LANES];
    for(unsigned i=0;i<LANES;++i) {
        state[i]=get32(src+pos);pos+=4;
        if(state[i]<LOWER || state[i]>=(uint32_t)LOWER*256) goto done;
    }
    uint8_t groups_for[256];contexts(groups_for,mode);
    unsigned prev=0;
    for(size_t i=0;i<expected;++i) {
        unsigned g=groups_for[prev];
        if(!(available&(1u<<g))) goto done;
        uint32_t x=state[i%LANES];unsigned slot=x&(TOTAL-1);
        unsigned c=lookup[g*TOTAL+slot];
        dst[i]=(uint8_t)c;prev=c;
        x=freq[g][c]*(x>>BITS)+slot-base[g][c];
        while(x<LOWER) {
            if(pos==n) goto done;
            x=(x<<8)|src[pos++];
        }
        state[i%LANES]=x;
    }
    for(unsigned i=0;i<LANES;++i) if(state[i]!=LOWER) goto done;
    result=pos==n?0:-1;
done:
    free(lookup);
    return result;
}
