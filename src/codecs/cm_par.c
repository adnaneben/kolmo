/* ---------------------------------------------------------------- codec 25: context mixing over parts (CMP)
 * Apache-2.0, Kolmo contributors. Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md).
 *
 * The block's bytes are cut into parts, and each part is coded by its own context-mixing model (codec 7's stream,
 * model 3 for text parts and model 2 otherwise, as codec 7 chooses) on its own thread, for encoding and decoding.
 * The cuts come from the encoder, never from the thread count, so a frame is the same whatever -T is:
 *   - the columns of a table (transpose plan, T_TP): one model per column (rc3 plan, xmode item 4), which on the
 *     tables measured is also smaller than one model over the rows;
 *   - two halves of a large non-text stream (xmode item 5, from CMP_SPLIT_MIN, chosen in core/blocks.c).
 * Payload: version u8 (0) | parts uv | per part: raw size uv, coded size uv | the parts' codec-7 streams in order.
 * Table memory per part follows the part's size (cm_bits_for), so parts in parallel need about what one model over
 * the whole block would. */
enum { CMP_MAXPARTS = 256 };
#ifndef CMP_SPLIT_MIN
#define CMP_SPLIT_MIN ((size_t)32 << 20)   /* single streams from 32 MiB are coded as two halves (not text) */
#endif
static size_t cmp_putv(uint8_t *p, uint64_t v) { size_t k = 0; while (v >= 128) { p[k++] = (uint8_t)(v | 128); v >>= 7; } p[k++] = (uint8_t)v; return k; }
static int cmp_getv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0; int s = 0;
    while (*i < n && s < 64) { uint8_t b = p[(*i)++]; r |= (uint64_t)(b & 127) << s; if (!(b & 128)) { *v = r; return 0; } s += 7; }
    return -1;
}
typedef struct {
    const uint8_t *in; uint8_t *out; const size_t *rawn, *rawo, *cn, *co; size_t *res; uint8_t **buf; int np, model, dec, bbcap; atomic_int next, err;
} cmp_job;
static void *cmp_worker(void *a) {
    cmp_job *j = a;
    for (;;) {
        int k = atomic_fetch_add(&j->next, 1); if (k >= j->np) break;
        const size_t n = j->rawn[k];
        if (j->dec) {
            if (cm_decompress(j->in + j->co[k], j->cn[k], j->out + j->rawo[k], n)) atomic_store(&j->err, 1);
        } else {
            const uint8_t *p = j->in + j->rawo[k]; const size_t cap = cm_bound(n);
            j->buf[k] = malloc(cap);
            /* each part: the raced learning settings of codec 7 (one thread each: the parts already run side by side) */
            j->res[k] = j->buf[k] ? cm_compress_raced(p, n, j->buf[k], cap, j->model ? j->model : looks_text(p, n) ? 3 : 2, j->bbcap, 1) : 0;
            if (!j->res[k]) atomic_store(&j->err, 1);
        }
    }
    return NULL;
}
static void cmp_run(cmp_job *j, int threads) {
    if (threads > j->np) threads = j->np;
    if (threads > 64) threads = 64;
    pthread_t th[64]; int started = 0;
    for (int t = 1; t < threads; t++) { if (pthread_create(&th[started], NULL, cmp_worker, j)) break; started++; }
    cmp_worker(j);
    for (int t = 0; t < started; t++) pthread_join(th[t], NULL);
}
/* parts: np sizes summing to n (each > 0); model 1, 2 or 3, or 0: model 3 for text parts, else 2 (codec 7's rule).
 * 0 on failure or when the result does not fit cap. */
static size_t cmp_compress_cap(const uint8_t *in, size_t n, const size_t *parts, int np, uint8_t *out, size_t cap, int threads, int model, int bbcap) {
    if (np < 1 || np > CMP_MAXPARTS) return 0;
    size_t rawo[CMP_MAXPARTS], res[CMP_MAXPARTS] = {0}, o = 0; uint8_t *buf[CMP_MAXPARTS] = {0};
    for (int k = 0; k < np; k++) { if (!parts[k] || parts[k] >= ((size_t)1 << 28)) return 0; rawo[k] = o; o += parts[k]; }
    if (o != n) return 0;
    cmp_job j = {in, NULL, parts, rawo, NULL, NULL, res, buf, np, model, 0, bbcap};
    atomic_init(&j.next, 0); atomic_init(&j.err, 0);
    cmp_run(&j, threads);
    size_t w = 0, tot = 1 + 10;
    for (int k = 0; k < np; k++) tot += 20 + res[k];
    if (!atomic_load(&j.err) && tot <= cap) {
        out[w++] = 0; w += cmp_putv(out + w, (uint64_t)np);
        for (int k = 0; k < np; k++) { w += cmp_putv(out + w, parts[k]); w += cmp_putv(out + w, res[k]); }
        for (int k = 0; k < np; k++) { memcpy(out + w, buf[k], res[k]); w += res[k]; }
    }
    for (int k = 0; k < np; k++) free(buf[k]);
    return w;
}
static size_t cmp_compress_model(const uint8_t *in, size_t n, const size_t *parts, int np, uint8_t *out, size_t cap, int threads, int model) {
    return cmp_compress_cap(in, n, parts, np, out, cap, threads, model, 0);
}
static int cmp_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n, int threads) {
    size_t i = 1; uint64_t np, v;
    if (cn < 2 || in[0] != 0 || cmp_getv(in, cn, &i, &np) || np < 1 || np > CMP_MAXPARTS) return -1;
    size_t rawn[CMP_MAXPARTS], rawo[CMP_MAXPARTS], cl[CMP_MAXPARTS], co[CMP_MAXPARTS], ro = 0;
    for (uint64_t k = 0; k < np; k++) {
        if (cmp_getv(in, cn, &i, &v) || !v || v > n - ro) return -1;
        rawn[k] = (size_t)v; rawo[k] = ro; ro += (size_t)v;
        if (cmp_getv(in, cn, &i, &v) || v > cn) return -1;
        cl[k] = (size_t)v;
    }
    if (ro != n) return -1;
    for (uint64_t k = 0; k < np; k++) { if (cl[k] > cn - i) return -1; co[k] = i; i += cl[k]; }
    if (i != cn) return -1;
    cmp_job j = {in, out, rawn, rawo, cl, co, NULL, NULL, (int)np, 0, 1, 0};
    atomic_init(&j.next, 0); atomic_init(&j.err, 0);
    cmp_run(&j, threads > 0 ? threads : 1);
    return atomic_load(&j.err) ? -1 : 0;
}

/* codec 25 table: written at block level (core/blocks.c chooses the parts); decoded with the block's threads */
static size_t cmp_bound(size_t n) { return n + n / 16 + 64 + 24 * CMP_MAXPARTS; }
static int cmp_codec_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) { return cmp_decompress(in, cn, out, n, x->threads); }
static const kolmo_codec codec_cm_par = { C_CMP, "cm_par", cmp_bound, NULL, cmp_codec_decompress, NULL };
