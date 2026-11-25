/* Compile with -O2 -Isrc -pthread; read-only probes inside a finite, nonzero Linux memory cgroup. */
#define _GNU_SOURCE
#include "core/common.h"
#include "transforms/common.h"
#include "core/memory.c"
#include <inttypes.h>
int main(void) {
    uint64_t room, limit;
    if (kmem_cgroup_room(&room, &limit) || !limit || limit > UINT64_MAX / 4) return 2;
    unsetenv("KOLMO_MEM_FORCE_BUDGET");
    uint64_t auto_limit = kmem_limit_of(0), bytes = limit * 2;
    /* These calls reserve accounting bytes only, without allocating guest memory. */
    kmem_reserve(bytes, 0);
    int jobs;
    kmem_stats(&jobs, NULL, NULL);
    int extra = kmem_try_reserve(bytes, 0);
    if (extra) kmem_release(bytes);
    kmem_release(bytes);
    uint64_t used;
    int remaining;
    kmem_stats(&remaining, NULL, &used);
    printf("limit=%" PRIu64 " room=%" PRIu64 " auto=%" PRIu64
           " first_jobs=%d extra=%d final_jobs=%d final_used=%" PRIu64 "\n",
           limit, room, auto_limit, jobs, extra, remaining, used);
    uint64_t phys = kmem_phys();
    return (auto_limit == (phys < limit ? phys : limit) / 2 && jobs == 1 && !extra && !remaining && !used) ? 0 : 1;
}
