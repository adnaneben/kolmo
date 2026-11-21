/* Opt-in research planner hooks of the frontier build (-DKOLMO_FRONTIER). */
#ifdef KOLMO_FRONTIER
/* A packed base costs two bits even in long gaps or repeated motifs. Check
 * several parts of the segment before bypassing the regular planner. */
static int is_simple_dna(const uint8_t *src, size_t len) {
    enum { WINDOW = 1024, MAX_WINDOWS = 12, SIXMERS = 4096 };
    size_t width = len < WINDOW ? len : WINDOW;
    size_t windows = len / width;
    if (windows > MAX_WINDOWS) windows = MAX_WINDOWS;
    size_t step = windows > 1 ? (len - width) / (windows - 1) : 0;
    unsigned lower = 0, upper = 0;
    for (size_t w = 0; w < windows; ++w) {
        const uint8_t *sample = src + w * step;
        size_t pos = 0;
        if (sample[0] == '>') {
            const uint8_t *newline = memchr(sample, '\n', width);
            if (!newline) return 0;
            pos = (size_t)(newline - sample) + 1;
        }
        uint64_t seen[SIXMERS / 64] = {0};
        unsigned kmer = 0, run = 0, known = 0, other = 0, words = 0, distinct = 0;
        for (; pos < width; ++pos) {
            uint8_t c = sample[pos];
            if (c == '\n' || c == '\r') continue;
            uint8_t base = c & 0xdf;
            if (base != 'A' && base != 'C' && base != 'T' && base != 'G') {
                ++other;
                run = 0;
                continue;
            }
            ++known;
            lower += (c >> 5) & 1;
            upper += 1 - ((c >> 5) & 1);
            kmer = ((kmer << 2) | ((c >> 1) & 3u)) & (SIXMERS - 1);
            if (++run < 6) continue;
            uint64_t bit = UINT64_C(1) << (kmer & 63);
            distinct += !(seen[kmer >> 6] & bit);
            seen[kmer >> 6] |= bit;
            ++words;
        }
        if (32 * other > known + other || (words >= 128 && distinct * 3 < words)) return 0;
    }
    /* Entirely lower-case sequence packs as efficiently as upper case. */
    unsigned minority = lower < upper ? lower : upper;
    return 64 * minority <= lower + upper;
}

/* Use the production encoder to measure the serialized block, including its
 * transform metadata and any fallback to the regular codec or raw storage. */
static size_t fast_sample_bytes(const uint8_t *src, size_t len, const plan_t *plan,
                                const kolmo_params *params, uint8_t *body) {
    /* One synchronous block writes directly to body. The ordered queue is
     * unused, and compress_worker manages its own thread-local scratch. */
    uint8_t index[IDX];
    uint64_t starts[] = {0, len}, offset = 0;
    uint32_t which = 0;
    job_t job = {
        .src = src, .input_fd = -1, .dst = body, .starts = starts, .nb = 1,
        .idx = index, .default_level = 1, .output_fd = -1, .B = len,
        .plans = plan, .bplan = &which, .rstart = &offset, .cap = len + 4,
    };
    mode_codec(params, &job.codec, &job.level);
    job.slot = slot_size((uint32_t)len, job.codec);
    if (pthread_mutex_init(&job.wmu, NULL)) return 0;
    atomic_init(&job.next, 0);
    atomic_init(&job.newmodes, 0);
    atomic_init(&job.err, 0);
    compress_worker(&job);
    pthread_mutex_destroy(&job.wmu);
    if (job.werr || atomic_load(&job.err) || job.wnext != 1) return 0;
    return job.wpos;
}

static int sampled_dna_wins(const uint8_t *src, size_t len, const plan_t *normal,
                            const kolmo_params *params) {
    /* A normal transform can depend on whole-segment record geometry. Honor
     * that decision without applying it to a slice with arbitrary boundaries. */
    if (normal->t != T_NONE || normal->experimental_table || normal->experimental_pcm ||
        normal->lags.near) return 0;
    size_t width = len / 32;
    if (width < 4096) width = len < 4096 ? len : 4096;
    if (width > (64u << 10)) width = 64u << 10;
    size_t windows = len / width;
    if (windows > 4) windows = 4;
    size_t step = windows > 1 ? (len - width) / (windows - 1) : 0;
    uint8_t *body = malloc(width + 4);
    if (!body) return 0;
    plan_t dna = {.experimental_dna = 1};
    size_t packed = 0, regular = 0;
    for (size_t w = 0; w < windows; ++w) {
        const uint8_t *s = src + w * step;
        size_t a = fast_sample_bytes(s, width, &dna, params, body);
        size_t b = fast_sample_bytes(s, width, normal, params, body);
        if (!a || !b) { free(body); return 0; }
        packed += a + IDX;
        regular += b + IDX;
    }
    free(body);
    return packed * 100 < regular * 97;
}

/* Select the wider layout against the incumbent bytes on the same complete
 * records. All buffers are bounded by the sample size, independent of input. */
static int prefer_frontier_table(const uint8_t *src, size_t n, const plan_t *pl, uint8_t delim) {
    size_t sample = n < (192u << 10) ? n : (192u << 10);
    while (sample && src[sample - 1] != '\n') --sample;
    if (sample < 8192) return 0;
    size_t cap = sample + TSLACK;
    uint8_t *tmp = malloc(sample + TBL_SLACK(sample) + 64);   /* tbl2_encode below may use the table slack */
    int keep = 0;
    if (!tmp) goto done;
    size_t trial = frontier_table_encode(src, sample, delim, tmp, cap);
    if (!trial) goto done;
    size_t candidate = kprobe_size(tmp, trial);
    const uint8_t *previous = src;
    size_t bytes = sample;
    if (pl->t == T_TBL) {
        int64_t count = tbl2_encode(src, sample, pl->a, pl->c + 1, pl->b, pl->fs, tmp, NULL);
        if (count > 0) { previous = tmp; bytes = (size_t)count; }
    } else if (pl->t == T_TP) {
        int64_t sizes[256];
        if (pl->wide) tpq_wide_fwd(src, sample, pl->a, tmp, sizes);
        else transpose_fwd(src, sample, pl->a, pl->c + 1, pl->b, tmp, sizes);
        previous = tmp;
    } else if (pl->t != T_NONE) goto done;
    size_t reference = kprobe_size_l(previous, bytes, pl->wide ? 1 : 3);
    keep = 100 * (candidate + 4) < 97 * (reference + (pl->t == T_TP ? 4 * (pl->c + 1) : 4));
done:
    free(tmp);
    return keep;
}
#endif
