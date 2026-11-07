#include "frontier_pcm.h"
#include <string.h>

enum { TILE=256, GROUP=32, GUARD=4 };
static uint16_t word(const uint8_t *p) { return (uint16_t)(p[0] | (unsigned)p[1]<<8); }
static void put16(uint8_t *p,uint16_t x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); }
static unsigned bits(uint16_t x) { return x?32u-(unsigned)__builtin_clz((unsigned)x):0; }
static unsigned fold(uint16_t v) { return (uint16_t)((v<<1) ^ -(int)(v>>15)); }
static uint16_t unfold(unsigned x) { return (uint16_t)((x>>1) ^ -(int)(x&1)); }

static uint16_t predict(const uint8_t *p,size_t i,unsigned stride,unsigned order) {
    if (i<stride) return 0;
    uint16_t a=word(p+2*(i-stride));
    if (!order || i<2*stride) return a;
    uint16_t b=word(p+2*(i-2*stride));
    return (uint16_t)(2u*a-b);
}

int frontier_pcm_probe(const uint8_t *src,size_t n) {
    if (n<8192) return 0;
    size_t count=n/2;
    uint64_t score[2]={0}, magnitude=0;
    for (unsigned part=0; part<4; ++part) {
        size_t start=2 + (count-514)*part/3;
        for (size_t i=start; i<start+512; ++i) {
            uint16_t v=word(src+2*i);
            magnitude+=bits((uint16_t)fold(v));
            for (unsigned ch=1; ch<=2; ++ch)
                score[ch-1]+=bits((uint16_t)fold((uint16_t)(v-word(src+2*(i-ch)))));
        }
    }
    unsigned best=score[1]<score[0]?2:1;
    return score[best-1]<12u*2048u && score[best-1]+1024<magnitude? (int)best:0;
}

static size_t packed_bytes(size_t n,unsigned width) { return (n*width+7)/8; }

/* A checked group is followed by four guard bytes so a 32-bit peeking
 * load is valid even at the last partial byte. The guard is in the frame. */
static void pack(const uint16_t *v,size_t n,unsigned width,uint8_t *out) {
    uint32_t carry=0, mask=(1u<<width)-1;
    unsigned fill=0;
    for (size_t i=0;i<n;++i) {
        carry|=((uint32_t)v[i]&mask)<<fill;
        fill+=width;
        while (fill>=8) { *out++=(uint8_t)carry; carry>>=8; fill-=8; }
    }
    if (fill) *out=(uint8_t)carry;
}

static void unpack(const uint8_t *p,size_t n,unsigned width,uint16_t *out) {
    uint32_t mask=(1u<<width)-1;
    for (size_t i=0;i<n;++i) {
        size_t bit=i*width;
        const uint8_t *a=p+bit/8;
        uint32_t v=(uint32_t)a[0]|(uint32_t)a[1]<<8|(uint32_t)a[2]<<16;
        out[i]=(uint16_t)((v>>(bit%8))&mask);
    }
}

static unsigned order_for(const uint8_t *src,size_t start,size_t stop,unsigned stride) {
    unsigned cost[2]={0};
    size_t limit=stop-start<40?stop:start+40;
    for (size_t i=start;i<limit;++i) {
        uint16_t value=word(src+2*i);
        for (unsigned o=0;o<2;++o)
            cost[o]+=bits((uint16_t)fold((uint16_t)(value-predict(src,i,stride,o))));
    }
    return cost[1]<cost[0];
}

size_t frontier_pcm_encode(const uint8_t *src,size_t n,unsigned channels,uint8_t *out,size_t cap) {
    if (n<512 || cap<8 || (channels!=1 && channels!=2)) return 0;
    size_t count=n/2,w=1;
    out[0]=(uint8_t)channels;
    for (size_t start=0;start<count;start+=TILE) {
        size_t stop=count-start<TILE?count:start+TILE;
        unsigned order=order_for(src,start,stop,channels);
        if (w>=cap) return 0;
        out[w++]=(uint8_t)order;
        for (size_t j=start;j<stop;j+=GROUP) {
            size_t k=stop-j<GROUP?stop-j:GROUP;
            uint16_t v[GROUP], high[GROUP], all=0;
            unsigned hist[17]={0};
            for (size_t i=0;i<k;++i) {
                v[i]=(uint16_t)fold((uint16_t)(word(src+2*(j+i))-predict(src,j+i,channels,order)));
                ++hist[bits(v[i])]; all|=v[i];
            }
            unsigned max=bits(all), best=max, above=0;
            size_t size=1+packed_bytes(k,max);
            for (unsigned b=max;b>0;) {
                above+=hist[b--];
                size_t cost=6+packed_bytes(k,b)+packed_bytes(above,max-b);
                if (cost<size) { best=b; size=cost; }
            }
            if (size>cap-w) return 0;
            out[w++]=(uint8_t)best;
            if (best==max) { pack(v,k,best,out+w); w+=packed_bytes(k,best); continue; }
            out[w-1]|=32;
            out[w++]=(uint8_t)(max-best);
            uint32_t mask=0; unsigned cnt=0;
            for (size_t i=0;i<k;++i) if (v[i]>>best) {
                mask|=1u<<i; high[cnt++]=v[i]>>best;
            }
            for (unsigned t=0;t<4;++t) out[w++]=(uint8_t)(mask>>(8*t));
            pack(v,k,best,out+w); w+=packed_bytes(k,best);
            pack(high,cnt,max-best,out+w); w+=packed_bytes(cnt,max-best);
        }
    }
    if (cap-w < GUARD+(n&1)) return 0;
    if (n&1) out[w++]=src[n-1];
    memset(out+w,0,GUARD); w+=GUARD;
    return w<n?w:0;
}

int frontier_pcm_decode(const uint8_t *src,size_t n,uint8_t *out,size_t raw) {
    if (n<1+GUARD+(raw&1) || raw<512 || (src[0]!=1 && src[0]!=2)) return -1;
    size_t count=raw/2,limit=n-GUARD-(raw&1),p=1;
    unsigned channels=src[0];
    for (size_t start=0;start<count;start+=TILE) {
        if (p>=limit) return -1;
        unsigned order=src[p++]; if (order>1) return -1;
        size_t stop=count-start<TILE?count:start+TILE;
        for (size_t j=start;j<stop;j+=GROUP) {
            if (p>=limit) return -1;
            size_t k=stop-j<GROUP?stop-j:GROUP;
            unsigned tag=src[p++],width=tag&31,extension=0;
            if (width>16 || tag>48) return -1;
            uint32_t mask=0;
            if (tag&32) {
                if (limit-p<5) return -1;
                extension=src[p++];
                for (unsigned t=0;t<4;++t) mask|=(uint32_t)src[p++]<<(8*t);
                if (!extension || width+extension>16 || !mask || (k<32 && (mask>>k))) return -1;
            }
            size_t lows=packed_bytes(k,width);
            unsigned n_high=(unsigned)__builtin_popcount(mask);
            size_t highs=packed_bytes(n_high,extension);
            if (lows+highs>limit-p) return -1;
            uint16_t v[GROUP],hi[GROUP];
            unpack(src+p,k,width,v); p+=lows;
            if (mask) {
                unpack(src+p,n_high,extension,hi); p+=highs;
                for (unsigned a=0;mask;mask&=mask-1)
                    v[__builtin_ctz(mask)]|=(uint16_t)(hi[a++]<<width);
            }
            for (size_t i=0;i<k;++i)
                put16(out+2*(j+i),unfold(v[i])+predict(out,j+i,channels,order));
        }
    }
    if (p!=limit) return -1;
    if (raw&1) out[raw-1]=src[p];
    return 0;
}
