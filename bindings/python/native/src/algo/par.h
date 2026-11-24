/* Fork-join helpers for the algorithms in src/algo (LZP, QLFC, suffix sorting, inverse BWT).
   Copyright 2026 Adnane Aqartit and contributors. Apache-2.0. */
#ifndef KOLMO_ALGO_PAR_H
#define KOLMO_ALGO_PAR_H

/* kolmo_par_for runs fn(ctx, task, worker) for every task in [0, ntasks) on up to `threads` threads, the caller being
   worker 0. Tasks are handed out in order from a shared counter, so the result never depends on the thread count:
   each task must write only its own output. Threads are started per call (tens of microseconds), which is noise
   against the megabyte-sized tasks the callers use; a call with threads <= 1 or ntasks <= 1 starts none. */
typedef void (*kolmo_par_fn)(void *ctx, int task, int worker);
void kolmo_par_for(int threads, int ntasks, kolmo_par_fn fn, void *ctx);

/* A crew: threads - 1 workers started once and kept for many short parallel regions (the shape of OpenMP's
   `parallel` with `barrier` and `master`, which the suffix sorter uses hundreds of times per block). Idle workers spin
   briefly, then sleep. kolmo_crew_run runs fn(ctx, worker, n, team) on n workers (the caller is worker 0) and
   returns when all have finished. n is capped at the crew's size; a NULL crew or n <= 1 runs fn on the caller. */
typedef struct kolmo_team kolmo_team;
typedef void (*kolmo_team_fn)(void *ctx, int worker, int nworkers, kolmo_team *team);
typedef struct kolmo_crew kolmo_crew;
kolmo_crew *kolmo_crew_create(int threads);       /* NULL when threads <= 1 or no thread could be started */
int  kolmo_crew_size(const kolmo_crew *crew);
void kolmo_crew_run(kolmo_crew *crew, int n, kolmo_team_fn fn, void *ctx);
void kolmo_crew_destroy(kolmo_crew *crew);
void kolmo_team_barrier(kolmo_team *team);        /* all n workers of the current run meet; no-op for n <= 1 */

/* A thread budget shared by the blocks of one call (the elastic budget, review40 #7): tokens are threads the call may
   run beyond those already busy. A block worker that runs out of blocks gives its threads back; helpers of a block's
   inner parallel region take one token per task and give it back after. Changes when work runs, never what it writes.
   The functions accept a NULL budget (no tokens). */
typedef struct kolmo_budget kolmo_budget;
kolmo_budget *kolmo_budget_create(int tokens);    /* NULL when it cannot be allocated */
void kolmo_budget_destroy(kolmo_budget *b);
int  kolmo_budget_try(kolmo_budget *b);           /* takes one token: 1, or 0 when none is free */
void kolmo_budget_give(kolmo_budget *b, int n);   /* returns (or adds) n tokens and wakes the waiters */
void kolmo_budget_take(kolmo_budget *b, int n);   /* takes n tokens, waiting for them; kolmo_budget_try fails meanwhile */
void kolmo_budget_wait(kolmo_budget *b, unsigned *seen);   /* sleeps until a token is free or kolmo_budget_wake is
                                                              called after *seen was read; updates *seen */
void kolmo_budget_wake(kolmo_budget *b);          /* wakes every waiter (a pipeline that stops) */
int  kolmo_budget_free(kolmo_budget *b);          /* tokens free now (a snapshot) */

/* A chunked class pipeline: the work of an inner region is K classes (independent serial streams, such as the hash
   buckets of a match finder split by bucket % K) over nch chunks taken in order. Class c's chunk j runs after its chunk
   j - 1, on whichever thread claims it; a chunk is complete when all K classes have run it, and chunks complete in
   order. At most R chunks run ahead of the consumer (kolmo_cp_need), so a caller can keep R result slots per class
   (chunk j uses slot j % R). Helpers are started once (K of them); `passes` of them run without tokens (the threads
   the caller owns beyond itself), the others take a token from the budget per task. The consumer runs the classes of
   the chunk it needs on its own thread when no helper has taken them, so the pipeline advances with no helper at all.
   fn(ctx, j, mask) runs chunk j for the classes in mask (bit c) and returns 0, or nonzero on failure (the pipeline
   stops). The result never depends on which thread ran what. */
typedef int (*kolmo_cp_fn)(void *ctx, unsigned chunk, unsigned mask);
typedef struct kolmo_cp kolmo_cp;
#define KOLMO_CP_MAXK 16
kolmo_cp *kolmo_cp_start(int K, unsigned nch, unsigned R, int passes, kolmo_budget *b, kolmo_cp_fn fn, void *ctx);   /* NULL: run on one thread */
int  kolmo_cp_need(kolmo_cp *cp, unsigned chunk);   /* the consumer: chunks before `chunk` are consumed; returns once
                                                       `chunk` is complete (0) or the pipeline failed (nonzero) */
void kolmo_cp_end(kolmo_cp *cp);                    /* stops and joins the helpers */
#endif
