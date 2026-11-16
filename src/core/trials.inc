/* ---------------------------------------------------------------- concurrent block trials (ultra, -x)
 * Apache-2.0, Kolmo contributors. Included by src/kolmo.c after the codecs (rc3 xmode lane; the structure lane moves it
 * into core/blocks.c).
 *
 * At ultra a block is coded by context mixing and then by one or more alternatives (LZMA2 or block sorting, the plain
 * bytes of a transformed block, the FASTQ family coder); the smallest is kept. Those trials do not depend on each
 * other's output, only the choice between them does. When the block has threads to spare (threads / blocks > 1), the
 * trials start on their own threads before the context-mixing pass and the choice is made afterwards, with the same
 * rules in the same order, so the frame is identical to the one-thread frame. A trial whose result the rules would not
 * have asked for (a slice test that fails, say) is simply discarded: the cost is CPU that was idle anyway.
 * Each trial is tagged with the input it coded; if the block's input changes before the choice (a marked plan that
 * falls back to the plain bytes), the result is ignored and the trial runs again inline. */
typedef struct {
    pthread_t th; int started, codec, level, fq_threads;
    const uint8_t *in; size_t n, cap, res;
    uint8_t *out;                                    /* codec trials: their own output buffer; family coder: its frame */
} xtrial_t;
static void *xtrial_main(void *a) {
    xtrial_t *t = a;
    if (t->codec == C_FQ) { t->out = kf_fastq_compress(t->in, t->n, t->fq_threads, KF_CHUNK, KF_BCHUNK, &t->res); if (!t->out) t->res = 0; }
    else {
        codec_ctx x; memset(&x, 0, sizeof x);
        t->res = t->out ? codec_compress(&x, t->codec, t->level, t->in, t->n, t->out, t->cap) : 0;
        codec_ctx_free(&x);
    }
    return NULL;
}
/* starts a trial on its own thread (inline if no thread can be made); codec C_FQ runs the family coder */
static void xtrial_start(xtrial_t *t, int codec, int level, const uint8_t *in, size_t n, size_t cap, int fq_threads) {
    memset(t, 0, sizeof *t);
    t->codec = codec; t->level = level; t->in = in; t->n = n; t->cap = cap; t->fq_threads = fq_threads;
    if (codec != C_FQ) t->out = malloc(cap ? cap : 1);
    if (pthread_create(&t->th, NULL, xtrial_main, t)) xtrial_main(t); else t->started = 1;
}
static void xtrial_join(xtrial_t *t) { if (t->started) { pthread_join(t->th, NULL); t->started = 0; } }
/* the trial's result if it coded exactly (codec, in, n), else 0 with *hit = 0 (the caller codes inline) */
static size_t xtrial_take(xtrial_t *t, int codec, const uint8_t *in, size_t n, int *hit) {
    *hit = 0;
    if (!t->codec || t->codec != codec || t->in != in || t->n != n) return 0;
    xtrial_join(t); *hit = 1; return t->res;
}
static void xtrial_free(xtrial_t *t) { xtrial_join(t); free(t->out); memset(t, 0, sizeof *t); }
