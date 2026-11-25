/* Standalone Darwin API measurement. DIAGNOSTIC adds a separate, untimed
 * inspection pass over precisely the already-existing workers. */
#ifdef DIAGNOSTIC
#include <pthread.h>
#include <stdatomic.h>
static atomic_uint_fast64_t pool_wakes;
static int observed_wait(pthread_cond_t *,pthread_mutex_t *);
#define pthread_cond_wait observed_wait
#include "kolmo.c"
#undef pthread_cond_wait
static int observed_wait(pthread_cond_t *c,pthread_mutex_t *m) {
    int rc=pthread_cond_wait(c,m);
    if (!rc && c==&pool_go) atomic_fetch_add(&pool_wakes,1);
    return rc;
}
#else
#include "kolmo.h"
#endif
#include <assert.h>
#include <inttypes.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct { unsigned char *data, *reference, *encoded, *decoded; size_t n, refn, bound; } Sample;
typedef struct { uint64_t rss,peak,footprint,minor,major,cpu_us; long long wakes; } Obs;
static const char *arm, *role = "parent";
static int pairno;

static uint64_t ns(void) {
    struct timespec t; assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return (uint64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}
static Obs observe(void) {
    struct task_vm_info v = {0}; mach_msg_type_number_t n = TASK_VM_INFO_COUNT;
    assert(task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&v, &n) == KERN_SUCCESS);
    struct rusage r; assert(getrusage(RUSAGE_SELF, &r) == 0);
    long long wakes=-1;
#ifdef DIAGNOSTIC
    wakes=(long long)atomic_load(&pool_wakes);
#endif
    uint64_t cpu_us=(uint64_t)(r.ru_utime.tv_sec+r.ru_stime.tv_sec)*1000000+r.ru_utime.tv_usec+r.ru_stime.tv_usec;
    return (Obs){v.resident_size, r.ru_maxrss, v.phys_footprint, r.ru_minflt, r.ru_majflt, cpu_us, wakes};
}

#ifdef DIAGNOSTIC
typedef struct { pthread_t caller; size_t self, helpers; unsigned inspected, with_tls; pthread_mutex_t lock; } Scan;
static void *record_tls(void *argument) {
    Scan *s = argument;
    size_t bytes = TLS ? TLS->tcap + TLS->scap + TLS->rbcap + TLS->pcap + TLS->rcap : 0;
    for (int i = 0; i < zs_n; i++) bytes += zs_cache[i].h->cap;
    pthread_mutex_lock(&s->lock);
    if (pthread_equal(pthread_self(), s->caller)) s->self = bytes;
    else { s->inspected++; s->helpers += bytes; }
    if (TLS) s->with_tls++;
    pthread_mutex_unlock(&s->lock);
    return NULL;
}
#endif

static void output(const char *stage, const char *op, size_t input, int threads,
                   size_t bytes, Obs a, Obs b, uint64_t elapsed) {
    long long selfbytes=-1, helperbytes=-1;
    int helpers=-1, structs=-1;
#ifdef DIAGNOSTIC
    Scan s = {.caller=pthread_self(), .lock=PTHREAD_MUTEX_INITIALIZER};
    int existing = pool_n; /* no concurrent benchmark calls during a snapshot */
    run_pool(record_tls, &s, existing + 1, existing + 1);
    assert(s.inspected == (unsigned)existing);
    helpers=s.inspected; structs=s.with_tls; selfbytes=s.self; helperbytes=s.helpers;
    pthread_mutex_destroy(&s.lock);
#endif
    printf("%d,%s,%s,%s,%s,%zu,%d,%zu,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d,%d,%lld,%lld,%" PRIu64 ",%lld\n",
           pairno,arm,role,stage,op,input,threads,bytes,elapsed,a.rss,b.rss,a.peak,b.peak,
           a.footprint,b.footprint,a.minor,b.minor,a.major,b.major,helpers,structs,selfbytes,helperbytes,b.cpu_us-a.cpu_us,a.wakes<0?-1:b.wakes-a.wakes);
}
static kolmo_params preset(int threads) {
    kolmo_params p={0}; assert(kolmo_params_for_level(3,&p)==0);
    p.threads=threads; p.block=1u<<20; return p;
}
static unsigned char *read_all(const char *name, size_t *n) {
    FILE *f=fopen(name,"rb"); assert(f && !fseek(f,0,SEEK_END));
    long size=ftell(f); assert(size>0 && !fseek(f,0,SEEK_SET)); *n=(size_t)size;
    unsigned char *v=malloc(*n); assert(v && fread(v,1,*n,f)==*n); assert(!fclose(f)); return v;
}
static void touch(unsigned char *p,size_t n) {
    long step=sysconf(_SC_PAGESIZE); assert(step>0);
    volatile unsigned char *v=p;
    for (size_t i=0;i<n;i+=(size_t)step) v[i]=0;
    v[n-1]=0;
}
static Sample load_sample(const char *source, const char *reference, int create) {
    Sample s={0}; s.data=read_all(source,&s.n);
    kolmo_params p=preset(1); s.bound=kolmo_bound(s.n,&p);
    s.encoded=malloc(s.bound); s.decoded=malloc(s.n);
    assert(s.encoded && s.decoded); touch(s.encoded,s.bound); touch(s.decoded,s.n);
    if (create) {
        int64_t n=kolmo_compress(s.data,s.n,s.encoded,s.bound,&p);assert(n>0);
        assert(kolmo_decompress(s.encoded,n,s.decoded,s.n,1)==(int64_t)s.n && !memcmp(s.data,s.decoded,s.n));
        FILE *f=fopen(reference,"wb");assert(f && fwrite(s.encoded,1,n,f)==(size_t)n && !fclose(f));
    } else s.reference=read_all(reference,&s.refn);
    return s;
}
static void roundtrip(const char *stage,Sample *s,int threads) {
    kolmo_params p=preset(threads);
    Obs a=observe(); uint64_t start=ns();
    int64_t n=kolmo_compress(s->data,s->n,s->encoded,s->bound,&p);
    uint64_t elapsed=ns()-start; Obs b=observe();
    assert(n>0 && (size_t)n==s->refn && !memcmp(s->encoded,s->reference,s->refn));
    output(stage,"encode",s->n,threads,(size_t)n,a,b,elapsed);
    a=observe(); start=ns();
    int64_t recovered=kolmo_decompress(s->encoded,n,s->decoded,s->n,threads);
    elapsed=ns()-start; b=observe();
    assert(recovered==(int64_t)s->n && !memcmp(s->data,s->decoded,s->n));
    output(stage,"decode",s->n,threads,(size_t)n,a,b,elapsed);
}
static void boundary(const char *stage,int release) {
    Obs a=observe(); uint64_t start=ns();
    if (release) kolmo_release_buffers();
    uint64_t elapsed=ns()-start; Obs b=observe();
    output(stage,release ? "release" : "observe",0,0,0,a,b,elapsed);
}
int main(int argc,char **argv) {
    assert(argc==7); pairno=atoi(argv[1]);arm=argv[2];
    int create=!strcmp(arm,"prepare"),release=!strcmp(arm,"release");
    setvbuf(stdout,NULL,_IONBF,0);
    Sample small=load_sample(argv[3],argv[5],create),large=load_sample(argv[4],argv[6],create);
    if (create) return 0;
    boundary("initial",0);
    roundtrip("small-cold-T1",&small,1);
    roundtrip("large-first-T4",&large,4);
    roundtrip("large-repeat-T4",&large,4);
    roundtrip("small-after-T4",&small,1);
    boundary("policy",release);
    roundtrip("small-after-policy",&small,1);
    roundtrip("large-after-policy",&large,4);
    roundtrip("small-final-T1",&small,1);
    if (pairno==0) {
        pid_t c=fork(); assert(c>=0);
        if (!c) {
            role="child";alarm(45);boundary("post-fork",0);
            boundary("child-release-first",1);
            roundtrip("child-T4",&large,4);
            roundtrip("child-T1",&small,1);
            boundary("child-release-final",1);_exit(0);
        }
        int status;assert(waitpid(c,&status,0)==c && WIFEXITED(status) && WEXITSTATUS(status)==0);
        boundary("parent-after-child",0);
        roundtrip("parent-after-fork-T4",&large,4);
        roundtrip("parent-after-fork-T1",&small,1);
    }
    boundary("final-release",1);boundary("double-release",1);
    return 0;
}
