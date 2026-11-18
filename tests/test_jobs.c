/* The machine-wide pool (src/core/jobs.c, docs/CONCURRENCY.md): tokens are taken and given back, a process killed
 * with SIGKILL leaves no token and no record behind, the memory ledger lowers the others' room, a forked child does not
 * share its parent's tokens, an unusable directory falls back to the in-process governor, and the frames are the same
 * bytes whatever the contention (all tokens held elsewhere, some, released in the middle of a run, one token, pool
 * off). Includes the library to reach its internals. --quick: smaller inputs (sanitizer builds). */
#include "../src/kolmo.c"
#include <signal.h>
#include <sys/wait.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
static const uint64_t MiB = 1u << 20;
static char dir[256];
static int quick;

static uint8_t *text_input(size_t n, uint32_t seed) {
    uint8_t *b = malloc(n); size_t o = 0; uint32_t s = seed;
    static const char *w[] = {"tokens", "change", "speed", "never", "bytes", "kolmo", "block", "pool", "the", "a", "of", "frame", "machine", "worker"};
    while (o < n) {
        s = s * 1103515245u + 12345u; const char *x = w[(s >> 16) % 14]; size_t l = strlen(x);
        for (size_t i = 0; i < l && o < n; i++) b[o++] = (uint8_t)x[i];
        if (o < n) b[o++] = (s >> 8) % 11 ? ' ' : '\n';
        if (o + 12 < n && (s >> 20) % 97 == 0) o += (size_t)snprintf((char *)b + o, 12, "%u,", s % 100000);
    }
    return b;
}
static uint8_t *binary_input(size_t n, uint32_t seed) {
    uint8_t *b = malloc(n); uint32_t s = seed, v = 0;
    for (size_t i = 0; i < n; i++) { s = s * 1664525u + 1013904223u; v += (s >> 28) - 7; b[i] = (uint8_t)(i % 4 ? v : s >> 24); }
    return b;
}
/* a child that takes k tokens (and reserves mem bytes), says so on the pipe, then waits for its end; with hold_ms > 0
 * it gives them back after that long and exits */
static pid_t holder(int k, uint64_t mem, int hold_ms) {
    int p[2]; CHECK(!pipe(p));
    pid_t pid = fork(); CHECK(pid >= 0);
    if (!pid) {
        close(p[0]); CHECK(kjob_on());
        int t[KJ_MAX], got = 0;
        for (int i = 0; i < k; i++) if ((t[got] = kjob_take()) >= 0) got++;
        if (mem) kmem_reserve(mem, KMEM_UNLIMITED);
        char c = (char)got; if (write(p[1], &c, 1) != 1) _exit(3);
        if (hold_ms > 0) { usleep((useconds_t)hold_ms * 1000); for (int i = 0; i < got; i++) kjob_give(t[i]); if (mem) kmem_release(mem); _exit(0); }
        for (;;) pause();
    }
    close(p[1]); char c = 0; CHECK(read(p[0], &c, 1) == 1); close(p[0]);
    CHECK(c == k);
    return pid;
}
static void reap(pid_t pid, int sig) { if (sig) kill(pid, sig); int st; CHECK(waitpid(pid, &st, 0) == pid); }
static int records(void) {      /* m.* files in the pool's directory */
    DIR *d = opendir(dir); int n = 0; struct dirent *e;
    while (d && (e = readdir(d))) if (e->d_name[0] == 'm' && e->d_name[1] == '.') n++;
    if (d) closedir(d);
    return n;
}

static void tokens_basic(void) {
    CHECK(kjob_on());
    CHECK(kj_n == 4);
    int t[5];
    for (int i = 0; i < 4; i++) { t[i] = kjob_take(); CHECK(t[i] >= 0); }
    CHECK(kjob_take() == KJ_REFUSED);
    kolmo_jobs_info in; CHECK(kolmo_jobs_status(&in, NULL, 0) == 0);
    CHECK(in.enabled && in.tokens == 4 && in.busy == 4 && in.held_here == 4);
    CHECK(kolmo_jobs_shared(1) == -1);                      /* tokens held: cannot restart */
    for (int i = 0; i < 4; i++) kjob_give(t[i]);
    CHECK(kolmo_jobs_status(&in, NULL, 0) == 0 && in.busy == 0 && in.held_here == 0);
    /* a call that stays out of the pool needs no token */
    kj_tls_off = 1; CHECK(kjob_take() == KJ_NONE); kj_tls_off = 0;
    printf("tokens: take, refuse, give back: ok\n");
}
static void kill9(void) {
    pid_t pid = holder(4, 64 * MiB, 0);
    CHECK(kjob_take() == KJ_REFUSED);                        /* the child holds them all */
    kolmo_jobs_proc pr[8]; kolmo_jobs_info in;
    CHECK(kolmo_jobs_status(&in, pr, 8) == 1);
    CHECK(in.busy == 4 && pr[0].pid == pid && pr[0].tokens == 4 && in.reserved == 64 * MiB);
    reap(pid, SIGKILL);
    int t[4];
    for (int i = 0; i < 4; i++) { t[i] = kjob_take(); CHECK(t[i] >= 0); }    /* no stale token */
    for (int i = 0; i < 4; i++) kjob_give(t[i]);
    CHECK(kolmo_jobs_status(&in, NULL, 0) == 0 && in.reserved == 0);           /* the dead record is gone */
    CHECK(records() == 1);                                                      /* ours */
    /* many children killed at random moments: never a stale token */
    for (int r = 0; r < (quick ? 8 : 40); r++) {
        pid_t c = fork(); CHECK(c >= 0);
        if (!c) { for (;;) { int a = kjob_take(), b = kjob_take(); kmem_reserve(MiB, KMEM_UNLIMITED); kjob_give(a); kmem_release(MiB); kjob_give(b); } }
        usleep((useconds_t)(200 + (r * 7919) % 3000));
        reap(c, SIGKILL);
    }
    for (int i = 0; i < 4; i++) { t[i] = kjob_take(); CHECK(t[i] >= 0); }
    for (int i = 0; i < 4; i++) kjob_give(t[i]);
    CHECK(kolmo_jobs_status(&in, NULL, 0) == 0 && records() == 1);
    printf("kill -9: no stale tokens or records: ok\n");
}
static void memory_ledger(void) {
    kolmo_set_memlimit(4096 * MiB);
    pid_t pid = holder(0, 3072 * MiB, 0);
    kmem_others_ns = 0;                                       /* read the others now */
    kmem_force_budget(KMEM_UNLIMITED);
    kmem_reserve(64 * MiB, KMEM_UNLIMITED);                   /* the first job always runs */
    CHECK(!kmem_try_reserve(2048 * MiB, KMEM_UNLIMITED));     /* 4 GiB shared, 3 GiB held by the child */
    CHECK(kmem_try_reserve(512 * MiB, KMEM_UNLIMITED));
    kmem_release(512 * MiB);
    kj_tls_off = 1; CHECK(kmem_try_reserve(2048 * MiB, KMEM_UNLIMITED)); kmem_release(2048 * MiB); kj_tls_off = 0;
    reap(pid, SIGKILL);
    kmem_others_ns = 0;
    CHECK(kmem_try_reserve(2048 * MiB, KMEM_UNLIMITED));      /* the dead child's memory is free again */
    kmem_release(2048 * MiB); kmem_release(64 * MiB);
    kmem_force_budget(0); kolmo_set_memlimit(0); kmem_others_ns = 0;
    printf("memory ledger: the others' reservations count, the dead's do not: ok\n");
}
static void fork_child(void) {
    int t0 = kjob_take(), t1 = kjob_take(); CHECK(t0 >= 0 && t1 >= 0);
    pid_t c = fork(); CHECK(c >= 0);
    if (!c) {     /* the child starts afresh: the parent's two are not its own, the other two are free */
        int a = kjob_take(), b = kjob_take();
        _exit(a >= 0 && b >= 0 && kjob_take() == KJ_REFUSED ? 0 : 1);
    }
    int st; CHECK(waitpid(c, &st, 0) == c && WIFEXITED(st) && WEXITSTATUS(st) == 0);
    /* the parent still holds its tokens after the child exited: a fresh descriptor cannot lock them */
    char p[600]; snprintf(p, sizeof p, "%s/c.%d", dir, t0);
    int fd = open(p, O_RDONLY); CHECK(fd >= 0); CHECK(flock(fd, LOCK_EX | LOCK_NB)); close(fd);
    kjob_give(t0); kjob_give(t1);
    printf("fork: the child does not share or release the parent's tokens: ok\n");
}

typedef struct { const char *name; uint8_t *in; size_t n; kolmo_params p; uint8_t *ref; int64_t rn; } tcase;
static int64_t enc(tcase *c, uint8_t *out, size_t cap, int shared) {
    kolmo_params p = c->p; p.jobs_shared = shared;
    return kolmo_compress(c->in, c->n, out, cap, &p);
}
/* compress (and decode) under one contention scenario: the frame must be the reference's bytes */
static void same_frames(tcase *cs, int nc, const char *what, int expect_one) {
    for (int k = 0; k < nc; k++) {
        tcase *c = &cs[k]; size_t cap = kolmo_bound(c->n, &c->p); uint8_t *o = malloc(cap), *d = malloc(c->n + 1);
        kmem_stats_reset_peak();
        int64_t r = enc(c, o, cap, 0); CHECK(r > 0);
        int peak; kmem_stats(NULL, &peak, NULL);
        if (r != c->rn || memcmp(o, c->ref, (size_t)r)) { fprintf(stderr, "FAIL %s: %s frame differs\n", what, c->name); exit(1); }
        if (expect_one && peak != 1) { fprintf(stderr, "FAIL %s: %s ran %d jobs at once with no token free\n", what, c->name, peak); exit(1); }
        CHECK(kolmo_decompress(o, (size_t)r, d, c->n, c->p.threads) == (int64_t)c->n && !memcmp(d, c->in, c->n));
        printf("  %-10s %-24s %lld bytes, %d job%s at once\n", c->name, what, (long long)r, peak, peak == 1 ? "" : "s");
        free(o); free(d);
    }
}
static void determinism(void) {
    size_t tn = quick ? 3 * MiB : 12 * MiB, bn = quick ? 2 * MiB : 6 * MiB, xn = quick ? 192 << 10 : 768 << 10;
    tcase cs[] = {
        {"balanced", text_input(tn, 7), tn, {KOLMO_FAST, 0, 4, 512 << 10, 1, 0, 0}, NULL, 0},
        {"bin-bal", binary_input(bn, 9), bn, {KOLMO_FAST, 0, 4, 256 << 10, 1, 0, 0}, NULL, 0},
        {"ratio", text_input(tn / 2, 11), tn / 2, {KOLMO_MAX, 0, 4, 1 << 20, 1, 0, 0}, NULL, 0},
        {"-x", text_input(xn, 13), xn, {KOLMO_ULTRA, 1, 4, 128 << 10, 1, 0, 0}, NULL, 0},
    };
    const int nc = (int)(sizeof cs / sizeof cs[0]);
    for (int k = 0; k < nc; k++) {          /* the reference: pool off */
        size_t cap = kolmo_bound(cs[k].n, &cs[k].p); cs[k].ref = malloc(cap);
        cs[k].rn = enc(&cs[k], cs[k].ref, cap, -1); CHECK(cs[k].rn > 0);
    }
    printf("frames under contention (reference: pool off):\n");
    same_frames(cs, nc, "pool on, no contention", 0);
    pid_t h = holder(4, 0, 0); same_frames(cs, nc, "all 4 tokens elsewhere", 1); reap(h, SIGKILL);
    h = holder(3, 0, 0); same_frames(cs, nc, "3 of 4 elsewhere", 0); reap(h, SIGKILL);
    h = holder(4, 0, 60); same_frames(cs, nc, "freed after 60 ms", 0); reap(h, 0);
    /* every case in parallel in four processes, while holders come and go */
    pid_t kids[4];
    for (int k = 0; k < 4; k++) {
        kids[k] = fork(); CHECK(kids[k] >= 0);
        if (!kids[k]) {
            for (int rep = 0; rep < 2; rep++) for (int c = 0; c < nc; c++) {
                size_t cap = kolmo_bound(cs[c].n, &cs[c].p); uint8_t *o = malloc(cap);
                int64_t r = enc(&cs[c], o, cap, 0);
                if (r != cs[c].rn || memcmp(o, cs[c].ref, (size_t)r)) _exit(1);
                free(o);
            }
            _exit(0);
        }
    }
    for (int k = 0; k < 4; k++) { int st; CHECK(waitpid(kids[k], &st, 0) == kids[k] && WIFEXITED(st) && !WEXITSTATUS(st)); }
    printf("  4 processes x %d cases x 2, sharing 4 tokens: same frames\n", nc);
    /* one token in the pool */
    setenv("KOLMO_JOBS", "1", 1); CHECK(!kolmo_jobs_shared(1)); CHECK(kjob_on() && kj_n == 1);
    same_frames(cs, nc, "a pool of one token", 0);
    setenv("KOLMO_JOBS", "4", 1); CHECK(!kolmo_jobs_shared(1)); CHECK(kjob_on() && kj_n == 4);
    /* a directory that cannot be used: the in-process governor alone */
    setenv("KOLMO_JOBS_DIR", "/proc/kolmo-no-such-dir", 1); CHECK(!kolmo_jobs_shared(1));
    kolmo_jobs_info in; CHECK(kolmo_jobs_status(&in, NULL, 0) == -1 && !in.enabled);
    CHECK(kjob_take() == KJ_NONE);
    same_frames(cs, nc, "pool unavailable", 0);
    setenv("KOLMO_JOBS_DIR", dir, 1); CHECK(!kolmo_jobs_shared(1)); CHECK(kjob_on());
    for (int k = 0; k < nc; k++) { free(cs[k].in); free(cs[k].ref); }
}

int main(int argc, char **argv) {
    quick = argc > 1 && !strcmp(argv[1], "--quick");
    snprintf(dir, sizeof dir, "/tmp/kolmo-test-jobs-%ld", (long)getpid());
    setenv("KOLMO_JOBS_DIR", dir, 1); setenv("KOLMO_JOBS", "4", 1); unsetenv("KOLMO_JOBS_SHARED");
    CHECK(!mkdir(dir, 0700));
    tokens_basic();
    kill9();
    memory_ledger();
    fork_child();
    determinism();
    CHECK(!kolmo_jobs_shared(0));                            /* removes our record */
    DIR *d = opendir(dir); struct dirent *e; char p[600];
    while (d && (e = readdir(d))) if (e->d_name[0] != '.') { snprintf(p, sizeof p, "%s/%s", dir, e->d_name); unlink(p); }
    if (d) closedir(d);
    rmdir(dir);
    printf("test_jobs: all passed\n");
    return 0;
}
