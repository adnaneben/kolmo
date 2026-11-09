#include "frontier_prefix.h"
#include <stdlib.h>
#include <string.h>

enum { CONTEXTS = 4, LANES = 4, PADDING = 4 };
typedef struct {
    uint16_t code[256], decode[512];
    uint8_t bits[256];
} Model;

typedef struct { uint32_t count; unsigned symbol; } Frequency;

static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put32(uint8_t *p, uint32_t x) {
    for (unsigned i=0; i<4; ++i) p[i]=(uint8_t)(x>>(8*i));
}
static int compare(const void *a, const void *b) {
    const Frequency *x = a, *y = b;
    if (x->count != y->count) return x->count > y->count ? -1 : 1;
    return (int)x->symbol - (int)y->symbol;
}
static unsigned reverse(unsigned x, unsigned n) {
    unsigned r = 0;
    while (n--) { r = (r<<1) | (x&1); x >>= 1; }
    return r;
}

/* k short codes consume exactly the leaves released by 2*k long codes.
 * This gives a complete single-lookup alphabet with maximum length nine. */
static void tables(Model *m, unsigned k) {
    unsigned next[3] = {0, 2*k, 512-2*k};
    for (unsigned s=0; s<256; ++s) {
        unsigned len=m->bits[s], word=reverse(next[len-7]++, len);
        m->code[s]=(uint16_t)word;
        for (unsigned i=word; i<512; i+=1u<<len) m->decode[i]=(uint16_t)((s<<4)|len);
    }
}

static size_t model(const uint32_t *counts, Model *m, uint8_t *header) {
    Frequency freq[256];
    for (unsigned i=0; i<256; ++i) freq[i]=(Frequency){counts[i],i};
    qsort(freq,256,sizeof(freq[0]),compare);
    unsigned best=0;
    int64_t saved=0, maximum=0;
    for (unsigned k=1; k<=85; ++k) {
        saved += (int64_t)freq[k-1].count - freq[256-2*k].count - freq[257-2*k].count;
        int64_t net=saved-24*(int64_t)k;
        if (net>maximum) { maximum=net; best=k; }
    }
    memset(m->bits,8,sizeof(m->bits));
    header[0]=(uint8_t)best;
    for (unsigned i=0; i<best; ++i) {
        unsigned s=freq[i].symbol; header[1+i]=(uint8_t)s; m->bits[s]=7;
    }
    for (unsigned i=0; i<2*best; ++i) {
        unsigned s=freq[255-i].symbol; header[1+best+i]=(uint8_t)s; m->bits[s]=9;
    }
    tables(m,best);
    return 1+3*best;
}

static size_t split(size_t n, unsigned lane) {
    return n / LANES * lane + (n % LANES) * lane / LANES;
}

size_t frontier_prefix_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    if (n<16384 || n>UINT32_MAX || (uint64_t)cap < (uint64_t)n*9/8 + 1100) return 0;
    uint32_t counts[CONTEXTS][256]={{0}};
    for (unsigned l=0; l<LANES; ++l) {
        unsigned previous=0;
        size_t stop=split(n,l+1);
        for (size_t i=split(n,l); i<stop; ++i) {
            unsigned c=src[i]; ++counts[previous][c]; previous=c>>6;
        }
    }
    Model m[CONTEXTS];
    put32(dst,(uint32_t)n);
    size_t w=4;
    for (unsigned c=0; c<CONTEXTS; ++c) w+=model(counts[c],m+c,dst+w);
    uint8_t *lengths=dst+w;
    w+=LANES*4;
    for (unsigned l=0; l<LANES; ++l) {
        unsigned previous=0, fill=0, carry=0;
        size_t start=w, stop=split(n,l+1);
        for (size_t i=split(n,l); i<stop; ++i) {
            unsigned c=src[i], bits=m[previous].bits[c];
            unsigned word=carry | ((unsigned)m[previous].code[c]<<fill);
            dst[w]=(uint8_t)word; dst[w+1]=(uint8_t)(word>>8);
            fill+=bits; w+=fill>>3;
            carry=word>>(fill & ~7u); fill &= 7;
            previous=c>>6;
        }
        if (fill) dst[w++]=(uint8_t)carry;
        put32(lengths+4*l,(uint32_t)(w-start));
        memset(dst+w,0,PADDING);
        w+=PADDING;
    }
    return w < n ? w : 0;
}

typedef struct {
    const uint8_t *stream;
    size_t limit, pos, output, end;
    unsigned context;
} Reader;

static inline unsigned symbol(Reader *r,const Model *m) {
    const uint8_t *p=r->stream+r->pos/8;
    unsigned v=(unsigned)p[0] | (unsigned)p[1]<<8;
    unsigned entry=m[r->context].decode[(v>>(r->pos%8))&511u];
    r->pos+=entry&15;
    r->context=entry>>10;
    return entry>>4;
}

int frontier_prefix_decode(const uint8_t *src, size_t n, uint8_t *dst, size_t raw) {
    if (n<4 || load32(src)!=raw || raw<16384) return -1;
    size_t h=4;
    Model m[CONTEXTS];
    for (unsigned c=0; c<CONTEXTS; ++c) {
        if (h==n) return -1;
        unsigned k=src[h++];
        if (k>85 || n-h<3*k) return -1;
        memset(m[c].bits,8,256);
        for (unsigned i=0; i<3*k; ++i) {
            unsigned s=src[h++];
            if (m[c].bits[s]!=8) return -1;
            m[c].bits[s]=i<k?7:9;
        }
        tables(m+c,k);
    }
    if (n-h<LANES*4) return -1;
    const uint8_t *lengths=src+h;
    h+=4*LANES;
    Reader r[LANES];
    for (unsigned l=0; l<LANES; ++l) {
        size_t bytes=load32(lengths+4*l);
        if (n-h<PADDING || bytes>n-h-PADDING || bytes>SIZE_MAX/8) return -1;
        r[l]=(Reader){src+h,bytes*8,0,split(raw,l),split(raw,l+1),0};
        h+=bytes+PADDING;
    }
    if (h!=n) return -1;
    uint16_t lookup[CONTEXTS * 512];
    for (unsigned c=0; c<CONTEXTS; ++c) memcpy(lookup+512*c,m[c].decode,sizeof(m[c].decode));
    for (;;) {
        int available=1;
        for (unsigned l=0; l<LANES; ++l)
            available &= r[l].end-r[l].output>=6 && r[l].limit-r[l].pos>=64;
        if (!available) break;
        uint64_t window[LANES];
        unsigned shift[LANES], context[LANES];
        for (unsigned l=0; l<LANES; ++l) {
            const uint8_t *p=r[l].stream+r[l].pos/8;
            window[l]=load32(p) | ((uint64_t)load32(p+4)<<32);
            shift[l]=r[l].pos%8;
            context[l]=r[l].context*512;
        }
        for (unsigned k=0; k<6; ++k) {
            for (unsigned l=0; l<LANES; ++l) {
                unsigned idx=(unsigned)(window[l]>>shift[l])&511;
                unsigned entry=lookup[context[l]+idx];
                dst[r[l].output+k]=(uint8_t)(entry>>4);
                context[l]=(entry&0xc00)>>1;
                shift[l]+=entry&15;
            }
        }
        for (unsigned l=0; l<LANES; ++l) {
            r[l].output+=6;
            r[l].pos=(r[l].pos&~(size_t)7)+shift[l];
            r[l].context=context[l]/512;
        }
    }
    for (unsigned l=0; l<LANES; ++l) {
        while (r[l].output<r[l].end) {
            if (r[l].pos>=r[l].limit) return -1;
            dst[r[l].output++]=(uint8_t)symbol(r+l,m);
            if (r[l].pos>r[l].limit) return -1;
        }
        if (r[l].limit-r[l].pos>=8) return -1;
    }
    return 0;
}
