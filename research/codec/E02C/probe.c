/* E02C full-source numeric test adapter. Never linked into production Kolmo.
 * build.py inserts ONE numeric serializer interception in a generated full-source
 * translation unit; KG_INT prediction and all fixed leaf coders remain original.
 * Forced numeric interpretation, single-threaded, <=4 MiB complete input. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
static int e02c_capture_enabled;
#include E02C_CORE
#include "packing.h"
#define EC_INPUT (4u << 20)
#define EC_HEADER 64u
static ec_buf ec_par, ec_res, ec_rest;
static uint64_t ec_ns(clockid_t id){struct timespec t;if(clock_gettime(id,&t))abort();return (uint64_t)t.tv_sec*1000000000u+(unsigned long)t.tv_nsec;}
static void ec_u64(uint8_t*p,uint64_t v){for(unsigned j=0;j<8;j++)p[j]=(uint8_t)(v>>(j*8));}
static uint64_t ec_r64(const uint8_t*p){uint64_t v=0;for(unsigned j=0;j<8;j++)v|=(uint64_t)p[j]<<(8*j);return v;}
static void ec_u32(uint8_t*p,uint32_t v){for(unsigned j=0;j<4;j++)p[j]=(uint8_t)(v>>(j*8));}
static uint32_t ec_r32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void ec_clear_capture(void){ec_free(&ec_par);ec_free(&ec_res);ec_free(&ec_rest);}
static size_t e02c_capture_num(const kgb *g,const kbuf *par){
    if(g->ns!=3||g->nodes!=1)return 0;
    if(ec_add(&ec_par,par->p,par->n)||ec_add(&ec_res,g->s[1].p,g->s[1].n)||ec_add(&ec_rest,g->s[2].p,g->s[2].n))return 0;
    return 1;
}
static int ec_capture(const uint8_t *src,size_t n,unsigned width,size_t head){
    if((width!=2&&width!=4&&width!=8)||n>EC_INPUT||head>n||(n-head)/width<16||(n-head)/width>EC_MAX_VALUES)return -1;
    ec_clear_capture();codec_ctx ctx={0};uint8_t unused=0;e02c_capture_enabled=1;
    size_t ok=kg_encode_num(&ctx,C_STORE,1,src,n,(int)width,head,&unused,1);
    e02c_capture_enabled=0;codec_ctx_free(&ctx);return ok==1?0:-1;
}
typedef struct {unsigned width,ow,planes,order;size_t count,values,head;} ec_meta;
static int ec_params(const uint8_t*p,size_t n,size_t rest,size_t raw,size_t residual,ec_meta*m){
    if(n<3||n>128||raw>EC_INPUT||rest>raw)return -1;
    unsigned fl=p[1];m->width=p[0];m->ow=p[2];m->order=(fl>>1)&3;m->planes=(fl>>3)&1;
    if((m->width!=2&&m->width!=4&&m->width!=8)||(m->ow!=1&&m->ow!=2&&m->ow!=4&&m->ow!=8)||fl>15||m->order>2)return -1;
    size_t at=3;uint64_t head,count,g,bias,seed;
    if(ec_getvar(p,n,&at,&head)||ec_getvar(p,n,&at,&count)||ec_getvar(p,n,&at,&g)||ec_getvar(p,n,&at,&bias)||!g)return -1;
    for(unsigned j=0;j<m->order;j++)if(ec_getvar(p,n,&at,&seed))return -1;
    if(at!=n||head>rest||count<m->order||count>EC_MAX_VALUES||count>(raw-rest)/m->width||count*m->width!=raw-rest)return -1;
    if((count-m->order)*m->ow!=residual)return -1;
    m->head=(size_t)head;m->count=(size_t)count;m->values=(size_t)count-m->order;return 0;
}
static void ec_values(const uint8_t*p,const ec_meta*m,uint64_t*v){
    for(size_t i=0;i<m->values;i++){uint64_t x=0;for(unsigned j=0;j<m->ow;j++)x|=(uint64_t)p[m->planes?j*m->values+i:i*m->ow+j]<<(8*j);v[i]=x;}
}
static int ec_layout(const uint64_t*v,const ec_meta*m,ec_buf*o){
    size_t n=m->values*m->ow;o->n=0;if(ec_reserve(o,n))return -1;o->n=n;
    for(size_t i=0;i<m->values;i++){
        if(m->ow<8&&(v[i]>>(8*m->ow)))return -1;
        for(unsigned j=0;j<m->ow;j++)o->p[m->planes?j*m->values+i:i*m->ow+j]=(uint8_t)(v[i]>>(8*j));
    }return 0;
}
static int ec_coder(unsigned c){return c==C_STORE||c==C_HUF||c==C_RANS||c==C_ZSTD;}
static uint32_t ec_frame_crc(const uint8_t*p,size_t n){uint32_t c=kolmo_crc32c(0,p,60);return kolmo_crc32c(c,p+64,n-64);}
static int ec_encode(const uint8_t*src,size_t n,unsigned width,size_t head,unsigned method,unsigned coder,ec_buf*frame,uint64_t*kernel){
    int status=-1;ec_buf rep={0},coded={0};uint64_t *v=NULL;codec_ctx ctx={0};ec_meta m;
    frame->n=0;if(method>4||!ec_coder(coder)||ec_capture(src,n,width,head)||ec_params(ec_par.p,ec_par.n,ec_rest.n,n,ec_res.n,&m))goto done;
    uint64_t start=ec_ns(CLOCK_MONOTONIC);
    if(method==0){if(ec_add(&rep,ec_res.p,ec_res.n))goto done;}
    else {v=malloc((m.values?m.values:1)*sizeof(*v));if(!v)goto done;ec_values(ec_res.p,&m,v);if(ec_pack(v,m.values,method,&rep))goto done;}
    if(rep.n>EC_LIMIT/4||ec_reserve(&coded,rep.n*2+8192))goto done;
    size_t z=coder?codec_compress(&ctx,(int)coder,1,rep.p,rep.n,coded.p,coded.cap):0;unsigned actual=coder;
    if(!z||z>=rep.n){actual=C_STORE;z=rep.n;if(z)memcpy(coded.p,rep.p,z);}coded.n=z;
    *kernel=ec_ns(CLOCK_MONOTONIC)-start;
    uint8_t hdr[EC_HEADER]={0};memcpy(hdr,"E2C1",4);hdr[4]=1;hdr[5]=(uint8_t)method;hdr[6]=(uint8_t)coder;hdr[7]=(uint8_t)actual;
    ec_u64(hdr+8,n);ec_u64(hdr+16,ec_par.n);ec_u64(hdr+24,ec_rest.n);ec_u64(hdr+32,rep.n);ec_u64(hdr+40,z);ec_u64(hdr+48,ec_res.n);ec_u32(hdr+56,kolmo_crc32c(0,src,n));
    if(ec_add(frame,hdr,64)||ec_add(frame,ec_par.p,ec_par.n)||ec_add(frame,ec_rest.p,ec_rest.n)||ec_add(frame,coded.p,z))goto done;
    ec_u32(frame->p+60,ec_frame_crc(frame->p,frame->n));status=0;
 done:free(v);ec_free(&rep);ec_free(&coded);codec_ctx_free(&ctx);return status;
}
static int ec_decode(const uint8_t*f,size_t fn,ec_buf*out,ec_buf*debugrep,uint64_t*kernel){
    int status=-1;ec_buf rep={0},res={0};uint64_t*v=NULL;codec_ctx ctx={0};kbuf restored={0};ec_meta m;
    out->n=0;if(fn<64||fn>EC_LIMIT||memcmp(f,"E2C1",4)||f[4]!=1||f[5]>4||!ec_coder(f[6])||!ec_coder(f[7])||(f[7]&&f[7]!=f[6]))goto done;
    if(ec_r32(f+60)!=ec_frame_crc(f,fn))goto done;
    uint64_t raw=ec_r64(f+8),pn=ec_r64(f+16),rn=ec_r64(f+24),replen=ec_r64(f+32),cn=ec_r64(f+40),reslen=ec_r64(f+48);
    if(raw>EC_INPUT||pn>128||rn>raw||replen>EC_LIMIT/4||reslen>EC_MAX_VALUES*8u||cn>EC_LIMIT/2)goto done;
    if(pn>fn-64||rn>fn-64-pn||cn!=fn-64-pn-rn)goto done;
    if(ec_params(f+64,(size_t)pn,(size_t)rn,(size_t)raw,(size_t)reslen,&m))goto done;
    const uint8_t *payload=f+64+pn+rn;
    uint64_t start=ec_ns(CLOCK_MONOTONIC);
    if(ec_reserve(&rep,(size_t)replen+1))goto done;rep.n=(size_t)replen;
    if(codec_decompress(&ctx,f[7],payload,(size_t)cn,rep.p,rep.n))goto done;
    if(f[5]==0){if(rep.n!=reslen||ec_add(&res,rep.p,rep.n))goto done;}
    else {v=malloc((m.values?m.values:1)*8);if(!v||ec_unpack(rep.p,rep.n,m.values,f[5],v)||ec_layout(v,&m,&res))goto done;}
    *kernel=ec_ns(CLOCK_MONOTONIC)-start;
    kbuf kr={res.p,res.n,res.cap},rest={(uint8_t*)f+64+pn,(size_t)rn,(size_t)rn};
    if(kg_int_inv(f+64,(size_t)pn,&kr,&rest,(size_t)raw,&restored)||restored.n!=raw||kolmo_crc32c(0,restored.p,restored.n)!=ec_r32(f+56))goto done;
    if(debugrep && ec_add(debugrep,rep.p,rep.n))goto done;
    if(ec_add(out,restored.p,restored.n))goto done;status=0;
 done:free(v);ec_free(&rep);ec_free(&res);free(restored.p);codec_ctx_free(&ctx);return status;
}
static int ec_read(const char*path,ec_buf*b,size_t max){FILE*f=fopen(path,"rb");if(!f)return -1;uint8_t tmp[8192];size_t n;
    while((n=fread(tmp,1,sizeof tmp,f))){if(b->n>max||n>max-b->n||ec_add(b,tmp,n)){fclose(f);return -1;}}
    int bad=ferror(f);if(fclose(f))bad=1;return bad?-1:0;}
static int ec_write(const char*prefix,const char*suffix,const void*p,size_t n){char path[4096];int k=snprintf(path,sizeof path,"%s%s",prefix,suffix);if(k<0||(size_t)k>=sizeof path)return -1;
    FILE*f=fopen(path,"wb");if(!f)return -1;int bad=n&&fwrite(p,1,n,f)!=n;if(fclose(f))bad=1;return bad?-1:0;}
static int ec_arg(const char*s,unsigned long long*v){char*end;errno=0;if(!s[0]||s[0]=='-')return -1;*v=strtoull(s,&end,10);return errno||*end?-1:0;}
static int ec_huf_modes(const uint8_t*p,size_t n,unsigned modes[3]){size_t i=0;while(i<n){unsigned mode=p[i++];uint64_t m,z;if(mode>2||ec_getvar(p,n,&i,&m)||!m)return -1;modes[mode]++;
    if(mode==2){if(ec_getvar(p,n,&i,&z)||z>n-i)return -1;i+=(size_t)z;}else{z=mode==1?1:m;if(z>n-i)return -1;i+=(size_t)z;}}return 0;}
static uint64_t ec_rand_state=731094;
static uint64_t ec_rand(void){ec_rand_state^=ec_rand_state<<13;ec_rand_state^=ec_rand_state>>7;ec_rand_state^=ec_rand_state<<17;return ec_rand_state;}
static int ec_test(void){ec_buf frame={0},out={0};uint64_t t;size_t tests=0,mut=0,trunc=0;uint8_t src[3+513*8+1];
    unsigned coders[]={0,13,14,3};
    for(unsigned width=2;width<=8;width*=2)for(unsigned kind=0;kind<4;kind++){
        size_t n=3+513*width+1;memcpy(src,"HDR",3);src[n-1]=17;
        for(size_t i=0;i<513;i++){uint64_t x=kind==0?0:kind==1?i*1000+(ec_rand()&7):kind==2?(i%127?ec_rand()&7:ec_rand()):ec_rand();for(unsigned b=0;b<width;b++)src[3+i*width+b]=(uint8_t)(x>>(8*b));}
        for(unsigned method=0;method<5;method++)for(unsigned c=0;c<4;c++){
            if(ec_encode(src,n,width,3,method,coders[c],&frame,&t)||ec_decode(frame.p,frame.n,&out,NULL,&t)||out.n!=n||memcmp(src,out.p,n))return 1;tests++;
            for(size_t k=0;k<frame.n;k+=frame.n/17+1){if(!ec_decode(frame.p,k,&out,NULL,&t))return 2;trunc++;}
            for(unsigned k=0;k<24;k++){size_t at=ec_rand()%frame.n;uint8_t saved=frame.p[at];uint32_t crc=ec_r32(frame.p+60);frame.p[at]^=(uint8_t)(1+ec_rand()%255);
                if(k&1)ec_u32(frame.p+60,ec_frame_crc(frame.p,frame.n));
                int r=ec_decode(frame.p,frame.n,&out,NULL,&t);if(!r&&(out.n!=n||memcmp(out.p,src,n)))return 3;
                frame.p[at]=saved;ec_u32(frame.p+60,crc);mut++;}
        }
    }ec_free(&frame);ec_free(&out);ec_clear_capture();printf("E02C_TEST roundtrips=%zu truncations=%zu mutations=%zu\n",tests,trunc,mut);return 0;}
int main(int argc,char**argv){
    if(argc==2&&!strcmp(argv[1],"test"))return ec_test();
    if(argc==4&&!strcmp(argv[1],"decode")){ec_buf f={0},o={0};uint64_t t=0;int fail=ec_read(argv[2],&f,EC_LIMIT)||ec_decode(f.p,f.n,&o,NULL,&t)||ec_write(argv[3],"",o.p,o.n);ec_free(&f);ec_free(&o);return fail?1:0;}
    if(argc!=9||strcmp(argv[1],"bench")){fprintf(stderr,"usage: probe test | decode FRAME OUT | bench INPUT WIDTH HEAD METHOD CODER REPEATS PREFIX\n");return 2;}
    unsigned long long width,head,method,coder,reps;
    if(ec_arg(argv[3],&width)||ec_arg(argv[4],&head)||ec_arg(argv[5],&method)||ec_arg(argv[6],&coder)||ec_arg(argv[7],&reps)||width>8||head>EC_INPUT||method>4||coder>14||reps<1||reps>100)return 2;
    ec_buf input={0},frame={0},first={0},decoded={0},rep={0};int fail=1;uint64_t ek=0,dk=0;
    if(ec_read(argv[2],&input,EC_INPUT))goto done;
    /* warmup, reference frame, and full round trip before timing */
    if(ec_encode(input.p,input.n,(unsigned)width,(size_t)head,(unsigned)method,(unsigned)coder,&first,&ek)||ec_decode(first.p,first.n,&decoded,NULL,&dk)||decoded.n!=input.n||memcmp(decoded.p,input.p,input.n))goto done;
    for(unsigned long long i=0;i<reps;i++){
        uint64_t cpu0=ec_ns(CLOCK_PROCESS_CPUTIME_ID),t0=ec_ns(CLOCK_MONOTONIC);
        if(ec_encode(input.p,input.n,(unsigned)width,(size_t)head,(unsigned)method,(unsigned)coder,&frame,&ek))goto done;
        uint64_t t1=ec_ns(CLOCK_MONOTONIC),cpu1=ec_ns(CLOCK_PROCESS_CPUTIME_ID);
        if(ec_decode(frame.p,frame.n,&decoded,NULL,&dk))goto done;
        uint64_t t2=ec_ns(CLOCK_MONOTONIC),cpu2=ec_ns(CLOCK_PROCESS_CPUTIME_ID);
        if(frame.n!=first.n||memcmp(frame.p,first.p,frame.n)||decoded.n!=input.n||memcmp(decoded.p,input.p,input.n))goto done;
        printf("{\"type\":\"timing\",\"repeat\":%llu,\"encode_ns\":%llu,\"decode_ns\":%llu,\"encode_cpu_ns\":%llu,\"decode_cpu_ns\":%llu,\"representation_encode_ns\":%llu,\"representation_decode_ns\":%llu}\n",i,(unsigned long long)(t1-t0),(unsigned long long)(t2-t1),(unsigned long long)(cpu1-cpu0),(unsigned long long)(cpu2-cpu1),(unsigned long long)ek,(unsigned long long)dk);
    }
    if(ec_decode(frame.p,frame.n,&decoded,&rep,&dk)||ec_write(argv[8],".frame",frame.p,frame.n)||ec_write(argv[8],".represented",rep.p,rep.n)||ec_write(argv[8],".decoded",decoded.p,decoded.n)||ec_write(argv[8],".params",ec_par.p,ec_par.n)||ec_write(argv[8],".residual",ec_res.p,ec_res.n)||ec_write(argv[8],".rest",ec_rest.p,ec_rest.n))goto done;
    struct rusage ru;if(getrusage(RUSAGE_SELF,&ru))goto done;
    unsigned modes[3]={0};if(frame.p[7]==13&&ec_huf_modes(frame.p+64+ec_par.n+ec_rest.n,frame.n-64-ec_par.n-ec_rest.n,modes))goto done;
    printf("{\"type\":\"summary\",\"input_bytes\":%zu,\"stored_bytes\":%zu,\"residual_bytes\":%zu,\"represented_bytes\":%zu,\"requested_codec\":%llu,\"actual_codec\":%u,\"huf_stored_chunks\":%u,\"huf_rle_chunks\":%u,\"huf_coded_chunks\":%u,\"peak_process_rss_kib\":%ld,\"count_scope\":\"forced_typed_numeric_C_pipeline\"}\n",input.n,frame.n,ec_res.n,rep.n,coder,(unsigned)frame.p[7],modes[0],modes[1],modes[2],ru.ru_maxrss);fail=0;
 done:ec_free(&input);ec_free(&frame);ec_free(&first);ec_free(&decoded);ec_free(&rep);ec_clear_capture();if(fail)fprintf(stderr,"E02C operation failed\n");return fail;
}
