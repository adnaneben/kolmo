/* Public entry points (kolmo.h): parameter checks and the calls into core/. */
/* worst case: every block stored raw (+ index entry); blocks are >= B/2 except the last */
size_t kolmo_bound(size_t n, const kolmo_params *p) {
    uint32_t B = p && p->block ? p->block : (p && p->mode >= KOLMO_MAX ? (128u << 20) : p && p->mode >= KOLMO_ECO ? (16u << 20) : kolmo_fast_block(n));
    size_t nb = n / (B / 2 > 0 ? B / 2 : 1) + 2 + 2 * (n >> 20);     /* + archive segments (>= 1 MiB each) */
    return HDR + nb * (IDX + 4) + n + 64 + 2 * IDX;
}

int64_t kolmo_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p) {
    return compress_impl(src, -1, n, dst, -1, cap, p, NULL, 0, 0);
}

int64_t kolmo_original_size(const uint8_t *src, size_t n) { return n >= HDR && !memcmp(src, "CZN1", 4) ? (int64_t)get64(src + 12) : KOLMO_ERR_CORRUPT; }

int64_t kolmo_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads) {
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, NULL, 0, 0, NULL);
}

int64_t kolmo_decompress_limited(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                               int threads, const kolmo_decode_limits *limits) {
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if (limits && limits->max_output_bytes && (uint64_t)o > limits->max_output_bytes) return KOLMO_ERR_LIMIT;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, NULL, 0, 0, limits);
}

int64_t kolmo_read_range(const uint8_t *src, size_t n, uint64_t off, size_t len, uint8_t *dst, int threads) {
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if (off > (uint64_t)o) return KOLMO_ERR_CORRUPT;
    if (len > (uint64_t)o - off) len = (size_t)((uint64_t)o - off);
    if (!len) return 0;
    return run_decode(src, -1, n, dst, -1, threads, off, off + len, 1, NULL, 0, 0, NULL);
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
    if (P.ver < 2 || P.psum || tflag != 13 || cl < 4 || pay[0] != T_TBL) return 1;
    if (P.needdict) return KOLMO_ERR_DICT;
    codec_ctx x = {0}; kbuf o = {0};
    r = tblc_column(&x, codec, pay + 4, cl - 4, len, col, &o, decoded, read);
    codec_ctx_free(&x);
    if (r) { free(o.p); return KOLMO_ERR_CORRUPT; }
    *read += 4;
    *out = o.p ? o.p : malloc(1); *outn = o.n;
    if (!*out) return KOLMO_ERR_ALLOC;
    return 0;
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
    }
    return c >= 0 ? "ok" : "unknown error";
}

/* ------------------------------------------------------------------ dictionary */

int64_t kolmo_train_dict(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                       uint8_t *dict, size_t dict_cap) {
    if (!samples || !sizes || !nsamples || !dict || dict_cap < 256) return KOLMO_ERR_CORRUPT;
    size_t r = ZDICT_trainFromBuffer(dict, dict_cap, samples, sizes, (unsigned)nsamples);
    if (ZDICT_isError(r)) return KOLMO_ERR_CODEC;
    return (int64_t)r;
}

int64_t kolmo_compress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                          const kolmo_params *p, const void *dict, size_t dictlen) {
    if (!dict || !dictlen) return kolmo_compress(src, n, dst, cap, p);
    uint32_t id = (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u);   /* dictionary id seed, fixed by the format */
    return compress_impl(src, -1, n, dst, -1, cap, p, dict, dictlen, id);
}

int64_t kolmo_decompress_dict(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                            const void *dict, size_t dictlen) {
    if (!dict || !dictlen) return kolmo_decompress(src, n, dst, cap, threads);
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    uint32_t id = (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u);
    return run_decode(src, -1, n, dst, -1, threads, 0, 0, 0, dict, dictlen, id, NULL);
}

int64_t kolmo_compress_from_fd(int fd, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                             const void *dict, size_t dictlen) {
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (!dictlen) dict = NULL;
    uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
    return compress_impl((const uint8_t *)"", fd, n, dst, -1, cap, p, dict, dictlen, id);
}

int64_t kolmo_decompress_to_fd(const uint8_t *src, size_t n, int fd, int threads,
                             const void *dict, size_t dictlen) {
    if (fd < 0) return KOLMO_ERR_IO;
    int64_t size = kolmo_original_size(src, n);
    if (size < 0) return size;
    if (!dictlen) dict = NULL;
    uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
    return run_decode(src, -1, n, NULL, fd, threads, 0, 0, 0, dict, dictlen, id, NULL);
}

int64_t kolmo_compress_file(int input_fd, int output_fd, size_t n, const kolmo_params *p,
                          const void *dict, size_t dictlen) {
    struct stat st;
    if (output_fd < 0 || fstat(input_fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (!dictlen) dict = NULL;
    uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
    return compress_impl((const uint8_t *)"", input_fd, n, NULL, output_fd, INT64_MAX, p, dict, dictlen, id);
}

int64_t kolmo_decompress_file(int input_fd, int output_fd, size_t n, int threads,
                            const void *dict, size_t dictlen) {
    uint8_t header[HDR];
    if (n < HDR) return KOLMO_ERR_CORRUPT;
    struct stat st;
    if (output_fd < 0 || fstat(input_fd, &st) || !S_ISREG(st.st_mode) ||
        st.st_size < 0 || (uintmax_t)st.st_size < n) return KOLMO_ERR_IO;
    if (read_block(input_fd, header, HDR, 0)) return KOLMO_ERR_IO;
    int64_t size = kolmo_original_size(header, HDR);
    uint32_t nb = get32(header + 20);
    if (size < 0 || header[4] < 1 || header[4] > KOLMO_MAX_VERSION || nb > (n - HDR) / IDX) return KOLMO_ERR_CORRUPT;
    size_t index_size = HDR + (size_t)nb * IDX;
    uint8_t *index = malloc(index_size);
    if (!index) return KOLMO_ERR_ALLOC;
    memcpy(index, header, HDR);
    int64_t ret = read_block(input_fd, index + HDR, index_size - HDR, HDR);
    if (!ret) {
        if (!dictlen) dict = NULL;
        uint32_t id = dict ? (uint32_t)kolmo_xxh3(dict, dictlen, 0x637a6e44u) : 0;
        ret = run_decode(index, input_fd, n, NULL, output_fd, threads, 0, 0, 0, dict, dictlen, id, NULL);
    }
    free(index);
    zs_trim();
    return ret;
}
