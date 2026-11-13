/* Bounded RC encoder API/typing/compatibility test. Full generic decoder is kept
 * as an independent numeric inverse control; no detector or leaf stubs. */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t state=0x823a17cdef054a91ULL;
static uint64_t rv(void){state^=state<<13;state^=state>>7;state^=state<<17;return state;}
static size_t mk_npy(uint8_t *p,int w,int sg,size_t count) {
    char d[128];int len=snprintf(d,sizeof d,"{'descr': '<%c%d', 'fortran_order': False, 'shape': (%zu,), }",sg?'i':'u',w,count);
    size_t h=128;memset(p,' ',h);memcpy(p,"\x93NUMPY\1\0",8);p[8]=118;p[9]=0;memcpy(p+10,d,(size_t)len);p[127]='\n';return h;
}
static int check(const uint8_t *s,size_t n,kolmo_params p,int expect_plain_ignored) {
    size_t cap=kolmo_bound(n,&p);uint8_t *f=malloc(cap),*g=malloc(cap),*o=malloc(n+1);int ok=0;
    if(!f||!g||!o)goto out;
    int64_t z=kolmo_compress(s,n,f,cap,&p),z2=kolmo_compress(s,n,g,cap,&p);
    if(z<0 || z2!=z || memcmp(f,g,(size_t)z) || kolmo_decompress(f,(size_t)z,o,n,1)!=(int64_t)n || memcmp(s,o,n)){fprintf(stderr,"roundtrip failed n=%zu z=%lld z2=%lld mode=%d tr=%d\n",n,(long long)z,(long long)z2,p.mode,p.transforms);goto out;}
    if(expect_plain_ignored){p.transforms&=~(KOLMO_T_TYPED_HUF|KOLMO_T_CSV_ARCHIVE);z2=kolmo_compress(s,n,g,cap,&p);if(z!=z2||memcmp(f,g,(size_t)z))goto out;}
    if(n>4){uint8_t rr[3];if(kolmo_read_range(f,(size_t)z,1,3,rr,1)!=3||memcmp(rr,s+1,3))goto out;}
    for(int j=0;j<8;j++){size_t q=(size_t)rv()%(size_t)z;memcpy(g,f,(size_t)z);g[q]^=1;int64_t got=kolmo_decompress(g,(size_t)z,o,n,1);if(got>=0 && (got!=(int64_t)n || memcmp(s,o,n)))goto out;}
    ok=1;
out:free(f);free(g);free(o);return ok;
}
int main(void) {
    uint8_t *s=malloc(128+4096*8+64);if(!s)return 2;size_t roundtrips=0,headers=0,choices=0;
    for(int w=2;w<=8;w*=2)for(int sg=0;sg<2;sg++)for(int shape=0;shape<6;shape++){
        size_t count=1024,h=mk_npy(s,w,sg,count),n=h+count*(size_t)w;
        for(size_t i=0;i<count;i++){
            uint64_t x=shape==0?i*i:shape==1?rv():shape==2?0:shape==3?(i%11==0?UINT64_MAX:i):shape==4?(i%2?1ULL<<63:0):(i/31)*7;
            for(int b=0;b<w;b++)s[h+i*w+b]=(uint8_t)(x>>(8*b));
        }
        int ww=0;size_t hh=0;if(!ktf_type(s,n,&ww,&hh)||ww!=w||hh!=h)return 3;headers++;
        ktf_prog P;if(ktf_forward(s,n,w,h,&P))return 4;
        int bo=0,bs=0;kg_num_choose(s+h,count,w,&bo,&bs);if(bo!=P.bo||bs!=P.bs)return 5;
        kbuf back={0};if(kg_int_inv(P.par.p,P.par.n,&P.res,&P.rest,n,&back)||back.n!=n||memcmp(back.p,s,n))return 6;
        free(back.p);ktf_free(&P);choices++;
        kolmo_params p={KOLMO_FAST,0,1,0,1|KOLMO_T_TYPED_HUF};if(!check(s,n,p,0)){fprintf(stderr,"case w=%d sg=%d shape=%d\n",w,sg,shape);return 7;}roundtrips++;
        p.mode=KOLMO_RT;if(!check(s,n,p,1))return 8;roundtrips++;
        p.mode=KOLMO_FAST;p.block=1024;if(!check(s,n,p,1))return 9;roundtrips++;
        /* A shape mismatch cannot silently become an approved typed declaration. */
        s[h+count*w]=0; if(ktf_type(s,n+1,&ww,&hh))return 10;headers++;
        /* Huge dimensions and truncated dictionary are rejected before allocation. */
        memset(s+10,' ',117);memcpy(s+10,"{'descr': '<i8', 'fortran_order': False, 'shape': (184467440737095516160,), }",76);
        if(ktf_type(s,n,&ww,&hh))return 11;headers++;
    }
    memset(s,0,2048);memcpy(s,"RIFF",4);put32(s+4,2048-8);memcpy(s+8,"WAVEfmt ",8);put32(s+16,16);s[20]=1;s[22]=1;s[32]=2;s[34]=16;memcpy(s+36,"data",4);put32(s+40,2004);
    for(size_t i=44;i<2048;i++)s[i]=(uint8_t)rv();
    kolmo_params p={KOLMO_FAST,0,1,0,1|KOLMO_T_TYPED_HUF};if(!check(s,2048,p,0))return 12;roundtrips++;
    for(int variant=0;variant<5;variant++){
        size_t n=0;n+=(size_t)sprintf((char*)s,"id,value,name\n");
        for(int i=0;i<80;i++)n+=(size_t)sprintf((char*)s+n,variant==0?"%d,%d,a\n":variant==1?"%d,+%04d,\"a,b\"\r\n":variant==2?"%d,%d,a\r":variant==3?"%d,%d,a,,\n":"%d,%d,\"a\nline\"\n",i,i*3);
        p.mode=KOLMO_ECO;p.transforms=1|KOLMO_T_CSV_ARCHIVE;if(!check(s,n,p,0))return 13;roundtrips++;
        p.mode=KOLMO_FAST;if(!check(s,n,p,1))return 14;roundtrips++;
    }
    for(int i=0;i<5000;i++){
        size_t n=(size_t)rv()%1024;for(size_t j=0;j<n;j++)s[j]=(uint8_t)rv();int w;size_t h;
        if(i%2==0&&n>=10)memcpy(s,"\x93NUMPY",6);
        (void)ktf_type(s,n,&w,&h);headers++;
    }
    free(s);printf("RC_OPTIONS roundtrips=%zu headers=%zu forward_differentials=%zu PASSED\n",roundtrips,headers,choices);return 0;
}
