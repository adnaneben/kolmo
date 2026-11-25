/* Read the existing public ABI, including formula costs absent from the CLI text. */
#include "kolmo.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(argc==3);
    FILE *f=fopen(argv[2],"rb");assert(f && !fseek(f,0,SEEK_END));
    long n=ftell(f);assert(n>0 && !fseek(f,0,SEEK_SET));
    unsigned char *input=malloc(n);assert(input && fread(input,1,n,f)==(size_t)n);fclose(f);
    kolmo_params p={0};p.threads=1;
    size_t cap=kolmo_bound(n,&p);unsigned char *frame=malloc(cap),*restored=malloc(n);
    kolmo_target *t=NULL;assert(frame && restored && !kolmo_target_parse(argv[1],&t));
    kolmo_target_report r;int64_t z=kolmo_compress_target(input,n,frame,cap,&p,t,NULL,&r);
    assert(z>0 && r.chunks==1 && kolmo_decompress(frame,z,restored,n,1)==n && !memcmp(restored,input,n));
    int selection=0;while(selection<16 && !r.per_setting[selection]) selection++;assert(selection<16);
    printf("%s,%llu,%lld,%.17g,%.17g,%.17g,%zu,%zu\n",kolmo_target_setting_name(selection),
        (unsigned long long)r.predicted_size,(long long)z,r.predicted_cost,r.predicted_enc_s,r.predicted_dec_s,
        sizeof r,offsetof(kolmo_target_report,predicted_size));
    kolmo_target_free(t);free(input);free(frame);free(restored);return 0;
}
