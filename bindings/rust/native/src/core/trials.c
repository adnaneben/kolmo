/* ---------------------------------------------------------------- concurrent block trials (ultra, -x)
 * Apache-2.0, Kolmo contributors. Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md); used by
 * core/blocks.c.
 *
 * At ultra a block is coded by context mixing and then by one or more alternatives (LZMA2 or block sorting, the plain
 * bytes of a transformed block, the FASTQ family coder); the smallest is kept. Those trials do not depend on each
 * other's output, only the choice between them does. When the block has threads to spare (threads / blocks > 1), the
 * trials start on their own threads before the context-mixing pass and the choice is made afterwards, with the same
 * rules in the same order, so the frame is identical to the one-thread frame. A trial whose result the rules would not
 * have asked for (a slice test that fails, say) is simply discarded: the cost is CPU that was idle anyway.
 * Each trial is tagged with the input it coded; if the block's input changes before the choice (a marked plan that
 * falls back to the plain bytes), the result is ignored and the trial runs again inline. */
/* FASTQ family coder: name and quality chunks of a sixteenth of the block, between 1 and 4 MiB (rc3 xmode item 6):
 * a 10 MB block decodes 1.4x faster on 4 threads for 0.57%; from 64 MiB the chunks stay at 4 MiB. Bases keep 64 MiB
 * chunks (16 MiB cost 13% on a 100 MB run). The cut depends on the block only, never on the thread count. */
#ifndef KF_CHUNK_MIN
#define KF_CHUNK_MIN ((size_t)1 << 20)
#endif
static size_t kf_chunk_for(size_t n) { size_t c = n / 16; return c < KF_CHUNK_MIN ? KF_CHUNK_MIN : c > (size_t)KF_CHUNK ? (size_t)KF_CHUNK : c; }
typedef struct {
    pthread_t th; int started, codec, level, fq_threads;
    const uint8_t *in; size_t n, cap, res;
    uint8_t *out;                                    /* codec trials: their own output buffer; family coder: its frame */
    int used_native;                                 /* codec trials: a native id was written inside the payload */
    uint64_t resv; kmem_ctx ctx;                     /* the trial's reservation with the governor, the call's cap */
} xtrial_t;
static void *xtrial_main(void *a) {
    xtrial_t *t = a;
    kmem_ctx_set(t->ctx); kmem_tls_job = 1;          /* covered by the trial's own reservation */
    if (t->codec == C_FQ) { t->out = kf_fastq_compress(t->in, t->n, t->fq_threads, kf_chunk_for(t->n), KF_BCHUNK, &t->res); if (!t->out) t->res = 0; }
    else {
        codec_ctx x; memset(&x, 0, sizeof x);
        t->res = t->out ? codec_compress(&x, t->codec, t->level, t->in, t->n, t->out, t->cap) : 0;
        t->used_native = x.used_native;
        codec_ctx_free(&x);
    }
    kmem_tls_job = 0;
    if (t->resv) kmem_release(t->resv);             /* released by the trial itself, as soon as it is done */
    return NULL;
}
/* Starts a trial on its own thread when the governor grants its working set now (optional work); otherwise, or if no
 * thread can be made, nothing starts and xtrial_take misses, so the caller codes it inline after its main coder, with
 * the same result. Codec C_FQ runs the family coder. */
static void xtrial_start(xtrial_t *t, int codec, int level, const uint8_t *in, size_t n, size_t cap, int fq_threads) {
    memset(t, 0, sizeof *t);
    uint64_t need = kmem_enc_trial(codec, level, n, cap);
    if (!kmem_try_reserve(need, kmem_tls_cap)) return;
    t->codec = codec; t->level = level; t->in = in; t->n = n; t->cap = cap; t->fq_threads = fq_threads; t->resv = need; t->ctx = kmem_ctx_get();
    if (codec != C_FQ) t->out = malloc(cap ? cap : 1);
    if (pthread_create(&t->th, NULL, xtrial_main, t)) { kmem_release(need); t->resv = 0; t->codec = 0; free(t->out); t->out = NULL; }
    else t->started = 1;
}
static void xtrial_join(xtrial_t *t) { if (t->started) { pthread_join(t->th, NULL); t->started = 0; t->resv = 0; } }
/* the trial's result if it coded exactly (codec, in, n), else 0 with *hit = 0 (the caller codes inline) */
static __thread int xtrial_native_hit;              /* a taken trial wrote a native id (the block worker's thread) */
static size_t xtrial_take(xtrial_t *t, int codec, const uint8_t *in, size_t n, int *hit) {
    *hit = 0;
    if (!t->codec || t->codec != codec || t->in != in || t->n != n) return 0;
    xtrial_join(t); *hit = 1; if (t->used_native) xtrial_native_hit = 1; return t->res;
}
static void xtrial_free(xtrial_t *t) { xtrial_join(t); free(t->out); memset(t, 0, sizeof *t); }
