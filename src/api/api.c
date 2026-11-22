/* Public entry points (kolmo.h): parameter checks and the calls into core/. */
/* worst case: every block stored raw (+ index entry); blocks are >= B/2 except the last */
size_t kolmo_bound(size_t n, const kolmo_params *p) {
    uint32_t B = p && p->block ? p->block : (p && p->mode >= KOLMO_MAX ? (128u << 20) : p && p->mode >= KOLMO_ECO ? (16u << 20) : kolmo_fast_block(n));
    size_t nb = n / (B / 2 > 0 ? B / 2 : 1) + 2 + 2 * (n >> 20);     /* + archive segments (>= 1 MiB each) */
    return HDR + nb * (IDX + 4) + n + 64 + 2 * IDX;
}

int64_t kolmo_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p) {
    return compress_impl(src, -1, n, dst, -1, cap, p);
}

int kolmo_set_gpu(int on) {
#ifdef KOLMO_GPU
    return kolmo_gpu_set(on);
#else
    (void)on; return 0;
#endif
}

/* the header and index checked (decode.c, frame_head): KOLMO_ERR_LEGACY for frames of earlier versions */
int64_t kolmo_original_size(const uint8_t *src, size_t n) {
    if (!src) return KOLMO_ERR_CORRUPT;
    uint64_t o, e; int r = frame_head(src, n, &o, &e); if (r) return r;
    return (int64_t)o;
}
int64_t kolmo_frame_size(const uint8_t *src, size_t n) {
    if (!src) return KOLMO_ERR_CORRUPT;
    uint64_t o, e; int r = frame_head(src, n, &o, &e); if (r) return r;
    if (e > (uint64_t)INT64_MAX) return KOLMO_ERR_CORRUPT;
    if (e < n && frame_tail_ok(src, n, e)) return (int64_t)n;   /* container extensions ending the buffer belong to it */
    return (int64_t)e;
}
/* the public decoders take one whole frame: nothing after its last payload but container extensions */
static int64_t whole_frame(const uint8_t *src, size_t n) {
    if (!src) return KOLMO_ERR_CORRUPT;
    uint64_t o, e; int r = frame_head(src, n, &o, &e); if (r) return r;
    if (!frame_tail_ok(src, n, e)) return KOLMO_ERR_CORRUPT;
    return (int64_t)o;
}

int64_t kolmo_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads) {
    int64_t o = whole_frame(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, NULL);
}

int64_t kolmo_decompress_limited(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                               int threads, const kolmo_decode_limits *limits) {
    int64_t o = whole_frame(src, n); if (o < 0) return o;
    if (limits && limits->max_output_bytes && (uint64_t)o > limits->max_output_bytes) return KOLMO_ERR_LIMIT;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, limits);
}

int64_t kolmo_read_range(const uint8_t *src, size_t n, uint64_t off, size_t len, uint8_t *dst, int threads) {
    int64_t o = whole_frame(src, n); if (o < 0) return o;
    if (off > (uint64_t)o) return KOLMO_ERR_RANGE;
    if (len > (uint64_t)o - off) len = (size_t)((uint64_t)o - off);
    if (!len) return 0;
    return run_decode(src, -1, n, dst, -1, threads, off, off + len, 1, NULL);
}

/* Internal hook for queryable archives (src/core/index.c, declared in src/core/features.h): column col of block i
 * when the block is a column-split table (plan TBL, flag 13). 0 and *out (one line per row), 1 when the block is
 * another kind (the caller decodes it whole), or a negative error. */
int kolmo__block_column(const uint8_t *src, size_t n, uint32_t i, int col, uint8_t **out, size_t *outn,
                        uint64_t *decoded, uint64_t *read) {
    *out = NULL; *outn = 0; *decoded = *read = 0;
    parsed_t P; int r = parse(src, n, &P, NULL); if (r) return r;
    if (i >= P.nb) { free(P.poff); free(P.roff); return KOLMO_ERR_CORRUPT; }
    const uint8_t *e = P.idx + (size_t)i * IDX, *pay = src + P.poff[i];
    size_t len = get32(e), cl = get32(e + 4); int codec = e[12], tflag = e[13];
    free(P.poff); free(P.roff);
    if (P.psum || tflag != 13 || cl < 4 || pay[0] != T_TBL) return 1;
    codec_ctx x = {0}; kbuf o = {0};
    r = tblc_column(&x, codec, pay + 4, cl - 4, len, col, &o, decoded, read);
    codec_ctx_free(&x);
    if (r) { free(o.p); return KOLMO_ERR_CORRUPT; }
    *read += 4;
    *out = o.p ? o.p : malloc(1); *outn = o.n;
    if (!*out) return KOLMO_ERR_ALLOC;
    return 0;
}

/* ------------------------------------------------------------------ memory limits (docs/MEMORY-MODEL.md) */
int kolmo_mem_parse(const char *s, uint64_t *out) { uint64_t v; if (!out || kmem_parse_size(s, &v)) return -1; *out = v; return 0; }
uint64_t kolmo_mem_floor(kolmo_mode mode) { return kmem_floor((int)mode); }
uint64_t kolmo_mem_auto(void) { return kmem_limit_of(0); }
int kolmo_check_frame(const uint8_t *src, size_t n, const kolmo_decode_limits *limits) {
    int64_t w = whole_frame(src, n); if (w < 0) return (int)w;   /* no allocation before the header checks pass */
    parsed_t P; int r = parse(src, n, &P, limits); if (r) return r;
    uint64_t maxraw = 0; for (uint32_t i = 0; i < P.nb; i++) if (P.roff[i + 1] - P.roff[i] > maxraw) maxraw = P.roff[i + 1] - P.roff[i];
    r = frame_admit(&P, maxraw, kmem_limit_of(limits ? limits->max_memory : 0));
    free(P.poff); free(P.roff); return r;
}
void kolmo_set_memlimit(uint64_t limit) { atomic_store_explicit(&kmem_default_limit, limit, memory_order_relaxed); }
int64_t kolmo_decode_memory(const uint8_t *src, size_t n) {
    if (!src) return KOLMO_ERR_CORRUPT;
    { int v = frame_version_check(src, n); if (v) return v; }
    uint64_t d = kmem_declared(src[11]);
    if (d) return (int64_t)d;
    /* a frame that declares nothing: the buffers of its largest block, from the index when it is there */
    uint32_t nb = get32(src + 20); uint64_t maxraw = 0;
    if ((uint64_t)nb * IDX + HDR > n) return 0;
    for (uint32_t i = 0; i < nb; i++) { uint64_t r = get32(src + HDR + (size_t)i * IDX); if (r > maxraw) maxraw = r; }
    return (int64_t)blk_dec_floor(maxraw);
}

const char *kolmo_strerror(int64_t c) {
    switch (c) {
        case KOLMO_ERR_CORRUPT: return "corrupt or unsupported input";
        case KOLMO_ERR_QUERY: return "no such column, or a pattern that does not compile";
        case KOLMO_ERR_CHECKSUM: return "checksum mismatch (data corrupted)";
        case KOLMO_ERR_DSTSIZE: return "destination buffer too small";
        case KOLMO_ERR_ALLOC: return "out of memory";
        case KOLMO_ERR_CODEC: return "codec error";
        case KOLMO_ERR_DICT: return "dictionary required or mismatched (compressed with a different dictionary)";
        case KOLMO_ERR_IO: return "file I/O error";
        case KOLMO_ERR_LIMIT: return "decoding resource limit exceeded";
        case KOLMO_ERR_PROFILE: return "trained profile required, mismatched or damaged (decode with the profile the file was compressed with)";
        case KOLMO_ERR_TARGET: return "invalid --target or calibration table";
        case KOLMO_ERR_MEMORY: return "memory limit: the frame needs more memory to decode than the limit, or the limit is below the mode's floor";
        case KOLMO_ERR_LEGACY: return "written by an earlier Kolmo (1.x or a release candidate) that used library codecs; this build reads only Kolmo-native frames";
        case KOLMO_ERR_UNSUPPORTED: return "not available in this build: dictionaries and trained profiles need library codecs";
        case KOLMO_ERR_PARAM: return "invalid parameter (mode, level or transforms out of range)";
        case KOLMO_ERR_RANGE: return "range outside the decoded data";
    }
    return c >= 0 ? "ok" : "unknown error";
}

/* ------------------------------------------------------------------ dictionaries (not available in this build)
 * Dictionaries were applied by the zstd, Brotli and LZ4 coders; with Kolmo's own coders only (container version 12)
 * there are none. The calls stay: training fails with KOLMO_ERR_UNSUPPORTED, and so does every call that is given a
 * dictionary; without one they behave as the plain calls. */

int64_t kolmo_train_dict(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                       uint8_t *dict, size_t dict_cap) {
    (void)samples; (void)sizes; (void)nsamples; (void)dict; (void)dict_cap;
    return KOLMO_ERR_UNSUPPORTED;
}

int64_t kolmo_compress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                          const kolmo_params *p, const void *dict, size_t dictlen) {
    if (dict && dictlen) return KOLMO_ERR_UNSUPPORTED;
    return kolmo_compress(src, n, dst, cap, p);
}

int64_t kolmo_decompress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                            const void *dict, size_t dictlen) {
    if (dict && dictlen) return KOLMO_ERR_UNSUPPORTED;
    return kolmo_decompress(src, n, dst, cap, threads);
}

int64_t kolmo_compress_from_fd(int fd, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                             const void *dict, size_t dictlen) {
    struct stat st;
    if (dict && dictlen) return KOLMO_ERR_UNSUPPORTED;
    if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    return compress_impl((const uint8_t *)"", fd, n, dst, -1, cap, p);
}

int64_t kolmo_decompress_to_fd(const uint8_t *src, size_t n, int fd, int threads,
                             const void *dict, size_t dictlen) {
    if (dict && dictlen) return KOLMO_ERR_UNSUPPORTED;
    if (fd < 0) return KOLMO_ERR_IO;
    int64_t size = whole_frame(src, n);
    if (size < 0) return size;
    return run_decode(src, -1, n, NULL, fd, threads, 0, 0, 0, NULL);
}

int64_t kolmo_compress_file(int input_fd, int output_fd, size_t n, const kolmo_params *p,
                          const void *dict, size_t dictlen) {
    struct stat st;
    if (dict && dictlen) return KOLMO_ERR_UNSUPPORTED;
    if (output_fd < 0 || fstat(input_fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    return compress_impl((const uint8_t *)"", input_fd, n, NULL, output_fd, INT64_MAX, p);
}

int64_t kolmo_decompress_file(int input_fd, int output_fd, size_t n, int threads,
                            const void *dict, size_t dictlen) {
    uint8_t header[HDR];
    if (dict && dictlen) return KOLMO_ERR_UNSUPPORTED;
    if (n < HDR) return KOLMO_ERR_CORRUPT;
    struct stat st;
    if (output_fd < 0 || fstat(input_fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (read_block(input_fd, header, HDR, 0)) return KOLMO_ERR_IO;
    { int v = frame_version_check(header, HDR); if (v) return v; }  /* KOLMO_ERR_LEGACY for frames of earlier versions */
    uint32_t nb = get32(header + 20);
    if (nb > (n - HDR) / IDX) return KOLMO_ERR_CORRUPT;
    size_t index_size = HDR + (size_t)nb * IDX;
    uint8_t *index = malloc(index_size);
    if (!index) return KOLMO_ERR_ALLOC;
    memcpy(index, header, HDR);
    int64_t ret = read_block(input_fd, index + HDR, index_size - HDR, HDR);
    uint64_t orig, end;
    if (!ret) ret = frame_head(index, index_size, &orig, &end);
    if (!ret && end != n) {                                     /* after the last payload: container extensions only */
        uint8_t t[24];
        if (end > n || n - end < 24) ret = KOLMO_ERR_CORRUPT;
        else if (read_block(input_fd, t, 24, (off_t)(n - 24))) ret = KOLMO_ERR_IO;
        else if (memcmp(t, "KEXT", 4) || t[4] != 1 || t[5] || t[6] || t[7] || kolmo_crc32c(0, t, 20) != get32(t + 20) ||
                 get64(t + 12) != n - 24 - end) ret = KOLMO_ERR_CORRUPT;
    }
    if (!ret) ret = run_decode(index, input_fd, n, NULL, output_fd, threads, 0, 0, 0, NULL);
    free(index);
    zs_trim();
    return ret;
}
