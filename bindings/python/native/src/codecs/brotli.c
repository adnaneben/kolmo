/* Codec 4: Brotli (eco), window 2^24. vendor/brotli. */
static size_t brotli_bound(size_t n) { return BrotliEncoderMaxCompressedSize(n) ? BrotliEncoderMaxCompressedSize(n) : n + 1024; }
/* The encoder's large buffers (1 MiB and up) come straight from mmap and go back with munmap. Brotli grows its block
 * split and command arrays in many steps (about 300 allocations for a 16 MiB block at quality 8, many near 23 MiB, each a
 * little larger than the last). macOS malloc keeps freed large blocks resident in its large cache, so `-m eco` peaked at
 * 1,078 MiB encoding a 36 MB FASTQ file while brotli's live heap never passed 106 MiB; with the blocks unmapped the peak
 * is about 130 MiB. The allocator never changes the bytes written. */
#define KB_MAP_MIN ((size_t)1 << 20)
#define KB_HDR 16
static void *kb_alloc(void *o, size_t n) {
    (void)o; size_t t = n + KB_HDR; uint8_t *p;
    if (t < n) return NULL;
    if (t >= KB_MAP_MIN) { void *m = mmap(NULL, t, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0); p = m == MAP_FAILED ? NULL : m; }
    else p = malloc(t);
    if (!p) return NULL;
    memcpy(p, &t, sizeof t); return p + KB_HDR;
}
static void kb_free(void *o, void *q) {
    (void)o; if (!q) return;
    uint8_t *p = (uint8_t *)q - KB_HDR; size_t t; memcpy(&t, p, sizeof t);
    if (t >= KB_MAP_MIN) munmap(p, t); else free(p);
}
/* the same calls as BrotliEncoderCompress (vendor/brotli/enc/encode.c) with the allocator above; its fallback cases (an
 * empty input, a failed or oversized stream) go to BrotliEncoderCompress itself, so the output is byte-identical */
static size_t brotli_compress(codec_ctx *x, int level, const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
    (void)x; size_t o = cap;
    if (n && cap) {
        BrotliEncoderState *s = BrotliEncoderCreateInstance(kb_alloc, kb_free, NULL);
        if (s) {
            size_t ai = n, ao = cap, total = 0; const uint8_t *ip = in; uint8_t *op = out;
            BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, (uint32_t)level);
            BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, 24);
            BrotliEncoderSetParameter(s, BROTLI_PARAM_MODE, (uint32_t)BROTLI_MODE_GENERIC);
            BrotliEncoderSetParameter(s, BROTLI_PARAM_SIZE_HINT, (uint32_t)n);
            BROTLI_BOOL r = BrotliEncoderCompressStream(s, BROTLI_OPERATION_FINISH, &ai, &ip, &ao, &op, &total);
            if (!BrotliEncoderIsFinished(s)) r = BROTLI_FALSE;
            BrotliEncoderDestroyInstance(s);
            const size_t mx = BrotliEncoderMaxCompressedSize(n);
            if (r && !(mx && total > mx)) return total;
        }
    }
    int ok = BrotliEncoderCompress(level, 24, BROTLI_MODE_GENERIC, n, in, &o, out);
    return ok ? o : 0;
}
static int brotli_decompress(codec_ctx *x, const uint8_t *in, size_t cn, uint8_t *out, size_t n) {
    (void)x; size_t o = n; int r = BrotliDecoderDecompress(cn, in, &o, out); return (r == BROTLI_DECODER_RESULT_SUCCESS && o == n) ? 0 : -1;
}
static const kolmo_codec codec_brotli = { C_BROTLI, "brotli", brotli_bound, brotli_compress, brotli_decompress, NULL };
