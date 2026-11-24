/* The encoder: plan, split, code the blocks on the pool, write the container. */
#ifndef KOLMO_REORDER_MIN
#define KOLMO_REORDER_MIN (64u << 20)   /* the dispatch window: at least this many input bytes, and 2 x threads blocks */
#endif
static uint64_t kolmo_reorder_window;            /* tests: a fixed window in bytes (0: the default) */
static size_t kolmo_reorder_held_max;            /* tests: the last call's reorder-buffer high-water mark, in bytes */
static int64_t compress_impl(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *p);
#ifndef KOLMO_VID_MODES
#define KOLMO_VID_MODES 0x3E   /* modes (bit = kolmo_mode) in which a Y4M file takes the video route (codec 20): all but rt */
#endif
/* Format 2, video (codec 20): a whole 8-bit YUV4MPEG2 file becomes one block whose payload is a Kolmo frame of the
 * kvid layout (codecs/video.c), coded with the caller's parameters (blocks and threads as usual). Returns the frame
 * length written to dst (or output_fd), or 0 when the route does not apply or is not smaller than the input. */
static int64_t vid_compress_frame(const uint8_t *s, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *q) {
    if (n < 4096 || n > UINT32_MAX || memcmp(s, "YUV4MPEG2 ", 10) || !((KOLMO_VID_MODES >> q->mode) & 1)) return 0;
    size_t tn; uint8_t *t = kv_transform(s, n, &tn, q->threads > 0 ? q->threads : default_threads()); if (!t) return 0;
    size_t ic = kolmo_bound(tn, q), tot; uint8_t *in = malloc(ic), *f = NULL; int64_t z = -1;
    if (in) z = compress_impl(t, -1, tn, in, -1, ic, q);
    free(t);
    if (z <= 0 || (tot = HDR + IDX + 4 + (size_t)z) >= n) { free(in); return 0; }
    f = output_fd >= 0 ? malloc(tot) : tot <= cap ? dst : NULL;
    if (!f) { free(in); return 0; }
    memset(f, 0, HDR + IDX + 4); memcpy(f, "CZN1", 4); f[4] = KOLMO_MAX_VERSION; f[5] = (uint8_t)q->mode;
    f[11] = kmem_code(kmem_declared(in[11]) + 2 * (uint64_t)n + tn);   /* the nested frame's figure, the layout and the output */
    put64(f + 12, n); put32(f + 20, 1); put32(f + 24, (uint32_t)n);
    uint8_t *e = f + HDR; put32(e, (uint32_t)n); put32(e + 4, (uint32_t)(4 + z)); put32(e + 8, blk_sum(s, n)); e[12] = C_VID;
    f[HDR + IDX] = T_NONE; memcpy(f + HDR + IDX + 4, in, (size_t)z); free(in);
    uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, IDX); put32(f + 28, hc);
    if (output_fd >= 0) { int r = write_block(output_fd, f, tot, 0); free(f); return r ? r : (int64_t)tot; }
    return (int64_t)tot;
}
/* The parameters the library defines (kolmo.h): mode rt..ultra; level 0..22 (max also 100..999, KLZMA's lc/lp/pb
 * digits); transforms 0..3 with the opt-in bits KOLMO_T_NATIVE_LEAF, KOLMO_T_TYPED_HUF and KOLMO_T_COLUMNS. */
static int kolmo_params_valid(const kolmo_params *p) {
    if ((unsigned)p->mode > KOLMO_ULTRA) return 0;
    if (p->level < (p->mode == KOLMO_FAST ? -1 : 0) || (p->level > 22 && !(p->mode == KOLMO_MAX && p->level >= 100 && p->level <= 999))) return 0;
    int t = p->transforms & ~(KOLMO_T_NATIVE_LEAF | KOLMO_T_TYPED_HUF | KOLMO_T_COLUMNS);
    return t >= 0 && t <= 3;
}
/* Every frame written is version 12 (KOLMO_MAX_VERSION): Kolmo's own coders only, no dictionary, no profile. */
static int64_t compress_impl(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, size_t cap, const kolmo_params *p) {
    kolmo_params d = {KOLMO_FAST, 0, 0, 0, 1}; if (!p) p = &d;
    if (!kolmo_params_valid(p)) return KOLMO_ERR_PARAM;
    /* KOLMO_T_NATIVE_LEAF and KOLMO_T_TYPED_HUF are opt-in bits of transforms: read them, then clear them so every
     * other use of transforms sees the analysis level alone. The native leaf applies to balanced (KOLMO_FAST) with
     * analysis on; elsewhere it is ignored. */
    kolmo_params q = *p; int native_leaf = (q.transforms & KOLMO_T_NATIVE_LEAF) != 0;
    int typed_huf = (q.transforms & KOLMO_T_TYPED_HUF) != 0;
    int columns = (q.transforms & KOLMO_T_COLUMNS) != 0;
    q.transforms &= ~(KOLMO_T_NATIVE_LEAF | KOLMO_T_TYPED_HUF | KOLMO_T_COLUMNS);
    columns = columns && q.transforms && q.mode != KOLMO_RT;
    native_leaf = native_leaf && q.mode == KOLMO_FAST && q.transforms; p = &q;
    if (cap < HDR + IDX) return KOLMO_ERR_DSTSIZE;
    if (n > INT64_MAX) return KOLMO_ERR_CORRUPT;
    /* The typed route below produces a whole frame on its own (one block, one thread, at most 16 MiB, buffer API, no
     * dictionary, default level) and returns it; when it does not apply the normal planner runs. A declared integer
     * NPY / PCM16 WAV that the typed route's size guard rejects is compressed with the plain codec (transforms 0). */
    if (typed_huf && !native_leaf && q.transforms && q.mode == KOLMO_FAST && !q.level && q.threads == 1
        && input_fd < 0 && output_fd < 0 && n <= (16u << 20) && (!q.block || n <= q.block)) {
        int plain = 0; int64_t tr = ktf_encode(src, n, dst, cap, &plain);
        if (tr) return tr;
        if (plain) q.transforms = 0;
    }
    if (q.mode != KOLMO_RT && n >= 4096 && n <= UINT32_MAX) {   /* format 2: video route (codec 20) */
        const uint8_t *vs = src; void *vm = MAP_FAILED;
        if (input_fd >= 0) {                                    /* map the file only when it starts like a Y4M file */
            char m[10];
            if (pread(input_fd, m, 10, 0) == 10 && !memcmp(m, "YUV4MPEG2 ", 10)) {
                vm = mmap(NULL, n, PROT_READ, MAP_PRIVATE, input_fd, 0); vs = vm == MAP_FAILED ? NULL : vm;
            } else vs = NULL;
        }
        int64_t vr = vs && !memcmp(vs, "YUV4MPEG2 ", 10) ? vid_compress_frame(vs, n, dst, output_fd, cap, &q) : 0;
        if (vm != MAP_FAILED) munmap(vm, n);
        if (vr) return vr;
    }
    int threads = p->threads > 0 ? p->threads : default_threads();
    uint32_t B = p->block ? p->block : kolmo_default_block(p, n);   /* slow modes: larger blocks; speed modes: ~8 balanced blocks */
    if (B < 4096) B = 4096;
    int codec, level; mode_codec(p, &codec, &level);
    /* memory (docs/MEMORY-MODEL.md): the governor's limit, and the explicit cap, the only memory figure that may shape
     * the output (blocks of ratio and -x, the -x tables), so that it decodes within the same limit */
    const uint64_t xcap = p->mem_limit && p->mem_limit != KOLMO_MEM_MAX ? p->mem_limit : 0, memcap = kmem_limit_of(p->mem_limit);
    if (xcap && xcap < kmem_floor(p->mode)) return KOLMO_ERR_MEMORY;
    if (!p->block) B = kmem_block_for_cap(p->mode, codec, level, B, xcap);
    enum { MAXSEG = 4096 };
    int64_t ret = KOLMO_ERR_ALLOC; size_t w = 0;
    uint8_t *file_index = NULL;
    job_t J = {0}; int mutex_ready = 0;
    uint32_t nseg = 0, nb = 0; size_t bcap = 2;
    plan_t *plans = NULL;
    uint64_t *starts = NULL, *rstart = NULL; uint32_t *bplan = NULL;
    uint64_t *ss = array_alloc(MAXSEG, sizeof(uint64_t), 0), *se = array_alloc(MAXSEG, sizeof(uint64_t), 0); uint32_t *skind = array_alloc(MAXSEG, sizeof(uint32_t), 1);
    void *planning_map = MAP_FAILED;
    if (!ss || !se || !skind) goto cleanup;
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
    nseg = meta_segments_x(src, n, ss, se, skind, MAXSEG, p->mode == KOLMO_RT ? MT_EXE_MIN_SPEED : MT_EXE_MIN);   /* members, sections, tensors, packed content (plan/meta.c) */
    /* no structure in the headers: one segment per homogeneous run (plan/segment.c), for the modes where a plan per
     * kind of data pays (balanced, eco, hc, ratio; speed keeps its minimal analysis and -x's context mixing adapts on
     * its own). Packed (already compressed) inputs stay one segment. */
    if (nseg == 1 && skind[0] == MK_DATA && src && p->transforms && p->mode >= KOLMO_FAST && p->mode <= KOLMO_MAX && !columns)
        nseg = content_segments(src, n, ss, se, MAXSEG);   /* skind stays MK_DATA (zeroed) for every part */
    plans = array_alloc(nseg, sizeof(plan_t), 1);
    starts = array_alloc(bcap, sizeof(uint64_t), 0);
    bplan = array_alloc(bcap, sizeof(uint32_t), 0);
    rstart = array_alloc(bcap, sizeof(uint64_t), 0);
    if (!plans || !starts || !bplan || !rstart) goto cleanup;
    /* Parallelize independent members, or formula discovery within a single
     * member. Never multiply the two thread budgets. Plans remain in input order. */
    plan_job planning = {src, ss, se, plans, p, nseg, nseg > 1 ? 1 : threads,
                         p->transforms && p->mode == KOLMO_FAST && !p->level,
                         nseg > 1 && p->transforms && p->mode == KOLMO_FAST ? input_fd : -1, native_leaf};
    planning.xcap = xcap; planning.kind = skind;
    atomic_init(&planning.next, 0);
    run_pool(plan_worker, &planning, p->transforms ? threads : 1, nseg);
    /* balanced, eco, hc: neighbouring segments (tar members, content runs) that got the same plain or word-dictionary
     * plan are coded as one, so an LZ window spans them (a source tarball repeats across members: samba -4.6% at hc) */
    if (!p->block && !columns && p->mode >= KOLMO_FAST && p->mode <= KOLMO_HC && nseg > 1) {
        uint32_t w = 1;
        for (uint32_t g = 1; g < nseg; g++) {
            plan_t *a = &plans[w - 1], *b = &plans[g];
            if (a->t == b->t && (a->t == T_NONE || a->t == T_WORD) && !a->fs && !b->fs && !a->rec && !b->rec && !a->cmpz && !b->cmpz
                && a->verify == b->verify && a->packed == b->packed && a->lines == b->lines && !a->wide && !b->wide
                && !a->experimental_dna && !b->experimental_dna && !a->experimental_table && !b->experimental_table
                && !a->experimental_pcm && !b->experimental_pcm && skind[w - 1] == skind[g] && se[w - 1] == ss[g]) { se[w - 1] = se[g]; continue; }
            if (w != g) { plans[w] = plans[g]; memset(&plans[g], 0, sizeof plans[g]); ss[w] = ss[g]; se[w] = se[g]; skind[w] = skind[g]; }
            w++;
        }
        nseg = w;
    }
    if (columns) for (uint32_t g = 0; g < nseg; g++) plans[g].lines = 1;   /* queryable archives: line-aligned blocks */
    if (!p->block && nseg == 1 && (plans[0].wide || plans[0].experimental_table) && B > (2u << 20)) B = 2u << 20;
    for (uint32_t g = 0; g < nseg; g++) {
        const uint8_t *s0 = src + ss[g]; size_t len = (size_t)(se[g] - ss[g]);
        plan_t pl = plans[g];
        uint32_t Bs = B;
        if (!p->block && p->mode == KOLMO_HC && (pl.t == T_WORD || (pl.t == T_NONE && src && !p->level && looks_text(s0, len))))
            Bs = 128u << 20;   /* hc text: one large window */
        if (!p->block && p->mode == KOLMO_ECO && !p->level && src && (pl.t == T_WORD || (pl.t == T_NONE && !pl.packed && looks_text8(s0, len))))
            Bs = len <= (32u << 20) ? 32u << 20 : B;   /* eco text up to 32 MiB: one block-sorting block (gharchive NDJSON, 26 MB: 16 MiB
                                                          * blocks +4.4%, one block -0.9% vs brotli -11); larger files keep 16 MiB (memory) */
#if KOLMO_BAL_WIDE
        if (!p->block && p->mode == KOLMO_FAST && !p->level && pl.t == T_NONE && src && len > KOLMO_BAL_WIDE_MIN) {
                                                                    /* balanced, plain coding: blocks of up to 6 MiB, */
            uint64_t k = (len + KOLMO_BAL_WIDE_BLOCK - 1) / KOLMO_BAL_WIDE_BLOCK;   /* split evenly */
            uint64_t b = ((len + k - 1) / k + 65535) & ~(uint64_t)65535;
            Bs = (uint32_t)(b < KOLMO_BAL_WIDE_BLOCK ? b : KOLMO_BAL_WIDE_BLOCK);
        }
#endif
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
    J.aud_prof = -1;
    if (p->mode != KOLMO_RT && src && n >= 4096) {
        /* format 2: a 16-bit PCM WAV file (mono or stereo). Every block that holds samples is also coded by the audio
         * codec (19, kaud: profile 0 up to hc, profile 1 at max and ultra) and the smaller coding is kept */
        ka_Wav w; const char *werr;
        pthread_once(&ka_once, ka_init);
        if (!ka_wav_parse(src, n, &w, &werr) && w.n) {
            J.aud_prof = p->mode >= KOLMO_MAX ? 1 : 0; J.aud_ch = w.ch; J.aud_off = w.data_off; J.aud_end = w.trl_off;
        } else {                                                /* AIFF (plan/meta.c): the same, samples byte-swapped */
            unsigned ach; uint64_t aoff, aend;
            if (meta_aiff(src, n, &ach, &aoff, &aend)) { J.aud_prof = p->mode >= KOLMO_MAX ? 1 : 0; J.aud_ch = (int)ach; J.aud_off = aoff; J.aud_end = aend; J.aud_be = 1; }
        }
    }
    J.img_on = 0;
    if (p->mode >= KOLMO_MAX && src && n >= (128u << 10)) {
        /* kimg (codec 31, ratio and ultra): an 8-bit image header (PNM/PAM, NPY, BMP, TIFF: plan/meta.c). Every block
         * that holds pixels is also coded by the image codec and the smaller coding is kept */
        size_t iw = 0, ih = 0; unsigned ic = 0;
        if (meta_image8(src, n, &iw, &ic, &ih) && ic >= 1 && ic <= 4 && iw && iw % ic == 0 && iw < ((size_t)1 << 30) && ih < n && n - ih >= iw) {
            J.img_on = 1; J.img_w = (uint32_t)iw; J.img_ch = ic; J.img_off = ih; J.img_end = ih + (n - ih) / iw * iw;
        }
    }
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
    J.output_fd = output_fd; J.default_level = !p->level || (p->mode == KOLMO_ULTRA && p->level == 2); J.native_leaf = native_leaf; J.columns = columns;   /* ultra level 2 = default ultra + CM graph leaves */
    J.idx = dst + HDR; J.codec = codec; J.level = level; J.B = maxb; J.plans = plans; J.bplan = bplan; J.rstart = rstart;
    J.fqthreads = threads / (int)(nb < 256 ? nb : 256) > 0 ? threads / (int)(nb < 256 ? nb : 256) : 1;   /* threads left over per block */
    J.out = array_alloc(nb, sizeof(uint8_t *), 1); J.outlen = array_alloc(nb, sizeof(size_t), 1);
    J.started = array_alloc(nb, 1, 1);
    if (!J.out || !J.outlen || !J.started) goto cleanup;
    /* dispatch (review40 #7): longest first inside a window of input bytes past the first unwritten block, which
     * bounds the reorder buffer; the elastic budget starts with the threads no worker holds (each worker holds its own
     * and fqthreads - 1 for its block's inner regions) and gains a worker's threads when it runs out of blocks */
    {
        uint64_t wb = 2 * (uint64_t)threads * maxb; if (wb < KOLMO_REORDER_MIN) wb = KOLMO_REORDER_MIN;
        J.win = kolmo_reorder_window ? kolmo_reorder_window : wb;
        J.threads = threads;
        const int workers = threads < (int)nb ? threads : (int)nb;
        J.bud = kolmo_budget_create(threads - workers * J.fqthreads);
        if (!J.bud) goto cleanup;
    }
    J.psum = p->mode == KOLMO_RT;
    J.memcap = memcap; J.xcap = xcap; J.cm_leaves = codec == C_CM && level == 2; atomic_init(&J.dneed, 0);
    J.dst = dst; J.cap = cap; J.wnext = 0; J.wpos = HDR + (size_t)nb * IDX; J.werr = 0;
    if (pthread_mutex_init(&J.wmu, NULL)) goto cleanup;
    mutex_ready = 1;
    if (pthread_cond_init(&J.wcv, NULL)) goto cleanup;
    J.wcv_ready = 1;
    atomic_init(&J.next, 0); atomic_init(&J.err, 0);
    ret = 0;
    if ((size_t)nb > (cap - HDR) / IDX) ret = KOLMO_ERR_DSTSIZE;
#if defined(KOLMO_PROF) || defined(KOLMO_TIMING)
    double pt1 = prof_now();
#endif
#ifdef KOLMO_GPU
    /* modes whose blocks may be block-sorted (ratio, -x): the GPU worker joins when there are more blocks than threads */
    if (!ret && (codec == R_LZMA || codec == C_CM)) kolmo_hetero_run(run_pool, compress_worker, &J, threads, nb);
    else
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
    if (J.wcv_ready) pthread_cond_destroy(&J.wcv);
    kolmo_reorder_held_max = J.held_max;   /* (tests: the reorder buffer's high-water mark of the last call) */
    free(J.started); kolmo_budget_destroy(J.bud);
    if (plans) for (uint32_t g = 0; g < nseg; g++) { free(plans[g].fs); free(plans[g].rec); free(plans[g].cmpz); }
    free(J.out); free(starts); free(bplan); free(rstart); free(plans); free(ss); free(se); free(skind);
    zs_trim();
    if (ret) { free(file_index); zs_trim(); return ret; }
    memcpy(dst, "CZN1", 4); dst[4] = KOLMO_MAX_VERSION; dst[5] = (uint8_t)p->mode; dst[6] = dst[7] = dst[8] = dst[9] = 0; dst[10] = (uint8_t)J.psum;
    dst[11] = kmem_code(atomic_load(&J.dneed) ? atomic_load(&J.dneed) : blk_dec_floor(maxb));   /* decode working set per block */
    put64(dst + 12, n); put32(dst + 20, nb); put32(dst + 24, B);
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, (size_t)nb * IDX); put32(dst + 28, hc);
    if (output_fd >= 0) ret = write_block(output_fd, file_index, HDR + (size_t)nb * IDX, 0);
    if (file_index) { free(file_index); zs_trim(); }
    return ret ? ret : (int64_t)w;
}
