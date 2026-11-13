/* R3: one costed gate; no re-running both schedules; old wire readers/inverse.
 * R2 algorithms remain frozen in their original files. Build stages counters and
 * a separately reviewed rANS scratch-lifetime variant in a temporary directory. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
static size_t r3_huf_chunks, r3_rans_chunks, r3_singletons;
#include "track.h"
#include R3_R2_SOURCE
#include R3_SHARED_RANS
#include "gate.h"
typedef struct { result result;gate_result gate;size_t huf_chunks,rans_chunks,singletons,allocations,peak_heap,scratch_bytes; } r3_result;
static int candidate_encode(const sample *s,buffer *out,result *info,gate_result *gate,size_t *scratch_bytes) {
    *gate=choose_schedule(s->res.p,s->res.n,s->planes);*scratch_bytes=0;
    if(!gate->aligned) return encode(s,0,out,info);
    memset(info,0,sizeof *info);out->p=NULL;out->n=0;
    size_t n=s->res.n,cap=n*2+65536,plen=n/s->planes;
    size_t cw=(plen<RANS_CHUNK?plen:RANS_CHUNK)+64;
    uint8_t *trial=malloc(cap), *best=malloc(cap);uint32_t *scratch=malloc(cw*sizeof(*scratch));
    if(!trial||!best||!scratch){free(trial);free(best);free(scratch);return -1;}
    *scratch_bytes=cw*sizeof(*scratch);
    size_t best_n=n;unsigned code=0;memcpy(best,s->res.p,n);
    size_t z=split_encode(huf_compress_all,s->res.p,n,s->planes,trial,cap);info->calls++;
    if(!z) info->huf_failed++;
    else if(z<best_n){best_n=z;code=13;memcpy(best,trial,z);}
    size_t off=0;
    for(unsigned j=0;j<s->planes;j++){
        z=shared_rans_compress_all(s->res.p+(size_t)j*plen,plen,trial+off,cap-off,scratch,cw);
        if(!z||z>cap-off){off=0;break;}off+=z;
    }
    info->calls++;if(!off)info->rans_failed++;
    else if(off<best_n){best_n=off;code=14;memcpy(best,trial,off);}
    size_t overhead=64+s->pn+s->rn;out->n=overhead+best_n;out->p=malloc(out->n);
    if(!out->p){out->n=0;free(trial);free(best);free(scratch);return -1;}
    memcpy(out->p,s->source.p,overhead);out->p[6]=out->p[7]=(uint8_t)code;
    wr64(out->p+40,best_n);memcpy(out->p+overhead,best,best_n);wr32(out->p+60,frame_crc(out->p,out->n));
    info->codec=code;info->payload=best_n;info->frame=out->n;
    free(trial);free(best);free(scratch);return 0;
}
/* Policies: 0 original, 7 HRS, 8 strict safety, 9 bounded candidate, 10 duplicate original. */
static int policy_encode(const sample*s,unsigned policy,buffer*out,r3_result*r){
    memset(r,0,sizeof *r);r3_huf_chunks=r3_rans_chunks=r3_singletons=0;heap_reset();
    int status=policy==9?candidate_encode(s,out,&r->result,&r->gate,&r->scratch_bytes):encode(s,policy==10?0:policy,out,&r->result);
    r->huf_chunks=r3_huf_chunks;r->rans_chunks=r3_rans_chunks;r->singletons=r3_singletons;
    r->allocations=heap_calls;r->peak_heap=heap_peak-heap_origin;return status;
}
/* PREVALIDATED frame entropy kernel: no input checksum, integer inverse or frame
 * validation. Never exposed as the public malformed-input decoder. */
static int kernel_decode(const buffer*f,buffer*out){
    size_t n=(size_t)rd64(f->p+48),pn=(size_t)rd64(f->p+16),rn=(size_t)rd64(f->p+24),cn=(size_t)rd64(f->p+40);
    const uint8_t*p=f->p+64+pn+rn;out->p=malloc(n);out->n=n;if(!out->p)return -1;
    int rc=0;if(!f->p[7]){if(cn!=n)rc=-1;else memcpy(out->p,p,n);}
    else rc=(f->p[7]==13?huf_decompress_all:rans_decompress_all)(p,cn,out->p,n);
    if(rc)drop(out);return rc;
}
static const unsigned policies[5]={0,7,8,9,10};
static void shuffle5(unsigned*order){for(unsigned j=0;j<5;j++)order[j]=j;for(unsigned j=4;j;j--){unsigned k=(unsigned)(random64()%(j+1));unsigned t=order[k];order[k]=order[j];order[j]=t;}}
static int gate_tests(void){
    uint8_t *p=malloc(65536);if(!p)return 1;unsigned checked=0;
    memset(p,0,65536);if(choose_schedule(p,65536,1).aligned) return 2;checked++;
    if(choose_schedule(p,4096,4).sampled)return 2;checked++;
    if(choose_schedule(p,65536,4).aligned)return 2;checked++;
    for(unsigned k=0;k<4;k++)memset(p+k*16384,(int)k,16384);
    gate_result g=choose_schedule(p,65536,4);if(!g.aligned||g.sampled!=4096)return 2;checked++;
    for(unsigned k=0;k<4;k++)for(size_t i=0;i<16384;i++)p[k*16384+i]=(uint8_t)(i%256);
    if(choose_schedule(p,65536,4).aligned)return 2;checked++;
    for(unsigned k=0;k<4;k++)for(size_t i=0;i<16384;i++)p[k*16384+i]=(uint8_t)(random64()&255);
    if(choose_schedule(p,65536,3).sampled || choose_schedule(NULL,65536,4).sampled)return 2;checked+=2;
    free(p);printf("{\"gate_tests\":%u,\"status\":\"passed\"}\n",checked);return 0;
}
static int newtests(void){
    if(selftests()||gate_tests())return 1;
    /* Independently exercised sample and scratch boundary controls. */
    size_t sizes[]={1024,4096,8192,16384,131071,131072,131073,1048576,1048577};
    size_t checked=0;uint8_t *p=malloc(2*1048577),*a=malloc(5*1048577),*b=malloc(5*1048577);
    if(!p||!a||!b)return 2;
    for(unsigned j=0;j<sizeof sizes/sizeof sizes[0];j++)for(unsigned constant=0;constant<2;constant++){
        size_t n=sizes[j],cw=(n<RANS_CHUNK?n:RANS_CHUNK)+64;uint32_t*tmp=malloc(cw*4);if(!tmp)return 2;
        for(size_t i=0;i<n;i++)p[i]=constant?7:(uint8_t)(i*17+i/13);
        size_t za=review_rans_compress_all(p,n,a,5*1048577),zb=shared_rans_compress_all(p,n,b,5*1048577,tmp,cw);
        if(!za||za!=zb||memcmp(a,b,za)||check_decode(rans_decompress_all,p,n,b,zb))return 3;
        free(tmp);checked++;
    }
    free(p);free(a);free(b);
    if(heap_live)return 4;
    printf("{\"shared_scratch_identity_cases\":%zu,\"tracked_outstanding_bytes\":%zu,\"status\":\"passed\"}\n",checked,heap_live);return 0;
}
static int run_case(const char*input,const char*directory,unsigned rounds,uint64_t seed,int timing){
    sample s={0};buffer frames[5]={{0}};r3_result details[5];uint8_t*flush=NULL;int fail=1;state=seed;
    if(sample_load(input,&s))goto done;
    for(unsigned j=0;j<5;j++){
        buffer out={0},same={0};r3_result r;
        if(policy_encode(&s,policies[j],&frames[j],&details[j])||policy_encode(&s,policies[j],&same,&r)||same.n!=frames[j].n||memcmp(same.p,frames[j].p,same.n)){drop(&same);goto done;}drop(&same);
        heap_reset();if(decode(&frames[j],&out,NULL)||out.n!=s.raw.n||memcmp(out.p,s.raw.p,out.n)){drop(&out);goto done;}
        size_t decode_peak=heap_peak-heap_origin,decode_allocs=heap_calls;drop(&out);
        if(kernel_decode(&frames[j],&out)||out.n!=s.res.n||memcmp(out.p,s.res.p,out.n)){drop(&out);goto done;}drop(&out);
        char filename[32];snprintf(filename,sizeof filename,"policy-%u.frame",policies[j]);if(writebuf(directory,filename,&frames[j]))goto done;
        r3_result*rj=&details[j];printf("{\"type\":\"size\",\"policy\":%u,\"bytes\":%zu,\"actual_codec\":%u,\"planes\":%u,\"residual_bytes\":%zu,\"input_bytes\":%zu,\"aligned\":%u,\"sampled_bytes\":%u,\"estimated_saving\":%.9g,\"gate_margin\":%.9g,\"leaf_arms\":%u,\"huf_chunks\":%zu,\"rans_chunks\":%zu,\"singleton_chunks\":%zu,\"huf_failed\":%u,\"rans_failed\":%u,\"encode_tracked_allocations\":%zu,\"encode_tracked_peak_bytes\":%zu,\"decode_tracked_allocations\":%zu,\"decode_tracked_peak_bytes\":%zu,\"shared_scratch_bytes\":%zu}\n",policies[j],frames[j].n,rj->result.codec,s.planes,s.res.n,s.raw.n,rj->gate.aligned,rj->gate.sampled,rj->gate.predicted_bytes,rj->gate.margin_bytes,rj->result.calls,rj->huf_chunks,rj->rans_chunks,rj->singletons,rj->result.huf_failed,rj->result.rans_failed,rj->allocations,rj->peak_heap,decode_allocs,decode_peak,rj->scratch_bytes);
    }
    unsigned chosen=details[3].gate.aligned?1:0;
    if(frames[3].n!=frames[chosen].n||memcmp(frames[3].p,frames[chosen].p,frames[3].n)||frames[0].n!=frames[4].n||memcmp(frames[0].p,frames[4].p,frames[0].n)||frames[2].n>frames[0].n||frames[2].n>frames[1].n)goto done;
    if(writebuf(directory,"original.npy",&s.raw)||writebuf(directory,"residual.bin",&s.res))goto done;
    /* Fail each directly tracked candidate allocation in turn. Recovered coder
     * failures may select a valid fallback; all paths must clean their ownership. */
    size_t live=heap_live;unsigned allocchecks=0;
    if(!timing)for(long f=0;f<(long)details[3].allocations+2;f++){
        buffer out={0};r3_result r;heap_fail_after=f;int rc=policy_encode(&s,9,&out,&r);heap_fail_after=-1;
        if(!rc){buffer decoded={0};if(decode(&out,&decoded,NULL)||decoded.n!=s.raw.n||memcmp(decoded.p,s.raw.p,s.raw.n)){drop(&decoded);drop(&out);goto done;}drop(&decoded);}drop(&out);
        if(heap_live!=live)goto done;allocchecks++;
    }
    if(timing){
        unsigned batch=(unsigned)((1048576+s.raw.n-1)/s.raw.n);if(batch>128)batch=128;if(!batch)batch=1;unsigned order[5];shuffle5(order);
        for(unsigned round=0;round<rounds;round++)for(unsigned k=0;k<5;k++){
            unsigned j=order[(round+k)%5];uint64_t c0=ns(CLOCK_PROCESS_CPUTIME_ID),t0=ns(CLOCK_MONOTONIC);
            for(unsigned b=0;b<batch;b++){buffer out={0};r3_result r;if(policy_encode(&s,policies[j],&out,&r))goto done;consumed^=out.n;drop(&out);}
            uint64_t t1=ns(CLOCK_MONOTONIC),c1=ns(CLOCK_PROCESS_CPUTIME_ID);
            printf("{\"type\":\"timing\",\"regime\":\"encode\",\"policy\":%u,\"round\":%u,\"batch\":%u,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n",policies[j],round,batch,(unsigned long long)(t1-t0),(unsigned long long)(c1-c0));
        }
        flush=malloc(32u<<20);if(!flush)goto done;memset(flush,1,32u<<20);
        for(unsigned regime=0;regime<4;regime++){
            int kernel=regime>=2,displaced=regime&1;shuffle5(order);
            for(unsigned round=0;round<rounds;round++)for(unsigned k=0;k<5;k++){
                unsigned j=order[(round+k)%5],nb=displaced?1:batch;
                if(displaced)displace(flush,32u<<20);else{buffer o={0};int rc=kernel?kernel_decode(&frames[j],&o):decode(&frames[j],&o,NULL);drop(&o);if(rc)goto done;}
                uint64_t c0=ns(CLOCK_PROCESS_CPUTIME_ID),t0=ns(CLOCK_MONOTONIC);
                for(unsigned b=0;b<nb;b++){buffer o={0};int rc=kernel?kernel_decode(&frames[j],&o):decode(&frames[j],&o,NULL);if(rc){drop(&o);goto done;}consumed^=o.n+(o.n?o.p[o.n-1]:0);drop(&o);}
                uint64_t t1=ns(CLOCK_MONOTONIC),c1=ns(CLOCK_PROCESS_CPUTIME_ID);
                printf("{\"type\":\"timing\",\"regime\":\"decode_%s_%s\",\"policy\":%u,\"round\":%u,\"batch\":%u,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n",kernel?"kernel":"verified",displaced?"displaced32MiB":"warm",policies[j],round,nb,(unsigned long long)(t1-t0),(unsigned long long)(c1-c0));
            }
        }
    }
    printf("{\"type\":\"verification\",\"candidate_control_identity\":true,\"allocation_failure_positions\":%u}\n",allocchecks);fail=0;
 done:heap_fail_after=-1;free(flush);for(unsigned j=0;j<5;j++)drop(&frames[j]);sample_drop(&s);if(heap_live){fprintf(stderr,"tracked leak %zu\n",heap_live);fail=1;}return fail;
}
int main(int argc,char**argv){
    crc_init();if(argc==2&&!strcmp(argv[1],"test"))return newtests();
    if(argc!=7||strcmp(argv[1],"bench")){fprintf(stderr,"usage: probe test | bench BASE_FRAME DIR ROUNDS SEED TIMING\n");return 2;}
    char*e;errno=0;unsigned long r=strtoul(argv[4],&e,10);if(errno||*e||!r||r>31)return 2;uint64_t seed=strtoull(argv[5],&e,10);if(*e||!seed)return 2;
    if(strcmp(argv[6],"0")&&strcmp(argv[6],"1"))return 2;
    return run_case(argv[2],argv[3],(unsigned)r,seed,argv[6][0]=='1');
}
