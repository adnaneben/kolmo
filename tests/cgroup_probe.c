/* Read the live process layout by default. Two arguments substitute fixture files. */
#define _GNU_SOURCE
#include "../src/core/cgroup.h"
#include <inttypes.h>

int main(int argc, char **argv) {
    if (argc != 1 && argc != 3) return 2;
    uint64_t room, limit;
    int rc = kmem_cgroup_layout(argc == 3 ? argv[1] : "/proc/self/cgroup",
                                argc == 3 ? argv[2] : "/proc/self/mountinfo", &room, &limit);
    printf("%d %" PRIu64 " %" PRIu64 "\n", rc, room, limit);
    return 0;
}
