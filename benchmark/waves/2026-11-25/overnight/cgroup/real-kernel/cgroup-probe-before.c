#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
static int kmem_read_u64_file(const char *path, uint64_t *v) {
    FILE *f = fopen(path, "r"); if (!f) return -1;
    char b[64]; int ok = fgets(b, sizeof b, f) != NULL; fclose(f);
    if (!ok || !strncmp(b, "max", 3)) return -1;
    char *e; unsigned long long x = strtoull(b, &e, 10); if (e == b) return -1;
    *v = x; return 0;
}
/* the cgroup's room: limit minus usage (v2, else v1); -1 when there is no limit */
static int kmem_cgroup_room(uint64_t *room, uint64_t *limit) {
    FILE *f = fopen("/proc/self/cgroup", "r"); if (!f) return -1;
    char line[512], v2[512] = "", v1[512] = ""; int have2 = 0, have1 = 0;
    while (fgets(line, sizeof line, f)) {
        size_t l = strlen(line); if (l && line[l - 1] == '\n') line[--l] = 0;
        if (!strncmp(line, "0::", 3)) { snprintf(v2, sizeof v2, "%s", line + 3); have2 = 1; }
        else { char *c = strchr(line, ':'); if (c && !strncmp(c + 1, "memory:", 7)) { snprintf(v1, sizeof v1, "%s", c + 8); have1 = 1; } }
    }
    fclose(f);
    char p[640]; uint64_t lim, cur;
    if (have2) {
        snprintf(p, sizeof p, "/sys/fs/cgroup%s/memory.max", v2);
        if (!kmem_read_u64_file(p, &lim)) {
            snprintf(p, sizeof p, "/sys/fs/cgroup%s/memory.current", v2);
            if (!kmem_read_u64_file(p, &cur)) { *limit = lim; *room = lim > cur ? lim - cur : 0; return 0; }
        }
    }
    if (have1) {
        snprintf(p, sizeof p, "/sys/fs/cgroup/memory%s/memory.limit_in_bytes", v1);
        if (!kmem_read_u64_file(p, &lim) && lim < ((uint64_t)1 << 60)) {
            snprintf(p, sizeof p, "/sys/fs/cgroup/memory%s/memory.usage_in_bytes", v1);
            if (!kmem_read_u64_file(p, &cur)) { *limit = lim; *room = lim > cur ? lim - cur : 0; return 0; }
        }
    }
    return -1;
}
int main(void) {
    uint64_t room = UINT64_MAX, limit = UINT64_MAX;
    int rc = kmem_cgroup_room(&room, &limit);
    printf("%d %" PRIu64 " %" PRIu64 "\n", rc, room, limit);
    return 0;
}
