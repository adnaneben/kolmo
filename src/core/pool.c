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
static void *(*pool_fn)(void *); static void *pool_arg; static uint64_t pool_start_gen[256]; static int pool_kj_off;
static void *pool_main(void *a) {
    int idx = (int)(intptr_t)a;
    pthread_mutex_lock(&pool_mu);
    uint64_t seen = pool_start_gen[idx];          /* generation at creation: a job posted before we start is not missed */
    for (;;) {
        while (seen == pool_gen) pthread_cond_wait(&pool_go, &pool_mu);
        seen = pool_gen;
        if (idx >= pool_want) continue;
        void *(*fn)(void *) = pool_fn; void *arg = pool_arg;
        kj_tls_off = pool_kj_off; kj_tls_role = 2;           /* a helper of the run: a machine token per block job */
        pthread_mutex_unlock(&pool_mu);
        fn(arg);
        kj_tls_role = 0; kj_tls_off = 0;
        pthread_mutex_lock(&pool_mu);
        if (--pool_active == 0) pthread_cond_signal(&pool_done);
    }
    return NULL;
}
/* fork: the child has only the thread that forked, so the pool's threads are gone and its locks may be held by threads
 * that no longer exist. The child starts with an empty pool (a library used before a fork, as Python's multiprocessing
 * does, would otherwise wait forever for helpers that are not there). */
static pthread_once_t pool_fork_once = PTHREAD_ONCE_INIT;
static void pool_atfork_child(void) {
    pthread_mutex_init(&pool_mu, NULL); pthread_mutex_init(&pool_job, NULL);
    pthread_cond_init(&pool_go, NULL); pthread_cond_init(&pool_done, NULL);
    pool_n = pool_want = pool_active = 0;
}
static void pool_fork_init(void) { pthread_atfork(NULL, NULL, pool_atfork_child); }
/* a private helper (the pool is busy with another call): same role as a pool helper */
typedef struct { void *(*fn)(void *); void *J; int kj_off; } pool_priv;
static void *pool_priv_main(void *a) {
    pool_priv *p = a; kj_tls_off = p->kj_off; kj_tls_role = 2;
    p->fn(p->J);
    kj_tls_role = 0; kj_tls_off = 0; return NULL;
}
/* the caller of a run is its guaranteed worker: it never waits for a machine token (core/jobs.c) */
static void pool_run_caller(void *(*fn)(void *), void *J) {
    int role = kj_tls_role; kj_tls_role = 1;
    fn(J);
    kj_tls_role = role;
    kjob_wake();                                         /* helpers still waiting for a token: no blocks are left */
}
static void run_pool(void *(*fn)(void *), void *J, int threads, uint32_t units) {
    pthread_once(&pool_fork_once, pool_fork_init);
    if (threads > 256) threads = 256;
    if ((uint32_t)threads > units) threads = (int)units;
    if (threads <= 1) { pool_run_caller(fn, J); return; }
    if (pthread_mutex_trylock(&pool_job)) {               /* pool busy (concurrent caller): private threads */
        pthread_t th[255]; int started = 0; pool_priv pp = {fn, J, kj_tls_off};
        for (int t = 1; t < threads; t++) {
            if (pthread_create(&th[started], NULL, pool_priv_main, &pp)) break;
            started++;
        }
        pool_run_caller(fn, J);
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
    pool_fn = fn; pool_arg = J; pool_want = helpers; pool_active = helpers; pool_kj_off = kj_tls_off; pool_gen++;
    pthread_cond_broadcast(&pool_go);
    pthread_mutex_unlock(&pool_mu);
    pool_run_caller(fn, J);
    pthread_mutex_lock(&pool_mu);
    while (pool_active) pthread_cond_wait(&pool_done, &pool_mu);
    pthread_mutex_unlock(&pool_mu);
    pthread_mutex_unlock(&pool_job);
}
