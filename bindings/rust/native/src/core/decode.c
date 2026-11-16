/* The decoder: header and index parsing, block decoding on the pool. */
/* ------------------------------------------------------------------ decoding */
typedef struct { uint64_t orig; uint32_t nb; int ver, psum, needdict; uint32_t dictid; plan_t pl; const uint8_t *idx; uint64_t *poff; uint64_t *roff; } parsed_t;

static int parse(const uint8_t *s, size_t n, parsed_t *P, const kolmo_decode_limits *limits) {
    if (n < HDR || memcmp(s, "CZN1", 4) || s[4] < 1 || s[4] > KOLMO_MAX_VERSION) return KOLMO_ERR_CORRUPT;
    P->ver = s[4]; P->psum = P->ver >= 3 && (s[10] & 1);
    P->needdict = P->ver >= 3 && (s[10] & 2); P->dictid = P->needdict ? get32(s + 6) : 0;
    P->orig = get64(s + 12); P->nb = get32(s + 20);
    if (limits && ((limits->max_output_bytes && P->orig > limits->max_output_bytes) ||
                   (limits->max_blocks && P->nb > limits->max_blocks))) return KOLMO_ERR_LIMIT;
    P->pl = (plan_t){s[6], s[7], s[8], s[9]};
    if ((uint64_t)P->nb * IDX + HDR > n) return KOLMO_ERR_CORRUPT;
    uint32_t hc = kolmo_crc32c(0, s, 28); hc = kolmo_crc32c(hc, s + HDR, (size_t)P->nb * IDX);
    if (hc != get32(s + 28)) return KOLMO_ERR_CHECKSUM;
    P->idx = s + HDR;
    P->poff = array_alloc((size_t)P->nb + 1, sizeof(uint64_t), 0);
    P->roff = array_alloc((size_t)P->nb + 1, sizeof(uint64_t), 0);
    if (!P->poff || !P->roff) { free(P->poff); free(P->roff); return KOLMO_ERR_ALLOC; }
    uint64_t po = HDR + (uint64_t)P->nb * IDX, ro = 0;
    for (uint32_t i = 0; i < P->nb; i++) {
        P->poff[i] = po; P->roff[i] = ro;
        uint32_t cl = get32(P->idx + (size_t)i * IDX + 4), raw = get32(P->idx + (size_t)i * IDX);
        if (limits && limits->max_block_bytes && raw > limits->max_block_bytes) {
            free(P->poff); free(P->roff); return KOLMO_ERR_LIMIT;
        }
        if (po > n || cl > n - po || ro > P->orig || raw > P->orig - ro) {
            free(P->poff); free(P->roff); return KOLMO_ERR_CORRUPT;
        }
        po += cl; ro += raw;
    }
    P->poff[P->nb] = po; P->roff[P->nb] = ro;
    if (po > n || ro != P->orig) { free(P->poff); free(P->roff); return KOLMO_ERR_CORRUPT; }
    return 0;
}

typedef struct { const uint8_t *src; parsed_t *P; uint8_t *dst; uint32_t lo, hi; atomic_uint next; atomic_int err;
                 int input_fd, output_fd, fqthreads; size_t maxraw, maxcompressed; uint64_t clip_lo, clip_hi; const void *dict; size_t dictlen; } djob_t;

static int decode_block(const djob_t *J, uint32_t i, codec_ctx *x, tls_t *S, const uint8_t *pay, uint8_t *tbuf, uint8_t *out) {
    const parsed_t *P = J->P; const uint8_t *e = P->idx + (size_t)i * IDX;
    size_t len = get32(e), cl = get32(e + 4); int codec = e[12], tflag = e[13];
    size_t meta = 0;
    x->allow_nplane = 0; x->allow_f2 = 0;
    if (codec == C_NPLANE || codec == C_PLANES || codec == C_HZ || codec == C_ZRUN) return KOLMO_ERR_CORRUPT;   /* graph leaves only */
    plan_t pl = P->pl;
    if (P->psum) {                                             /* rt: the checksum covers the stored bytes; checked first */
        if (cl < 4 || (uint32_t)kolmo_xxh3(pay + 4, cl - 4, 0x637a6e31u ^ get32(pay)) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
    }
    if (P->ver >= 2) {                                         /* per-block plan */
        if (cl < 4) return KOLMO_ERR_CORRUPT;
        pl.t = pay[0]; pl.a = pay[1]; pl.b = pay[2]; pl.c = pay[3]; pay += 4; cl -= 4;
        if (pl.t > T_LINES || (pl.t == T_KG && P->ver < 7) || (pl.t == T_LINES && P->ver < 8)) return KOLMO_ERR_CORRUPT;
    }
    if (codec == C_FQ) {                                        /* FASTQ family block (version 10) */
        if (P->ver < 10 || pl.t != T_NONE || pl.a || pl.b || pl.c || tflag) return KOLMO_ERR_CORRUPT;
        if (kf_fastq_decompress(pay, cl, out, len, J->fqthreads)) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (codec == C_VID) return KOLMO_ERR_CORRUPT;                /* video frames are decoded whole, by run_decode */
    if (codec == C_AUD) {                                       /* audio (kaud) block (version 11) */
        if (P->ver < 11 || pl.t != T_NONE || pl.a || pl.b || pl.c || tflag) return KOLMO_ERR_CORRUPT;
        if (aud_decompress(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (codec == C_GRAPH || pl.t == T_KG) {                     /* kolmo graph block (version 7) */
        if (codec != C_GRAPH || pl.t != T_KG || tflag || P->ver < 7) return KOLMO_ERR_CORRUPT;
        x->allow_nplane = P->ver >= 9; x->allow_f2 = P->ver >= 11;
        int r = kg_decode(x, pay, cl, out, len); x->allow_nplane = 0; x->allow_f2 = 0; if (r) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
#ifdef KOLMO_FRONTIER
    if (pl.t == T_TBL && (tflag == 10 || tflag == 11)) {   /* experimental wide table / lag (the log plan's flag 10 is minified JSON) */
        if (P->ver < 6 || P->psum || pl.t != T_TBL || codec != C_ZSTD || cl < 4) return KOLMO_ERR_CORRUPT;
        size_t bytes = get32(pay);
        if (bytes > len + TSLACK || codec_decompress(x, codec, pay + 4, cl - 4, tbuf, bytes)) return KOLMO_ERR_CORRUPT;
        int error = tflag == 10 ? frontier_table_decode(tbuf, bytes, (uint8_t)pl.a, out, len)
                                : frontier_lag_decode(tbuf, bytes, out, len);
        if (error) return KOLMO_ERR_CORRUPT;
        return blk_sum(P->ver, out, len) == get32(e + 8) ? 0 : KOLMO_ERR_CHECKSUM;
    }
#endif
    if (tflag && pl.t == T_TP) meta = 4 * (size_t)(pl.c + 1);
    if ((tflag >= 3 && tflag <= 7) || tflag == 9) meta = 4;
    if (tflag == 10) meta = 8;
    if (tflag == 12) meta = 4;
    int maxf = pl.t == T_TP ? 2 : pl.t == T_TBL ? 4 : pl.t == T_LOG ? (P->ver >= 6 ? 10 : 5) : pl.t == T_IMG ? 6 : pl.t == T_REC ? 7 : pl.t == T_X86 ? 8 : pl.t == T_WORD ? 9 : pl.t == T_LINES ? 12 : 1;
    if (tflag > maxf || (pl.t == T_LINES && tflag && tflag != 12) || (pl.t == T_TBL && tflag && tflag < 3) || (pl.t == T_LOG && tflag && tflag != 5 && tflag != 10)
        || (pl.t == T_IMG && tflag && tflag != 6) || (pl.t == T_REC && tflag && tflag != 7) || (pl.t == T_X86 && tflag && tflag != 8) || (pl.t == T_WORD && tflag && tflag != 9)
        || (pl.t == T_NONE && tflag)) return KOLMO_ERR_CORRUPT;
    if (meta > cl) return KOLMO_ERR_CORRUPT;
    uint8_t *tgt = (tflag && tflag != 8) ? tbuf : out;
    size_t dlen = len;
    if ((tflag >= 3 && tflag <= 7) || tflag == 9 || tflag == 10 || tflag == 12) { dlen = get32(pay); if (dlen > len + TSLACK) return KOLMO_ERR_CORRUPT; }
#ifdef KOLMO_FRONTIER
    if (codec == C_PCM) {
        if (P->ver < 6 || tflag || pl.t != T_NONE || frontier_pcm_decode(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
    } else if (codec == C_DNA || codec == C_DNA_PREFIX || codec == C_DNA_RUNS) {
        if (P->ver < 6 || tflag || pl.t != T_NONE) return KOLMO_ERR_CORRUPT;
        if (codec != C_DNA) {
            if (cl < 4) return KOLMO_ERR_CORRUPT;
            size_t bytes = get32(pay);
            if (bytes > len) return KOLMO_ERR_CORRUPT;
            int error = codec == C_DNA_PREFIX ? frontier_prefix_decode(pay, cl, tbuf, bytes)
                                              : frontier_runs_decode(pay, cl, tbuf, bytes);
            if (error) return KOLMO_ERR_CORRUPT;
            pay = tbuf; cl = bytes;
        }
        if (frontier_dna_decode(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
    } else
#endif
    if (codec_decompress(x, codec, pay + meta, cl - meta, tgt, dlen)) return KOLMO_ERR_CORRUPT;
    if (tflag == 3) { if (tbl_decode(tbuf, dlen, (uint8_t)pl.a, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 4) { if (tbl2_decode(tbuf, dlen, (uint8_t)pl.a, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 5) { if (log_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 10) {                                    /* minified JSON: decode the split text, drop the newlines */
        size_t k = get32(pay + 4); if (k > len) return KOLMO_ERR_CORRUPT;
        uint8_t *sp = malloc(len + k + 64); if (!sp) return KOLMO_ERR_ALLOC;
        int bad = log_decode(tbuf, dlen, sp, len + k); size_t o = 0;
        for (size_t q = 0, e = len + k; !bad && q < e;) {     /* copy the runs between newlines */
            const uint8_t *nl = memchr(sp + q, '\n', e - q); size_t r = (nl ? (size_t)(nl - sp) : e) - q;
            if (r > len - o) { bad = 1; break; }
            memcpy(out + o, sp + q, r); o += r; q += r + (nl != NULL);
        }
        free(sp);
        if (bad || o != len) return KOLMO_ERR_CORRUPT;
    }
    else if (tflag == 12) { if (lines_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 6) { if (img16_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 7) { if (rec_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag == 8) e8e9_inv(out, len);
    else if (tflag == 9) { if (word_decode(tbuf, dlen, out, len)) return KOLMO_ERR_CORRUPT; }
    else if (tflag && pl.t == T_TP) {
        int k = pl.c + 1; int64_t colsz[256];
        if (cl < 4 * (size_t)k) return KOLMO_ERR_CORRUPT;
        for (int j = 0; j < k; j++) colsz[j] = get32(pay + 4 * j);
        if (tflag == 2) {
            int uniform = 1;
            if (k == 256) uniform = tpq_inv_cols(tbuf, len, (uint8_t)pl.a, k, colsz, out, NULL, NULL);
            if (uniform > 0) {
                uint32_t *pos = S->pos = grow(S->pos, &S->pcap, (J->maxraw + 1) * sizeof(uint32_t));
                if (!pos) return KOLMO_ERR_ALLOC;
                if (k != 256) {
                    uint32_t *rows = S->rowbuf = grow(S->rowbuf, &S->rcap, (J->maxraw + 8) * sizeof(uint32_t));
                    if (!rows) return KOLMO_ERR_ALLOC;
                    uniform = tpq_inv_cols(tbuf, len, (uint8_t)pl.a, k, colsz, out, pos, rows);
                }
                if (uniform > 0 && tpq_inv(tbuf, len, (uint8_t)pl.a, k, colsz, out, pos)) return KOLMO_ERR_CORRUPT;
            }
            if (uniform < 0) return KOLMO_ERR_CORRUPT;
        }
        else if (transpose_inv(tbuf, len, (uint8_t)pl.a, k, pl.b, colsz, out)) return KOLMO_ERR_CORRUPT;
    } else if (tflag && pl.t == T_SHUF) {
        size_t u = (size_t)pl.a * pl.b, rows = u ? len / u : 0;
        if (!u || u > 64 * 8 || (pl.a != 2 && pl.a != 4 && pl.a != 8) || pl.b < 1 || pl.b > 8) return KOLMO_ERR_CORRUPT;
        shuffle_inv(tbuf, rows, pl.a, pl.b, pl.c, out);
        memcpy(out + rows * u, tbuf + rows * u, len - rows * u);
    } else if (tflag) return KOLMO_ERR_CORRUPT;
    if (!P->psum && blk_sum(P->ver, out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
    return 0;
}

static void *decompress_worker(void *arg) {
    djob_t *J = arg; tls_t *S = tls_begin();
    if (!S) { atomic_store(&J->err, KOLMO_ERR_ALLOC); return NULL; }
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->maxraw + TSLACK + 64);
    int buffered = J->clip_hi || J->output_fd >= 0;
    uint8_t *rbuf = buffered ? (S->rbuf = grow(S->rbuf, &S->rbcap, J->maxraw + 64)) : NULL;
    uint8_t *packed = J->input_fd >= 0 ? malloc(J->maxcompressed ? J->maxcompressed : 1) : NULL;
    if ((J->input_fd >= 0 && !packed) || !tbuf || (buffered && !rbuf)) { free(packed); atomic_store(&J->err, KOLMO_ERR_ALLOC); tls_end(S); return NULL; }
    codec_ctx *xp = &S->x;
    xp->dict = J->dict; xp->dictlen = J->dictlen;                          /* per-call dictionary (read-only) */
    for (;;) {
        uint32_t i = J->lo + atomic_fetch_add(&J->next, 1); if (i >= J->hi) break;
        if (atomic_load(&J->err)) break;
        int r;
        const uint8_t *payload;
        if (packed) {
            size_t bytes = J->P->poff[i + 1] - J->P->poff[i];
            if (read_block(J->input_fd, packed, bytes, J->P->poff[i])) { atomic_store(&J->err, KOLMO_ERR_IO); break; }
            payload = packed;
        } else payload = J->src + J->P->poff[i];
        if (!rbuf) r = decode_block(J, i, xp, S, payload, tbuf, J->dst + J->P->roff[i]);
        else {                                               /* range read: decode block, copy the overlap */
            r = decode_block(J, i, xp, S, payload, tbuf, rbuf);
            if (!r && J->output_fd >= 0) {
                r = write_block(J->output_fd, rbuf, J->P->roff[i + 1] - J->P->roff[i], J->P->roff[i]);
            } else if (!r) {
                uint64_t b0 = J->P->roff[i], b1 = J->P->roff[i + 1];
                uint64_t a = J->clip_lo > b0 ? J->clip_lo : b0, z = J->clip_hi < b1 ? J->clip_hi : b1;
                memcpy(J->dst + (a - J->clip_lo), rbuf + (a - b0), z - a);
            }
        }
        if (r) atomic_store(&J->err, r);
    }
    free(packed);
    tls_end(S);
    return NULL;
}

static int64_t run_decode(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, int threads, uint64_t lo, uint64_t hi, int range,
                          const void *dict, size_t dictlen, uint32_t dictid, const kolmo_decode_limits *limits);
/* the single codec-20 block of a frame: the payload's plan bytes are 0, the rest is a Kolmo frame of the kvid layout */
static int64_t vid_decode_frame(const parsed_t *P, const uint8_t *src, int input_fd, uint8_t *dst, int output_fd, int threads,
                                uint64_t lo, uint64_t hi, int range) {
    const uint8_t *e = P->idx; size_t raw = get32(e), cl = (size_t)(P->poff[1] - P->poff[0]);
    if (P->ver < 11 || P->psum || e[13] || cl < 4 + HDR + IDX || raw != P->orig) return KOLMO_ERR_CORRUPT;
    uint8_t *pb = NULL, *t = NULL, *full = NULL; const uint8_t *pay; int64_t ret = KOLMO_ERR_CORRUPT;
    if (input_fd >= 0) {
        pb = malloc(cl); if (!pb) return KOLMO_ERR_ALLOC;
        if (read_block(input_fd, pb, cl, (off_t)P->poff[0])) { free(pb); return KOLMO_ERR_IO; }
        pay = pb;
    } else pay = src + P->poff[0];
    if (pay[0] != T_NONE || pay[1] || pay[2] || pay[3]) goto out;
    /* the nested frame is never itself a video frame (no recursion); the layout is the input plus 4 bytes and one
     * byte per strip, at most one per sample */
    if (get32(pay + 4 + 20) == 1 && pay[4 + HDR + 12] == C_VID) goto out;
    int64_t tn = kolmo_original_size(pay + 4, cl - 4);
    if (tn <= 0 || (uint64_t)tn > 2 * (uint64_t)raw + 4096) goto out;
    t = malloc((size_t)tn); full = !range && output_fd < 0 ? dst : malloc(raw ? raw : 1);
    if (!t || !full) { ret = KOLMO_ERR_ALLOC; goto out; }
    if (run_decode(pay + 4, -1, cl - 4, t, -1, threads, 0, 0, 0, NULL, 0, 0, NULL) != tn) goto out;
    if (kv_inverse(t, (size_t)tn, full, raw)) goto out;
    if (blk_sum(P->ver, full, raw) != get32(e + 8)) { ret = KOLMO_ERR_CHECKSUM; goto out; }
    if (range) { if (hi > raw || lo > hi) goto out; memcpy(dst, full + lo, (size_t)(hi - lo)); ret = (int64_t)(hi - lo); }
    else if (output_fd >= 0) { int w = write_block(output_fd, full, raw, 0); ret = w ? w : (int64_t)raw; }
    else ret = (int64_t)raw;
out:
    free(pb); free(t); if (full != dst) free(full); return ret;
}
static int64_t run_decode(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, int threads, uint64_t lo, uint64_t hi, int range,
                          const void *dict, size_t dictlen, uint32_t dictid, const kolmo_decode_limits *limits) {
    parsed_t P; int r = parse(src, n, &P, limits); if (r) return r;
    if (P.needdict && (!dict || dictid != P.dictid)) { free(P.poff); free(P.roff); return KOLMO_ERR_DICT; }
    if (P.nb == 1 && P.idx[12] == C_VID) {                      /* format 2: a video frame, decoded whole (codec 20) */
        int64_t vr = vid_decode_frame(&P, src, input_fd, dst, output_fd, threads, lo, hi, range);
        free(P.poff); free(P.roff); return vr;
    }
    djob_t J = {0}; J.src = src; J.P = &P; J.input_fd = input_fd; J.dst = dst; J.output_fd = output_fd; J.dict = dict; J.dictlen = dictlen;
    for (uint32_t i = 0; i < P.nb; i++) {
        size_t raw = P.roff[i + 1] - P.roff[i], stored = P.poff[i + 1] - P.poff[i];
        if (raw > J.maxraw) J.maxraw = raw;
        if (stored > J.maxcompressed) J.maxcompressed = stored;
    }
    if (range) {
        uint32_t a = 0, b = P.nb;
        while (a < P.nb && P.roff[a + 1] <= lo) a++;
        b = a; while (b < P.nb && P.roff[b] < hi) b++;
        J.lo = a; J.hi = b; J.clip_lo = lo; J.clip_hi = hi;
    } else { J.lo = 0; J.hi = P.nb; }
    atomic_init(&J.next, 0); atomic_init(&J.err, 0);
    int nt = threads > 0 ? threads : default_threads(), nblk = (int)(J.hi - J.lo < 256 ? J.hi - J.lo : 256);
    J.fqthreads = nblk > 0 && nt / nblk > 0 ? nt / nblk : 1;    /* threads left over per block (FASTQ family blocks) */
    run_pool(decompress_worker, &J, nt, J.hi - J.lo);
    free(P.poff); free(P.roff);
    r = atomic_load(&J.err);
    return r ? r : (int64_t)(range ? hi - lo : P.orig);
}
