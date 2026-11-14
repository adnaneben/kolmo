/* R2 interaction experiment. One existing leaf, no new codec/node identifiers.
 * Exact pinned leaf readers and KG_INT inverse. Encode begins at captured
 * residuals; decode reconstructs/CRC-checks complete input separately.
 */
#define main r1_probe_main
#include R2_REVIEW_SOURCE
#undef main
#include R2_INVERSE
#include <time.h>
#include <sys/resource.h>
#define R2_MAX (16u << 20)
typedef struct {uint8_t *p;size_t n;} buffer;
typedef struct {buffer source,raw,res;size_t pn,rn;unsigned planes;} sample;
typedef struct {unsigned codec,calls,huf_failed,rans_failed;size_t payload,frame;} result;
static uint32_t crc_table[256];
static volatile uint64_t consumed;
static void crc_init(void){for(unsigned i=0;i<256;i++){uint32_t x=i;for(unsigned j=0;j<8;j++)x=(x>>1)^((0u-(x&1u))&0x82f63b78u);crc_table[i]=x;}}
static uint32_t crc32(const uint8_t*p,size_t n,uint32_t previous){uint32_t x=previous^0xffffffffu;for(size_t i=0;i<n;i++)x=crc_table[(x^p[i])&255]^(x>>8);return x^0xffffffffu;}
static uint32_t rd32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t rd64(const uint8_t*p){uint64_t x=0;for(unsigned i=0;i<8;i++)x|=(uint64_t)p[i]<<(8*i);return x;}
static void wr32(uint8_t*p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));}
static void wr64(uint8_t*p,uint64_t x){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t frame_crc(const uint8_t*p,size_t n){return crc32(p+64,n-64,crc32(p,60,0));}
static uint64_t ns(clockid_t c){struct timespec t;if(clock_gettime(c,&t))abort();return (uint64_t)t.tv_sec*1000000000u+(unsigned long)t.tv_nsec;}
static void drop(buffer*b){free(b->p);b->p=NULL;b->n=0;}
static void sample_drop(sample*s){drop(&s->source);drop(&s->raw);drop(&s->res);}
static int load(const char*path,buffer*b){FILE*f=fopen(path,"rb");if(!f)return -1;int bad=1;
 if(fseek(f,0,SEEK_END))goto done;long n=ftell(f);if(n<1||n>2*R2_MAX)goto done;rewind(f);b->p=malloc((size_t)n);if(!b->p)goto done;b->n=(size_t)n;if(fread(b->p,1,b->n,f)!=b->n)goto done;bad=0;
 done:if(fclose(f))bad=1;if(bad)drop(b);return bad?-1:0;}
static int writebuf(const char*dir,const char*name,const buffer*b){char p[4096];int k=snprintf(p,sizeof p,"%s/%s",dir,name);if(k<0||(size_t)k>=sizeof p)return -1;FILE*f=fopen(p,"wb");if(!f)return -1;int bad=fwrite(b->p,1,b->n,f)!=b->n;if(fclose(f))bad=1;return bad?-1:0;}
static int coder(unsigned c){return c==0||c==13||c==14;}
/* Bounds and CRC checked before the unchanged pinned inverse is called. */
static int decode(const buffer*f,buffer*out,buffer*debugres){
 int fail=1;uint8_t*rp=NULL;kbuf restored={0};out->p=NULL;out->n=0;
 if(f->n<64||f->n>2*R2_MAX||memcmp(f->p,"E2C1",4)||f->p[4]!=1||f->p[5]||!coder(f->p[6])||!coder(f->p[7])||(f->p[7]&&f->p[7]!=f->p[6]))goto done;
 if(frame_crc(f->p,f->n)!=rd32(f->p+60))goto done;
 uint64_t raw=rd64(f->p+8),pn=rd64(f->p+16),rn=rd64(f->p+24),replen=rd64(f->p+32),cn=rd64(f->p+40),reslen=rd64(f->p+48);
 if(!raw||raw>R2_MAX||pn<3||pn>128||rn>raw||replen!=reslen||reslen>R2_MAX||!reslen||cn>R2_MAX||pn>f->n-64||rn>f->n-64-pn||cn!=f->n-64-pn-rn)goto done;
 const uint8_t*p=f->p+64+pn+rn;rp=malloc((size_t)reslen);if(!rp)goto done;
 if(f->p[7]==0){if(cn!=reslen)goto done;memcpy(rp,p,(size_t)reslen);}
 else if((f->p[7]==13?huf_decompress_all:rans_decompress_all)(p,(size_t)cn,rp,(size_t)reslen))goto done;
 kbuf r={rp,(size_t)reslen,(size_t)reslen},rest={(uint8_t*)f->p+64+pn,(size_t)rn,(size_t)rn};
 if(kg_int_inv(f->p+64,(size_t)pn,&r,&rest,(size_t)raw,&restored)||restored.n!=raw||crc32(restored.p,restored.n,0)!=rd32(f->p+56))goto done;
 if(debugres){debugres->p=rp;debugres->n=(size_t)reslen;rp=NULL;}
 out->p=restored.p;out->n=restored.n;restored.p=NULL;fail=0;
 done:free(rp);free(restored.p);if(fail)drop(out);return fail?-1:0;
}
static int sample_load(const char*p,sample*s){memset(s,0,sizeof *s);if(load(p,&s->source)||decode(&s->source,&s->raw,&s->res)){sample_drop(s);return -1;}s->pn=(size_t)rd64(s->source.p+16);s->rn=(size_t)rd64(s->source.p+24);s->planes=(s->source.p[65]&8)?s->source.p[66]:1;if((s->planes!=1&&s->planes!=2&&s->planes!=4&&s->planes!=8)||s->res.n%s->planes){sample_drop(s);return -1;}return 0;}
/* Bits H=1, R=2, S=4. Policy8 is the costed union of baseline and HRS.
 * Ties: stored, HUF, rANS; same-codec ties keep the original candidate. */
static int encode(const sample*s,unsigned policy,buffer*out,result*info){
 memset(info,0,sizeof *info);out->p=NULL;out->n=0;if(policy>8)return -1;
 size_t n=s->res.n,cap=n*2+65536;uint8_t*trial=malloc(cap),*best=malloc(cap);if(!trial||!best){free(trial);free(best);return -1;}
 size_t zbest=n;unsigned codebest=0;memcpy(best,s->res.p,n);unsigned kinds[4],count=0;
 /* kind: HO=0, HA=1, RO=2, RA=3, RSO=4, RSA=5 */
 if(policy<8){kinds[count++]=(policy&1)?1:0;kinds[count++]=2+((policy&2)?1:0)+((policy&4)?2:0);}
 else {kinds[count++]=0;kinds[count++]=2;if(s->planes>1)kinds[count++]=1;kinds[count++]=5;}
 for(unsigned i=0;i<count;i++){
  unsigned kind=kinds[i],aligned=kind==1||kind==3||kind==5,code=kind<2?13:14;
  encoder_fn fn=kind<2?huf_compress_all:kind<4?rans_compress_all:review_rans_compress_all;
  size_t z=aligned&&s->planes>1?split_encode(fn,s->res.p,n,s->planes,trial,cap):fn(s->res.p,n,trial,cap);
  info->calls++;if(!z){if(code==13)info->huf_failed++;else info->rans_failed++;continue;}
  if(z>cap){free(trial);free(best);return -1;}
  if(z<zbest||(z==zbest&&codebest&&code<codebest)){memcpy(best,trial,z);zbest=z;codebest=code;}
 }
 size_t overhead=64+s->pn+s->rn;out->n=overhead+zbest;out->p=malloc(out->n);if(!out->p){free(trial);free(best);return -1;}
 memcpy(out->p,s->source.p,overhead);out->p[6]=out->p[7]=(uint8_t)codebest;wr64(out->p+40,zbest);memcpy(out->p+overhead,best,zbest);wr32(out->p+60,frame_crc(out->p,out->n));
 info->codec=codebest;info->payload=zbest;info->frame=out->n;free(trial);free(best);return 0;
}
static uint64_t state=20261112;
static uint64_t random64(void){state^=state<<13;state^=state>>7;state^=state<<17;return state;}
static void shuffle(unsigned*order){for(unsigned i=0;i<9;i++)order[i]=i;for(unsigned i=8;i;i--){unsigned j=(unsigned)(random64()%(i+1));unsigned tmp=order[i];order[i]=order[j];order[j]=tmp;}}
static void displace(uint8_t*p,size_t n){uint64_t x=0;for(size_t i=0;i<n;i+=64){p[i]++;x+=p[i];}consumed^=x;}
static int selftests(void){
 if(selftest())return 1; /* R1 351 compatibility checks. */
 size_t tests=0,trunc=0,mut=0,identity=0;unsigned widths[]={1,2,4,8};
 for(unsigned wi=0;wi<4;wi++)for(unsigned kind=0;kind<4;kind++){
  unsigned width=widths[wi];size_t count=257;sample s={0};kbuf params={0};kb_byte(&params,(uint8_t)width);kb_byte(&params,8);kb_byte(&params,(uint8_t)width);kb_uv(&params,3);kb_uv(&params,count);kb_uv(&params,1);kb_uv(&params,0);
  s.raw.n=count*width+5;s.raw.p=malloc(s.raw.n);s.res.n=count*width;s.res.p=malloc(s.res.n);if(!s.raw.p||!s.res.p)return 1;
  memcpy(s.raw.p,"HDR",3);memcpy(s.raw.p+3+count*width,"TL",2);
  for(size_t i=0;i<count;i++){uint64_t v=kind==0?0:kind==1?7:kind==2?(i%71?i%9:random64()):random64();for(unsigned j=0;j<width;j++){s.raw.p[3+i*width+j]=(uint8_t)(v>>(8*j));s.res.p[j*count+i]=(uint8_t)(v>>(8*j));}}
  s.pn=params.n;s.rn=5;s.planes=width;s.source.n=64+s.pn+5+s.res.n;s.source.p=calloc(s.source.n,1);if(!s.source.p)return 1;
  uint8_t*h=s.source.p;memcpy(h,"E2C1",4);h[4]=1;wr64(h+8,s.raw.n);wr64(h+16,s.pn);wr64(h+24,5);wr64(h+32,s.res.n);wr64(h+40,s.res.n);wr64(h+48,s.res.n);wr32(h+56,crc32(s.raw.p,s.raw.n,0));memcpy(h+64,params.p,s.pn);memcpy(h+64+s.pn,"HDRTL",5);memcpy(h+69+s.pn,s.res.p,s.res.n);wr32(h+60,frame_crc(h,s.source.n));free(params.p);
  buffer base={0};result ri;if(encode(&s,0,&base,&ri))return 2;
  for(unsigned policy=0;policy<9;policy++){
   buffer f={0},f2={0},o={0};result r;
   if(encode(&s,policy,&f,&r)||encode(&s,policy,&f2,&ri)||f.n!=f2.n||memcmp(f.p,f2.p,f.n)||decode(&f,&o,NULL)||o.n!=s.raw.n||memcmp(o.p,s.raw.p,o.n))return 3;
   if(policy==8&&f.n>base.n)return 4;drop(&f2);drop(&o);tests++;
   if(width==1&&(policy==1||policy==2||policy==3)&&f.n==base.n&&!memcmp(f.p,base.p,f.n))identity++;
   for(size_t cut=0;cut<f.n;cut+=f.n/12+1){buffer partial={f.p,cut};if(!decode(&partial,&o,NULL))return 5;trunc++;}
   for(unsigned k=0;k<24;k++){size_t at=random64()%f.n;uint8_t saved=f.p[at];uint32_t crc=rd32(f.p+60);f.p[at]^=(uint8_t)(1+random64()%255);if(k&1)wr32(f.p+60,frame_crc(f.p,f.n));int rc=decode(&f,&o,NULL);if(!rc&&(o.n!=s.raw.n||memcmp(o.p,s.raw.p,o.n)))return 6;drop(&o);f.p[at]=saved;wr32(f.p+60,crc);mut++;}drop(&f);
  }drop(&base);sample_drop(&s);
 }
 printf("{\"r2_selftest\":\"passed\",\"roundtrips_deterministic\":%zu,\"truncations\":%zu,\"mutations\":%zu,\"single_plane_identity\":%zu}\n",tests,trunc,mut,identity);return 0;
}
int main(int argc,char**argv){
 crc_init();if(argc==2&&!strcmp(argv[1],"test"))return selftests();
 if(argc!=7||strcmp(argv[1],"bench")){fprintf(stderr,"usage: stack test | bench BASE_FRAME OUTPUT_DIR ROUNDS SEED TIMING(0/1)\n");return 2;}
 char*end;errno=0;unsigned long rounds=strtoul(argv[4],&end,10);if(errno||*end||rounds<1||rounds>31)return 2;state=strtoull(argv[5],&end,10);if(*end||!state)return 2;int timing=atoi(argv[6]);if(timing!=0&&timing!=1)return 2;
 sample s={0};buffer frames[9]={{0}};result details[9];int fail=1;uint8_t*flush=NULL;
 if(sample_load(argv[2],&s))goto done;
 for(unsigned policy=0;policy<9;policy++){
  buffer out={0};if(encode(&s,policy,&frames[policy],&details[policy])||decode(&frames[policy],&out,NULL)||out.n!=s.raw.n||memcmp(out.p,s.raw.p,out.n)){drop(&out);goto done;}drop(&out);
  char filename[32];snprintf(filename,sizeof filename,"policy-%u.frame",policy);if(writebuf(argv[3],filename,&frames[policy]))goto done;
  printf("{\"type\":\"size\",\"policy\":%u,\"bytes\":%zu,\"payload_bytes\":%zu,\"actual_codec\":%u,\"entropy_calls\":%u,\"huf_failed\":%u,\"rans_failed\":%u,\"planes\":%u,\"residual_bytes\":%zu,\"input_bytes\":%zu}\n",policy,frames[policy].n,details[policy].payload,details[policy].codec,details[policy].calls,details[policy].huf_failed,details[policy].rans_failed,s.planes,s.res.n,s.raw.n);
 }
 if(frames[0].n!=s.source.n||memcmp(frames[0].p,s.source.p,s.source.n)||frames[8].n>frames[0].n||frames[8].n>frames[7].n)goto done;
 if(writebuf(argv[3],"residual.bin",&s.res)||writebuf(argv[3],"original.npy",&s.raw))goto done;
 if(timing){
  unsigned batch=(unsigned)((65536+s.raw.n-1)/s.raw.n);if(batch>32)batch=32;if(!batch)batch=1;unsigned order[9];shuffle(order);
  for(unsigned long round=0;round<rounds;round++)for(unsigned k=0;k<9;k++){
   unsigned policy=order[(k+round)%9];uint64_t c0=ns(CLOCK_PROCESS_CPUTIME_ID),t0=ns(CLOCK_MONOTONIC);
   for(unsigned b=0;b<batch;b++){buffer f={0};result r;if(encode(&s,policy,&f,&r))goto done;consumed^=f.n;drop(&f);}
   uint64_t t1=ns(CLOCK_MONOTONIC),c1=ns(CLOCK_PROCESS_CPUTIME_ID);
   printf("{\"type\":\"timing\",\"regime\":\"encode_residual_to_frame\",\"policy\":%u,\"round\":%lu,\"batch\":%u,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n",policy,round,batch,(unsigned long long)(t1-t0),(unsigned long long)(c1-c0));
  }
  flush=malloc(32u<<20);if(!flush)goto done;memset(flush,1,32u<<20);
  for(unsigned regime=0;regime<2;regime++){
   shuffle(order);
   for(unsigned long round=0;round<rounds;round++)for(unsigned k=0;k<9;k++){
    unsigned policy=order[(k+round)%9],nb=regime?1:batch;
    if(regime)displace(flush,32u<<20);else{buffer o={0};if(decode(&frames[policy],&o,NULL))goto done;drop(&o);}
    uint64_t c0=ns(CLOCK_PROCESS_CPUTIME_ID),t0=ns(CLOCK_MONOTONIC);
    for(unsigned b=0;b<nb;b++){buffer o={0};if(decode(&frames[policy],&o,NULL))goto done;consumed^=o.n;drop(&o);}
    uint64_t t1=ns(CLOCK_MONOTONIC),c1=ns(CLOCK_PROCESS_CPUTIME_ID);
    printf("{\"type\":\"timing\",\"regime\":\"decode_%s_verified_full_input\",\"policy\":%u,\"round\":%lu,\"batch\":%u,\"wall_ns\":%llu,\"cpu_ns\":%llu}\n",regime?"displaced32MiB":"warm",policy,round,nb,(unsigned long long)(t1-t0),(unsigned long long)(c1-c0));
   }
  }
 }
 fail=0;
 done:free(flush);for(unsigned k=0;k<9;k++)drop(&frames[k]);sample_drop(&s);if(fail)fprintf(stderr,"R2 stack test failed\n");return fail;
}
