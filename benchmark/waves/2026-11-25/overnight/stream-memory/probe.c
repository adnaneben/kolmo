#include <assert.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
static pthread_mutex_t obs_mu=PTHREAD_MUTEX_INITIALIZER;
static size_t frame_peak,index_peak,packed_peak,packed_live,total_peak,window_peak,nframes;
static void maxwith(size_t *out,size_t x) { if (x>*out) *out=x; }
static void member(size_t capacity) {
    maxwith(&frame_peak,capacity);maxwith(&total_peak,capacity);nframes++;
}
static void read_index(size_t size) { index_peak=size;maxwith(&total_peak,size); }
static void packed_add(size_t n) {
    pthread_mutex_lock(&obs_mu);packed_live+=n;
    maxwith(&packed_peak,packed_live);maxwith(&total_peak,index_peak+packed_live);
    pthread_mutex_unlock(&obs_mu);
}
static void packed_drop(size_t n) { pthread_mutex_lock(&obs_mu);assert(packed_live>=n);packed_live-=n;pthread_mutex_unlock(&obs_mu); }
#include "kolmo.c"
static size_t tls_cap;
static void *sample_tls(void *unused) {
    (void)unused;
    size_t n=TLS?TLS->tcap+TLS->scap+TLS->rbcap+TLS->pcap+TLS->rcap:0;
    for(int i=0;i<zs_n;i++) n+=zs_cache[i].h->cap;
    pthread_mutex_lock(&obs_mu);tls_cap+=n;pthread_mutex_unlock(&obs_mu);return NULL;
}
static uint64_t rss(void) {
    struct task_vm_info v={0};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
    assert(task_info(mach_task_self(),TASK_VM_INFO,(task_info_t)&v,&count)==KERN_SUCCESS);return v.resident_size;
}
int main(int argc,char **argv) {
    assert(argc==5); const char *api=argv[1]; int nt=atoi(argv[2]);
    struct stat st;assert(!fstat(0,&st));
    int output=open(argv[3],O_CREAT|O_TRUNC|O_RDWR,0600);assert(output>=0);
    if(!strncmp(api,"encode-",7)) {
        kolmo_params p={0};assert(kolmo_params_for_level(atoi(argv[4]),&p)==0);p.threads=1;p.block=1u<<20;
        int64_t z=!strcmp(api,"encode-stream") ? kolmo_stream_compress(0,output,&p,2) :kolmo_compress_file(0,output,st.st_size,&p,NULL,0);
        assert(z>0); return close(output)!=0;
    }
    size_t caller=0;uint8_t *mapped=NULL;
    if(!strcmp(api,"mapped")) {
        caller=(size_t)st.st_size; mapped=mmap(NULL,caller,PROT_READ,MAP_PRIVATE,0,0);assert(mapped!=MAP_FAILED);
    }
    uint64_t before=rss(),ticks=kmem_clock_ns();
    int64_t z=!strcmp(api,"stream") ? kolmo_stream_decompress(0,output,nt) :
        mapped ? kolmo_decompress_to_fd(mapped,caller,output,nt,NULL,0) : kolmo_decompress_file(0,output,st.st_size,nt,NULL,0);
    uint64_t dt=kmem_clock_ns()-ticks,after=rss(); struct rusage stats;assert(!getrusage(RUSAGE_SELF,&stats));
    assert(z>=0 && !packed_live && !close(output));
    int helpers=pool_n;run_pool(sample_tls,NULL,helpers+1,helpers+1);
    printf("%lld,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%llu,%llu,%ld,%llu\n",(long long)z,nframes,frame_peak,index_peak,packed_peak,total_peak,window_peak,tls_cap,caller,
            (unsigned long long)before,(unsigned long long)after,stats.ru_maxrss,(unsigned long long)dt);
    if(mapped) munmap(mapped,caller); return 0;
}
