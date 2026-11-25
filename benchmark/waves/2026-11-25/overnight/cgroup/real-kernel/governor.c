#define _GNU_SOURCE
#include "kolmo.c"
#include <inttypes.h>
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    uint64_t maximum = strtoull(argv[1], NULL, 10), room, limit;
    int rc = kmem_cgroup_room(&room, &limit);
    pthread_mutex_lock(&kmem_mu);
    uint64_t budget = kmem_budget_locked();
    pthread_mutex_unlock(&kmem_mu);
    int first = kmem_try_reserve(maximum + (1ul<<20), 0);
    int concurrent = kmem_try_reserve(1ul<<20, 0);
    if (concurrent) kmem_release(1ul<<20);
    if (first) kmem_release(maximum + (1ul<<20));
    int next = kmem_try_reserve(1ul<<20, 0);
    if (next) kmem_release(1ul<<20);
    printf("GOVERNOR rc=%d room=%" PRIu64 " limit=%" PRIu64 " budget=%" PRIu64
           " first_oversize=%d concurrent_extra=%d after_release=%d\n",
           rc, room, limit, budget, first, concurrent, next);
    return !(rc == 0 && limit == maximum && budget > 0 && budget <= maximum &&
             first && !concurrent && next);
}
