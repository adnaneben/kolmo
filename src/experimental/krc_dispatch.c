#include "krc_gpu.h"
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

/* The GPU takes multi-megabyte submissions while CPU workers take short
 * ranges. Both write disjoint records in the same canonical plan array. */
typedef struct {
    const uint8_t *src;
    size_t bytes, tiles;
    krc_plan *plans;
    atomic_size_t next;
    atomic_int error;
} work_queue;

static void *cpu_ranges(void *arg) {
    work_queue *q = arg;
    const size_t step = 128;
    for (;;) {
        size_t start = atomic_fetch_add_explicit(&q->next,step,memory_order_relaxed);
        if (start >= q->tiles) break;
        size_t count = q->tiles - start < step ? q->tiles - start : step;
        size_t bytes = q->bytes - start * 1024;
        if (count < q->tiles - start) bytes = count * 1024;
        int rc = krc_plan_cpu(q->src+start*1024,bytes,q->plans+start,count,1);
        if (rc) {atomic_store(&q->error,rc);break;}
    }
    return NULL;
}

int krc_dispatch_plan(krc_gpu *gpu,const uint8_t *src,size_t n,krc_plan *plans,size_t count,
                      int requested,int require_device,double *seconds,size_t *device_tiles) {
    size_t tiles = krc_plan_count(n);
    if (!krc_bound(n) || count < tiles || (!src && n) || (!plans && tiles)) return KRC_ERR_PLAN;
    if (seconds) *seconds = 0;
    if (device_tiles) *device_tiles = 0;
    if (!gpu) return require_device ? KRC_ERR_PLAN : krc_plan_cpu(src,n,plans,count,requested);
    if (!tiles) return 0;
    if (requested <= 0) {
        long online = sysconf(_SC_NPROCESSORS_ONLN);
        requested = online > 256 ? 256 : online > 0 ? (int)online : 1;
    }
    if (requested == 1 && require_device) {
        int rc = krc_gpu_plan(gpu,src,n,plans,count,seconds);
        if (!rc && device_tiles) *device_tiles=tiles;
        return rc;
    }
    if (requested == 1 || (n < (8u<<20) && !require_device))
        return krc_plan_cpu(src,n,plans,count,requested);
    if (requested > 256) requested = 256;
    work_queue q = {.src=src,.bytes=n,.tiles=tiles,.plans=plans};
    const size_t first = tiles < 4096 ? tiles : 4096;
    atomic_init(&q.next,first); atomic_init(&q.error,0);
    pthread_t workers[255];
    unsigned started=0;
    for (int i=1; i<requested && (size_t)(i-1)*128 < tiles-first; ++i) {
        if (pthread_create(&workers[started],NULL,cpu_ranges,&q)) break;
        ++started;
    }
    size_t start=0, batch=first;
    double total_seconds=0;
    size_t completed=0;
    for (;;) {
        size_t bytes=n-start*1024;
        if (batch < tiles-start) bytes=batch*1024;
        double elapsed=0;
        int rc=krc_gpu_plan(gpu,src+start*1024,bytes,plans+start,batch,&elapsed);
        if (rc) {
            if (!require_device) rc=krc_plan_cpu(src+start*1024,bytes,plans+start,batch,1);
            if (rc) {atomic_store(&q.error,rc);break;}
            cpu_ranges(&q);
            break;
        }
        total_seconds+=elapsed;completed+=batch;
        const size_t step=8192;
        start=atomic_fetch_add_explicit(&q.next,step,memory_order_relaxed);
        if (start>=tiles) break;
        batch=tiles-start<step?tiles-start:step;
    }
    for(unsigned i=0;i<started;++i) pthread_join(workers[i],NULL);
    if (seconds) *seconds=total_seconds;
    if (device_tiles) *device_tiles=completed;
    return atomic_load(&q.error);
}
