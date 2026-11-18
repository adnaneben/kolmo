/* Planning of segments (archive members) on the pool, and ultra's check of the text transforms. */
/* ultra: the context-mixing coder models text, logs and CSV directly (column and word contexts), so a text transform is
 * kept only if CM on a 1 MiB transformed sample beats CM on the raw sample */
typedef struct { const uint8_t *in; size_t n; uint8_t *out; size_t cap, res; kmem_ctx ctx; uint64_t resv; int tok; } ukt_arg;
/* the helper releases its own reservation as soon as it is done: its parent may be waiting for that memory */
static void *ukt_main(void *a) {
    ukt_arg *u = a; kmem_ctx_set(u->ctx); kmem_tls_job = 1;
    u->res = cm_compress(u->in, u->n, u->out, u->cap);
    kmem_tls_job = 0; kmem_release(u->resv); kjob_give(u->tok); return NULL;
}
/* ultra, a table (codec 25): the sample's columns, each coded by its own context-mixing model (model 2, model 1 at
 * level 1), as the block coder will code them. Returns the payload size plus the column sizes the frame carries, 0 if
 * the transpose does not apply. */
static size_t ultra_columns(const uint8_t *s, size_t m, const plan_t *pl, int threads, int level, uint8_t **z, size_t *zn) {
    const int k = pl->c + 1; int64_t colsz[256]; size_t parts[256]; int np = 0, quoted = pl->b && (memchr(s, '"', m) || memchr(s, '[', m) || memchr(s, ']', m));
    uint8_t *t = malloc(m + TSLACK + 64); uint32_t *pos = quoted ? NULL : malloc((m + 1) * sizeof(uint32_t));
    *z = NULL; *zn = 0;
    if (!t || (!quoted && !pos)) { free(t); free(pos); return 0; }
    if (quoted) transpose_fwd(s, (int64_t)m, (uint8_t)pl->a, k, pl->b, t, colsz); else tpq_fwd(s, m, (uint8_t)pl->a, k, t, colsz, pos);
    free(pos);
    size_t tot = 0;
    for (int j = 0; j < k; j++) { if (colsz[j] < 0) { free(t); return 0; } if (colsz[j]) parts[np++] = (size_t)colsz[j]; tot += (size_t)colsz[j]; }
    size_t r = 0;
    if (tot == m && np >= 2) {
        size_t cap = codec_bound(C_CMP, m); uint8_t *o = malloc(cap);
        r = o ? cmp_compress_model(t, m, parts, np, o, cap, threads, level == 1 ? 1 : 2) : 0;
        if (r) { *z = o; *zn = r; r += 4 * (size_t)k; } else free(o);
    }
    free(t);
    return r;
}
typedef struct { const uint8_t *s; size_t m; const plan_t *pl; int threads, level; uint8_t *z; size_t zn, res; kmem_ctx ctx; } ukc_arg;
static void *ukc_main(void *a) { ukc_arg *u = a; kmem_ctx_set(u->ctx); u->res = ultra_columns(u->s, u->m, u->pl, u->threads, u->level, &u->z, &u->zn); return NULL; }

/* ultra: the context-mixing coder models text, logs and CSV directly (column and word contexts), so a text transform is
 * kept only if CM on a 1 MiB transformed sample beats CM on the raw sample. A table (table or transpose plan) is also
 * tried as columns, one model per column (codec 25, which codes and decodes the columns in parallel): chosen when the
 * columns beat the raw sample's model-2 size by 1.5% on text (the margin model 3 gains over model 2 on the raw bytes
 * of the tables measured) or come within 0.5% of it on binary data, and beat the kept transform. Sets pl->cmp (and
 * pl->cmpz, the coded block, when the sample is the whole block). */
static int ultra_keep_transform(const uint8_t *s, size_t n, plan_t *pl, int threads, int level) {
    if (pl->t != T_TBL && pl->t != T_LOG && pl->t != T_WORD && pl->t != T_TP) return 1;
    size_t m = n < (1u << 20) ? n : (1u << 20); while (m > 0 && s[m - 1] != '\n') m--;
    if (m < 65536) return 1;
    uint8_t *t = malloc(m + TBL_SLACK(m) + 64), *z = malloc(m + m / 8 + 64); int64_t tn = -1;
    if (!t || !z) { free(t); free(z); return 1; }
    if (pl->t == T_TBL) tn = tbl2_encode(s, m, (uint8_t)pl->a, pl->c + 1, pl->b, pl->fs, t, NULL);
    else if (pl->t == T_LOG) tn = log_encode_m(s, m, t, m + TSLACK, pl->a == 1);
    else if (pl->t == T_WORD) tn = word_encode(s, m, t, m + TSLACK);
    const int cols = (pl->t == T_TBL || pl->t == T_TP) && !pl->wide && pl->c >= 1;
    int keep = 1;
    if (tn > 0 || cols) {
        /* the samples are coded on separate threads when there are threads to spare (same results) */
        /* the second sample is optional work: it starts only when the governor grants its tables now */
        uint64_t un = cm_need_bound(m) + m + 8;
        /* each extra thread also needs a machine token (core/jobs.c); refused, the work runs here, same result */
        int utok = threads > 1 ? kjob_take() : KJ_REFUSED, ctok = cols && threads > 2 ? kjob_take() : KJ_REFUSED;
        int ures = utok != KJ_REFUSED && kmem_try_reserve(un, kmem_tls_cap);
        if (!ures) { kjob_give(utok); utok = KJ_REFUSED; }
        uint8_t *z2 = ures ? malloc(m + m / 8 + 64) : NULL; pthread_t th, tc; ukt_arg u = {s, m, z2, m + m / 8 + 64, 0, kmem_ctx_get(), un, utok};
        ukc_arg c = {s, m, pl, threads > 2 ? threads - 2 : 1, level, NULL, 0, 0, kmem_ctx_get()};
        int par = z2 && !pthread_create(&th, NULL, ukt_main, &u), parc = ctok != KJ_REFUSED && !pthread_create(&tc, NULL, ukc_main, &c);
        if (ures && !par) { kmem_release(un); kjob_give(utok); ures = 0; }
        if (ctok != KJ_REFUSED && !parc) kjob_give(ctok);
        size_t a = tn > 0 ? cm_compress(t, (size_t)tn, z, m + m / 8 + 64) : 0, b;
        if (par) { pthread_join(th, NULL); b = u.res; } else b = cm_compress(s, m, z, m + m / 8 + 64);
        if (cols) { if (parc) { pthread_join(tc, NULL); kjob_give(ctok); } else ukc_main(&c); }
        free(z2);
        keep = tn > 0 ? a && (!b || a < b) : pl->t != T_TP;
        if (cols && c.res && b) {
            double ref = looks_text(s, m) ? 0.985 * (double)b : 1.005 * (double)b;
            if (keep && tn > 0 && (double)(a + 4) < ref) ref = (double)(a + 4);
            if ((double)c.res < ref) {
                pl->t = T_TP; pl->cmp = 1; keep = 1;
                if (m == n) { pl->cmpz = c.z; pl->cmpzn = c.zn; pl->cmpn = n; c.z = NULL; }
            }
        }
        free(c.z);
    } else if (pl->t == T_TP) keep = 0;
    free(t); free(z);
    return keep;
}

typedef struct {
    const uint8_t *src; const uint64_t *starts, *ends;
    plan_t *plans; const kolmo_params *params;
    uint32_t count; int threads_per_plan, allow_frontier, member_fd, native_leaf; atomic_uint next;
    const struct kolmo_profile *prof;   /* trained profile: its pinned plans replace the analysis where they match */
    uint64_t xcap;                      /* the call's explicit memory cap (0: none): -x samples size tables with it */
    const uint32_t *kind;               /* per segment: MK_* | phase << 8 (plan/meta.c), or NULL */
} plan_job;
#ifdef KOLMO_PLAN_HOOK
static void (*kolmo_plan_hook)(const uint8_t *src, size_t len, const kolmo_params *p, plan_t *pl);
#endif
static int kp_plan_pinned(const struct kolmo_profile *P, const uint8_t *s, size_t n, const kolmo_params *p, plan_t *out);
/* The analysis of one member, as plan_worker runs it (also used by kolmo train to record the plans it pins). */
static plan_t plan_for(const uint8_t *src, size_t len, const kolmo_params *p, int threads, int native_leaf) {
    return p->transforms ? make_plan(src, len, p->mode >= KOLMO_ECO,
        p->mode != KOLMO_RT && p->transforms != 3, p->mode != KOLMO_RT && p->transforms != 3, threads, p->mode >= KOLMO_MAX,
        p->transforms == 2 ? 1 : p->transforms == 3 ? (p->mode == KOLMO_RT ? 3 : 2) : p->mode == KOLMO_FAST ? 4 : 0, native_leaf) : (plan_t){0};
}
static void *plan_worker(void *arg) {
    plan_job *j = arg; const kolmo_params *p = j->params;
    kmem_tls_cap = j->xcap;                                  /* -x samples size their tables as the blocks will */
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
        plan_t pl;
        const uint32_t mk = j->kind ? j->kind[g] : MK_DATA;
        if ((mk & 0xFF) == MK_PACKED && p->transforms) pl = (plan_t){.t = T_NONE, .packed = 1};   /* compressed content: no analysis */
        else if ((mk & 0xFF) == MK_A64 && p->transforms && p->mode != KOLMO_RT)   /* ARM64 code: the branch transform, a, b: page phase */
            pl = (plan_t){.t = T_A64, .a = (int)((mk >> 8) & 0xFF), .b = (int)((mk >> 16) & 0xF)};
        else if (!j->prof || !kp_plan_pinned(j->prof, src, len, p, &pl)) pl = plan_for(src, len, p, j->threads_per_plan, j->native_leaf);
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
        if (p->mode == KOLMO_ULTRA && !ultra_keep_transform(src, len, &pl, j->threads_per_plan, p->level)) {
            free(pl.fs); pl.fs = NULL; pl.t = T_NONE;
        }
#ifdef KOLMO_PLAN_HOOK
        if (kolmo_plan_hook) kolmo_plan_hook(src, len, p, &pl);   /* research builds only (benchmark/rc3w2/twopass) */
#endif
        j->plans[g] = pl;
        if (member != MAP_FAILED) munmap(member, mapped);
    }
    kmem_tls_cap = 0;
    zs_trim();
    return NULL;
}
