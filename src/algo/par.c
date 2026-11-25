/* Fork-join helpers for the algorithms in src/algo. See par.h.
   Copyright 2026 Adnane Aqartit and contributors. Apache-2.0. */
#include "par.h"
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#define PAR_MAX 256
#define SPIN_LIMIT 20000      /* idle crew workers: pause loops before sleeping */
#define BARRIER_SPINS 4000

static inline void cpu_relax(void) {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#endif
}

/* ---- kolmo_par_for ---- */

typedef struct { kolmo_par_fn fn; void *ctx; int ntasks; atomic_int next; } par_job;
typedef struct { par_job *job; int worker; } par_arg;

static void par_loop(par_job *j, int worker) {
    for (;;) {
        int t = atomic_fetch_add_explicit(&j->next, 1, memory_order_relaxed);
        if (t >= j->ntasks) break;
        j->fn(j->ctx, t, worker);
    }
}
static void *par_main(void *p) { par_arg *a = (par_arg *)p; par_loop(a->job, a->worker); return NULL; }

void kolmo_par_for(int threads, int ntasks, kolmo_par_fn fn, void *ctx) {
    if (ntasks <= 0) return;
    if (threads > ntasks) threads = ntasks;
    if (threads > PAR_MAX) threads = PAR_MAX;
    par_job j; j.fn = fn; j.ctx = ctx; j.ntasks = ntasks; atomic_init(&j.next, 0);
    if (threads <= 1) { par_loop(&j, 0); return; }
    pthread_t th[PAR_MAX]; par_arg arg[PAR_MAX]; int started = 0;
    for (int t = 1; t < threads; t++) {
        arg[started].job = &j; arg[started].worker = started + 1;
        if (pthread_create(&th[started], NULL, par_main, &arg[started])) break;
        started++;
    }
    par_loop(&j, 0);
    for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
}

/* ---- crew ---- */

struct kolmo_team {                 /* phase barrier for the workers of one run: spin briefly, then sleep */
    atomic_int arrived;
    atomic_uint phase;
    atomic_int sleepers;
    int n;
    pthread_mutex_t *m;
    pthread_cond_t *cv;
};

struct kolmo_crew;
typedef struct { struct kolmo_crew *crew; int id; } crew_arg;

struct kolmo_crew {
    int size;                       /* workers including the caller */
    pthread_t th[PAR_MAX];
    pthread_mutex_t m;
    pthread_cond_t wake, done_cv, barrier_cv;
    pthread_mutex_t barrier_m;
    atomic_uint gen;                /* incremented for each run; workers wait for it to change */
    atomic_int done;                /* workers (other than the caller) finished with the current run */
    atomic_int sleepers;            /* the caller sleeps on done_cv when 1 */
    int quit;
    kolmo_team_fn fn; void *ctx; int n;
    kolmo_team team;
    crew_arg arg[PAR_MAX];
};

void kolmo_team_barrier(kolmo_team *t) {
    if (!t || t->n <= 1) return;
    unsigned ph = atomic_load_explicit(&t->phase, memory_order_acquire);
    if (atomic_fetch_add_explicit(&t->arrived, 1, memory_order_acq_rel) == t->n - 1) {
        atomic_store_explicit(&t->arrived, 0, memory_order_relaxed);
        pthread_mutex_lock(t->m);
        atomic_store_explicit(&t->phase, ph + 1, memory_order_release);
        if (atomic_load_explicit(&t->sleepers, memory_order_relaxed)) pthread_cond_broadcast(t->cv);
        pthread_mutex_unlock(t->m);
        return;
    }
    for (unsigned spins = 0; spins < BARRIER_SPINS; spins++) {
        if (atomic_load_explicit(&t->phase, memory_order_acquire) != ph) return;
        cpu_relax();
    }
    pthread_mutex_lock(t->m);
    atomic_fetch_add_explicit(&t->sleepers, 1, memory_order_relaxed);
    while (atomic_load_explicit(&t->phase, memory_order_acquire) == ph) pthread_cond_wait(t->cv, t->m);
    atomic_fetch_sub_explicit(&t->sleepers, 1, memory_order_relaxed);
    pthread_mutex_unlock(t->m);
}

static void *crew_main(void *p) {
    struct kolmo_crew *c = ((crew_arg *)p)->crew; int id = ((crew_arg *)p)->id;
    unsigned seen = 0;
    for (;;) {
        unsigned g;
        for (unsigned spins = 0; (g = atomic_load_explicit(&c->gen, memory_order_acquire)) == seen; spins++) {
            if (spins < SPIN_LIMIT) { cpu_relax(); continue; }
            pthread_mutex_lock(&c->m);
            while ((g = atomic_load_explicit(&c->gen, memory_order_acquire)) == seen && !c->quit) pthread_cond_wait(&c->wake, &c->m);
            pthread_mutex_unlock(&c->m);
            break;
        }
        if (c->quit) return NULL;
        seen = g;
        if (id < c->n) c->fn(c->ctx, id, c->n, &c->team);
        /* the last worker always signals under the mutex. Testing `sleepers` first (without the mutex) lost wakeups: the
         * caller's store to sleepers and its load of done, and this increment and load, are not all sequentially
         * consistent, so on a weakly ordered CPU both sides could miss each other and the caller slept forever (seen
         * on Apple M5 under load: 9 of 20 test-bwt runs hung). Under the mutex, the caller either sees the final count
         * before it waits or is woken by this signal. */
        if (atomic_fetch_add_explicit(&c->done, 1, memory_order_acq_rel) == c->size - 2) {
            pthread_mutex_lock(&c->m); pthread_cond_signal(&c->done_cv); pthread_mutex_unlock(&c->m);
        }
    }
}

kolmo_crew *kolmo_crew_create(int threads) {
    if (threads > PAR_MAX) threads = PAR_MAX;
    if (threads <= 1) return NULL;
    struct kolmo_crew *c = (struct kolmo_crew *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    pthread_mutex_init(&c->m, NULL); pthread_cond_init(&c->wake, NULL); pthread_cond_init(&c->done_cv, NULL);
    pthread_mutex_init(&c->barrier_m, NULL); pthread_cond_init(&c->barrier_cv, NULL);
    c->team.m = &c->barrier_m; c->team.cv = &c->barrier_cv; atomic_init(&c->team.sleepers, 0);
    atomic_init(&c->gen, 0); atomic_init(&c->done, 0); atomic_init(&c->sleepers, 0);
    c->size = 1;
    for (int w = 1; w < threads; w++) {
        c->arg[w].crew = c; c->arg[w].id = w;
        if (pthread_create(&c->th[w], NULL, crew_main, &c->arg[w])) break;
        c->size++;
    }
    if (c->size == 1) { kolmo_crew_destroy(c); return NULL; }
    return c;
}

int kolmo_crew_size(const kolmo_crew *c) { return c ? c->size : 1; }

void kolmo_crew_run(kolmo_crew *c, int n, kolmo_team_fn fn, void *ctx) {
    if (!c || n <= 1) { kolmo_team t; t.n = 1; t.m = NULL; t.cv = NULL; atomic_init(&t.arrived, 0); atomic_init(&t.phase, 0); atomic_init(&t.sleepers, 0); fn(ctx, 0, 1, &t); return; }
    if (n > c->size) n = c->size;
    c->fn = fn; c->ctx = ctx; c->n = n;
    c->team.n = n; atomic_store(&c->team.arrived, 0);
    atomic_store(&c->done, 0);
    pthread_mutex_lock(&c->m);
    atomic_fetch_add_explicit(&c->gen, 1, memory_order_release);
    pthread_cond_broadcast(&c->wake);
    pthread_mutex_unlock(&c->m);
    fn(ctx, 0, n, &c->team);
    /* every worker reports, including those beyond n, so none is still reading fn/ctx when the next run starts */
    for (unsigned spins = 0; atomic_load_explicit(&c->done, memory_order_acquire) < c->size - 1; spins++) {
        if (spins < SPIN_LIMIT) { cpu_relax(); continue; }
        pthread_mutex_lock(&c->m);
        atomic_store(&c->sleepers, 1);
        while (atomic_load_explicit(&c->done, memory_order_acquire) < c->size - 1) pthread_cond_wait(&c->done_cv, &c->m);
        atomic_store(&c->sleepers, 0);
        pthread_mutex_unlock(&c->m);
        break;
    }
}

void kolmo_crew_destroy(kolmo_crew *c) {
    if (!c) return;
    pthread_mutex_lock(&c->m);
    c->quit = 1;
    atomic_fetch_add(&c->gen, 1);
    pthread_cond_broadcast(&c->wake);
    pthread_mutex_unlock(&c->m);
    for (int w = 1; w < c->size; w++) pthread_join(c->th[w], NULL);
    pthread_cond_destroy(&c->wake); pthread_cond_destroy(&c->done_cv); pthread_mutex_destroy(&c->m);
    pthread_cond_destroy(&c->barrier_cv); pthread_mutex_destroy(&c->barrier_m);
    free(c);
}

/* ---- thread budget ---- */

struct kolmo_budget { pthread_mutex_t m; pthread_cond_t cv; int free, want; unsigned gen; };   /* want: threads kolmo_budget_take waits for */

kolmo_budget *kolmo_budget_create(int tokens) {
    kolmo_budget *b = (kolmo_budget *)calloc(1, sizeof *b);
    if (!b) return NULL;
    if (pthread_mutex_init(&b->m, NULL)) { free(b); return NULL; }
    if (pthread_cond_init(&b->cv, NULL)) { pthread_mutex_destroy(&b->m); free(b); return NULL; }
    b->free = tokens > 0 ? tokens : 0;
    return b;
}
void kolmo_budget_destroy(kolmo_budget *b) {
    if (!b) return;
    pthread_cond_destroy(&b->cv); pthread_mutex_destroy(&b->m); free(b);
}
int kolmo_budget_try(kolmo_budget *b) {
    if (!b) return 0;
    pthread_mutex_lock(&b->m);
    int r = b->free > 0 && !b->want; if (r) b->free--;   /* a waiting kolmo_budget_take goes first */
    pthread_mutex_unlock(&b->m);
    return r;
}
void kolmo_budget_give(kolmo_budget *b, int n) {
    if (!b || n <= 0) return;
    pthread_mutex_lock(&b->m); b->free += n; b->gen++; pthread_cond_broadcast(&b->cv); pthread_mutex_unlock(&b->m);
}
void kolmo_budget_wait(kolmo_budget *b, unsigned *seen) {
    if (!b) return;
    pthread_mutex_lock(&b->m);
    while ((b->free <= 0 || b->want) && b->gen == *seen) pthread_cond_wait(&b->cv, &b->m);
    *seen = b->gen;
    pthread_mutex_unlock(&b->m);
}
void kolmo_budget_take(kolmo_budget *b, int n) {
    if (!b || n <= 0) return;
    pthread_mutex_lock(&b->m);
    b->want += n;
    while (n > 0) {
        if (b->free > 0) { int k = b->free < n ? b->free : n; b->free -= k; b->want -= k; n -= k; continue; }
        pthread_cond_wait(&b->cv, &b->m);
    }
    pthread_mutex_unlock(&b->m);
}
void kolmo_budget_wake(kolmo_budget *b) {
    if (!b) return;
    pthread_mutex_lock(&b->m); b->gen++; pthread_cond_broadcast(&b->cv); pthread_mutex_unlock(&b->m);
}
int kolmo_budget_free(kolmo_budget *b) {
    if (!b) return 0;
    pthread_mutex_lock(&b->m); int f = b->free; pthread_mutex_unlock(&b->m);
    return f;
}

/* ---- chunked class pipeline ---- */

struct kolmo_cp {
    int K; unsigned nch, R; kolmo_cp_fn fn; void *ctx; kolmo_budget *b;
    pthread_mutex_t mu; pthread_cond_t cv;
    unsigned cnext[KOLMO_CP_MAXK];       /* each class's next chunk to run */
    unsigned char busy[KOLMO_CP_MAXK];   /* a thread runs the class now */
    unsigned *done;                      /* per slot (chunk % R): classes that ran the chunk */
    unsigned released, ready;            /* chunks below released are consumed; chunks below ready are complete */
    int err, stop, go, passes, nth;
    pthread_t th[KOLMO_CP_MAXK];
};
/* under mu: the class whose next chunk is the earliest one allowed to run, or -1 */
static int cp_pick(struct kolmo_cp *q) {
    unsigned lim = (q->released < q->ready ? q->released : q->ready) + q->R; int best = -1;
    for (int c = 0; c < q->K; c++)
        if (!q->busy[c] && q->cnext[c] < q->nch && q->cnext[c] < lim && (best < 0 || q->cnext[c] < q->cnext[best])) best = c;
    return best;
}
/* under mu: chunk j ran for the classes in mask */
static void cp_finish(struct kolmo_cp *q, unsigned j, unsigned mask, int r) {
    unsigned k = 0;
    for (int c = 0; c < q->K; c++) if (mask >> c & 1) { q->busy[c] = 0; q->cnext[c] = j + 1; k++; }
    unsigned *d = &q->done[j % q->R];
    if ((*d += k) == (unsigned)q->K) { *d = 0; q->ready = j + 1; }
    if (r) { q->err = 1; q->stop = 1; }
    pthread_cond_broadcast(&q->cv);
}
static void *cp_main(void *p) {
    struct kolmo_cp *q = (struct kolmo_cp *)p; unsigned seen = 0;
    pthread_mutex_lock(&q->mu);
    while (!q->go && !q->stop) pthread_cond_wait(&q->cv, &q->mu);
    for (;;) {
        if (q->stop) break;
        int c = cp_pick(q);
        if (c < 0) { pthread_cond_wait(&q->cv, &q->mu); continue; }
        int tok;   /* 1: one of the caller's own threads; 2: a token of the budget */
        if (q->passes > 0) { q->passes--; tok = 1; }   /* (mu is held from the pick to the claim) */
        else if (kolmo_budget_try(q->b)) tok = 2;
        else if (!q->b) { pthread_cond_wait(&q->cv, &q->mu); continue; }
        else { pthread_mutex_unlock(&q->mu); kolmo_budget_wait(q->b, &seen); pthread_mutex_lock(&q->mu); continue; }
        const unsigned j = q->cnext[c]; q->busy[c] = 1;
        pthread_mutex_unlock(&q->mu);
        int r = q->fn(q->ctx, j, 1u << c);
        pthread_mutex_lock(&q->mu);
        cp_finish(q, j, 1u << c, r);
        if (tok == 1) q->passes++; else kolmo_budget_give(q->b, 1);
    }
    pthread_mutex_unlock(&q->mu);
    return NULL;
}
kolmo_cp *kolmo_cp_start(int K, unsigned nch, unsigned R, int passes, kolmo_budget *b, kolmo_cp_fn fn, void *ctx) {
    if (K < 1 || K > KOLMO_CP_MAXK || R < 1) return NULL;
    struct kolmo_cp *q = (struct kolmo_cp *)calloc(1, sizeof *q);
    if (!q) return NULL;
    q->done = (unsigned *)calloc(R, sizeof *q->done);
    if (!q->done) { free(q); return NULL; }
    q->K = K; q->nch = nch; q->R = R; q->fn = fn; q->ctx = ctx; q->b = b; q->passes = passes < 0 ? 0 : passes > K ? K : passes;
    if (pthread_mutex_init(&q->mu, NULL)) { free(q->done); free(q); return NULL; }
    if (pthread_cond_init(&q->cv, NULL)) { pthread_mutex_destroy(&q->mu); free(q->done); free(q); return NULL; }
    for (int t = 0; t < K; t++) {
        if (pthread_create(&q->th[t], NULL, cp_main, q)) break;
        q->nth = t + 1;
    }
    if (q->nth < K) { kolmo_cp_end(q); return NULL; }   /* the helpers started stop before touching anything */
    pthread_mutex_lock(&q->mu); q->go = 1; pthread_cond_broadcast(&q->cv); pthread_mutex_unlock(&q->mu);
    return q;
}
int kolmo_cp_need(kolmo_cp *q, unsigned j) {
    pthread_mutex_lock(&q->mu);
    if (q->released < j) { q->released = j; pthread_cond_broadcast(&q->cv); }
    while (q->ready <= j && !q->err) {
        /* run, on this thread, the earliest pending chunk m <= j of the classes no helper holds (all of them at once):
         * the consumer may have skipped chunks (a long match), and with no helper running nobody else runs them */
        const unsigned lim = (q->released < q->ready ? q->released : q->ready) + q->R;
        unsigned m = UINT32_MAX, mask = 0;
        for (int c = 0; c < q->K; c++) if (!q->busy[c] && q->cnext[c] <= j && q->cnext[c] < lim && q->cnext[c] < m) m = q->cnext[c];
        if (m != UINT32_MAX)
            for (int c = 0; c < q->K; c++) if (!q->busy[c] && q->cnext[c] == m) { mask |= 1u << c; q->busy[c] = 1; }
        if (!mask) { pthread_cond_wait(&q->cv, &q->mu); continue; }
        pthread_mutex_unlock(&q->mu);
        int r = q->fn(q->ctx, m, mask);
        pthread_mutex_lock(&q->mu);
        cp_finish(q, m, mask, r);
    }
    int e = q->err;
    pthread_mutex_unlock(&q->mu);
    return e;
}
void kolmo_cp_end(kolmo_cp *q) {
    if (!q) return;
    pthread_mutex_lock(&q->mu); q->stop = 1; pthread_cond_broadcast(&q->cv); pthread_mutex_unlock(&q->mu);
    kolmo_budget_wake(q->b);
    for (int t = 0; t < q->nth; t++) pthread_join(q->th[t], NULL);
    pthread_cond_destroy(&q->cv); pthread_mutex_destroy(&q->mu); free(q->done); free(q);
}
