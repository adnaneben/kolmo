/* Planning of segments (archive members) on the pool, and ultra's check of the text transforms. */
/* ultra: the context-mixing coder models text, logs and CSV directly (column and word contexts), so a text transform is
 * kept only if CM on a 1 MiB transformed sample beats CM on the raw sample */
static int ultra_keep_transform(const uint8_t *s, size_t n, const plan_t *pl) {
    if (pl->t != T_TBL && pl->t != T_LOG && pl->t != T_WORD && pl->t != T_TP) return 1;
    size_t m = n < (1u << 20) ? n : (1u << 20); while (m > 0 && s[m - 1] != '\n') m--;
    if (m < 65536) return 1;
    uint8_t *t = malloc(m + TSLACK + 64), *z = malloc(m + m / 8 + 64); int64_t tn = -1;
    if (!t || !z) { free(t); free(z); return 1; }
    if (pl->t == T_TBL) tn = tbl2_encode(s, m, (uint8_t)pl->a, pl->c + 1, pl->b, pl->fs, t, NULL);
    else if (pl->t == T_LOG) tn = log_encode_m(s, m, t, m + TSLACK, pl->a == 1);
    else if (pl->t == T_WORD) tn = word_encode(s, m, t, m + TSLACK);
    int keep = 1;
    if (tn > 0) { size_t a = cm_compress(t, (size_t)tn, z, m + m / 8 + 64), b = cm_compress(s, m, z, m + m / 8 + 64); keep = a && (!b || a < b); }
    else if (pl->t == T_TP) keep = 0;
    free(t); free(z);
    return keep;
}

typedef struct {
    const uint8_t *src; const uint64_t *starts, *ends;
    plan_t *plans; const kolmo_params *params;
    uint32_t count; int threads_per_plan, allow_frontier, member_fd, native_leaf; atomic_uint next;
} plan_job;
static void *plan_worker(void *arg) {
    plan_job *j = arg; const kolmo_params *p = j->params;
    long page_size = j->member_fd >= 0 ? sysconf(_SC_PAGESIZE) : 0;
    for (;;) {
        uint32_t g = atomic_fetch_add(&j->next, 1); if (g >= j->count) break;
        const uint8_t *src = j->src + j->starts[g]; size_t len = j->ends[g] - j->starts[g];
        /* Keep pages touched by this archive member resident only while its
         * planner runs. Block construction retains the original file mapping. */
        void *member = MAP_FAILED;
        size_t mapped = 0;
        if (page_size > 0 && len >= (1u << 20)) {
            size_t offset = (size_t)j->starts[g], delta = offset % (size_t)page_size;
            mapped = len + delta;
            member = mmap(NULL, mapped, PROT_READ, MAP_PRIVATE, j->member_fd, (off_t)(offset - delta));
            if (member != MAP_FAILED) {
                madvise(member, mapped, MADV_RANDOM);
                src = (const uint8_t *)member + delta;
            }
        }
#ifdef KOLMO_FRONTIER
        int dna_candidate = j->allow_frontier && frontier_dna_probe(src, len);
        if (dna_candidate && is_simple_dna(src, len)) {
            j->plans[g] = (plan_t){.experimental_dna = 1};
            if (member != MAP_FAILED) munmap(member, mapped);
            continue;
        }
#endif
        /* transforms: 1 full analysis, 2 quick analysis, 3 minimal (speed first: only the byte-plane shuffle, whose
         * trial costs microseconds and whose decode is faster than the bytes it saves) */
#ifdef KOLMO_TIMING
        double mp0_ = prof_now();
#endif
        plan_t pl = p->transforms ? make_plan(src, len, p->mode >= KOLMO_ECO,
            p->mode != KOLMO_RT && p->transforms != 3, p->mode != KOLMO_RT && p->transforms != 3, j->threads_per_plan, p->mode >= KOLMO_MAX,
            p->transforms == 2 ? 1 : p->transforms == 3 ? (p->mode == KOLMO_RT ? 3 : 2) : p->mode == KOLMO_FAST ? 4 : 0, j->native_leaf) : (plan_t){0};
#ifdef KOLMO_TIMING
        fprintf(stderr, "mp n=%zu ms=%.3f t=%d\n", len, (prof_now() - mp0_) * 1000, pl.t);
#endif
#ifdef KOLMO_FRONTIER
        uint8_t delim = 0;
        if (j->allow_frontier && frontier_table_probe(src, len, &delim) && prefer_frontier_table(src, len, &pl, delim))
            pl.experimental_table = delim;
#if defined(KOLMO_FRONTIER_PATCHED_PCM) || defined(KOLMO_FRONTIER_FAST_PCM)
        else if (j->allow_frontier && pl.t == T_SHUF && pl.a == 2)
            pl.experimental_pcm = frontier_pcm_probe(src, len);
#endif
#ifdef KOLMO_FRONTIER_LAG
        else if (j->allow_frontier &&
                 (pl.t == T_IMG || pl.t == T_REC || pl.t == T_NONE) && !looks_text(src,len)) {
            frontier_lags trial = {0};
            if (frontier_lag_probe(src, len, &trial)) pl.lags = trial;
        }
#endif
        if (dna_candidate && sampled_dna_wins(src, len, &pl, p)) {
            free(pl.fs);
            free(pl.rec);
            pl = (plan_t){.experimental_dna = 1};
        }
#endif
        if (p->mode == KOLMO_RT && (pl.t == T_TP || pl.t == T_LOG)) pl.t = T_NONE;
        if (p->mode == KOLMO_ULTRA && !ultra_keep_transform(src, len, &pl)) {
            free(pl.fs); pl.fs = NULL; pl.t = T_NONE;
        }
        j->plans[g] = pl;
        if (member != MAP_FAILED) munmap(member, mapped);
    }
    zs_trim();
    return NULL;
}
