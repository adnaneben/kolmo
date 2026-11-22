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
#endif
