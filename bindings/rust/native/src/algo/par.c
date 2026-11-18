/* Fork-join helpers for the algorithms in src/algo. See par.h.
   Copyright 2026 Adnane Aqartit and contributors. Apache-2.0. */
#include "par.h"
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
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

int (*kolmo_par_take)(void);
void (*kolmo_par_give)(int token);
static int par_take(void) { int (*f)(void) = kolmo_par_take; return f ? f() : -2; }
static void par_give(int t) { void (*f)(int) = kolmo_par_give; if (f && t >= 0) f(t); }

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
    pthread_t th[PAR_MAX]; par_arg arg[PAR_MAX]; int tok[PAR_MAX]; int started = 0;
    for (int t = 1; t < threads; t++) {
        if ((tok[started] = par_take()) == -1) break;       /* no machine token: the threads started share the tasks */
        arg[started].job = &j; arg[started].worker = started + 1;
        if (pthread_create(&th[started], NULL, par_main, &arg[started])) { par_give(tok[started]); break; }
        started++;
    }
    par_loop(&j, 0);
    for (int t = 0; t < started; t++) { pthread_join(th[t], NULL); par_give(tok[t]); }
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
    int tok[PAR_MAX];               /* each worker's machine token, held for the crew's life (-2: none needed) */
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
        if ((c->tok[w] = par_take()) == -1) break;
        c->arg[w].crew = c; c->arg[w].id = w;
        if (pthread_create(&c->th[w], NULL, crew_main, &c->arg[w])) { par_give(c->tok[w]); break; }
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
    for (int w = 1; w < c->size; w++) { pthread_join(c->th[w], NULL); par_give(c->tok[w]); }
    pthread_cond_destroy(&c->wake); pthread_cond_destroy(&c->done_cv); pthread_mutex_destroy(&c->m);
    pthread_cond_destroy(&c->barrier_cv); pthread_mutex_destroy(&c->barrier_m);
    free(c);
}
