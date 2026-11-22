/* Original implementation of bounded, independently predicted integer tiles.
 * Primitives are established mathematics: modular prediction, zigzag mapping,
 * fixed-width packing, sparse exceptions, CRC32C. No third-party codec code. */
#include "krc.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#elif defined(__SSE4_2__) && defined(__x86_64__)
#include <nmmintrin.h>
#endif

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void wr32(uint8_t *p,uint32_t v) { for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i)); }
static uint64_t rd64(const uint8_t *p) { return rd32(p) | (uint64_t)rd32(p+4)<<32; }
static void wr64(uint8_t *p,uint64_t v) { wr32(p,(uint32_t)v);wr32(p+4,(uint32_t)(v>>32)); }
static uint32_t crc(const uint8_t *p,size_t n) {
#if defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    uint32_t c=~0u;size_t i=0;
    for(;n-i>=8;i+=8)c=__crc32cd(c,rd64(p+i));
    for(;i<n;i++)c=__crc32cb(c,p[i]);return ~c;
#elif defined(__SSE4_2__) && defined(__x86_64__)
    uint64_t c=~0u;size_t i=0;
    for(;n-i>=8;i+=8)c=_mm_crc32_u64(c,rd64(p+i));
    for(;i<n;i++)c=_mm_crc32_u8((uint32_t)c,p[i]);return ~(uint32_t)c;
#else
    uint32_t table[256];
    for(uint32_t i=0;i<256;i++){uint32_t c=i;for(int b=0;b<8;b++)c=(c>>1)^(0x82f63b78u & (0u-(c&1)));table[i]=c;}
    uint32_t c=~0u;for(size_t i=0;i<n;i++)c=table[(c^p[i])&255]^(c>>8);return ~c;
#endif
}
size_t krc_plan_count(size_t n) { size_t words=n/4;return words/256+(words%256!=0); }
size_t krc_bound(size_t n) {
    size_t count=krc_plan_count(n);
    if(n>INT64_MAX || count>UINT32_MAX || n>SIZE_MAX-32 || count>(SIZE_MAX-32-n)/6)return 0;
    return n+32+count*6;
}
static uint32_t residue(const uint8_t *p,unsigned i,unsigned mode,unsigned stride) {
    uint32_t v=rd32(p+4*i),prev=i>=stride?rd32(p+4*(i-stride)):0;
    if(mode==1)return v;
    if(mode==4)return v^prev;
    uint32_t prediction=prev;
    if(mode==3){uint32_t prev2=i>=2*stride?rd32(p+4*(i-2*stride)):0;prediction=2*prev-prev2;}
    uint32_t r=v-prediction;return (r<<1)^(0u-(r>>31));
}
static unsigned bits(uint32_t v){return v?32u-(unsigned)__builtin_clz(v):0;}
static krc_plan choose(const uint8_t *p,unsigned words) {
    krc_plan best={0,0,0,0};unsigned best_cost=4*words;
    for(unsigned candidate=1;candidate<KRC_CANDIDATES;candidate++) {
        unsigned mode=candidate==1?1:2+(candidate-2)%3;
        unsigned stride=candidate==1?1:1u<<((candidate-2)/3);
        unsigned hist[33]={0};for(unsigned i=0;i<words;i++)hist[bits(residue(p,i,mode,stride))]++;
        unsigned exceptions=words;
        for(unsigned width=0;width<=32;width++) {
            exceptions-=hist[width];unsigned cost=(words*width+7)/8+5*exceptions;
            if(cost<best_cost){best_cost=cost;best=(krc_plan){mode,stride,width,exceptions};}
        }
    }
    return best;
}
typedef struct {const uint8_t *src;size_t words,count; krc_plan *plans;atomic_size_t next;} planning;
static void *plan_worker(void *arg) {
    planning *p=arg;
    for(;;){size_t begin=atomic_fetch_add(&p->next,8);if(begin>=p->count)break;
        size_t end=p->count-begin<8?p->count:begin+8;
        for(size_t tile=begin;tile<end;tile++){size_t left=p->words-tile*256;unsigned words=(unsigned)(left<256?left:256);p->plans[tile]=choose(p->src+tile*1024,words);}
    }return NULL;
}
int krc_plan_cpu(const uint8_t *src,size_t n,krc_plan *plans,size_t count,int threads) {
    size_t tiles=krc_plan_count(n);if(!krc_bound(n)||count<tiles||(!src&&n)||(!plans&&tiles))return KRC_ERR_PLAN;
    if(!tiles)return 0;
    if(threads<=0){long cores=sysconf(_SC_NPROCESSORS_ONLN);threads=cores>0?(cores<256?(int)cores:256):1;}
    if(threads>256)threads=256;size_t batches=tiles/8+(tiles%8!=0);if((size_t)threads>batches)threads=(int)batches;
    planning p={src,n/4,tiles,plans};atomic_init(&p.next,0);
    pthread_t helpers[255];int started=0;
    for(int i=1;i<threads;i++){if(pthread_create(&helpers[started],NULL,plan_worker,&p))break;started++;}
    plan_worker(&p);for(int i=0;i<started;i++)pthread_join(helpers[i],NULL);return 0;
}
static int valid_plan(krc_plan p) {
    if(p.mode==0)return p.stride==0&&p.width==0&&p.exceptions==0;
    return p.mode<=4 && p.width<=32 && p.exceptions<=256 && p.stride>=1&&p.stride<=32&&!(p.stride&(p.stride-1)) && (p.mode!=1||p.stride==1);
}
static int64_t encode_tiles(const uint8_t *src,size_t words,const krc_plan *plans,size_t tiles,uint8_t *dst) {
    size_t pos=0;
    for(size_t tile=0;tile<tiles;tile++) {
        unsigned m=(unsigned)(words-tile*256<256?words-tile*256:256);
        const uint8_t *input=src+tile*1024;krc_plan plan=plans[tile];uint32_t r[256];unsigned ex=0;
        if(!valid_plan(plan))return KRC_ERR_PLAN;
        if(plan.mode){for(unsigned i=0;i<m;i++){r[i]=residue(input,i,plan.mode,plan.stride);ex+=bits(r[i])>plan.width;}
            if(ex!=plan.exceptions)return KRC_ERR_PLAN;
            if((m*plan.width+7)/8+5*ex>=4*m)plan=(krc_plan){0,0,0,0};
        }
        uint8_t *h=dst+pos;h[0]=(uint8_t)plan.mode;h[1]=(uint8_t)plan.stride;h[2]=(uint8_t)plan.width;h[3]=0;h[4]=(uint8_t)plan.exceptions;h[5]=(uint8_t)(plan.exceptions>>8);pos+=6;
        if(!plan.mode){memcpy(dst+pos,input,m*4);pos+=m*4;continue;}
        unsigned packed=(m*plan.width+7)/8;memset(dst+pos,0,packed);
        uint64_t mask=plan.width==32?UINT32_MAX:((UINT64_C(1)<<plan.width)-1);
        uint64_t acc=0;unsigned have=0;size_t at=pos;
        for(unsigned i=0;i<m;i++){acc|=((uint64_t)r[i]&mask)<<have;have+=plan.width;
            while(have>=8){dst[at++]=(uint8_t)acc;acc>>=8;have-=8;}}
        if(have)dst[at++]=(uint8_t)acc;
        pos+=packed;
        for(unsigned i=0;i<m;i++)if(bits(r[i])>plan.width){dst[pos++]=(uint8_t)i;wr32(dst+pos,r[i]);pos+=4;}
    }
    return (int64_t)pos;
}
int64_t krc_encode(const uint8_t *src,size_t n,const krc_plan *plans,size_t count,uint8_t *dst,size_t cap) {
    size_t bound=krc_bound(n),tiles=krc_plan_count(n),words=n/4,pos=32;
    if(!bound||cap<bound||!dst)return KRC_ERR_CAPACITY;
    if((!src&&n)||count<tiles||(!plans&&tiles))return KRC_ERR_PLAN;
    int64_t used=encode_tiles(src,words,plans,tiles,dst+pos);
    if(used<0)return used;
    pos+=(size_t)used;
    size_t tail=n%4;if(tail){memcpy(dst+pos,src+n-tail,tail);pos+=tail;}
    memcpy(dst,"KRC1",4);dst[4]=1;dst[5]=0;dst[6]=8;dst[7]=0;wr64(dst+8,n);wr32(dst+16,(uint32_t)tiles);
    wr32(dst+20,crc(src,n));wr32(dst+24,crc(dst+32,pos-32));wr32(dst+28,crc(dst,28));return (int64_t)pos;
}
int64_t krc_compress(const uint8_t *src,size_t n,uint8_t *dst,size_t cap,int threads) {
    size_t count=krc_plan_count(n);if(!krc_bound(n)||cap<krc_bound(n))return KRC_ERR_CAPACITY;
    if(count>SIZE_MAX/sizeof(krc_plan))return KRC_ERR_ALLOC;
    krc_plan *plans=count?malloc(count*sizeof(*plans)):NULL;if(count&&!plans)return KRC_ERR_ALLOC;
    int r=krc_plan_cpu(src,n,plans,count,threads);
    int64_t result=r?r:krc_encode_threads(src,n,plans,count,dst,cap,threads);free(plans);return result;
}
int64_t krc_original_size(const uint8_t *src,size_t n) {
    if(n<32||!src||memcmp(src,"KRC1",4)||src[4]!=1||src[5]||src[6]!=8||src[7]||crc(src,28)!=rd32(src+28))return KRC_ERR_CORRUPT;
    uint64_t size=rd64(src+8);
    if(size>SIZE_MAX||size>INT64_MAX||!krc_bound((size_t)size)||krc_plan_count((size_t)size)!=rd32(src+16)||n>krc_bound((size_t)size))return KRC_ERR_CORRUPT;
    return (int64_t)size;
}
static int64_t decode_tiles(const uint8_t *src,size_t n,uint8_t *dst,size_t words,size_t tiles) {
    size_t pos=0;
    for(size_t tile=0;tile<tiles;tile++) {
        if(n-pos<6)return KRC_ERR_CORRUPT;
        krc_plan plan={src[pos],src[pos+1],src[pos+2],(uint32_t)src[pos+4]|(uint32_t)src[pos+5]<<8};
        if(src[pos+3]||!valid_plan(plan))return KRC_ERR_CORRUPT;pos+=6;
        unsigned m=(unsigned)(words-tile*256<256?words-tile*256:256);uint8_t *output=dst+tile*1024;
        if(!plan.mode){if(n-pos<m*4)return KRC_ERR_CORRUPT;memcpy(output,src+pos,m*4);pos+=m*4;continue;}
        unsigned packed=(m*plan.width+7)/8;
        if(plan.exceptions>m||packed+5*plan.exceptions>=4*m||n-pos<packed+5*plan.exceptions)return KRC_ERR_CORRUPT;
        uint32_t r[256];uint64_t mask=plan.width==32?UINT32_MAX:((UINT64_C(1)<<plan.width)-1),acc=0;unsigned have=0;size_t at=pos;
        for(unsigned i=0;i<m;i++){while(have<plan.width){acc|=(uint64_t)src[at++]<<have;have+=8;}r[i]=(uint32_t)(acc&mask);acc>>=plan.width;have-=plan.width;}
        pos+=packed;int last=-1;
        for(unsigned e=0;e<plan.exceptions;e++){unsigned i=src[pos++];uint32_t value=rd32(src+pos);pos+=4;
            if(i>=m||(int)i<=last||bits(value)<=plan.width||(value&mask)!=r[i])return KRC_ERR_CORRUPT;r[i]=value;last=(int)i;}
        for(unsigned i=0;i<m;i++){uint32_t value=r[i],prev=i>=plan.stride?rd32(output+4*(i-plan.stride)):0;
            if(plan.mode==4)value^=prev;
            else if(plan.mode>=2){uint32_t prediction=prev;if(plan.mode==3){uint32_t p2=i>=2*plan.stride?rd32(output+4*(i-2*plan.stride)):0;prediction=2*prev-p2;}value=((value>>1)^(0u-(value&1)))+prediction;}
            wr32(output+4*i,value);}
    }
    return (int64_t)pos;
}
int64_t krc_decompress(const uint8_t *src,size_t n,uint8_t *dst,size_t cap) {
    if(n<32||!src||memcmp(src,"KRC1",4)||src[4]!=1||src[5]||src[6]!=8||src[7]||crc(src,28)!=rd32(src+28))return KRC_ERR_CORRUPT;
    uint64_t size=rd64(src+8);if(size>SIZE_MAX||size>INT64_MAX)return KRC_ERR_CORRUPT;
    size_t bytes=(size_t)size,words=bytes/4,tiles=krc_plan_count(bytes),pos=32;
    if(tiles!=rd32(src+16)||!krc_bound(bytes)||n>krc_bound(bytes)||crc(src+32,n-32)!=rd32(src+24))return KRC_ERR_CORRUPT;
    if(bytes>cap||(!dst&&bytes))return KRC_ERR_CAPACITY;
    int64_t used=decode_tiles(src+pos,n-pos,dst,words,tiles);
    if(used<0)return used;
    pos+=(size_t)used;
    size_t tail=bytes%4;if(n-pos!=tail)return KRC_ERR_CORRUPT;if(tail)memcpy(dst+bytes-tail,src+pos,tail);
    if(crc(dst,bytes)!=rd32(src+20))return KRC_ERR_CORRUPT;return (int64_t)bytes;
}

#include "krc_parallel.inc"
