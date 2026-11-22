import sys
p=sys.argv[1]; s=open(p).read()
old='''#if defined(LIBSAIS_OPENMP)
    #include <omp.h>
#else
    #define UNUSED(_x)                  (void)(_x)
#endif
'''
new='''/* Kolmo: OpenMP is replaced by a crew of pthreads (src/algo/par.c). Every former `#pragma omp parallel` region is a
   function run through libsais_omp_parallel(); omp_get_thread_num() and omp_get_num_threads() read the values the
   crew sets for the current region, `#pragma omp barrier` is libsais_omp_barrier(), `#pragma omp master` a test of
   thread 0. Outside a region a thread counts as a team of one, as in OpenMP. LIBSAIS_OPENMP stays defined so libsais's
   multithreaded code paths are compiled. */
#include "../par.h"
#define LIBSAIS_OPENMP 1
#define UNUSED(_x)                      (void)(_x)

typedef ptrdiff_t                       fast_sint_t;

static __thread int             libsais_omp_tid  = 0;
static __thread int             libsais_omp_nth  = 1;
static __thread kolmo_team *    libsais_omp_team = NULL;
static __thread kolmo_crew *    libsais_omp_crew = NULL;    /* set by the entry points for the calling thread */

#define omp_get_thread_num()            ((fast_sint_t)libsais_omp_tid)
#define omp_get_num_threads()           ((fast_sint_t)libsais_omp_nth)

typedef struct { void (* fn)(void *); void * args; } libsais_omp_job;

static void libsais_omp_entry(void * ctx, int worker, int nworkers, kolmo_team * team)
{
    const libsais_omp_job * job = (const libsais_omp_job *)ctx;
    int tid = libsais_omp_tid, nth = libsais_omp_nth; kolmo_team * outer = libsais_omp_team;
    libsais_omp_tid = worker; libsais_omp_nth = nworkers; libsais_omp_team = team;
    job->fn(job->args);
    libsais_omp_tid = tid; libsais_omp_nth = nth; libsais_omp_team = outer;
}

static void libsais_omp_parallel(void (* fn)(void *), void * args, fast_sint_t threads)
{
    libsais_omp_job job = { fn, args };
    /* a region inside a region runs on one thread (OpenMP's default: nesting disabled) */
    kolmo_crew * crew = libsais_omp_nth > 1 ? NULL : libsais_omp_crew;
    kolmo_crew_run(crew, threads > 1 ? (int)threads : 1, libsais_omp_entry, &job);
}

static void libsais_omp_barrier(void) { kolmo_team_barrier(libsais_omp_team); }
'''
assert old in s; s=s.replace(old,new,1)
s=s.replace("typedef ptrdiff_t                       fast_sint_t;\ntypedef size_t","typedef size_t",1) if s.count("typedef ptrdiff_t                       fast_sint_t;")>1 else s
open(p,'w').write(s)
