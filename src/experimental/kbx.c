/* Experimental KBX: exact KBC1 optimization (profile 0), tagged 2/3/4/6-byte
 * contexts (1), or smallest baseline/strong payload per block (2).
 * Established coding mathematics and credited lowbias32 primitive; see PROVENANCE.md. */
/* Written independently from integer interval-coding equations; no external codec.
 * This is an implementation experiment, not a claim of mathematical novelty. */
#include "kbx.h"
#include "krc.h"
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HASH_BITS 19
#define HASH_SIZE (1u << HASH_BITS)
#define MODEL_N (256u + 65536u + 2u * HASH_SIZE)
#define HALF UINT32_C(0x80000000)
#define QUARTER UINT32_C(0x40000000)
typedef uint16_t counts; /* zero in low byte, one in high byte; both <= 63 */
#define MODEL_STRONG_N (256u + 65536u + 4u * HASH_SIZE)
typedef struct {
    counts *table;
    uint16_t *tags;
    size_t entries;
    uint64_t history;
    uint32_t base2,base3,base4,base6;
    int strong;
    unsigned prefix;
} model;
static int reserve_model(model *m,int strong){
    size_t count=strong?MODEL_STRONG_N:MODEL_N;
    if(m->entries<count){counts *p=realloc(m->table,count*sizeof(counts));if(!p)return KRC_ERR_ALLOC;m->table=p;m->entries=count;}
    if(strong && !m->tags){m->tags=malloc(4u*HASH_SIZE*sizeof(uint16_t));if(!m->tags)return KRC_ERR_ALLOC;}
    return 0;
}
typedef struct {
    uint8_t *out;
    const uint8_t *in;
    size_t bytes, capacity, bits;
    int bad;
    unsigned accumulator;
} bitstream;

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void wr32(uint8_t *p, uint32_t x) { for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(8*i)); }
static uint64_t rd64(const uint8_t *p) { return rd32(p) | (uint64_t)rd32(p+4)<<32; }
static void wr64(uint8_t *p, uint64_t x) { wr32(p,(uint32_t)x);wr32(p+4,(uint32_t)(x>>32)); }
static uint32_t crc(const uint8_t *p, size_t n) {
    uint32_t table[256];
    for (uint32_t i=0;i<256;i++) {
        uint32_t c=i;
        for (unsigned j=0;j<8;j++) c=(c>>1)^(0x82f63b78u&(0u-(c&1)));
        table[i]=c;
    }
    uint32_t c=~0u;
    for (size_t i=0;i<n;i++) c=table[(c^p[i])&255]^(c>>8);
    return ~c;
}
static size_t blocks(size_t n) { return n/KBX_BLOCK_BYTES+(n%KBX_BLOCK_BYTES!=0); }
size_t kbx_bound(size_t n) {
    size_t count=blocks(n);
    if (n>INT64_MAX || n>SIZE_MAX-32 || count>UINT32_MAX || count>(SIZE_MAX-n-32)/12) return 0;
    return n+32+count*12;
}
static void putbit(bitstream *s, unsigned bit) {
    if (s->bits/8>=s->capacity) { s->bad=1;return; }
    s->accumulator=(s->accumulator<<1)|bit;s->bits++;
    if (!(s->bits&7)) s->out[(s->bits-1)/8]=(uint8_t)s->accumulator;
}
static unsigned getbit(bitstream *s) {
    if (!(s->bits&7)) {
        if (s->bits/8>=s->bytes) { s->bad=1;return 0; }
        s->accumulator=s->in[s->bits/8];
    }
    unsigned bit=(s->accumulator>>(7-(s->bits&7)))&1;
    s->bits++;return bit;
}
static void set_bases(model *m) {
    uint32_t low=(uint32_t)m->history;
    m->base2=(low&65535)*257u;m->base3=(low&0xffffff)*257u;
    if(m->strong){m->base4=low*257u;m->base6=(low^((uint32_t)(m->history>>32)*0x9e3779b9u))*257u;}
}
#ifdef KBX_RECIPROCAL
static uint32_t prob_reciprocal[4096];
static pthread_once_t prob_once=PTHREAD_ONCE_INIT;
static void probability_init(void){
    for(unsigned d=2;d<4096;d++)prob_reciprocal[d]=(uint32_t)(((UINT64_C(1)<<32)+d-1)/d);
}
#endif
static inline unsigned normalize_probability(unsigned zero,unsigned total){
    unsigned numerator=zero*4096u;
#ifdef KBX_RECIPROCAL
    unsigned estimate=(unsigned)(((uint64_t)numerator*prob_reciprocal[total])>>32);
    return estimate-(estimate*total>numerator);
#else
    return numerator/total;
#endif
}
static void reset(model *m, int strong) {
#ifdef KBX_RECIPROCAL
    pthread_once(&prob_once,probability_init);
#endif
    size_t count=strong?MODEL_STRONG_N:MODEL_N;
    for(size_t i=0;i<count;i++)m->table[i]=0x0101;
    if(strong)memset(m->tags,0,4u*HASH_SIZE*sizeof(uint16_t));
    m->history=0;m->prefix=1;m->strong=strong;set_bases(m);
}
static uint32_t mixhash(uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);
}
static unsigned probability(model *m, counts **contexts) {
    contexts[0]=m->table+m->prefix;
    contexts[1]=m->table+256+((unsigned)m->history&255)*256+m->prefix;
    unsigned models=m->strong?6:4;
    uint32_t bases[4]={m->base2,m->base3,m->base4,m->base6};
    for(unsigned i=2;i<models;i++){
        uint32_t h=mixhash(bases[i-2]+m->prefix),index=h&(HASH_SIZE-1);
        size_t slot=(size_t)(i-2)*HASH_SIZE+index;
        contexts[i]=m->table+256+65536+slot;
        if(m->strong && m->tags[slot]!=(uint16_t)(h>>HASH_BITS)){
            m->tags[slot]=(uint16_t)(h>>HASH_BITS);*contexts[i]=0x0101;
        }
    }
    unsigned zero=0,total=0;
    for(unsigned i=0;i<models;i++){
        unsigned v=*contexts[i],z=v&255,o=v>>8;
        if(m->strong && i>=2){z--;o--;}
        zero+=z<<i;total+=(z+o)<<i;
    }
    return normalize_probability(zero,total);
}
static void update(model *m, counts **contexts, unsigned bit) {
    unsigned models=m->strong?6:4,increment=bit?256:1;
    for(unsigned i=0;i<models;i++){
        unsigned value=*contexts[i]+increment;
        if((value&255)+(value>>8)>=64)value=((value+0x0101)>>1)&0x7f7f;
        *contexts[i]=(uint16_t)value;
    }
    m->prefix=2*m->prefix+bit;
    if(m->prefix>=256){m->history=((m->history<<8)|(m->prefix&255))&UINT64_C(0xffffffffffff);m->prefix=1;set_bases(m);}
}
static void emit(bitstream *s, unsigned bit, size_t *pending) {
    putbit(s,bit);
    while (*pending) { putbit(s,bit^1);(*pending)--; }
}
static size_t arithmetic_encode(model *m, const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int strong) {
    bitstream s={.out=dst,.capacity=cap};uint32_t low=0,high=UINT32_MAX;size_t pending=0;
    reset(m,strong);
    for (size_t i=0;i<n;i++) for (int b=7;b>=0;b--) {
        counts *contexts[6];unsigned p=probability(m,contexts),bit=(src[i]>>b)&1;
        uint32_t split=low+(uint32_t)((((uint64_t)high-low+1)*p)>>12)-1;
        if (bit) low=split+1;else high=split;
        update(m,contexts,bit);
        for (;;) {
            if ((low&HALF)==(high&HALF)) emit(&s,low>>31,&pending);
            else if ((low&QUARTER) && !(high&QUARTER)) { pending++;low-=QUARTER;high-=QUARTER; }
            else break;
            low<<=1;high=(high<<1)|1;
        }
        if (s.bad) return 0;
    }
    pending++;emit(&s,low<QUARTER?0:1,&pending);
    while (s.bits%8) putbit(&s,0);
    /* Explicit lookahead; the decoder never reads past this stream. */
    for (unsigned i=0;i<32;i++) putbit(&s,0);
    return s.bad?0:s.bits/8;
}
/* Inclusive-interval arithmetic, normalized by common leading bytes.
 * A point interval emits four bytes and expands before the next symbol,
 * so each branch always has at least one state. All operations are unsigned. */
static size_t byte_encode(model *m,const uint8_t *src,size_t n,uint8_t *dst,size_t cap) {
    uint32_t low=0,high=UINT32_MAX;size_t at=0;reset(m,1);
    for(size_t i=0;i<n;i++)for(int b=7;b>=0;b--){
        counts *contexts[6];unsigned p=probability(m,contexts),bit=(src[i]>>b)&1;
        uint32_t split=low+(uint32_t)(((uint64_t)(high-low)*p)>>12);
        if(bit)low=split+1;else high=split;update(m,contexts,bit);
        while((low^high)<UINT32_C(0x01000000)){
            if(at>=cap)return 0;dst[at++]=(uint8_t)(high>>24);low<<=8;high=(high<<8)|255;
        }
    }
    if(cap-at<4)return 0;
    for(unsigned i=0;i<4;i++){dst[at++]=(uint8_t)(low>>24);low<<=8;}
    return at;
}
static int byte_decode(model *m,const uint8_t *src,size_t n,uint8_t *dst,size_t size){
    if(n<4)return KRC_ERR_CORRUPT;
    uint32_t low=0,high=UINT32_MAX,code=0;size_t at=4;reset(m,1);
    for(unsigned i=0;i<4;i++)code=(code<<8)|src[i];
    for(size_t i=0;i<size;i++){
        unsigned value=0;
        for(unsigned b=0;b<8;b++){
            counts *contexts[6];unsigned p=probability(m,contexts);
            uint32_t split=low+(uint32_t)(((uint64_t)(high-low)*p)>>12);
            unsigned bit=code>split;if(bit)low=split+1;else high=split;
            value=2*value+bit;update(m,contexts,bit);
            while((low^high)<UINT32_C(0x01000000)){
                if(at>=n)return KRC_ERR_CORRUPT;
                low<<=8;high=(high<<8)|255;code=(code<<8)|src[at++];
            }
        }
        dst[i]=(uint8_t)value;
    }
    return 0;
}
static int arithmetic_decode(model *m, const uint8_t *src, size_t n, uint8_t *dst, size_t size, int strong) {
    bitstream s={.in=src,.bytes=n};uint32_t low=0,high=UINT32_MAX,code=0;
    reset(m,strong);
    for (unsigned i=0;i<32;i++) code=(code<<1)|getbit(&s);
    for (size_t i=0;i<size;i++) {
        unsigned byte=0;
        for (unsigned b=0;b<8;b++) {
            counts *contexts[6];unsigned p=probability(m,contexts);
            uint32_t split=low+(uint32_t)((((uint64_t)high-low+1)*p)>>12)-1;
            unsigned bit=code>split;
            if (bit) low=split+1;else high=split;
            byte=2*byte+bit;update(m,contexts,bit);
            for (;;) {
                if ((low&HALF)==(high&HALF)) { /* Common leading bit. */ }
                else if ((low&QUARTER) && !(high&QUARTER)) { code-=QUARTER;low-=QUARTER;high-=QUARTER; }
                else break;
                low<<=1;high=(high<<1)|1;code=(code<<1)|getbit(&s);
            }
            if (s.bad) return KRC_ERR_CORRUPT;
        }
        dst[i]=(uint8_t)byte;
    }
    return 0;
}

typedef struct {
    const uint8_t *src;
    uint8_t *dst;
    size_t n,count,base;
    size_t *offsets;
    int decode,profile;
    atomic_size_t next;
    atomic_int error;
} job;
static void *worker(void *arg) {
    job *j=arg;model m={0};uint8_t *candidate=NULL,*residual=NULL;
    for (;;) {
        size_t b=atomic_fetch_add(&j->next,1);
        if (b>=j->count || atomic_load(&j->error)) break;
        size_t orig=j->n-b*KBX_BLOCK_BYTES;if (orig>KBX_BLOCK_BYTES) orig=KBX_BLOCK_BYTES;
        if (j->decode) {
            const uint8_t *h=j->src+32+b*12,*payload=j->src+j->offsets[b];
            uint8_t *out=j->dst+b*KBX_BLOCK_BYTES;size_t packed=rd32(h+8);int status=0;
            if (h[0]==0) memcpy(out,payload,orig);
            else if (h[0]==1) status=krc_decompress(payload,packed,out,orig)==(int64_t)orig?0:KRC_ERR_CORRUPT;
            else {
                int allocated=reserve_model(&m,h[0]==3);
                status=!allocated?(h[0]==3?byte_decode(&m,payload,packed,out,orig):arithmetic_decode(&m,payload,packed,out,orig,0)):KRC_ERR_ALLOC;
            }
            if (status) atomic_store(&j->error,status);
        } else {
            uint8_t *header=j->dst+32+b*12;
            int allocated=reserve_model(&m,j->profile!=0);
            if (!candidate) candidate=malloc(KBX_BLOCK_BYTES);
            if (!residual) residual=malloc(krc_bound(KBX_BLOCK_BYTES));
            if (allocated || !candidate || !residual) { atomic_store(&j->error,KRC_ERR_ALLOC);break; }
            const uint8_t *in=j->src+b*KBX_BLOCK_BYTES;
            uint8_t *out=j->dst+j->base+b*KBX_BLOCK_BYTES;
            size_t best=orig;unsigned mode=0;memcpy(out,in,orig);
            int64_t r=krc_compress(in,orig,residual,krc_bound(KBX_BLOCK_BYTES),1);
            if (r<0) { atomic_store(&j->error,(int)r);break; }
            if ((size_t)r<best) { best=(size_t)r;mode=1;memcpy(out,residual,best); }
            if (j->profile!=1) {
                size_t coded=arithmetic_encode(&m,in,orig,candidate,best,0);
                if (coded && coded<best) { best=coded;mode=2;memcpy(out,candidate,best); }
            }
            if (j->profile!=0) {
                size_t coded=byte_encode(&m,in,orig,candidate,best);
                if (coded && coded<best) { best=coded;mode=3;memcpy(out,candidate,best); }
            }
            memset(header,0,12);header[0]=(uint8_t)mode;wr32(header+4,(uint32_t)orig);wr32(header+8,(uint32_t)best);
        }
    }
    free(m.table);free(m.tags);free(candidate);free(residual);return NULL;
}
static int execute(job *j,int threads) {
    if (!j->count) return 0;
    if (threads<=0) { long n=sysconf(_SC_NPROCESSORS_ONLN);threads=n>0?(n<64?(int)n:64):1; }
    if (threads>64) threads=64;
    if ((size_t)threads>j->count) threads=(int)j->count;
    pthread_t helpers[63];unsigned started=0;
    atomic_init(&j->next,0);atomic_init(&j->error,0);
    for (int i=1;i<threads;i++) {
        if (pthread_create(&helpers[started],NULL,worker,j)) break;
        started++;
    }
    worker(j);
    for (unsigned i=0;i<started;i++) pthread_join(helpers[i],NULL);
    return atomic_load(&j->error);
}
int64_t kbx_compress(const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads,int profile) {
    if (profile<0 || profile>2) return KRC_ERR_PLAN;
    size_t bound=kbx_bound(n),count=blocks(n),base=32+12*count;
    if (!bound || cap<bound || !dst) return KRC_ERR_CAPACITY;
    if (!src && n) return KRC_ERR_PLAN;
    job j={.src=src,.dst=dst,.n=n,.count=count,.base=base,.profile=profile};int result=execute(&j,threads);
    if (result) return result;
    size_t pos=base;
    for (size_t b=0;b<count;b++) {
        size_t packed=rd32(dst+32+b*12+8);
        memmove(dst+pos,dst+base+b*KBX_BLOCK_BYTES,packed);pos+=packed;
    }
    memcpy(dst,profile?"KBX3":"KBC1",4);dst[4]=profile?3:1;dst[5]=18;dst[6]=dst[7]=0;
    wr64(dst+8,n);wr32(dst+16,(uint32_t)count);wr32(dst+20,crc(src,n));
    wr32(dst+24,crc(dst+32,pos-32));wr32(dst+28,crc(dst,28));return (int64_t)pos;
}
int64_t kbx_original_size(const uint8_t *src,size_t n) {
    if (!src || n<32 || (memcmp(src,"KBC1",4) && memcmp(src,"KBX3",4)) || src[4]!=(memcmp(src,"KBC1",4)?3:1) || src[5]!=18 || src[6] || src[7] || crc(src,28)!=rd32(src+28)) return KRC_ERR_CORRUPT;
    uint64_t original=rd64(src+8);
    if (original>SIZE_MAX || original>INT64_MAX || !kbx_bound((size_t)original) || n>kbx_bound((size_t)original) || blocks((size_t)original)!=rd32(src+16)) return KRC_ERR_CORRUPT;
    return (int64_t)original;
}
int64_t kbx_decompress(const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads) {
    int64_t original=kbx_original_size(src,n);
    if (original<0) return original;
    size_t size=(size_t)original,count=blocks(size),base=32+12*count;
    if (size>cap || (!dst && size)) return KRC_ERR_CAPACITY;
    if (base>n || crc(src+32,n-32)!=rd32(src+24)) return KRC_ERR_CORRUPT;
    size_t *offsets=count?malloc(count*sizeof(*offsets)):NULL;
    if (count && !offsets) return KRC_ERR_ALLOC;
    size_t pos=base;int result=0;
    for (size_t b=0;b<count;b++) {
        const uint8_t *h=src+32+b*12;size_t orig=size-b*KBX_BLOCK_BYTES,packed=rd32(h+8);
        if (orig>KBX_BLOCK_BYTES) orig=KBX_BLOCK_BYTES;
        if (h[0]>3 || (h[0]==3 && memcmp(src,"KBX3",4)) || h[1] || h[2] || h[3] || rd32(h+4)!=orig || !packed || packed>orig || packed>n-pos || (h[0]==0 && packed!=orig) || (h[0]!=0 && packed>=orig)) { result=KRC_ERR_CORRUPT;break; }
        offsets[b]=pos;pos+=packed;
    }
    if (!result && pos!=n) result=KRC_ERR_CORRUPT;
    if (!result) {
        job j={.src=src,.dst=dst,.n=size,.count=count,.base=base,.offsets=offsets,.decode=1};
        result=execute(&j,threads);
        if (!result && crc(dst,size)!=rd32(src+20)) result=KRC_ERR_CORRUPT;
    }
    free(offsets);return result?result:original;
}
