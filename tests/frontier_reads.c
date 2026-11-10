#include "../src/experimental/frontier_entropy.h"
#include "../src/experimental/frontier_fastq.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t seed=4413;
static uint32_t random_word(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void entropy(void) {
    uint8_t src[65536],coded[100000],out[65536];
    for(unsigned mode=0;mode<=2;++mode) for(unsigned j=0;j<5;++j) {
        size_t n=61733+j;
        for(size_t i=0;i<n;++i) src[i]=j==0?66:(uint8_t)(random_word()%(j*13)+32);
        size_t size=frontier_entropy_encode(src,n,mode,coded,sizeof(coded));
        assert(size && size<n && !frontier_entropy_decode(coded,size,out,n) && !memcmp(src,out,n));
        assert(frontier_entropy_decode(coded,size-1,out,n));assert(frontier_entropy_decode(coded,size,out,n+1));
        coded[4]=128;assert(frontier_entropy_decode(coded,size,out,n));
    }
}
static void reads(void) {
    size_t cap=4u<<20,at=0;
    uint8_t *src=malloc(cap),*dest=malloc(cap),*out=malloc(cap),*scratch=malloc(cap);
    assert(src&&dest&&out&&scratch);
    memcpy(src,"cut row\n",8);at=8;
    for(unsigned r=0;r<4200;++r) {
        const char *e=r%3==0?"\r\n":"\n";
        char head[100];snprintf(head,sizeof(head),"@RUN.%06u source 777 /1",r+8870);
        at+=(size_t)snprintf((char *)src+at,cap-at,"%s%s",head,e);
        unsigned n=80+r%7;
        for(unsigned i=0;i<n;++i) src[at++]=(uint8_t)("ACGTNacg"[random_word()%(r%11?4:8)]);
        at+=(size_t)snprintf((char *)src+at,cap-at,"%s+%s%s",e,r%11==3?head+1:r%11==4?" comment":"",e);
        for(unsigned i=0;i<n;++i) src[at++]=(uint8_t)(40+random_word()%18);
        memcpy(src+at,e,strlen(e));at+=strlen(e);
    }
    memcpy(src+at,"@partial\nAT",11);at+=11;
    assert(frontier_fastq_probe(src,at));
    size_t n=frontier_fastq_encode(src,at,dest,cap);
    assert(n && !frontier_fastq_decode(dest,n,out,at) && !memcmp(out,src,at));
    assert(!frontier_fastq_encode(src,at,scratch,32));
    assert(frontier_fastq_decode(dest,n-1,out,at));
    for (unsigned rep=0;rep<400;++rep) {
        memcpy(scratch,dest,n);
        size_t where=rep<130?random_word()%61:random_word()%n;
        scratch[where]^=(uint8_t)(1u<<(random_word()%8));
        /* A mutation may decode to another sequence. The containing frame
           supplies its checksum. Here we check decoder memory safety. */
        (void)frontier_fastq_decode(scratch,n,out,at);
    }
    free(src);free(dest);free(out);free(scratch);
}
int main(void){entropy();reads();puts("FASTQ and integer entropy round trips and corruption probes passed.");}
