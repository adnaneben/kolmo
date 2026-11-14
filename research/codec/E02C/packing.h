/* Research-only scalar residual representations. Not a production format.
 * LSB-first bit packing; local blocks are always 128 values. */
#ifndef E02C_PACKING_H
#define E02C_PACKING_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#define EC_MAX_VALUES 1048576u
#define EC_LIMIT (32u << 20)
typedef struct { uint8_t *p; size_t n, cap; } ec_buf;
static void ec_free(ec_buf *b) { free(b->p); memset(b, 0, sizeof *b); }
static int ec_reserve(ec_buf *b, size_t add) {
    if (b->n > EC_LIMIT || add > EC_LIMIT - b->n) return -1;
    size_t need = b->n + add;
    if (need <= b->cap) return 0;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < need) cap = cap > EC_LIMIT / 2 ? EC_LIMIT : cap * 2;
    void *p = realloc(b->p, cap); if (!p) return -1;
    b->p = p; b->cap = cap; return 0;
}
static int ec_add(ec_buf *b, const void *p, size_t n) {
    if (ec_reserve(b,n)) return -1;
    if (n) memcpy(b->p+b->n,p,n); b->n+=n; return 0;
}
static int ec_byte(ec_buf *b, unsigned x) { uint8_t c=(uint8_t)x; return ec_add(b,&c,1); }
static size_t ec_vbytes(uint64_t v) { size_t n=1; while(v>=128){v>>=7;n++;} return n; }
static int ec_var(ec_buf *b,uint64_t v) {
    do { unsigned c=(unsigned)(v&127);v>>=7;if(ec_byte(b,c|(v?128:0)))return -1;}while(v);return 0;
}
static int ec_getvar(const uint8_t *p,size_t n,size_t *i,uint64_t *v) {
    uint64_t x=0;
    for(unsigned k=0;k<10;k++) {
        if(*i>=n)return -1;unsigned c=p[(*i)++];
        if(k==9 && c>1)return -1;
        x|=(uint64_t)(c&127)<<(7*k);
        if(!(c&128)){if(k && !(c&127))return -1;*v=x;return 0;}
    }return -1;
}
static unsigned ec_bits(uint64_t v){unsigned n=0;while(v){n++;v>>=1;}return n;}
static unsigned ec_width(const uint64_t *v,size_t n){uint64_t z=0;for(size_t i=0;i<n;i++)z|=v[i];return ec_bits(z);}
static int ec_putbits(ec_buf *o,const uint64_t *v,size_t n,unsigned bits) {
    if(bits>64 || n>EC_MAX_VALUES)return -1;
    size_t bytes=(n*bits+7)/8,base=o->n;
    if(ec_reserve(o,bytes))return -1;
    if(bytes)memset(o->p+base,0,bytes);o->n+=bytes;
    for(size_t i=0;i<n;i++){
        size_t pos=i*bits;unsigned left=bits,shift=0;
        while(left){unsigned off=(unsigned)(pos&7),take=8-off;if(take>left)take=left;
            unsigned mask=(1u<<take)-1;
            o->p[base+pos/8]|=(uint8_t)(((v[i]>>shift)&mask)<<off);
            pos+=take;shift+=take;left-=take;}
    }return 0;
}
static int ec_getbits(const uint8_t *p,size_t n,size_t *at,uint64_t *v,size_t count,unsigned bits) {
    if(bits>64 || count>EC_MAX_VALUES || *at>n)return -1;
    size_t bytes=(count*bits+7)/8;if(bytes>n-*at)return -1;
    const uint8_t *s=p+*at;
    if(bytes && ((count*bits)&7) && (s[bytes-1]>>((count*bits)&7)))return -1;
    for(size_t i=0;i<count;i++){
        uint64_t x=0;size_t pos=i*bits;unsigned left=bits,shift=0;
        while(left){unsigned off=(unsigned)(pos&7),take=8-off;if(take>left)take=left;
            x|=(uint64_t)((s[pos/8]>>off)&((1u<<take)-1))<<shift;
            pos+=take;shift+=take;left-=take;}v[i]=x;
    }*at+=bytes;return 0;
}
/* method 1 global bits, 2 local bits, 3 patched local bits, 4 local byte planes. */
static int ec_pack(const uint64_t *v,size_t n,unsigned method,ec_buf *o) {
    o->n=0;if(n>EC_MAX_VALUES || method<1 || method>4)return -1;
    if(method==1){unsigned b=ec_width(v,n);return ec_byte(o,b)||ec_putbits(o,v,n,b)?-1:0;}
    for(size_t at=0;at<n;at+=128){size_t m=n-at<128?n-at:128;unsigned b=ec_width(v+at,m);
        if(method==2){if(ec_byte(o,b)||ec_putbits(o,v+at,m,b))return -1;continue;}
        if(method==4){unsigned w=b<=8?1:b<=16?2:b<=32?4:8;
            if(ec_byte(o,w))return -1;
            for(unsigned j=0;j<w;j++)for(size_t i=0;i<m;i++)if(ec_byte(o,(unsigned)(v[at+i]>>(j*8))))return -1;
            continue;}
        size_t best=(m*b+7)/8+3;unsigned chosen=b;
        for(unsigned k=0;k<b;k++){size_t cost=(m*k+7)/8+3;
            for(size_t i=0;i<m;i++){uint64_t hi=v[at+i]>>k;if(hi)cost+=1+ec_vbytes(hi);}
            if(cost<best){best=cost;chosen=k;}}
        unsigned ne=0;if(chosen<64)for(size_t i=0;i<m;i++)ne+=(v[at+i]>>chosen)!=0;
        if(ec_byte(o,chosen)||ec_byte(o,ne&255)||ec_byte(o,ne>>8)||ec_putbits(o,v+at,m,chosen))return -1;
        if(chosen<64)for(size_t i=0;i<m;i++){uint64_t hi=v[at+i]>>chosen;
            if(hi && (ec_byte(o,(unsigned)i)||ec_var(o,hi)))return -1;}
    }return 0;
}
static int ec_unpack(const uint8_t *p,size_t n,size_t count,unsigned method,uint64_t *v) {
    size_t at=0;if(count>EC_MAX_VALUES || n>EC_LIMIT || method<1 || method>4)return -1;
    if(method==1){if(!n)return -1;unsigned b=p[at++];return ec_getbits(p,n,&at,v,count,b)||at!=n?-1:0;}
    for(size_t q=0;q<count;q+=128){size_t m=count-q<128?count-q:128;if(at>=n)return -1;unsigned b=p[at++];
        if(method==4){if(b!=1&&b!=2&&b!=4&&b!=8)return -1;if(m*b>n-at)return -1;
            for(size_t i=0;i<m;i++){uint64_t x=0;for(unsigned j=0;j<b;j++)x|=(uint64_t)p[at+j*m+i]<<(8*j);v[q+i]=x;}
            at+=m*b;continue;}
        unsigned ne=0;if(method==3){if(n-at<2)return -1;ne=(unsigned)p[at]|(unsigned)p[at+1]<<8;at+=2;if(ne>m||(b==64&&ne))return -1;}
        if(ec_getbits(p,n,&at,v+q,m,b))return -1;
        unsigned next=0;for(unsigned e=0;e<ne;e++){if(at>=n)return -1;unsigned pos=p[at++];uint64_t hi;
            if(pos<next||pos>=m||b>=64||ec_getvar(p,n,&at,&hi)||!hi||hi>(UINT64_MAX>>b))return -1;
            v[q+pos]|=hi<<b;next=pos+1;}
    }return at==n?0:-1;
}
#endif
