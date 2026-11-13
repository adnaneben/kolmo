/* R4 research only. Compare against the exact pinned rANS reference and exercise
 * the complete production parser/inverse/checksum. No new wire identifiers. */
#define _GNU_SOURCE
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
static long r4_fail_after = -1;
static void *r4_malloc(size_t n) {
    if (r4_fail_after == 0) return NULL;
    if (r4_fail_after > 0) --r4_fail_after;
    return malloc(n);
}
#include R4_CORE
#include R4_REFERENCE
#define MAX_BYTES (32u << 20)
static uint64_t rng=0x80f011cc6419a3ULL, decisions=0;
static volatile uint64_t consumed;
static uint64_t random64(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
static uint64_t now_ns(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec;}
static uint64_t cpu_ns(void){struct timespec t;assert(!clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t));return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec;}
static kbuf load_file(const char *path){kbuf b={0};FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0&&(unsigned long)n<=MAX_BYTES);rewind(f);b.n=b.cap=(size_t)n;b.p=malloc(b.n?b.n:1);assert(b.p);assert(fread(b.p,1,b.n,f)==b.n);assert(!fclose(f));return b;}
static void save_file(const char *path,const kbuf*b){FILE*f=fopen(path,"wb");assert(f);assert(fwrite(b->p,1,b->n,f)==b->n);assert(!fclose(f));}
static void drop(kbuf*b){free(b->p);memset(b,0,sizeof *b);}
static kbuf singleton(unsigned symbol,size_t n){kbuf b={0};while(n){size_t m=n<RANS_CHUNK?n:RANS_CHUNK;assert(!kb_uv(&b,m));for(unsigned k=0;k<256;){if(k==symbol){assert(!kb_uv(&b,RANS_M));k++;}else{unsigned run=k<symbol?symbol-k:256-k;assert(run>=1&&run<=256);assert(!kb_byte(&b,0));assert(!kb_byte(&b,(uint8_t)(run-1)));k+=run;}}assert(!kb_uv(&b,32));for(unsigned k=0;k<4;k++){uint8_t v[8];put64(v,R64_L);assert(!kb_put(&b,v,8));}n-=m;}return b;}
static void leaf_equal(const uint8_t *p,size_t cn,size_t n,const uint8_t*expected){
    uint8_t*a=malloc(n+32),*b=malloc(n+32);assert(a&&b);memset(a,0xa5,n+32);memset(b,0xa5,n+32);
    int x=rans_decompress_all(p,cn,a+16,n),y=r4_reference_rans(p,cn,b+16,n);
    assert((x==0)==(y==0));decisions=decisions*1099511628211ULL+(unsigned)(x==0);
    if(!x){assert(!memcmp(a+16,b+16,n));if(expected)assert(!memcmp(a+16,expected,n));decisions^=blk_sum(7,a+16,n);}
    if(expected)assert(x==0);
    for(unsigned j=0;j<16;j++)assert(a[j]==0xa5&&b[j]==0xa5&&a[n+16+j]==0xa5&&b[n+16+j]==0xa5);
    free(a);free(b);
}
static kbuf outer_frame(const kbuf*g,const uint8_t*raw,size_t n){
    kbuf f={0};assert(n<=UINT32_MAX&&g->n<UINT32_MAX-4);assert(!kb_res(&f,HDR+IDX+4+g->n));f.n=HDR+IDX+4+g->n;memset(f.p,0,HDR+IDX+4);
    memcpy(f.p,"CZN1",4);f.p[4]=7;f.p[5]=KOLMO_FAST;put64(f.p+12,n);put32(f.p+20,1);put32(f.p+24,(uint32_t)(n?n:1));
    uint8_t*e=f.p+HDR;put32(e,(uint32_t)n);put32(e+4,(uint32_t)g->n+4);put32(e+8,blk_sum(7,raw,n));e[12]=C_GRAPH;
    f.p[HDR+IDX]=T_KG;memcpy(f.p+HDR+IDX+4,g->p,g->n);
    uint32_t c=kolmo_crc32c(0,f.p,28);c=kolmo_crc32c(c,e,IDX);put32(f.p+28,c);return f;
}
static kbuf graph_frame(const kbuf*params,const kbuf*rest,const kbuf*payload,unsigned codec,const uint8_t*raw,size_t n,size_t residual_n){
    kbuf g={0};assert(!kb_byte(&g,'G'));assert(!kb_byte(&g,1));assert(!kb_uv(&g,1));assert(!kb_byte(&g,KG_INT));
    assert(!kb_uv(&g,1));assert(!kb_uv(&g,0));assert(!kb_uv(&g,2));assert(!kb_uv(&g,params->n));assert(!kb_put(&g,params->p,params->n));assert(!kb_uv(&g,2));
    assert(!kb_uv(&g,1));assert(!kb_byte(&g,(uint8_t)codec));assert(!kb_uv(&g,residual_n));assert(!kb_uv(&g,payload->n));assert(!kb_put(&g,payload->p,payload->n));
    assert(!kb_uv(&g,2));assert(!kb_byte(&g,C_STORE));assert(!kb_uv(&g,rest->n));assert(!kb_uv(&g,rest->n));assert(!kb_put(&g,rest->p,rest->n));
    kbuf f=outer_frame(&g,raw,n);drop(&g);return f;
}
static kbuf identity_frame(const kbuf*payload,const uint8_t*raw,size_t n){
    kbuf p={0},empty={0};assert(!kb_byte(&p,1));assert(!kb_byte(&p,0));assert(!kb_byte(&p,1));assert(!kb_uv(&p,0));assert(!kb_uv(&p,n));assert(!kb_uv(&p,1));assert(!kb_uv(&p,0));
    kbuf f=graph_frame(&p,&empty,payload,C_RANS,raw,n,n);drop(&p);return f;
}
static void full_check(const kbuf*f,const uint8_t*raw,size_t n){
    uint8_t*out=malloc(n+32);assert(out);memset(out,0xa5,n+32);
    kolmo_decode_limits lim={n,(uint32_t)n,1};assert(kolmo_decompress_limited(f->p,f->n,out+16,n,1,&lim)==(int64_t)n);assert(!memcmp(out+16,raw,n));
    if(n>1){size_t a=n/3,m=n-a;assert(kolmo_read_range(f->p,f->n,a,m,out+16,1)==(int64_t)m);assert(!memcmp(out+16,raw+a,m));lim.max_output_bytes=n-1;assert(kolmo_decompress_limited(f->p,f->n,out+16,n,1,&lim)==KOLMO_ERR_LIMIT);}
    for(unsigned i=0;i<16;i++)assert(out[i]==0xa5&&out[n+16+i]==0xa5);free(out);
}
static void selftest(const char*dir){
    size_t valid=0,truncated=0,mutated=0,full=0,full_bad=0,alloc=0;
    const size_t small[]={1,2,3,4,7,16,31,255,1024,4097};
    assert(!mkdir(dir,0700)||errno==EEXIST);
    for(unsigned s=0;s<256;s++)for(unsigned j=0;j<sizeof small/sizeof small[0];j++){
        size_t n=small[j];uint8_t*raw=malloc(n);assert(raw);memset(raw,s,n);kbuf p=singleton(s,n);leaf_equal(p.p,p.n,n,raw);valid++;
        if((s==0||s==1||s==127||s==255)&&j==9){kbuf f=identity_frame(&p,raw,n);full_check(&f,raw,n);full++;char path[4096];snprintf(path,sizeof path,"%s/symbol-%u.czn",dir,s);save_file(path,&f);snprintf(path,sizeof path,"%s/symbol-%u.raw",dir,s);kbuf rb={raw,n,0};save_file(path,&rb);drop(&f);}
        if(j==7){for(size_t z=0;z<p.n;z++){leaf_equal(p.p,z,n,NULL);truncated++;}for(unsigned k=0;k<32;k++){size_t a=random64()%p.n;uint8_t x=p.p[a];p.p[a]^=(uint8_t)(1+random64()%255);leaf_equal(p.p,p.n,n,NULL);p.p[a]=x;mutated++;}}
        drop(&p);free(raw);
    }
    const size_t large[]={65535,65536,131071,131072,1048575,1048576,1048577};
    for(unsigned j=0;j<sizeof large/sizeof large[0];j++){size_t n=large[j];uint8_t*raw=malloc(n);assert(raw);memset(raw,241,n);kbuf p=singleton(241,n);leaf_equal(p.p,p.n,n,raw);valid++;kbuf f=identity_frame(&p,raw,n);full_check(&f,raw,n);full++;drop(&f);drop(&p);free(raw);}
    for(unsigned k=0;k<96;k++){
        size_t m=17+k*23,n=m*3;uint8_t*raw=malloc(n),*tmp=malloc(m*2+4096);assert(raw&&tmp);memset(raw,5,m);memset(raw+2*m,247,m);
        for(size_t j=0;j<m;j++)raw[m+j]=(uint8_t)((k&1)?random64()%2:random64());
        kbuf p=singleton(5,m),q=singleton(247,m);size_t z=rans_compress_all(raw+m,m,tmp,m*2+4096);assert(z);assert(!kb_put(&p,tmp,z));assert(!kb_put(&p,q.p,q.n));
        leaf_equal(p.p,p.n,n,raw);valid++;kbuf f=identity_frame(&p,raw,n);full_check(&f,raw,n);full++;
        for(unsigned j=0;j<48;j++){size_t a=random64()%p.n;uint8_t v=p.p[a];p.p[a]^=(uint8_t)(1+random64()%255);leaf_equal(p.p,p.n,n,NULL);p.p[a]=v;mutated++;}
        if(k<4){char path[4096];snprintf(path,sizeof path,"%s/mixed-%u.czn",dir,k);save_file(path,&f);snprintf(path,sizeof path,"%s/mixed-%u.raw",dir,k);kbuf rb={raw,n,0};save_file(path,&rb);}
        /* Full-frame mutations leave the checksum intact: successful alternate
         * legal encodings are allowed only if their reconstructed bytes agree. */
        uint8_t*out=malloc(n);assert(out);for(unsigned j=0;j<16;j++){size_t a=HDR+IDX+random64()%(f.n-HDR-IDX);uint8_t v=f.p[a];f.p[a]^=(uint8_t)(1+random64()%255);int64_t rc=kolmo_decompress_limited(f.p,f.n,out,n,1,&(kolmo_decode_limits){n,(uint32_t)n,1});if(rc>=0){assert(rc==(int64_t)n&&!memcmp(out,raw,n));}decisions=decisions*1099511628211ULL+(uint64_t)(rc+8);f.p[a]=v;full_bad++;}free(out);
        drop(&p);drop(&q);drop(&f);free(tmp);free(raw);
    }
    {kbuf p=singleton(33,32);uint8_t out[32];r4_fail_after=0;assert(rans_decompress_all(p.p,p.n,out,sizeof out)<0);r4_fail_after=-1;alloc++;drop(&p);}
    /* Explicit invalid terminal states and unused coded words. */
    {kbuf p=singleton(255,127);for(unsigned k=0;k<4;k++){uint64_t old=get64(p.p+p.n-32+8*k);put64(p.p+p.n-32+8*k,old+1);leaf_equal(p.p,p.n,127,NULL);put64(p.p+p.n-32+8*k,old);mutated++;}uint8_t zero[4]={0};p.p[p.n-33]=36;assert(!kb_put(&p,zero,4));leaf_equal(p.p,p.n,127,NULL);mutated++;drop(&p);}
    zs_trim();printf("{\"valid_leaves\":%zu,\"truncations\":%zu,\"leaf_mutations\":%zu,\"full_graph_roundtrips_and_ranges\":%zu,\"full_frame_mutations\":%zu,\"allocation_failure\":%zu,\"decision_digest\":\"%016llx\",\"status\":\"passed\"}\n",valid,truncated,mutated,full,full_bad,alloc,(unsigned long long)decisions);
}
static void wrap(const char*e2path,const char*outdir){
    kbuf e=load_file(e2path);assert(e.n>=64&&!memcmp(e.p,"E2C1",4)&&e.p[4]==1);size_t n=get64(e.p+8),pn=get64(e.p+16),rn=get64(e.p+24),cn=get64(e.p+40),resn=get64(e.p+48);unsigned codec=e.p[7];
    assert(n>0&&n<=MAX_BYTES&&pn>=3&&pn<=128&&rn<=n&&resn>0&&resn<=MAX_BYTES&&pn+rn+cn==e.n-64);assert(codec==0||codec==13||codec==14);
    uint32_t crc=kolmo_crc32c(0,e.p,60);crc=kolmo_crc32c(crc,e.p+64,e.n-64);assert(crc==get32(e.p+60));
    kbuf par={e.p+64,pn,0},rest={e.p+64+pn,rn,0},pay={e.p+64+pn+rn,cn,0},res={malloc(resn),resn,resn},raw={0};assert(res.p);
    if(codec==0){assert(cn==resn);memcpy(res.p,pay.p,cn);}else assert(!(codec==13?huf_decompress_all(pay.p,cn,res.p,resn):rans_decompress_all(pay.p,cn,res.p,resn)));
    assert(!kg_int_inv(par.p,par.n,&res,&rest,n,&raw)&&raw.n==n);assert(kolmo_crc32c(0,raw.p,raw.n)==get32(e.p+56));
    kbuf f=graph_frame(&par,&rest,&pay,codec,raw.p,raw.n,resn);full_check(&f,raw.p,raw.n);assert(!mkdir(outdir,0700)||errno==EEXIST);
    const char*names[]={"frame.czn","original.bin","payload.bin","residual.bin"};kbuf*bufs[]={&f,&raw,&pay,&res};char path[4096];for(unsigned j=0;j<4;j++){snprintf(path,sizeof path,"%s/%s",outdir,names[j]);save_file(path,bufs[j]);}
    printf("{\"codec\":%u,\"frame_bytes\":%zu,\"raw_bytes\":%zu,\"residual_bytes\":%zu,\"payload_bytes\":%zu}\n",codec,f.n,raw.n,resn,cn);drop(&f);drop(&raw);drop(&res);drop(&e);zs_trim();
}
static void bench(const char*dir,unsigned batch,unsigned displacement){
    assert(batch>=1&&batch<=256);char path[4096];const char*names[]={"frame.czn","original.bin","payload.bin","residual.bin"};kbuf f[4];for(unsigned j=0;j<4;j++){snprintf(path,sizeof path,"%s/%s",dir,names[j]);f[j]=load_file(path);}
    /* The wrapper's index is C_GRAPH; actual entropy id is recorded by Python. */
    uint8_t*out=malloc(f[1].n>f[3].n?f[1].n:f[3].n),*flush=NULL;assert(out);
    assert(!rans_decompress_all(f[2].p,f[2].n,out,f[3].n)&&!memcmp(out,f[3].p,f[3].n));assert(kolmo_decompress(f[0].p,f[0].n,out,f[1].n,1)==(int64_t)f[1].n&&!memcmp(out,f[1].p,f[1].n));
    if(displacement){flush=calloc(32u<<20,1);assert(flush);for(size_t j=0;j<(32u<<20);j+=64){flush[j]++;consumed+=flush[j];}}
    uint64_t c0=cpu_ns(),t0=now_ns();for(unsigned b=0;b<batch;b++){assert(!rans_decompress_all(f[2].p,f[2].n,out,f[3].n));consumed+=out[b%f[3].n];}uint64_t t1=now_ns(),c1=cpu_ns();assert(!memcmp(out,f[3].p,f[3].n));
    if(displacement)for(size_t j=0;j<(32u<<20);j+=64){flush[j]++;consumed+=flush[j];}
    uint64_t c2=cpu_ns(),t2=now_ns();for(unsigned b=0;b<batch;b++){assert(kolmo_decompress(f[0].p,f[0].n,out,f[1].n,1)==(int64_t)f[1].n);consumed+=out[b%f[1].n];}uint64_t t3=now_ns(),c3=cpu_ns();assert(!memcmp(out,f[1].p,f[1].n));
    printf("{\"batch\":%u,\"displaced\":%u,\"kernel_ns\":%llu,\"kernel_cpu_ns\":%llu,\"full_ns\":%llu,\"full_cpu_ns\":%llu}\n",batch,displacement,(unsigned long long)(t1-t0),(unsigned long long)(c1-c0),(unsigned long long)(t3-t2),(unsigned long long)(c3-c2));free(out);free(flush);for(unsigned j=0;j<4;j++)drop(&f[j]);zs_trim();
}
int main(int argc,char**argv){
    if(argc==3&&!strcmp(argv[1],"selftest")){selftest(argv[2]);return 0;}
    if(argc==4&&!strcmp(argv[1],"wrap")){wrap(argv[2],argv[3]);return 0;}
    if(argc==5&&!strcmp(argv[1],"bench")){char*e;unsigned long b=strtoul(argv[3],&e,10);if(*e||b<1||b>256)return 2;unsigned long d=strtoul(argv[4],&e,10);if(*e||d>1)return 2;bench(argv[2],(unsigned)b,(unsigned)d);return 0;}
    fprintf(stderr,"usage: probe selftest OUT | wrap E2C1 OUT | bench CASE BATCH DISPLACED\n");return 2;
}
