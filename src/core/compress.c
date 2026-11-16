/* The encoder: plan, split, code the blocks on the pool, write the container. */
static int64_t compress_impl_p(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *p,
                               const void *dict, size_t dictlen, uint32_t dictid, const struct kolmo_profile *prof);
static int64_t compress_impl(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *p,
                             const void *dict, size_t dictlen, uint32_t dictid) {
    return compress_impl_p(src, input_fd, n, dst, output_fd, cap, p, dict, dictlen, dictid, NULL);
}
/* prof: a trained profile (src/plan/profile.c); its dictionaries code the zstd, Brotli and LZ4 leaves, its pinned plans
 * replace the analysis where they match, and the frame records its id (version 11, flag bit 2). */
static int64_t compress_impl_p(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *p,
                               const void *dict, size_t dictlen, uint32_t dictid, const struct kolmo_profile *prof) {
    kolmo_params d = {KOLMO_FAST, 0, 0, 0, 1}; if (!p) p = &d;
    /* KOLMO_T_NATIVE_LEAF and KOLMO_T_TYPED_HUF are opt-in bits of transforms: read them, then clear them so every
     * other use of transforms sees the analysis level alone. The native leaf applies to balanced (KOLMO_FAST) with
     * analysis on; elsewhere it is ignored. */
    kolmo_params q = *p; int native_leaf = (q.transforms & KOLMO_T_NATIVE_LEAF) != 0;
    int typed_huf = (q.transforms & KOLMO_T_TYPED_HUF) != 0;
    q.transforms &= ~(KOLMO_T_NATIVE_LEAF | KOLMO_T_TYPED_HUF);
    native_leaf = native_leaf && q.mode == KOLMO_FAST && q.transforms; p = &q;
    if (cap < HDR + IDX) return KOLMO_ERR_DSTSIZE;
    if (n > INT64_MAX) return KOLMO_ERR_CORRUPT;
    /* The typed route below produces a whole frame on its own (one block, one thread, at most 16 MiB, buffer API, no
     * dictionary, default level) and returns it; when it does not apply the normal planner runs. A declared integer
     * NPY / PCM16 WAV that the typed route's size guard rejects is compressed with the plain codec (transforms 0). */
    if (typed_huf && !native_leaf && !prof && q.transforms && q.mode == KOLMO_FAST && !q.level && q.threads == 1
        && input_fd < 0 && output_fd < 0 && !dictlen && n <= (16u << 20) && (!q.block || n <= q.block)) {
        int plain = 0; int64_t tr = ktf_encode(src, n, dst, cap, &plain);
        if (tr) return tr;
        if (plain) q.transforms = 0;
    }
    int threads = p->threads > 0 ? p->threads : default_threads();
    uint32_t B = p->block ? p->block : (p->mode >= KOLMO_MAX ? (128u << 20) : p->mode >= KOLMO_ECO ? (16u << 20) : kolmo_fast_block(n));   /* slow modes: larger blocks; speed modes: ~8 balanced blocks */
    if (B < 4096) B = 4096;
    int codec, level; mode_codec(p, &codec, &level);
    enum { MAXSEG = 4096 };
    int64_t ret = KOLMO_ERR_ALLOC; size_t w = 0;
    uint8_t *file_index = NULL;
    job_t J = {0}; int mutex_ready = 0;
    uint32_t nseg = 0, nb = 0; size_t bcap = 2;
    plan_t *plans = NULL;
    uint64_t *starts = NULL, *rstart = NULL; uint32_t *bplan = NULL;
    uint64_t *ss = array_alloc(MAXSEG, sizeof(uint64_t), 0), *se = array_alloc(MAXSEG, sizeof(uint64_t), 0);
    void *planning_map = MAP_FAILED;
    if (!ss || !se) goto cleanup;
    if (input_fd >= 0 && n) {
        planning_map = mmap(NULL, n, PROT_READ, MAP_PRIVATE, input_fd, 0);
        if (planning_map == MAP_FAILED) { ret = KOLMO_ERR_IO; goto cleanup; }
        /* Formula discovery samples dispersed rows before the block pass. */
        madvise(planning_map, n, MADV_RANDOM);
        src = planning_map;
    }
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    double pt0 = prof_now();
#endif
    nseg = segments(src, n, ss, se, MAXSEG);
    plans = array_alloc(nseg, sizeof(plan_t), 1);
    starts = array_alloc(bcap, sizeof(uint64_t), 0);
    bplan = array_alloc(bcap, sizeof(uint32_t), 0);
    rstart = array_alloc(bcap, sizeof(uint64_t), 0);
    if (!plans || !starts || !bplan || !rstart) goto cleanup;
    /* Parallelize independent members, or formula discovery within a single
     * member. Never multiply the two thread budgets. Plans remain in input order. */
    plan_job planning = {src, ss, se, plans, p, nseg, nseg > 1 ? 1 : threads,
                         p->transforms && p->mode == KOLMO_FAST && !p->level && !dict && !prof,
                         nseg > 1 && p->transforms && p->mode == KOLMO_FAST ? input_fd : -1, native_leaf};
    planning.prof = prof;
    atomic_init(&planning.next, 0);
    run_pool(plan_worker, &planning, p->transforms ? threads : 1, nseg);
    if (!p->block && nseg == 1 && (plans[0].wide || plans[0].experimental_table) && B > (2u << 20)) B = 2u << 20;
    for (uint32_t g = 0; g < nseg; g++) {
        const uint8_t *s0 = src + ss[g]; size_t len = (size_t)(se[g] - ss[g]);
        plan_t pl = plans[g];
        uint32_t Bs = B;
        if (!p->block && p->mode == KOLMO_HC && pl.t == T_WORD) Bs = 128u << 20;   /* hc text: one large window */
        uint64_t *st; uint32_t m = make_blocks(s0, len, Bs, pl, &st, input_fd, ss[g], &ret);
        if (!st) goto cleanup;
        if (len == 0) m = 0;
        if (m > UINT32_MAX - nb - 1) { free(st); goto cleanup; }
        size_t needed = (size_t)nb + m + 1;
        if (needed > bcap) {
            size_t grown_cap = bcap;
            while (grown_cap < needed) {
                if (grown_cap > SIZE_MAX / 2) { free(st); goto cleanup; }
                grown_cap *= 2;
            }
            uint64_t *grown_starts = array_resize(starts, grown_cap, sizeof(uint64_t));
            if (!grown_starts) { free(st); goto cleanup; } starts = grown_starts;
            uint32_t *grown_bplan = array_resize(bplan, grown_cap, sizeof(uint32_t));
            if (!grown_bplan) { free(st); goto cleanup; } bplan = grown_bplan;
            uint64_t *grown_rstart = array_resize(rstart, grown_cap, sizeof(uint64_t));
            if (!grown_rstart) { free(st); goto cleanup; } rstart = grown_rstart;
            bcap = grown_cap;
        }
        for (uint32_t i = 0; i < m; i++) { starts[nb] = ss[g] + st[i]; rstart[nb] = st[i]; bplan[nb] = g; nb++; }
        free(st);
    }
    if (nb == 0) { starts[0] = 0; rstart[0] = 0; bplan[0] = 0; nb = 1; }
    starts[nb] = n;
    size_t maxb = 0; for (uint32_t i = 0; i < nb; i++) if (starts[i + 1] - starts[i] > maxb) maxb = starts[i + 1] - starts[i];
    if (planning_map != MAP_FAILED) {
        munmap(planning_map, n); planning_map = MAP_FAILED; src = NULL;
    }
    J.input_fd = input_fd; J.src = src; J.starts = starts; J.nb = nb; J.slot = slot_size((uint32_t)maxb, codec);
    if (output_fd >= 0) {
        if ((uint64_t)HDR + (uint64_t)nb * IDX > SIZE_MAX) goto cleanup;
        file_index = malloc(HDR + (size_t)nb * IDX);
        if (!file_index) goto cleanup;
        dst = file_index;
    }
    J.output_fd = output_fd; J.default_level = !p->level || (p->mode == KOLMO_ULTRA && p->level == 2); J.native_leaf = native_leaf;   /* ultra level 2 = default ultra + CM graph leaves */
    J.idx = dst + HDR; J.codec = codec; J.level = level; J.B = maxb; J.plans = plans; J.bplan = bplan; J.rstart = rstart;
    J.fqthreads = threads / (int)(nb < 256 ? nb : 256) > 0 ? threads / (int)(nb < 256 ? nb : 256) : 1;   /* threads left over per block */
    J.out = array_alloc(nb, sizeof(uint8_t *), 1); J.outlen = array_alloc(nb, sizeof(size_t), 1);
    if (!J.out || !J.outlen) goto cleanup;
    J.psum = p->mode == KOLMO_RT;
    J.dict = dict; J.dictlen = dictlen; J.prof = prof;
    J.dst = dst; J.cap = cap; J.wnext = 0; J.wpos = HDR + (size_t)nb * IDX; J.werr = 0;
    if (pthread_mutex_init(&J.wmu, NULL)) goto cleanup;
    mutex_ready = 1;
    atomic_init(&J.next, 0); atomic_init(&J.newmodes, 0); atomic_init(&J.err, 0);
    ret = 0;
    if ((size_t)nb > (cap - HDR) / IDX) ret = KOLMO_ERR_DSTSIZE;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    double pt1 = prof_now();
#endif
    if (!ret) run_pool(compress_worker, &J, threads, nb);
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    fprintf(stderr, "compress bytes=%zu threads=%d plan_ms=%.3f blocks_ms=%.3f\n", n, threads, (pt1 - pt0) * 1000, (prof_now() - pt1) * 1000);
#endif
    if (!ret) ret = atomic_load(&J.err);
    if (!ret) ret = J.werr;
    if (!ret && J.wnext != nb) ret = KOLMO_ERR_ALLOC;
    w = J.wpos;
cleanup:
    if (planning_map != MAP_FAILED) munmap(planning_map, n);
    if (J.out) for (uint32_t i = 0; i < nb; i++) free(J.out[i]);
    free(J.outlen); if (mutex_ready) pthread_mutex_destroy(&J.wmu);
    if (plans) for (uint32_t g = 0; g < nseg; g++) { free(plans[g].fs); free(plans[g].rec); }
    free(J.out); free(starts); free(bplan); free(rstart); free(plans); free(ss); free(se);
    zs_trim();
    if (ret) { free(file_index); zs_trim(); return ret; }
    { int nm = atomic_load_explicit(&J.newmodes, memory_order_relaxed); memcpy(dst, "CZN1", 4); dst[4] = (nm & 64) ? 10 : (nm & 32) ? 9 : (nm & 16) ? 8 : (nm & 8) ? 7 : (nm & 4) ? 6 : (nm & 2) ? 5 : (nm & 1) ? 4 : 3; } dst[5] = (uint8_t)p->mode; dst[6] = dst[7] = dst[8] = dst[9] = 0; dst[10] = (uint8_t)J.psum; dst[11] = 0;
    if (dict) { dst[10] |= 2; put32(dst + 6, dictid); }   /* flag bit1 = zstd dictionary; dst[6..9] = dict id for validation */
    else if (prof) { dst[10] |= 4; put32(dst + 6, kp_id(prof)); if (dst[4] < 11) dst[4] = 11; }   /* flag bit 2 = trained profile, its id */
    put64(dst + 12, n); put32(dst + 20, nb); put32(dst + 24, B);
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, (size_t)nb * IDX); put32(dst + 28, hc);
    if (output_fd >= 0) ret = write_block(output_fd, file_index, HDR + (size_t)nb * IDX, 0);
    if (file_index) { free(file_index); zs_trim(); }
    return ret ? ret : (int64_t)w;
}
