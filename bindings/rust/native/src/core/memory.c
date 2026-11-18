/* Memory: the per-thread scratch cache of the transforms, the output buffer type, and checked array allocation.
 * Budgets and limits: docs/MEMORY-MODEL.md. */
/* Per-thread reuse of large scratch blocks in the transforms. They allocate and free blocks of similar sizes block
 * after block; handing the same pages back avoids the page faults of fresh memory, which on the fast paths cost as
 * much as the work itself. Workers release the cache when they finish a call. */
typedef struct { size_t n, cap; } zs_hdr;                 /* 16 bytes: keeps malloc alignment */
typedef struct { zs_hdr *h; } zs_ent;
static _Thread_local zs_ent zs_cache[24]; static _Thread_local int zs_n;
#define ZS_MIN ((size_t)64 << 10)
static void *zs_malloc(size_t n) {
    if (n >= ZS_MIN) {
        int bi = -1;
        for (int i = 0; i < zs_n; i++) { size_t c = zs_cache[i].h->cap; if (c >= n && c <= 4 * n && (bi < 0 || c < zs_cache[bi].h->cap)) bi = i; }
        if (bi >= 0) { zs_hdr *h = zs_cache[bi].h; zs_cache[bi] = zs_cache[--zs_n]; h->n = n; return h + 1; }
    }
    if (n > SIZE_MAX - sizeof(zs_hdr)) return NULL;
    zs_hdr *h = malloc(sizeof(zs_hdr) + n); if (!h) return NULL;
    h->n = n; h->cap = n; return h + 1;
}
static void zs_free(void *p) {
    if (!p) return;
    zs_hdr *h = (zs_hdr *)p - 1;
    if (h->cap >= ZS_MIN && zs_n < 24) { zs_cache[zs_n++].h = h; return; }
    free(h);
}
static void *zs_calloc(size_t a, size_t b) {
    if (b && a > SIZE_MAX / b) return NULL;
    void *p = zs_malloc(a * b); if (p) memset(p, 0, a * b); return p;
}
static void *zs_realloc(void *p, size_t n) {
    if (!p) return zs_malloc(n);
    zs_hdr *h = (zs_hdr *)p - 1;
    if (n <= h->cap) { h->n = n; return p; }
    void *q = zs_malloc(n); if (!q) return NULL;
    memcpy(q, p, h->n); zs_free(p); return q;
}
static void zs_trim(void) { for (int i = 0; i < zs_n; i++) free(zs_cache[i].h); zs_n = 0; }
typedef struct { uint8_t *p; size_t n, cap; } Buf;
static void bput(Buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = zs_realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n); b->n += n;
}

/* Container allocations use checked element counts and retain the old pointer
 * on growth failure. The test-only counter exercises each cleanup path. */
#ifdef KOLMO_TESTING
static _Thread_local long kolmo_alloc_fail_after = -1;
static int kolmo_alloc_fail(void) {
    if (kolmo_alloc_fail_after < 0) return 0;
    if (!kolmo_alloc_fail_after) return 1;
    kolmo_alloc_fail_after--; return 0;
}
#else
#define kolmo_alloc_fail() 0
#endif
static void *array_alloc(size_t count, size_t width, int zero) {
    if (!width || count > SIZE_MAX / width || kolmo_alloc_fail()) return NULL;
    return zero ? calloc(count ? count : 1, width) : malloc((count ? count : 1) * width);
}
static void *array_resize(void *old, size_t count, size_t width) {
    if (!width || count > SIZE_MAX / width || kolmo_alloc_fail()) return NULL;
    return realloc(old, (count ? count : 1) * width);
}

/* ------------------------------------------------------------------ the memory governor (docs/MEMORY-MODEL.md)
 * Memory changes speed, never bytes. Everything below decides only how much work runs at once (execution decisions);
 * nothing here may be read by a format decision. The only memory figure a format decision may use is an explicit cap
 * (kolmo_params.mem_limit, --memlimit), passed down by the caller, never the free memory of the moment.
 *
 * Budget: the smaller of the call's cap and what the system can give us, sampled at most every 100 ms:
 *   Linux: MemAvailable (/proc/meminfo), the cgroup's limit minus its usage (v2 memory.max / memory.current, or v1
 *          memory.limit_in_bytes / memory.usage_in_bytes), and PSI (/proc/pressure/memory, "some avg10");
 *   macOS: host_statistics64 free + inactive + purgeable pages, and the memory-pressure dispatch source;
 *   elsewhere: physical memory / 2.
 * What we hold ourselves is part of what we may use, so the sample is "available + reserved by us". A pressure event
 * (PSI some avg10 >= KMEM_PSI_PCT, or a macOS warning) lowers the budget at once to what is reserved now; the budget
 * only grows again after KMEM_CALM_NS without pressure (hysteresis).
 * Reservations: a unit of work (a block job, a trial, a part) reserves its estimated working set before it starts and
 * releases it when done. A reservation that does not fit waits; when nothing is reserved it is granted whatever its
 * size, so one job always runs. Optional work (concurrent trials, extra parallel parts) only tries, and when it does not
 * fit it runs later on the thread that owns the job, with the same result.
 * KOLMO_MEM_FORCE_BUDGET (64M, 512M, 4G, unlimited ...) replaces the sampled figure, for tests and the determinism check. */
#include <time.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <dispatch/dispatch.h>
#include <sys/sysctl.h>
#endif
#define KMEM_SAMPLE_NS 100000000ull       /* re-read the system at most every 100 ms */
#define KMEM_CALM_NS 1000000000ull        /* grow again only after 1 s without pressure */
#define KMEM_PSI_PCT 10.0                 /* PSI "some avg10" at or above this is pressure */
#define KMEM_UNLIMITED UINT64_MAX
static pthread_mutex_t kmem_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t kmem_cv = PTHREAD_COND_INITIALIZER;
static uint64_t kmem_used;                /* bytes reserved now */
static int kmem_jobs, kmem_jobs_peak;     /* reservations held now, and the most at once (tests, RESULTS) */
static uint64_t kmem_budget_cur;          /* the governed budget (after hysteresis); 0 until first sampled */
static uint64_t kmem_last_sample_ns, kmem_last_pressure_ns;
static uint64_t kmem_forced;              /* KOLMO_MEM_FORCE_BUDGET or the test hook; 0 = sample the system */
static int kmem_inited;
static atomic_int kmem_pressure_flag;     /* set by the macOS pressure source or a test; consumed by the sampler */
static uint64_t (*kmem_sample_hook)(int *pressure);   /* tests: replaces the system sample */
static uint64_t kmem_clock_ns(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
static uint64_t kmem_phys(void) {
#if defined(__APPLE__)
    uint64_t m = 0; size_t l = sizeof m;
    if (!sysctlbyname("hw.memsize", &m, &l, NULL, 0) && m) return m;
#endif
    long pg = sysconf(_SC_PHYS_PAGES), ps = sysconf(_SC_PAGE_SIZE);
    return pg > 0 && ps > 0 ? (uint64_t)pg * (uint64_t)ps : (uint64_t)4 << 30;
}
/* SIZE: bytes with an optional unit K, M, G, T (powers of 1024; "512M", "1.5G"), N% of physical memory, "auto" (0),
 * "max", "unlimited" or "none" (KMEM_UNLIMITED). Returns 0 and *out, or -1. */
static int kmem_parse_size(const char *s, uint64_t *out) {
    if (!s || !*s) return -1;
    if (!strcmp(s, "auto")) { *out = 0; return 0; }
    if (!strcmp(s, "max") || !strcmp(s, "unlimited") || !strcmp(s, "none")) { *out = KMEM_UNLIMITED; return 0; }
    char *e; double v = strtod(s, &e);
    if (e == s || v < 0 || v != v) return -1;
    double m = 1;
    if (*e == '%') { if (v <= 0 || v > 100 || e[1]) return -1; *out = (uint64_t)((double)kmem_phys() * v / 100.0); return *out ? 0 : -1; }
    switch (*e) {
        case 'k': case 'K': m = 1024.0; e++; break;
        case 'm': case 'M': m = 1048576.0; e++; break;
        case 'g': case 'G': m = 1073741824.0; e++; break;
        case 't': case 'T': m = 1099511627776.0; e++; break;
        case 0: break;
        default: return -1;
    }
    if (*e == 'i' && e[1] == 'B') e += 2; else if (*e == 'B') e++;
    if (*e) return -1;
    double b = v * m;
    if (b < 1 || b >= 1.8e19) return -1;
    *out = (uint64_t)b; return 0;
}
#if defined(__linux__)
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
#endif
/* physical memory, or the cgroup's limit when lower: the base of "auto" (50%) and of N% */
static uint64_t kmem_machine(void) {
    uint64_t m = kmem_phys();
#if defined(__linux__)
    uint64_t room, lim; if (!kmem_cgroup_room(&room, &lim) && lim < m) m = lim;
#endif
    return m;
}
#if defined(__APPLE__)
static void kmem_mac_pressure(void *ctx) {
    dispatch_source_t s = ctx;
    unsigned long d = dispatch_source_get_data(s);
    if (d & (DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL)) atomic_store(&kmem_pressure_flag, 1);
}
#endif
/* the memory the system can give now (bytes), and *pressure when it reports pressure; KMEM_UNLIMITED if unknown */
static uint64_t kmem_sample_os(int *pressure) {
    *pressure = atomic_exchange(&kmem_pressure_flag, 0);
    if (kmem_sample_hook) { int p = 0; uint64_t v = kmem_sample_hook(&p); *pressure |= p; return v; }
#if defined(__linux__)
    uint64_t avail = KMEM_UNLIMITED;
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f)) if (!strncmp(line, "MemAvailable:", 13)) { avail = strtoull(line + 13, NULL, 10) * 1024ull; break; }
        fclose(f);
    }
    uint64_t room, lim; if (!kmem_cgroup_room(&room, &lim) && room < avail) avail = room;
    f = fopen("/proc/pressure/memory", "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof line, f)) { char *a = strstr(line, "avg10="); if (a && strtod(a + 6, NULL) >= KMEM_PSI_PCT) *pressure = 1; }
        fclose(f);
    }
    return avail;
#elif defined(__APPLE__)
    vm_statistics64_data_t vs; mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT; vm_size_t ps = 0;
    host_page_size(mach_host_self(), &ps);
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vs, &cnt) == KERN_SUCCESS && ps)
        return ((uint64_t)vs.free_count + vs.inactive_count + vs.purgeable_count) * (uint64_t)ps;
    return kmem_phys() / 2;
#else
    return kmem_phys() / 2;
#endif
}
/* fork: the child holds none of the parent's reservations, and the lock may belong to a thread it does not have */
static void kmem_atfork_child(void) {
    pthread_mutex_init(&kmem_mu, NULL); pthread_cond_init(&kmem_cv, NULL);
    kmem_used = 0; kmem_jobs = 0; kmem_jobs_peak = 0;
}
static void kmem_init_locked(void) {
    if (kmem_inited) return;
    kmem_inited = 1;
    pthread_atfork(NULL, NULL, kmem_atfork_child);
    const char *f = getenv("KOLMO_MEM_FORCE_BUDGET"); uint64_t v;
    if (f && !kmem_parse_size(f, &v)) kmem_forced = v ? v : KMEM_UNLIMITED;
#if defined(__APPLE__)
    dispatch_source_t s = dispatch_source_create(DISPATCH_SOURCE_TYPE_MEMORYPRESSURE, 0,
        DISPATCH_MEMORYPRESSURE_WARN | DISPATCH_MEMORYPRESSURE_CRITICAL, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
    if (s) { dispatch_set_context(s, s); dispatch_source_set_event_handler_f(s, kmem_mac_pressure); dispatch_resume(s); }
#endif
}
/* the governed budget (lock held): forced, or min(sample + reserved, previous) at once on a drop or pressure, and
 * the larger sample only after KMEM_CALM_NS of calm */
static uint64_t kmem_budget_locked(void) {
    kmem_init_locked();
    if (kmem_forced) return kmem_forced;
    uint64_t now = kmem_clock_ns();
    if (kmem_budget_cur && now - kmem_last_sample_ns < KMEM_SAMPLE_NS && !atomic_load(&kmem_pressure_flag)) return kmem_budget_cur;
    kmem_last_sample_ns = now;
    int pressure = 0; uint64_t avail = kmem_sample_os(&pressure);
    uint64_t target = avail == KMEM_UNLIMITED ? KMEM_UNLIMITED : avail + kmem_used;
    if (pressure && target > kmem_used) target = kmem_used;     /* pressure: no new concurrency until it eases */
    if (!kmem_budget_cur || target < kmem_budget_cur || pressure) {
        if (pressure || (kmem_budget_cur && target < kmem_budget_cur)) kmem_last_pressure_ns = now;
        kmem_budget_cur = target ? target : 1;
    } else if (target > kmem_budget_cur && now - kmem_last_pressure_ns >= KMEM_CALM_NS) kmem_budget_cur = target;
    return kmem_budget_cur;
}
/* The machine-wide share (core/jobs.c, docs/CONCURRENCY.md): the auto limit (50% of the machine, or what
 * kolmo_set_memlimit / --memlimit set) covers the reservations of every Kolmo process of the pool, so the room left to
 * this one is that limit minus what the others hold, re-read with the system sample (at most every 100 ms). The first
 * job of every process still runs whatever the others hold: no process waits for another to make progress. */
static uint64_t kmem_limit_of(uint64_t requested);
static uint64_t kmem_others, kmem_others_ns, kmem_others_lim;
static uint64_t kmem_shared_room_locked(void) {
    if (kj_tls_off || __atomic_load_n(&kj_state, __ATOMIC_ACQUIRE) != 1) return KMEM_UNLIMITED;
    uint64_t now = kmem_clock_ns();
    if (!kmem_others_ns || now - kmem_others_ns >= KMEM_SAMPLE_NS) {     /* the others and the limit, every 100 ms */
        uint64_t u; if (kj_scan(&u, NULL, 0) >= 0) kmem_others = u;
        kmem_others_lim = kmem_others ? kmem_limit_of(0) : KMEM_UNLIMITED;
        kmem_others_ns = now;
    }
    if (!kmem_others || kmem_others_lim == KMEM_UNLIMITED) return KMEM_UNLIMITED;
    return kmem_others_lim > kmem_others ? kmem_others_lim - kmem_others : 0;
}
static int kmem_fits_locked(uint64_t b, uint64_t cap) {
    if (!kmem_jobs) return 1;                                   /* one job always runs */
    uint64_t bud = kmem_budget_locked();
    if (cap && cap < bud) bud = cap;
    uint64_t room = kmem_shared_room_locked();
    if (room < bud) bud = room;
    return kmem_used <= bud && b <= bud - kmem_used;
}
static void kmem_take_locked(uint64_t b) {
    kmem_used += b; kmem_jobs++;
    if (kmem_jobs > kmem_jobs_peak) kmem_jobs_peak = kmem_jobs;
    kjob_pub_mem(kmem_used, kmem_jobs);
}
/* Reserve b bytes for a unit of work under the call's cap (0 or KMEM_UNLIMITED: none beyond the budget). Waits until
 * it fits; granted at once when nothing else is reserved. */
static void kmem_reserve(uint64_t b, uint64_t cap) {
    pthread_mutex_lock(&kmem_mu);
    while (!kmem_fits_locked(b, cap)) {
        struct timespec t; clock_gettime(CLOCK_REALTIME, &t);
        t.tv_nsec += 100000000; if (t.tv_nsec >= 1000000000) { t.tv_sec++; t.tv_nsec -= 1000000000; }
        pthread_cond_timedwait(&kmem_cv, &kmem_mu, &t);       /* re-sample while waiting: memory may free elsewhere */
    }
    kmem_take_locked(b);
    pthread_mutex_unlock(&kmem_mu);
}
/* optional work: reserve only if it fits now (never the first reservation's privilege); 1 when granted */
static int kmem_try_reserve(uint64_t b, uint64_t cap) {
    pthread_mutex_lock(&kmem_mu);
    int ok = kmem_jobs ? kmem_fits_locked(b, cap) : 1;
    if (ok) kmem_take_locked(b);
    pthread_mutex_unlock(&kmem_mu);
    return ok;
}
static void kmem_release(uint64_t b) {
    pthread_mutex_lock(&kmem_mu);
    kmem_used = kmem_used >= b ? kmem_used - b : 0; if (kmem_jobs) kmem_jobs--;
    kjob_pub_mem(kmem_used, kmem_jobs);
    pthread_cond_broadcast(&kmem_cv);
    pthread_mutex_unlock(&kmem_mu);
}
/* tests and benchmarks: force the budget (0 = sample the system again; KMEM_UNLIMITED = no limit), read the counters */
static void kmem_force_budget(uint64_t b) {
    pthread_mutex_lock(&kmem_mu); kmem_init_locked(); kmem_forced = b; kmem_budget_cur = 0;
    pthread_cond_broadcast(&kmem_cv); pthread_mutex_unlock(&kmem_mu);
}
static void kmem_stats(int *jobs, int *peak, uint64_t *used) {
    pthread_mutex_lock(&kmem_mu);
    if (jobs) *jobs = kmem_jobs;
    if (peak) *peak = kmem_jobs_peak;
    if (used) *used = kmem_used;
    pthread_mutex_unlock(&kmem_mu);
}
static void kmem_stats_reset_peak(void) { pthread_mutex_lock(&kmem_mu); kmem_jobs_peak = kmem_jobs; pthread_mutex_unlock(&kmem_mu); }

/* A thread inside a reserved unit of work: allocations it makes (CM tables) are covered by that reservation and do not
 * reserve again (a nested wait could deadlock two jobs that each hold a reservation). Outside one, the CM allocator
 * reserves for itself. kmem_tls_cap: the explicit cap of the current call (0: none), the one memory figure a format
 * decision (the -x table size) may read; set by the threads that code blocks for that call. */
static _Thread_local int kmem_tls_job;
static _Thread_local uint64_t kmem_tls_cap;
static _Thread_local uint64_t kmem_tls_dec_limit;   /* decoding: a model larger than this is refused (0: none) */
static _Thread_local size_t kmem_tls_raw;           /* the block being coded (bytes): the cap covers its buffers too */
/* What a thread hands to the threads it starts for the same call (trials, parts, planner samples). */
typedef struct { uint64_t cap, dec_limit; size_t raw; int kj_off; } kmem_ctx;   /* kj_off: the call left the machine pool */
static kmem_ctx kmem_ctx_get(void) { kmem_ctx c = {kmem_tls_cap, kmem_tls_dec_limit, kmem_tls_raw, kj_tls_off}; return c; }
static void kmem_ctx_set(kmem_ctx c) { kmem_tls_cap = c.cap; kmem_tls_dec_limit = c.dec_limit; kmem_tls_raw = c.raw; kj_tls_off = c.kj_off; }
/* the buffers every block needs besides its codec's own: the decoded block, the transform buffer (block + slack) and
 * the codec's output or input copy, plus 1 MiB of small state. Part of both the encoder's estimate and the figure a
 * frame declares, so the same function is used on both sides. */
static uint64_t kmem_buffers(uint64_t raw) { return 2 * raw + TBL_SLACK(raw) + ((uint64_t)1 << 20); }

/* the call's limit: an explicit cap (bytes), or auto (0) = 50% of the machine (physical memory or the cgroup limit),
 * or KMEM_UNLIMITED (max) */
static _Atomic uint64_t kmem_default_limit;   /* kolmo_set_memlimit: what "auto" means in this process (0: 50%) */
static uint64_t kmem_limit_of(uint64_t requested) {
    if (requested == KMEM_UNLIMITED) return KMEM_UNLIMITED;
    if (requested) return requested;
    uint64_t d = atomic_load_explicit(&kmem_default_limit, memory_order_relaxed);
    return d ? d : kmem_machine() / 2;
}

/* The decode working set a frame declares (header byte 11): 0 = not declared (frames written before rc3); otherwise
 * 2^(v/8) MiB, rounded up, from 1 MiB (v = 1: 1.09 MiB) by steps of 9%. kmem_code rounds up, so the declared figure
 * is never below the need. */
static const uint32_t kmem_steps[8] = {1048576, 1143481, 1246975, 1359835, 1482911, 1617126, 1763488, 1923097};
static uint64_t kmem_declared(uint8_t v) { return v ? (uint64_t)kmem_steps[v & 7] << (v >> 3) : 0; }
static uint8_t kmem_code(uint64_t need) {
    for (int v = 1; v < 256; v++) if (kmem_declared((uint8_t)v) >= need) return (uint8_t)v;
    return 255;
}
