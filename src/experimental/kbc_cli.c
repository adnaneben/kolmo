#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "kbc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9; }
int main(int argc,char **argv) {
    int decode=0,threads=0,opt;size_t maximum=(size_t)1<<30;
    while ((opt=getopt(argc,argv,"dT:M:"))!=-1) {
        if (opt=='d') decode=1;
        else if (opt=='T' || opt=='M') {
            char *end;errno=0;unsigned long long value=strtoull(optarg,&end,10);
            if (errno || *end || *optarg=='-' || !*optarg) return 1;
            if (opt=='T') { if (value>64) return 1;threads=(int)value; }
            else { if (!value || value>SIZE_MAX/(1<<20)) return 1;maximum=(size_t)value<<20; }
        } else return 1;
    }
    if (argc-optind!=2) { fprintf(stderr,"usage: kbc [-d] [-T 0..64] [-M decoded MiB] input output\n");return 1; }
    FILE *file=fopen(argv[optind],"rb");if (!file) { perror(argv[optind]);return 1; }
    long length;
    if (fseek(file,0,SEEK_END) || (length=ftell(file))<0 || fseek(file,0,SEEK_SET)) { fclose(file);return 1; }
    size_t n=(size_t)length;
    /* A valid frame under the decoded limit cannot exceed this bound. */
    if (n>(decode?kbc_bound(maximum):maximum)) { fprintf(stderr,"Input exceeds -M limit\n");fclose(file);return 1; }
    uint8_t *input=malloc(n?n:1);if (!input || fread(input,1,n,file)!=n) { fclose(file);free(input);return 1; }fclose(file);
    int64_t original=decode?kbc_original_size(input,n):(int64_t)n;
    if (original<0 || (uint64_t)original>maximum) { fprintf(stderr,"Invalid frame or decoded size exceeds -M limit\n");free(input);return 1; }
    size_t cap=decode?(size_t)original:kbc_bound(n);
    uint8_t *output=malloc(cap?cap:1);if (!output) { free(input);return 1; }
    double start=now();
    int64_t result=decode?kbc_decompress(input,n,output,cap,threads):kbc_compress(input,n,output,cap,threads);
    double elapsed=now()-start;
    if (result<0) { fprintf(stderr,"Codec error %lld\n",(long long)result);free(input);free(output);return 1; }
    if (!decode) {
        uint8_t *back=malloc(n?n:1);
        if (!back || kbc_decompress(output,result,back,n,threads)!=(int64_t)n || memcmp(back,input,n)) { fprintf(stderr,"Verification failed\n");free(back);free(input);free(output);return 1; }
        free(back);
    }
    file=fopen(argv[optind+1],"wb");if (!file) { perror(argv[optind+1]);free(input);free(output);return 1; }
    int bad=fwrite(output,1,(size_t)result,file)!=(size_t)result;if (fclose(file)) bad=1;
    struct rusage usage;uint64_t rss=0;
    if (!getrusage(RUSAGE_SELF,&usage) && usage.ru_maxrss>0) {
        rss=(uint64_t)usage.ru_maxrss;
#ifndef __APPLE__
        rss*=1024;
#endif
    }
    fprintf(stderr,"input_bytes=%zu output_bytes=%lld codec_seconds=%.9f requested_threads=%d process_peak_rss_bytes=%llu verified=1\n",n,(long long)result,elapsed,threads,(unsigned long long)rss);
    free(input);free(output);return bad;
}
