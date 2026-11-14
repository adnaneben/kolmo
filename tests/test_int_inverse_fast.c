/* Differential coverage of all integer widths, residual widths, signedness,
 * orders, plane/interleaved layouts, seeds, GCD and malformed lengths. */
#include "../src/kolmo.c"
#include <stdio.h>
static uint64_t rs = 0x52e9de6a9041f789ULL;
static uint64_t rand64(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static void drop(kbuf *p) { free(p->p); memset(p, 0, sizeof *p); }
static int equal(const kbuf *par, const kbuf *res, const kbuf *rest, size_t limit) {
    kbuf a={0}, b={0};
    int x=kg_int_inv(par->p,par->n,res,rest,limit,&a);
    int y=kgi_inverse(par->p,par->n,res,rest,limit,&b);
    int ok=((x==0)==(y==0)) && (x || (a.n==b.n && !memcmp(a.p,b.p,a.n)));
    drop(&a);drop(&b);return ok;
}
int main(void) {
    const int widths[]={1,2,4,8};const size_t counts[]={0,1,2,3,17,257};
    const uint64_t gcds[]={1,3,65537}; size_t valid=0,reject=0;
    for(int wi=0;wi<4;wi++)for(int oi=0;oi<4;oi++)for(int sg=0;sg<2;sg++)
    for(int pl=0;pl<2;pl++)for(int ord=0;ord<3;ord++)for(int gi=0;gi<3;gi++)
    for(int ci=0;ci<6;ci++) {
        int w=widths[wi],ow=widths[oi];size_t cnt=counts[ci];if(cnt<(size_t)ord)continue;
        size_t m=cnt-(size_t)ord; kbuf par={0},res={0},rest={0};
        kb_byte(&par,w);kb_byte(&par,sg|(ord<<1)|(pl<<3));kb_byte(&par,ow);
        kb_uv(&par,5);kb_uv(&par,cnt);kb_uv(&par,gcds[gi]);kb_uv(&par,rand64());
        for(int i=0;i<ord;i++)kb_uv(&par,rand64());
        for(size_t i=0;i<m*(size_t)ow;i++)kb_byte(&res,(uint8_t)rand64());
        /* Non-NULL residual storage for the zero-length seed-only cases. */
        if(!res.p && kb_res(&res,1))return 2;
        for(size_t i=0;i<12;i++)kb_byte(&rest,(uint8_t)rand64());
        size_t limit=cnt*(size_t)w+rest.n;
        if(!equal(&par,&res,&rest,limit)) {fprintf(stderr,"DIFF w%d ow%d sg%d p%d ord%d cnt%zu\n",w,ow,sg,pl,ord,cnt);return 1;}valid++;
        for(size_t z=0;z<par.n;z++){kbuf p=par;p.n=z;kbuf out={0};if(!kgi_inverse(p.p,p.n,&res,&rest,limit,&out))return 3;drop(&out);reject++;}
        if(limit && !equal(&par,&res,&rest,limit-1))return 4;
        if(res.n){kbuf p=res;p.n--;if(!equal(&par,&p,&rest,limit))return 5;reject++;}
        uint8_t saved=par.p[1];par.p[1]|=0x80;
        if(!equal(&par,&res,&rest,limit))return 6;par.p[1]=saved;reject++;
        drop(&par);drop(&res);drop(&rest);
    }
    /* Deliberately enormous declarations must fail before allocating/writing. */
    for(int i=0;i<3;i++){
        kbuf p={0},r={0},s={0},o={0};kb_res(&r,1);kb_res(&s,1);
        kb_byte(&p,8);kb_byte(&p,8);kb_byte(&p,8);kb_uv(&p,0);
        kb_uv(&p,UINT64_MAX-(unsigned)i);kb_uv(&p,1);kb_uv(&p,0);
        if(!kgi_inverse(p.p,p.n,&r,&s,SIZE_MAX,&o))return 7;
        drop(&p);drop(&r);drop(&s);drop(&o);reject++;
    }
    printf("FAST_INT_INVERSE valid=%zu rejected=%zu PASSED\n",valid,reject);return 0;
}
