/* R07 measurement shim (not part of Kolmo): logs every allocation >= MT_MIN bytes with a short backtrace.
 * LD_PRELOAD=./mtrace.so MT_OUT=log ./kolmo ...   then mtrace_report.py log binary */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
static void *(*r_malloc)(size_t); static void *(*r_calloc)(size_t, size_t); static void *(*r_realloc)(void *, size_t); static void (*r_free)(void *);
static void *(*r_mmap)(void *, size_t, int, int, int, off_t); static int (*r_munmap)(void *, size_t);
static int fd = -1; static size_t MT_MIN = 256 << 10; static __thread int in; static int ready;
static char boot[65536]; static size_t bootn;
#define TN 65536
static void *tab[TN]; static pthread_mutex_t tmu = PTHREAD_MUTEX_INITIALIZER;
static void tadd(void *p) { pthread_mutex_lock(&tmu); size_t h = ((size_t)p >> 4) * 2654435761u % TN; for (int i = 0; i < TN; i++, h = (h + 1) % TN) if (!tab[h] || tab[h] == (void *)1) { tab[h] = p; break; } pthread_mutex_unlock(&tmu); }
static int tdel(void *p) { int f = 0; pthread_mutex_lock(&tmu); size_t h = ((size_t)p >> 4) * 2654435761u % TN; for (int i = 0; i < TN && tab[h]; i++, h = (h + 1) % TN) if (tab[h] == p) { tab[h] = (void *)1; f = 1; break; } pthread_mutex_unlock(&tmu); return f; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static void init(void) {
    r_malloc = dlsym(RTLD_NEXT, "malloc"); r_calloc = dlsym(RTLD_NEXT, "calloc"); r_realloc = dlsym(RTLD_NEXT, "realloc"); r_free = dlsym(RTLD_NEXT, "free");
    r_mmap = dlsym(RTLD_NEXT, "mmap"); r_munmap = dlsym(RTLD_NEXT, "munmap");
    const char *o = getenv("MT_OUT"); if (o) fd = open(o, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    const char *m = getenv("MT_MIN"); if (m) MT_MIN = strtoull(m, 0, 10);
    in = 1; void *b[4]; backtrace(b, 4); in = 0; ready = 1;
}
static void logev(char k, void *p, size_t n, void *old) {
    if (fd < 0 || in) return; in = 1;
    void *b[12]; int c = backtrace(b, 12); char s[512]; int l = snprintf(s, sizeof s, "%c %.6f %p %zu %p", k, now(), p, n, old);
    for (int i = 2; i < c && l < 480; i++) l += snprintf(s + l, sizeof s - l, " %p", b[i]);
    s[l++] = '\n'; if (write(fd, s, l) < 0) {}
    if (k != 'U') tadd(p);
    in = 0;
}
void *malloc(size_t n) {
    if (!r_malloc) { if (!ready && r_calloc == NULL && bootn + n + 16 < sizeof boot && in) { void *p = boot + bootn; bootn += (n + 15) & ~15ul; return p; } init(); }
    void *p = r_malloc(n); if (n >= MT_MIN && p) logev('M', p, n, 0); return p;
}
void *calloc(size_t a, size_t b) {
    if (!r_calloc) { if (bootn + a * b + 16 < sizeof boot) { void *p = boot + bootn; bootn += (a * b + 15) & ~15ul; return p; } return NULL; }
    void *p = r_calloc(a, b); if (a * b >= MT_MIN && p) logev('C', p, a * b, 0); return p;
}
void *realloc(void *o, size_t n) {
    if (!r_realloc) init();
    if ((char *)o >= boot && (char *)o < boot + sizeof boot) { void *p = r_malloc(n); if (p) memcpy(p, o, n < 4096 ? n : 4096); return p; }
    int was = o && fd >= 0 && !in ? tdel(o) : 0;
    void *p = r_realloc(o, n); if (p && (n >= MT_MIN || was)) { logev('R', p, n, o); if (n < MT_MIN) tdel(p); } return p;
}
void free(void *p) {
    if (!p || ((char *)p >= boot && (char *)p < boot + sizeof boot)) return;
    if (!r_free) init();
    if (fd >= 0 && !in && tdel(p)) { in = 1; char s[64]; int l = snprintf(s, sizeof s, "F %.6f %p\n", now(), p); if (write(fd, s, l) < 0) {} in = 0; }
    r_free(p);
}
void *mmap(void *a, size_t n, int pr, int fl, int f, off_t off) {
    if (!r_mmap) init();
    void *p = r_mmap(a, n, pr, fl, f, off);
    if (p != MAP_FAILED && n >= MT_MIN && !in) logev(f >= 0 ? 'f' : 'A', p, n, 0);
    return p;
}
int munmap(void *a, size_t n) { if (!r_munmap) init(); if (n >= MT_MIN && !in) logev('U', a, n, 0); return r_munmap(a, n); }
