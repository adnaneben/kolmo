/* The frozen production/research algorithms are included from a verified staging file. */
#include "frozen.inc"
#include "rans1.inc"
struct SBuf { unsigned char *p; size_t n; };
struct SProg { struct SBuf par,res,rest; int ow,pln; };
int s_type(const unsigned char *p,size_t n,int *w,size_t *h) { return type_of(p,n,w,h); }
int s_forward(const unsigned char *s,size_t n,int w,size_t h,struct SProg *o){
 fiprog p={0};if(fi_forward(s,n,w,h,&p)){fi_free(&p);return -1;}
 *o=(struct SProg){{p.par.p,p.par.n},{p.res.p,p.res.n},{p.rest.p,p.rest.n},p.ow,p.pln};return 0;
}
struct SBuf s_inverse(const unsigned char*p,size_t pn,const unsigned char*r,size_t rn,const unsigned char*t,size_t tn,size_t max){
 kbuf rr={(uint8_t*)r,rn,rn},tt={(uint8_t*)t,tn,tn},o={0};
 if(kg_int_inv(p,pn,&rr,&tt,max,&o)){free(o.p);return (struct SBuf){0};}return (struct SBuf){o.p,o.n};
}
uint32_t s_crc(const unsigned char*p,size_t n){return kolmo_crc32c(0,p,n);}
struct SBuf s_compress(int c,const unsigned char*p,size_t n){
 size_t cap=2*n+16384;unsigned char *o=malloc(cap);if(!o)return (struct SBuf){0};size_t z=0;codec_ctx x={0};
 if(c=='H')z=huf_compress_all(p,n,o,cap);
 else if(c=='R')z=p13_rans_compress_all(p,n,o,cap);
 else if(c=='Z')z=codec_compress(&x,C_ZSTD,3,p,n,o,cap);
 else if(c=='L')z=codec_compress(&x,C_LZ4,1,p,n,o,cap);
 codec_ctx_free(&x);if(!z){free(o);return (struct SBuf){0};}return (struct SBuf){o,z};
}
struct SBuf s_decompress(int c,const unsigned char*p,size_t z,size_t n){
 if(n>64u*1024u*1024u||z>64u*1024u*1024u)return (struct SBuf){0};
 unsigned char*o=malloc(n?n:1);if(!o)return (struct SBuf){0};codec_ctx x={0};int id=c=='H'?C_HUF:c=='R'?C_RANS:c=='Z'?C_ZSTD:c=='L'?C_LZ4:-1;
 int rc=id<0?-1:codec_decompress(&x,id,p,z,o,n);codec_ctx_free(&x);
 if(rc){free(o);return (struct SBuf){0};}return (struct SBuf){o,n};
}
