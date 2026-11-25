/* Linux memory controller accounting. Included by memory.c on Linux and directly by the layout tests. */
#ifndef KOLMO_CGROUP_H
#define KOLMO_CGROUP_H
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int kmem_cgroup_token(const char *list, const char *wanted) {
    size_t size = strlen(wanted);
    while (*list) {
        size_t length = strcspn(list, ",");
        if (length == size && !memcmp(list, wanted, size)) return 1;
        list += length;
        if (*list) list++;
    }
    return 0;
}

/* mountinfo escapes space, tab, newline and backslash as three octal digits. Membership paths are literal. */
static void kmem_mount_unescape(char *path) {
    char *out = path;
    while (*path) {
        if (*path == '\\' && path[1] >= '0' && path[1] <= '3' && path[2] >= '0' && path[2] <= '7'
            && path[3] >= '0' && path[3] <= '7') {
            int value = (path[1] - '0') * 64 + (path[2] - '0') * 8 + path[3] - '0';
            if (value == ' ' || value == '\t' || value == '\n' || value == '\\') {
                *out++ = (char)value;
                path += 4;
                continue;
            }
        }
        *out++ = *path++;
    }
    *out = 0;
}

static int kmem_cgroup_number(int fd, const char *name, uint64_t *value) {
    int file = openat(fd, name, O_RDONLY | O_CLOEXEC);
    if (file < 0) return -1;
    char buf[64];
    ssize_t n = read(file, buf, sizeof(buf) - 1);
    close(file);
    if (n <= 0 || n == (ssize_t)sizeof(buf) - 1 || buf[0] < '0' || buf[0] > '9') return -1;
    buf[n] = 0;
    char *end;
    errno = 0;
    unsigned long long number = strtoull(buf, &end, 10);
    if (errno || (*end && strcmp(end, "\n"))) return -1;
    *value = (uint64_t)number;
    return 0;
}

/* Both the membership and mount root use the reader's cgroup namespace coordinates. The mount point
 * is in its filesystem namespace. A mount of a sibling subtree cannot describe this process. */
static void kmem_cgroup_mount(const char *member, const char *root, const char *mountpoint,
                              int unified, uint64_t *room, uint64_t *limit) {
    size_t base = strlen(root);
    if (!base || root[0] != '/' || mountpoint[0] != '/' || member[0] != '/') return;
    if (base == 1) base = 0;
    if (strncmp(member, root, base) || (member[base] && member[base] != '/')) return;
    const char *relative = member + base;
    /* A cgroup outside the namespace root can be reported with "..". Its mapping is ambiguous. */
    for (const char *p = member; *p; ) {
        p++;
        size_t n = strcspn(p, "/");
        if ((n == 2 && !memcmp(p, "..", 2)) || (n == 1 && *p == '.')) return;
        p += n;
    }
    size_t mountlen = strlen(mountpoint), suffix = strlen(relative);
    char *path = malloc(mountlen + suffix + 1);
    if (!path) return;
    memcpy(path, mountpoint, mountlen);
    memcpy(path + mountlen, relative, suffix + 1);
    size_t end = mountlen + suffix;
    if (end > mountlen && path[end - 1] == '/') path[--end] = 0;
    int leaf = 1;
    for (;;) {
        int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0 && leaf) break;
        if (fd >= 0) {
            uint64_t hierarchy, maximum, current;
            /* Legacy kernels allowed v1 parents to opt out of hierarchical accounting. */
            int applies = unified || leaf || kmem_cgroup_number(fd, "memory.use_hierarchy", &hierarchy) || hierarchy;
            if (applies && !kmem_cgroup_number(fd, unified ? "memory.max" : "memory.limit_in_bytes", &maximum)
                && (unified || maximum < (UINT64_C(1) << 60))) {
                if (maximum < *limit) *limit = maximum;
                if (!kmem_cgroup_number(fd, unified ? "memory.current" : "memory.usage_in_bytes", &current)) {
                    uint64_t left = current < maximum ? maximum - current : 0;
                    if (left < *room) *room = left;
                }
            }
            close(fd);
        }
        if (end == mountlen) break;
        while (end > mountlen && path[end - 1] != '/') end--;
        path[--end] = 0;
        leaf = 0;
    }
    free(path);
}

static int kmem_cgroup_layout(const char *membership, const char *mountinfo, uint64_t *room, uint64_t *limit) {
    *room = *limit = UINT64_MAX;
    FILE *f = fopen(membership, "r");
    if (!f) return -1;
    char *line = NULL, *member[2] = {NULL, NULL};
    size_t cap = 0;
    while (getline(&line, &cap, f) >= 0) {
        char *controllers = strchr(line, ':');
        if (!controllers) continue;
        *controllers++ = 0;
        char *path = strchr(controllers, ':');
        if (!path) continue;
        *path++ = 0;
        int unified = !strcmp(line, "0") && !*controllers;
        if (!unified && !kmem_cgroup_token(controllers, "memory")) continue;
        path[strcspn(path, "\n")] = 0;
        free(member[unified]);
        member[unified] = strdup(path);
    }
    fclose(f);
    f = fopen(mountinfo, "r");
    if (f) {
        while (getline(&line, &cap, f) >= 0) {
            char *separator = strstr(line, " - ");
            if (!separator) continue;
            *separator = 0;
            char *state, *type = strtok_r(separator + 3, " ", &state);
            char *source = strtok_r(NULL, " ", &state);
            char *options = strtok_r(NULL, " \n", &state);
            if (!type || !source || !options) continue;
            int unified = !strcmp(type, "cgroup2");
            if (!member[unified] || (!unified && (strcmp(type, "cgroup") || !kmem_cgroup_token(options, "memory"))))
                continue;
            /* Fields 4 and 5 precede optional fields, which may vary by kernel. */
            char *field = strtok_r(line, " ", &state);
            for (int i = 0; i < 3 && field; i++) field = strtok_r(NULL, " ", &state);
            char *root = field, *mountpoint = strtok_r(NULL, " ", &state);
            if (!root || !mountpoint) continue;
            kmem_mount_unescape(root);
            kmem_mount_unescape(mountpoint);
            kmem_cgroup_mount(member[unified], root, mountpoint, unified, room, limit);
        }
        fclose(f);
    }
    free(line);
    free(member[0]);
    free(member[1]);
    /* An unreadable usage counter cannot erase a known upper bound. */
    if (*room > *limit) *room = *limit;
    return *limit == UINT64_MAX ? -1 : 0;
}
#endif
