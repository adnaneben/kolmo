#include "../src/experimental/frontier_dna.h"
#include "../src/experimental/frontier_prefix.h"
#include "../src/experimental/frontier_table.h"
#include "../src/experimental/frontier_pcm.h"
#include "../src/experimental/frontier_lag.h"
#include "../src/experimental/frontier_runs.h"
#include "../src/experimental/frontier_shuffle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { LIMIT = 512 * 1024 };
static uint8_t src[LIMIT], encoded[2 * LIMIT], out[LIMIT + 8];
static uint32_t state = 72134;
static uint32_t random32(void) {
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

static void dna_rows(size_t width, unsigned ending, unsigned lower, int wrapped) {
    const char *alphabet = lower ? "ACtg" : "ACGT";
    size_t n = 0;
    if (wrapped) { memcpy(src, ">test\n", 6); n = 6; }
    for (unsigned r = 0; r < 200; ++r) {
        for (size_t i = 0; i < width; ++i) {
            uint32_t v = random32();
            src[n++] = r < 25 || r > 75 ? (uint8_t)alphabet[v & 3] : 'N';
        }
        if (wrapped) {
            if (ending == 2) src[n++] = '\r';
            src[n++] = '\n';
        }
    }
    if (wrapped) { memcpy(src+n, "act", 3); n+=3; }
    assert(frontier_dna_probe(src,n));
    size_t k = frontier_dna_encode(src,n,encoded,sizeof(encoded));
    if (!k) {
        /* Mixed-case high-entropy exceptions can make this recipe larger. */
        assert(lower); return;
    }
    unsigned char *dst = out + 4;
    memset(out, 0xa7, n + 8);
    assert(!frontier_dna_decode(encoded,k,dst,n) && !memcmp(src,dst,n));
    assert(out[3] == 0xa7 && out[n+4] == 0xa7);
    assert(frontier_dna_decode(encoded,19,dst,n));
    uint8_t saved = encoded[16]; encoded[16] = 2;
    assert(frontier_dna_decode(encoded,k,dst,n)); encoded[16] = saved;
    for (int rep=0; rep<60; ++rep) {
        size_t i=random32()%k; uint8_t mask=(uint8_t)(1u<<(random32()%8));
        encoded[i]^=mask;
        (void)frontier_dna_decode(encoded,k,dst,n);
        encoded[i]^=mask;
        assert(out[3] == 0xa7 && out[n+4] == 0xa7);
    }
}

static void table_rows(unsigned cols, int final_lf) {
    size_t n=0;
    for (unsigned r=0; r<100; ++r) {
        for (unsigned j=0; j<cols; ++j) {
            if ((j+r)%5) {
                int count=snprintf((char*)src+n,LIMIT-n,"%u.0000",random32()%6000);
                assert(count>0); n+=(size_t)count;
            }
            if (j+1<cols) src[n++]=',';
        }
        if (r+1<100 || final_lf) src[n++]='\n';
    }
    uint8_t delim=0;
    assert(frontier_table_probe(src,n,&delim) && delim==',');
    size_t k=frontier_table_encode(src,n,delim,encoded,sizeof(encoded));
    assert(k);
    memset(out,0xa7,n+8);
    assert(!frontier_table_decode(encoded,k,delim,out+4,n) && !memcmp(src,out+4,n));
    assert(frontier_table_decode(encoded,k-1,delim,out+4,n));
    assert(frontier_table_decode(encoded,k,delim,out+4,n-1));
    assert(out[3]==0xa7 && out[n+4]==0xa7);
    for (unsigned rep=0; rep<100; ++rep) {
        size_t pos=random32()%k; uint8_t b=encoded[pos]; encoded[pos]^=1u<<(random32()%8);
        (void)frontier_table_decode(encoded,k,delim,out+4,n);
        encoded[pos]=b; assert(out[3]==0xa7 && out[n+4]==0xa7);
    }
    size_t mid=n/2; while(mid<n && src[mid]!=',') ++mid;
    src[mid]='X'; assert(!frontier_table_encode(src,n,delim,encoded,sizeof(encoded)));
}

static void prefix_stream(unsigned n) {
    for (unsigned i=0; i<n; ++i) src[i]=(uint8_t)(random32()%71);
    size_t k=frontier_prefix_encode(src,n,encoded,sizeof(encoded));
    assert(k && k<n);
    memset(out,0xa7,n+8);
    assert(!frontier_prefix_decode(encoded,k,out+4,n) && !memcmp(src,out+4,n));
    assert(frontier_prefix_decode(encoded,k-1,out+4,n));
    for (unsigned rep=0; rep<100; ++rep) {
        size_t pos=random32()%k; uint8_t b=encoded[pos]; encoded[pos]^=1u<<(random32()%8);
        (void)frontier_prefix_decode(encoded,k,out+4,n);
        encoded[pos]=b; assert(out[3]==0xa7 && out[n+4]==0xa7);
    }
}

static void other_recipes(void) {
    for (unsigned i=0;i<16000;++i) {
        uint16_t sample=(uint16_t)((i%700)*11+(random32()%7));
        memcpy(src+2*i,&sample,2);
    }
    for(unsigned channels=1;channels<=2;++channels) {
        size_t n=32001;
        size_t k=frontier_pcm_encode(src,n,channels,encoded,sizeof(encoded));
        assert(k && !frontier_pcm_decode(encoded,k,out,n) && !memcmp(src,out,n));
        assert(frontier_pcm_decode(encoded,k-1,out,n));
        for(unsigned i=0;i<80;++i) {
            size_t at=random32()%k;uint8_t old=encoded[at];
            encoded[at]^=1u<<(random32()%8);
            (void)frontier_pcm_decode(encoded,k,out,n);
            encoded[at]=old;
        }
        for(unsigned delta=0;delta<=1;++delta) {
            size_t rows=16000/channels-3;
            if (frontier_shuffle16(src,rows,channels,delta,encoded,0)) {
                assert(frontier_shuffle16(encoded,rows,channels,delta,out,1));
                assert(!memcmp(src,out,2*rows*channels));
                for(unsigned c=0;c<channels;++c) {
                    uint16_t prev=0;
                    for(size_t r=0;r<rows;++r) {
                        uint16_t x;
                        memcpy(&x,src+2*(r*channels+c),2);
                        uint16_t v=delta?(uint16_t)(x-prev):x;prev=x;
                        assert(encoded[c*rows+r]==(uint8_t)v &&
                               encoded[(channels+c)*rows+r]==(uint8_t)(v>>8));
                    }
                }
            }
        }
    }
    frontier_lags l={1,64,8192};
    size_t n=32001,k=frontier_lag_encode(src,n,&l,encoded,sizeof(encoded));
    assert(k && !frontier_lag_decode(encoded,k,out,n) && !memcmp(src,out,n));
    encoded[12]=255;assert(frontier_lag_decode(encoded,k,out,n));
    memset(src+1024,100,18000);
    k=frontier_runs_encode(src,n,encoded,sizeof(encoded));
    assert(k && !frontier_runs_decode(encoded,k,out,n) && !memcmp(src,out,n));
    assert(frontier_runs_decode(encoded,k-1,out,n));
    size_t pos=0;
    for(unsigned j=0;j<3;++j) {
        memcpy(src+pos,">record\n",8);pos+=8;
        for(unsigned r=0;r<100;++r) {
            const char *alphabet = r < 70 ? "acgt" : "ACGT";
            for(unsigned i=0;i<70+j;++i) src[pos++]=(uint8_t)(r<40?'N':alphabet[i%4]);
            src[pos++]='\n';
        }
    }
    k=frontier_dna_encode(src,pos,encoded,sizeof(encoded));
    assert(k && !memcmp(encoded,"DFR1",4));
    assert(!frontier_dna_decode(encoded,k,out,pos) && !memcmp(src,out,pos));
    assert(frontier_dna_decode(encoded,k-1,out,pos));
}

int main(void) {
    for (unsigned e=1; e<=2; ++e) {
        dna_rows(50,e,0,1); dna_rows(64,e,0,1); dna_rows(79,e,0,1);
        dna_rows(80,e,0,1); dna_rows(128,e,0,1); dna_rows(81,e,1,1);
    }
    dna_rows(200,0,0,0);
    table_rows(16,0); table_rows(33,1); table_rows(371,0);
    prefix_stream(16384); prefix_stream(50001); prefix_stream(65535);
    other_recipes();
    puts("Frontier transforms round-tripped and rejected malformed boundaries.");
}
