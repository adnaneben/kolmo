/* The machine-wide pool (docs/CONCURRENCY.md): worker tokens and memory reservations shared by every Kolmo process of
 * the user (or, on request, of the machine), in the manner of GNU make's jobserver, so that four `kolmo -T 10` on a
 * ten-core machine share ten workers instead of running forty.
 *
 * What the kernel cleans up when a process dies, and nothing else:
 *   - a token is an exclusive flock(2) on one slot file, DIR/c.<i> (i < tokens). The lock belongs to the open file
 *     description, so it is gone the moment the process exits, however it exits (kill -9 included): there are no
 *     stale tokens to repair. Taking one never blocks (LOCK_NB).
 *   - every process publishes what it holds (memory reserved, block jobs, tokens) in its own record, DIR/m.<pid>.<n>,
 *     which it keeps exclusively flock'd for its life. A reader that can lock a record has found a dead owner, and
 *     removes it. The record is created under a temporary name, locked, then renamed, so no reader sees it unlocked.
 * DIR: KOLMO_JOBS_DIR, or $XDG_RUNTIME_DIR/kolmo-jobs, or $TMPDIR (else /tmp)/kolmo-jobs-<uid> (mode 0700, ours, not
 * writable by others); system-wide sharing (KOLMO_JOBS_SHARED=system): /tmp/kolmo-jobs (mode 1777, files 0666).
 * When the directory cannot be used (read-only, sandbox, another owner) the pool is off and the in-process governor
 * works alone, as before. Windows (not built) would use LockFileEx on one byte per slot of one file under
 * %LOCALAPPDATA%\kolmo, which the system releases with the process; see docs/CONCURRENCY.md.
 *
 * Rules (deadlock freedom, progress):
 *   - the caller of a pool run (the guaranteed worker) never waits for a token: it takes one if one is free, so that
 *     it counts, and runs without one otherwise. Every instance always makes progress.
 *   - a helper worker takes a token before each block job and gives it back when the job ends; while no token is free
 *     it waits (polling, never blocking in the kernel) only as long as blocks remain, holding nothing.
 *   - optional extra threads (trials, parts, inner parallel loops) only try; refused, the work runs on the thread
 *     that owns it, with the same result.
 *   - lock order: token (flock, non-blocking) -> kmem_mu (memory.c) -> kj_mu. No path takes kmem_mu while holding
 *     kj_mu, and no thread waits on another process: cross-process state is only polled.
 * Determinism: tokens decide how many threads run, never what they write (the thread counts that reach the format,
 * such as the threads left over per block, come from the requested -T, not from the tokens granted). */
#include <stddef.h>
#include <time.h>
#if !defined(_WIN32)
#include <sys/file.h>
#include <dirent.h>
#include <fcntl.h>
#endif
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#define KJ_MAX 256
#define KJ_NONE (-2)        /* no token needed: the pool is off for this thread, or the thread is not in a pool run */
#define KJ_REFUSED (-1)     /* no token free (optional work), or no work left (a waiting helper) */
#define KJ_MAGIC 0x4b4a4231u /* "KJB1" */
typedef struct { uint32_t magic; int32_t pid; int32_t tokens; int32_t jobs; uint64_t used; uint64_t limit; uint64_t ntok; } kj_rec;
static pthread_mutex_t kj_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t kj_cv = PTHREAD_COND_INITIALIZER;
static int kj_state;                 /* 0: not set up; 1: on; -1: off or unavailable (in-process governor alone) */
static int kj_mode_api = -1;         /* kolmo_jobs_shared(): -1 not set (environment), 0 off, 1 user, 2 system */
static int kj_mode;                  /* resolved: 0 off, 1 user, 2 system */
static int kj_n;                     /* tokens */
static char kj_dir[512];
static int kj_fd1[KJ_MAX];           /* slot file descriptors + 1 (0: not opened yet) */
static unsigned char kj_held[KJ_MAX];
static int kj_nheld, kj_hint, kj_poller, kj_waiting;
static unsigned kj_gen;              /* bumped by every local release: waiting helpers try again */
static int kj_rec_fd = -1;           /* our record, flock'd for our life */
static char kj_rec_name[64];
static const char *kj_why = "not started";   /* why the pool is off (kolmo --jobs-status) */
static atomic_ullong kj_stat_waits, kj_stat_refused;
static _Thread_local int kj_tls_role;   /* 0: not in a pool run; 1: the caller of a run (guaranteed); 2: a helper */
static _Thread_local int kj_tls_off;    /* this call turned the pool off (kolmo_params.jobs_shared < 0) */

/* physical cores this process may run on: Linux, distinct core_cpus/thread_siblings lists among the CPUs of our
 * affinity; macOS, hw.physicalcpu; else the online CPUs */
static int kj_physical_cores(void) {
    long on = sysconf(_SC_NPROCESSORS_ONLN); int n = on > 0 ? (int)on : 1;
#if defined(__APPLE__)
    int pc = 0; size_t l = sizeof pc;
    if (!sysctlbyname("hw.physicalcpu", &pc, &l, NULL, 0) && pc > 0) n = pc;
#elif defined(__linux__)
    cpu_set_t set; CPU_ZERO(&set);
    if (!sched_getaffinity(0, sizeof set, &set)) {
        int cores = 0;
        for (int c = 0; c < CPU_SETSIZE && c < 4096; c++) {
            if (!CPU_ISSET(c, &set)) continue;
            char p[128], b[256]; FILE *f;
            snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", c);
            if (!(f = fopen(p, "r"))) { cores++; continue; }
            int ok = fgets(b, sizeof b, f) != NULL; fclose(f);
            /* count a core once: at the first of its siblings that we may use */
            int first = ok ? atoi(b) : c, seen = 0;
            for (int d = first; d < c && !seen; d++) if (CPU_ISSET(d, &set)) {
                snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", d);
                if ((f = fopen(p, "r"))) { char b2[256]; if (fgets(b2, sizeof b2, f) && !strcmp(b, b2)) seen = 1; fclose(f); }
            }
            if (!seen) cores++;
        }
        if (cores > 0) n = cores;
    }
#endif
    return n < 1 ? 1 : n > KJ_MAX ? KJ_MAX : n;
}
static int kj_parse_mode(const char *s) {
    if (!s || !*s) return -1;
    if (!strcmp(s, "off") || !strcmp(s, "0") || !strcmp(s, "no") || !strcmp(s, "false")) return 0;
    if (!strcmp(s, "on") || !strcmp(s, "1") || !strcmp(s, "yes") || !strcmp(s, "true") || !strcmp(s, "user")) return 1;
    if (!strcmp(s, "system")) return 2;
    return -1;
}
#if !defined(_WIN32)
static void kj_pub(size_t off, const void *v, size_t n) {   /* one field of our record */
    if (kj_rec_fd >= 0) { ssize_t w = pwrite(kj_rec_fd, v, n, (off_t)off); (void)w; }
}
static void kj_pub_tokens_locked(void) { int32_t t = kj_nheld; kj_pub(offsetof(kj_rec, tokens), &t, sizeof t); }
/* fork: the child shares our open file descriptions, so it must not use (or unlock) them; it starts afresh */
static void kj_atfork_prepare(void) { pthread_mutex_lock(&kj_mu); }
static void kj_atfork_parent(void) { pthread_mutex_unlock(&kj_mu); }
static void kj_atfork_child(void) {
    for (int i = 0; i < KJ_MAX; i++) { if (kj_fd1[i]) close(kj_fd1[i] - 1); kj_fd1[i] = 0; kj_held[i] = 0; }
    if (kj_rec_fd >= 0) close(kj_rec_fd);
    kj_rec_fd = -1; kj_nheld = kj_poller = kj_waiting = 0; kj_state = 0; kj_why = "not started";
    pthread_mutex_unlock(&kj_mu);
}
static int kj_dir_ok(const char *d, int system) {
    struct stat st;
    if (lstat(d, &st) || !S_ISDIR(st.st_mode)) return 0;
    if (system) return st.st_uid == geteuid() || (st.st_mode & S_ISVTX);     /* a shared directory must be sticky */
    return st.st_uid == geteuid() && !(st.st_mode & 022);
}
static int kj_setup_dir(int system) {
    const char *e = getenv("KOLMO_JOBS_DIR");
    if (e && *e) snprintf(kj_dir, sizeof kj_dir, "%s", e);
    else if (system) snprintf(kj_dir, sizeof kj_dir, "/tmp/kolmo-jobs");
    else {
        const char *x = getenv("XDG_RUNTIME_DIR"), *t = getenv("TMPDIR");
        struct stat st;
        if (x && *x && !stat(x, &st) && S_ISDIR(st.st_mode) && st.st_uid == geteuid()) snprintf(kj_dir, sizeof kj_dir, "%s/kolmo-jobs", x);
        else snprintf(kj_dir, sizeof kj_dir, "%s/kolmo-jobs-%lu", t && *t ? t : "/tmp", (unsigned long)geteuid());
    }
    if (strlen(kj_dir) > sizeof kj_dir - 80) { kj_why = "directory path too long"; return -1; }
    if (!mkdir(kj_dir, system ? 01777 : 0700) && system) chmod(kj_dir, 01777);     /* past the umask */
    if (!kj_dir_ok(kj_dir, system)) { kj_why = "directory missing, not ours or writable by others"; return -1; }
    return 0;
}
/* our record: created under a temporary name, locked, filled, then renamed (a reader never sees it unlocked) */
static int kj_setup_record(void) {
    char tmp[600], fin[600];
    static atomic_uint seq;
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    unsigned s = atomic_fetch_add(&seq, 1) * 0x9e3779b9u + (unsigned)now.tv_nsec;
    snprintf(kj_rec_name, sizeof kj_rec_name, "m.%ld.%08x", (long)getpid(), s);
    snprintf(tmp, sizeof tmp, "%s/t.%ld.%08x", kj_dir, (long)getpid(), s);
    snprintf(fin, sizeof fin, "%s/%s", kj_dir, kj_rec_name);
    int fd = open(tmp, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, kj_mode == 2 ? 0644 : 0600);
    if (fd < 0) { kj_why = "cannot create files in the directory"; return -1; }
    if (flock(fd, LOCK_EX | LOCK_NB)) { close(fd); unlink(tmp); kj_why = "flock is not supported there"; return -1; }
    kj_rec r = {KJ_MAGIC, (int32_t)getpid(), 0, 0, 0, 0, (uint64_t)kj_n};
    if (pwrite(fd, &r, sizeof r, 0) != (ssize_t)sizeof r || rename(tmp, fin)) { close(fd); unlink(tmp); kj_why = "cannot write the record"; return -1; }
    kj_rec_fd = fd;
    return 0;
}
#endif
static int kj_par_take(void);
static void kj_par_give(int t);
typedef struct { int32_t pid, tokens, jobs; uint64_t used, limit; } kj_proc;
static int kj_scan(uint64_t *used, kj_proc *procs, int max);
static void kj_setup_locked(void) {
    if (kj_state) return;
    kj_state = -1;
#if defined(_WIN32)
    kj_why = "not built for this system"; return;
#else
    static int forked; if (!forked) { forked = 1; pthread_atfork(kj_atfork_prepare, kj_atfork_parent, kj_atfork_child); }
    int m = kj_mode_api >= 0 ? kj_mode_api : kj_parse_mode(getenv("KOLMO_JOBS_SHARED"));
    kj_mode = m < 0 ? 1 : m;
    if (!kj_mode) { kj_why = "turned off (--jobs-shared=off, KOLMO_JOBS_SHARED=off or kolmo_jobs_shared(0))"; return; }
    const char *t = getenv("KOLMO_JOBS"); int n = t && *t ? atoi(t) : 0;
    kj_n = n >= 1 ? (n > KJ_MAX ? KJ_MAX : n) : kj_physical_cores();
    if (kj_setup_dir(kj_mode == 2) || kj_setup_record()) return;
    kj_hint = (int)(getpid() % kj_n);
    kolmo_par_take = kj_par_take; kolmo_par_give = kj_par_give;   /* the inner loops of src/algo (par.h) */
    kj_state = 1; kj_why = "on";
    { uint64_t u; kj_scan(&u, NULL, 0); }                          /* removes the records of processes that died */
#endif
}
static int kjob_on(void) {          /* the pool is on in this process (sets it up the first time) */
    int s = __atomic_load_n(&kj_state, __ATOMIC_ACQUIRE);
    if (!s) { pthread_mutex_lock(&kj_mu); kj_setup_locked(); s = kj_state; pthread_mutex_unlock(&kj_mu); }
    return s == 1;
}
#if !defined(_WIN32)
static int kj_slot_fd_locked(int i) {
    if (kj_fd1[i]) return kj_fd1[i] - 1;
    char p[600]; snprintf(p, sizeof p, "%s/c.%d", kj_dir, i);
    int fd = open(p, O_RDWR | O_CREAT | O_CLOEXEC, kj_mode == 2 ? 0666 : 0600);
    if (fd >= 0 && kj_mode == 2) fchmod(fd, 0666);               /* shared: anyone may lock it (fails if not ours) */
    if (fd < 0) fd = open(p, O_RDONLY | O_CLOEXEC);              /* flock works on a read-only descriptor */
    if (fd < 0) return -1;
    kj_fd1[i] = fd + 1; return fd;
}
static int kj_take_locked(void) {
    for (int k = 0; k < kj_n; k++) {
        int i = (kj_hint + k) % kj_n;
        if (kj_held[i]) continue;
        int fd = kj_slot_fd_locked(i); if (fd < 0) continue;
        if (!flock(fd, LOCK_EX | LOCK_NB)) {
            kj_held[i] = 1; kj_nheld++; kj_hint = (i + 1) % kj_n; kj_pub_tokens_locked(); return i;
        }
    }
    return KJ_REFUSED;
}
#endif
/* optional work: a token if one is free now (KJ_REFUSED otherwise), KJ_NONE when the pool is off for this thread */
static int kjob_take(void) {
    if (kj_tls_off || !kjob_on()) return KJ_NONE;
#if defined(_WIN32)
    return KJ_NONE;
#else
    pthread_mutex_lock(&kj_mu); int t = kj_take_locked(); pthread_mutex_unlock(&kj_mu);
    if (t < 0) atomic_fetch_add(&kj_stat_refused, 1);
    return t;
#endif
}
static void kjob_give(int t) {
    if (t < 0) return;
#if !defined(_WIN32)
    pthread_mutex_lock(&kj_mu);
    if (t < kj_n && kj_held[t]) {
        flock(kj_fd1[t] - 1, LOCK_UN); kj_held[t] = 0; kj_nheld--; kj_gen++; kj_pub_tokens_locked();
        if (kj_waiting) pthread_cond_broadcast(&kj_cv);
    }
    pthread_mutex_unlock(&kj_mu);
#endif
}
/* wake helpers waiting for a token (the run has no blocks left) */
static void kjob_wake(void) {
    if (__atomic_load_n(&kj_state, __ATOMIC_ACQUIRE) != 1) return;
    pthread_mutex_lock(&kj_mu); if (kj_waiting) pthread_cond_broadcast(&kj_cv); pthread_mutex_unlock(&kj_mu);
}
/* Before each block job of a pool run. The caller of the run takes a token if one is free and runs anyway (KJ_NONE
 * otherwise). A helper waits for one while blocks remain (*next < lim), holding nothing, and gets KJ_REFUSED when
 * none remain. One waiting thread per process polls the slots (1 to 8 ms apart); the others try again when a token is
 * released in this process, and look every 20 ms whether blocks remain. Outside a pool run: KJ_NONE. */
static int kjob_block_begin(atomic_uint *next, uint32_t lim) {
    if (!kj_tls_role || kj_tls_off || !kjob_on()) return KJ_NONE;
#if defined(_WIN32)
    return KJ_NONE;
#else
    pthread_mutex_lock(&kj_mu);
    int t = kj_take_locked();
    if (t >= 0 || kj_tls_role == 1) { pthread_mutex_unlock(&kj_mu); return t >= 0 ? t : KJ_NONE; }
    atomic_fetch_add(&kj_stat_waits, 1);
    kj_waiting++;
    unsigned ms = 1, gen = kj_gen;
    for (;;) {
        if (atomic_load_explicit(next, memory_order_relaxed) >= lim) { t = KJ_REFUSED; break; }
        int poll = !kj_poller; if (poll) kj_poller = 1;
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        long ns = (long)(poll ? ms : 20) * 1000000L;
        ts.tv_nsec += ns; while (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&kj_cv, &kj_mu, &ts);
        if (poll) { kj_poller = 0; if (ms < 8) ms *= 2; }
        if (atomic_load_explicit(next, memory_order_relaxed) >= lim) { t = KJ_REFUSED; break; }
        if (poll || gen != kj_gen) { gen = kj_gen; if ((t = kj_take_locked()) >= 0) break; }
    }
    kj_waiting--;
    pthread_mutex_unlock(&kj_mu);
    return t;
#endif
}
/* the inner parallel loops of src/algo (kolmo_par_for, crews): each extra thread is optional work */
static int kj_par_take(void) { return kjob_take(); }
static void kj_par_give(int t) { kjob_give(t); }

/* --- memory: our record and the others' */
static void kjob_pub_mem(uint64_t used, int jobs) {   /* memory.c, kmem_mu held */
#if !defined(_WIN32)
    if (__atomic_load_n(&kj_state, __ATOMIC_ACQUIRE) != 1) return;
    int32_t j = jobs; kj_pub(offsetof(kj_rec, jobs), &j, sizeof j);
    kj_pub(offsetof(kj_rec, used), &used, sizeof used);
#else
    (void)used; (void)jobs;
#endif
}
/* Reads the other live processes' records (and removes the dead ones'): their memory reserved, and up to max entries
 * in procs. Returns the number of live processes, others only, or -1 when the pool is off. */
static int kj_scan(uint64_t *used, kj_proc *procs, int max) {
    *used = 0;
#if defined(_WIN32)
    (void)procs; (void)max; return -1;
#else
    if (__atomic_load_n(&kj_state, __ATOMIC_ACQUIRE) != 1) return -1;
    DIR *d = opendir(kj_dir); if (!d) return -1;
    int live = 0; struct dirent *de;
    while ((de = readdir(d))) {
        const char *nm = de->d_name;
        int rec = nm[0] == 'm' && nm[1] == '.', tmp = nm[0] == 't' && nm[1] == '.';
        if ((!rec && !tmp) || !strcmp(nm, kj_rec_name)) continue;
        char p[800]; snprintf(p, sizeof p, "%s/%s", kj_dir, nm);
        int fd = open(p, O_RDONLY | O_CLOEXEC); if (fd < 0) continue;
        if (!flock(fd, LOCK_SH | LOCK_NB)) {
            /* nobody holds it: its process died (a temporary name only once it is a minute old: it may be in the
             * instant between its creation and its lock) */
            struct stat st;
            if (rec || (!fstat(fd, &st) && time(NULL) - st.st_mtime > 60)) unlink(p);
            close(fd); continue;
        }
        kj_rec r;
        if (rec && pread(fd, &r, sizeof r, 0) == (ssize_t)sizeof r && r.magic == KJ_MAGIC) {
            *used += r.used;
            if (procs && live < max) { kj_proc q = {r.pid, r.tokens, r.jobs, r.used, r.limit}; procs[live] = q; }
            live++;
        }
        close(fd);
    }
    closedir(d);
    return live;
#endif
}
/* busy slots, by trying each one we do not hold (and letting it go at once): for kolmo --jobs-status */
static int kj_busy_slots(void) {
#if defined(_WIN32)
    return 0;
#else
    if (!kjob_on()) return 0;
    pthread_mutex_lock(&kj_mu);
    int busy = kj_nheld;
    for (int i = 0; i < kj_n; i++) {
        if (kj_held[i]) continue;
        int fd = kj_slot_fd_locked(i);
        if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB)) busy++; else flock(fd, LOCK_UN);
    }
    pthread_mutex_unlock(&kj_mu);
    return busy;
#endif
}

/* --- public */
static uint64_t kmem_limit_of(uint64_t requested);   /* core/memory.c */
int kolmo_jobs_shared(int mode) {
    if (mode < -1 || mode > 2) return -1;
    pthread_mutex_lock(&kj_mu);
    if (kj_nheld) { pthread_mutex_unlock(&kj_mu); return -1; }
#if !defined(_WIN32)
    for (int i = 0; i < KJ_MAX; i++) { if (kj_fd1[i]) close(kj_fd1[i] - 1); kj_fd1[i] = 0; }   /* start again */
    if (kj_rec_fd >= 0) {
        char p[600]; snprintf(p, sizeof p, "%s/%s", kj_dir, kj_rec_name); unlink(p);
        close(kj_rec_fd); kj_rec_fd = -1;
    }
#endif
    kj_mode_api = mode; __atomic_store_n(&kj_state, 0, __ATOMIC_RELEASE); kj_why = "not started";
    pthread_mutex_unlock(&kj_mu);
    return 0;
}
int kolmo_jobs_status(kolmo_jobs_info *info, kolmo_jobs_proc *procs, int max) {
    kolmo_jobs_info z; memset(&z, 0, sizeof z); if (!info) info = &z;
    memset(info, 0, sizeof *info);
    int on = kjob_on();
    pthread_mutex_lock(&kj_mu);
    info->enabled = on; info->mode = on ? kj_mode : 0; info->tokens = on ? kj_n : 0; info->held_here = kj_nheld;
    snprintf(info->dir, sizeof info->dir, "%s", on ? kj_dir : "");
    snprintf(info->state, sizeof info->state, "%s", kj_why);
    pthread_mutex_unlock(&kj_mu);
    if (!on) return -1;
    info->busy = kj_busy_slots();
    kj_proc pr[256]; uint64_t used = 0;
    int live = kj_scan(&used, pr, 256);
    info->processes = live < 0 ? 0 : live; info->reserved = used; info->limit = kmem_limit_of(0);
    for (int i = 0; i < live && i < 256 && procs && i < max; i++) {
        procs[i].pid = pr[i].pid; procs[i].tokens = pr[i].tokens; procs[i].jobs = pr[i].jobs; procs[i].reserved = pr[i].used;
    }
    return info->processes;
}
