/* Full production numeric predictor/GCD, with serialization intercepted in a
 * generated copy only. Produces a stored E2C1 control, not production framing. */
#include R3_CAPTURE_CORE
static kbuf saved_par,saved_res,saved_rest;
static size_t r3_capture_num(const kgb *g,const kbuf *par){
    if(g->ns!=3||g->nodes!=1)return 0;
    if(kb_put(&saved_par,par->p,par->n)||kb_put(&saved_res,g->s[1].p,g->s[1].n)||kb_put(&saved_rest,g->s[2].p,g->s[2].n))return 0;
    return 1;
}
static void u64(uint8_t*p,uint64_t x){for(unsigned i=0;i<8;i++)p[i]=(uint8_t)(x>>(8*i));}
static void u32(uint8_t*p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));}
int main(int argc,char**argv){
    if(argc!=3)return 2;FILE*f=fopen(argv[1],"rb");if(!f)return 2;
    int bad=1;uint8_t*in=NULL,*frame=NULL;codec_ctx ctx={0};
    if(fseek(f,0,SEEK_END))goto done;long n=ftell(f);if(n<16||n>(4<<20))goto done;rewind(f);in=malloc((size_t)n);if(!in||fread(in,1,(size_t)n,f)!=(size_t)n)goto done;
    if(memcmp(in,"\x93NUMPY\x01\x00",8))goto done;size_t head=10+(size_t)in[8]+256*(size_t)in[9];if(head>(size_t)n||(n-head)%4)goto done;
    uint8_t dummy=0;if(kg_encode_num(&ctx,C_STORE,1,in,(size_t)n,4,head,&dummy,1)!=1)goto done;
    size_t fn=64+saved_par.n+saved_rest.n+saved_res.n;frame=calloc(fn,1);if(!frame)goto done;
    memcpy(frame,"E2C1",4);frame[4]=1;u64(frame+8,(uint64_t)n);u64(frame+16,saved_par.n);u64(frame+24,saved_rest.n);u64(frame+32,saved_res.n);u64(frame+40,saved_res.n);u64(frame+48,saved_res.n);u32(frame+56,kolmo_crc32c(0,in,(size_t)n));
    memcpy(frame+64,saved_par.p,saved_par.n);memcpy(frame+64+saved_par.n,saved_rest.p,saved_rest.n);memcpy(frame+64+saved_par.n+saved_rest.n,saved_res.p,saved_res.n);
    u32(frame+60,kolmo_crc32c(kolmo_crc32c(0,frame,60),frame+64,fn-64));
    FILE*out=fopen(argv[2],"wb");if(!out)goto done;int writebad=fwrite(frame,1,fn,out)!=fn;if(fclose(out))writebad=1;if(writebad)goto done;
    printf("{\"input_bytes\":%ld,\"stored_frame_bytes\":%zu,\"residual_bytes\":%zu,\"width\":%u,\"order\":%u}\n",n,fn,saved_res.n,(unsigned)saved_par.p[2],(unsigned)((saved_par.p[1]>>1)&3));bad=0;
 done:if(fclose(f))bad=1;free(in);free(frame);free(saved_par.p);free(saved_rest.p);free(saved_res.p);return bad;
}
