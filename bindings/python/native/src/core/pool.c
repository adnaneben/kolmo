/* Threads: per-thread state kept across calls, the persistent worker pool, and the default thread count. */
/* per-thread state kept across calls (pool threads persist): codec contexts and scratch buffers */
typedef struct { codec_ctx x; uint8_t *tbuf, *slot, *rbuf; uint32_t *pos, *rowbuf; size_t tcap, scap, rbcap, pcap, rcap; int retained; } tls_t;
static __thread tls_t *TLS;
static pthread_key_t tls_key;
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;
static int tls_key_valid;
static void tls_destroy(void *arg) {
    tls_t *s = arg;
    codec_ctx_free(&s->x);
    free(s->tbuf); free(s->slot); free(s->rbuf); free(s->pos); free(s->rowbuf);
    free(s);
}
static void tls_init(void) { tls_key_valid = pthread_key_create(&tls_key, tls_destroy) == 0; }
static tls_t *tls_begin(void) {
    pthread_once(&tls_once, tls_init);
    if (!TLS) {
        TLS = calloc(1, sizeof(*TLS));
        if (!TLS) return NULL;
        if (tls_key_valid && !pthread_setspecific(tls_key, TLS)) TLS->retained = 1;
    }
    return TLS;
}
static void tls_end(tls_t *s) {
    zs_trim();
    /* Without a destructor, retain nothing after a completed call. */
    if (!s->retained) { TLS = NULL; tls_destroy(s); }
}
static void *grow(void *p, size_t *cap, size_t need) {
    if (need <= *cap) return p;
    free(p); p = malloc(need); *cap = p ? need : 0; return p;
}

static int default_threads(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1; }

/* persistent worker pool: threads (and their contexts) live across calls; the caller works too */
static pthread_mutex_t pool_mu = PTHREAD_MUTEX_INITIALIZER, pool_job = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pool_go = PTHREAD_COND_INITIALIZER, pool_done = PTHREAD_COND_INITIALIZER;
static int pool_n = 0, pool_want = 0, pool_active = 0; static uint64_t pool_gen = 0;
static void *(*pool_fn)(void *); static void *pool_arg; static uint64_t pool_start_gen[256];
static void *pool_main(void *a) {
    int idx = (int)(intptr_t)a;
    pthread_mutex_lock(&pool_mu);
    uint64_t seen = pool_start_gen[idx];          /* generation at creation: a job posted before we start is not missed */
    for (;;) {
        while (seen == pool_gen) pthread_cond_wait(&pool_go, &pool_mu);
        seen = pool_gen;
        if (idx >= pool_want) continue;
        void *(*fn)(void *) = pool_fn; void *arg = pool_arg;
        pthread_mutex_unlock(&pool_mu);
        fn(arg);
        pthread_mutex_lock(&pool_mu);
        if (--pool_active == 0) pthread_cond_signal(&pool_done);
    }
    return NULL;
}
/* fork(): only the calling thread exists in the child, so the pool's workers are gone while pool_n still counts them,
 * and a lock held by another thread at the fork stays held. The prepare handler takes the pool's bookkeeping lock and
 * the memory governor's lock (always in this order; no thread takes them the other way round), so neither is mid-update
 * at the fork; the parent releases them; the child starts both afresh: no workers (the next call creates them), no
 * reservations (the threads that held them do not exist in the child), and new locks and condition variables. pool_job
 * is not taken in prepare (a fork does not wait for a running call to finish); in the child it is re-created unlocked. */
static void pool_fork_prepare(void) { pthread_mutex_lock(&pool_mu); pthread_mutex_lock(&kmem_mu); }
static void pool_fork_parent(void) { pthread_mutex_unlock(&kmem_mu); pthread_mutex_unlock(&pool_mu); }
static void pool_fork_child(void) {
    pthread_mutex_init(&pool_mu, NULL); pthread_mutex_init(&pool_job, NULL);
    pthread_cond_init(&pool_go, NULL); pthread_cond_init(&pool_done, NULL);
    pool_n = pool_want = pool_active = 0;
    pthread_mutex_init(&kmem_mu, NULL); pthread_cond_init(&kmem_cv, NULL);
    kmem_used = 0; kmem_jobs = 0;
}
static pthread_once_t pool_fork_once = PTHREAD_ONCE_INIT;
static void pool_fork_register(void) { pthread_atfork(pool_fork_prepare, pool_fork_parent, pool_fork_child); }
/* Both locks are held on entry. Finish this job and release them, keeping
 * pool ownership while its existing helpers run the callback. */
static void pool_dispatch_owned(void *(*fn)(void *), void *J, int helpers) {
    pool_fn = fn; pool_arg = J; pool_want = helpers; pool_active = helpers; pool_gen++;
    pthread_cond_broadcast(&pool_go);
    pthread_mutex_unlock(&pool_mu);
    fn(J);
    pthread_mutex_lock(&pool_mu);
    while (pool_active) pthread_cond_wait(&pool_done, &pool_mu);
    pthread_mutex_unlock(&pool_mu);
    pthread_mutex_unlock(&pool_job);
}
static void run_pool(void *(*fn)(void *), void *J, int threads, uint32_t units) {
    if (threads > 256) threads = 256;
    if ((uint32_t)threads > units) threads = (int)units;
    if (threads <= 1) { fn(J); return; }
    pthread_once(&pool_fork_once, pool_fork_register);
    if (pthread_mutex_trylock(&pool_job)) {               /* pool busy (concurrent caller): private threads */
        pthread_t th[255]; int started = 0;
        for (int t = 1; t < threads; t++) {
            if (pthread_create(&th[started], NULL, fn, J)) break;
            started++;
        }
        fn(J);
        for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
        return;
    }
    pthread_mutex_lock(&pool_mu);
    while (pool_n < threads - 1) {
        pthread_t th; pthread_attr_t at;
        if (pthread_attr_init(&at)) break;
        if (pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED)) { pthread_attr_destroy(&at); break; }
        pool_start_gen[pool_n] = pool_gen;
        if (pthread_create(&th, &at, pool_main, (void *)(intptr_t)pool_n)) { pthread_attr_destroy(&at); break; }
        pthread_attr_destroy(&at); pool_n++;
    }
    int helpers = threads - 1 < pool_n ? threads - 1 : pool_n;
    pool_dispatch_owned(fn, J, helpers);
}

/* kolmo_release_buffers: each thread frees its own buffers (thread-local), so the pool's idle workers are given a job
 * that does it, and the caller does it for itself. */
static void tls_release_self(void) {
    tls_t *s = TLS;
    if (s) {
        codec_ctx_free(&s->x); memset(&s->x, 0, sizeof s->x);
        free(s->tbuf); free(s->slot); free(s->rbuf); free(s->pos); free(s->rowbuf);
        s->tbuf = s->slot = s->rbuf = NULL; s->pos = s->rowbuf = NULL; s->tcap = s->scap = s->rbcap = s->pcap = s->rcap = 0;
    }
    zs_trim();
}
static void *tls_release_job(void *arg) { (void)arg; tls_release_self(); return NULL; }   /* each worker runs a job once */
void kolmo_release_buffers(void) {
    if (pthread_mutex_trylock(&pool_job)) { tls_release_self(); return; }
    pthread_mutex_lock(&pool_mu);
    if (pool_n > 0) {
        pool_dispatch_owned(tls_release_job, NULL, pool_n);
    } else {
        pthread_mutex_unlock(&pool_mu);
        pthread_mutex_unlock(&pool_job);
        tls_release_self();
    }
}
