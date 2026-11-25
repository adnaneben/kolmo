/* The decoder: header and index parsing, block decoding on the pool. */
#include "features.h"         /* kx_find: container extensions after the last payload */
/* ------------------------------------------------------------------ decoding */
typedef struct { uint64_t orig; uint32_t nb; int ver, psum; plan_t pl; const uint8_t *idx; uint64_t *poff; uint64_t *roff;
                 uint64_t memd, end; uint32_t maxraw, maxcompressed; int mode; } parsed_t;   /* memd: the decode working set per block the header declares (0: none) */

/* the header's version byte: 0 for version 12, KOLMO_ERR_LEGACY for a frame of an earlier Kolmo (versions 1-11: library
 * codecs, XXH checksums, dictionaries and profiles), KOLMO_ERR_CORRUPT for anything else */
static int frame_version_check(const uint8_t *s, size_t n) {
    if (n >= 4 && !memcmp(s, "CZC1", 4)) return KOLMO_ERR_LEGACY;   /* compact profile frame (trained profiles, before version 12) */
    if (n < HDR || memcmp(s, "CZN1", 4)) return KOLMO_ERR_CORRUPT;
    if (s[4] >= 1 && s[4] < KOLMO_MAX_VERSION) return KOLMO_ERR_LEGACY;
    return s[4] == KOLMO_MAX_VERSION ? 0 : KOLMO_ERR_CORRUPT;
}
/* Scan the complete index without allocating. The payloads may be absent when only the frame sizes are needed. */
static int frame_scan(const uint8_t *s, size_t n, parsed_t *P) {
    int v = s ? frame_version_check(s, n) : KOLMO_ERR_CORRUPT;
    if (v) return v;
    if (s[10] & ~1) return KOLMO_ERR_CORRUPT;
    uint64_t o = get64(s + 12); uint32_t nb = get32(s + 20);
    uint64_t po = HDR + (uint64_t)nb * IDX, ro = 0;
    if (o > (uint64_t)INT64_MAX || po > n) return KOLMO_ERR_CORRUPT;
    uint32_t hc = kolmo_crc32c(0, s, 28); hc = kolmo_crc32c(hc, s + HDR, (size_t)nb * IDX);
    if (hc != get32(s + 28)) return KOLMO_ERR_CHECKSUM;
    uint32_t maxraw = 0, maxcompressed = 0;
    for (uint32_t i = 0; i < nb; i++) {
        const uint8_t *e = s + HDR + (size_t)i * IDX; uint32_t raw = get32(e), cl = get32(e + 4);
        if (raw > o - ro || (e[12] == C_STORE && (uint64_t)raw + 4 != cl) || cl > UINT64_MAX - po)
            return KOLMO_ERR_CORRUPT;
        if (raw > maxraw) maxraw = raw;
        if (cl > maxcompressed) maxcompressed = cl;
        ro += raw; po += cl;
    }
    if (ro != o) return KOLMO_ERR_CORRUPT;
    *P = (parsed_t){.orig = o, .nb = nb, .idx = s + HDR, .end = po, .maxraw = maxraw,
                    .maxcompressed = maxcompressed, .ver = s[4], .psum = s[10] & 1,
                    .memd = kmem_declared(s[11]), .mode = s[5], .pl = {s[6], s[7], s[8], s[9]}};
    return 0;
}
static int frame_head(const uint8_t *s, size_t n, uint64_t *orig, uint64_t *end) {
    parsed_t P; int r = frame_scan(s, n, &P);
    if (r) return r;
    *orig = P.orig; *end = P.end;
    return 0;
}
/* bytes after a frame's last payload: none, or container extensions ending in their trailer (core/extension.c) */
static int frame_tail_ok(const uint8_t *s, size_t n, uint64_t end) {
    if (end == n) return 1;
    if (end > n || n - end < 24 || memcmp(s + n - 24, "KEXT", 4)) return 0;
    uint64_t len; int err; kx_find(s, n, end, "\0\0\0\0", &len, &err);
    return !err;
}
/* The per-block estimate already includes 1 MiB for small allocations. Use up to 64 KiB of it
 * for frame metadata, and leave larger offset/index/order arrays out of the budget passed to the codecs. */
#define FRAME_METADATA_ALLOWANCE (64u << 10)
static uint64_t frame_codec_budget(uint64_t cap, uint64_t metadata) {
    if (cap == KMEM_UNLIMITED || metadata <= FRAME_METADATA_ALLOWANCE) return cap;
    uint64_t charge = metadata - FRAME_METADATA_ALLOWANCE;
    return charge >= cap ? 0 : cap - charge;
}
static uint64_t frame_offset_bytes(uint32_t blocks) { return 2 * ((uint64_t)blocks + 1) * sizeof(uint64_t); }

static int frame_limits(const parsed_t *P, const kolmo_decode_limits *limits) {
    if (limits && ((limits->max_output_bytes && P->orig > limits->max_output_bytes) ||
                   (limits->max_blocks && P->nb > limits->max_blocks) ||
                   (limits->max_block_bytes && P->maxraw > limits->max_block_bytes)))
        return KOLMO_ERR_LIMIT;
    return 0;
}

/* Rules 1 and 2 (docs/MEMORY-MODEL.md): compare the checked index to the frame's declared per-block
 * working set and the caller's remaining codec budget before allocating decoder state. */
static int frame_admit(const parsed_t *P, uint64_t maxraw, uint64_t memcap) {
    uint64_t floor = blk_dec_floor(maxraw);
    if (P->memd && floor > P->memd) return KOLMO_ERR_CORRUPT;
    if ((P->memd ? P->memd : floor) > memcap) return KOLMO_ERR_MEMORY;
    return 0;
}
/* Build the shared offsets only after checking the frame's declared limits and metadata budget. */
static int frame_prepare(parsed_t *P, const kolmo_decode_limits *limits, uint64_t codec_cap) {
    int r = frame_limits(P, limits);
    if (r) return r;
    r = frame_admit(P, P->maxraw, codec_cap);
    if (r) return r;
    if (frame_offset_bytes(P->nb) / 2 > SIZE_MAX) return KOLMO_ERR_ALLOC;
    P->poff = array_alloc((size_t)P->nb + 1, sizeof(uint64_t), 0);
    P->roff = array_alloc((size_t)P->nb + 1, sizeof(uint64_t), 0);
    if (!P->poff || !P->roff) { free(P->poff); free(P->roff); return KOLMO_ERR_ALLOC; }
    uint64_t po = HDR + (uint64_t)P->nb * IDX, ro = 0;
    for (uint32_t i = 0; i < P->nb; i++) {
        const uint8_t *e = P->idx + (size_t)i * IDX;
        P->poff[i] = po; P->roff[i] = ro;
        po += get32(e + 4); ro += get32(e);
    }
    P->poff[P->nb] = po; P->roff[P->nb] = ro;
    return 0;
}
typedef struct { const uint8_t *src; const parsed_t *P; uint8_t *dst; uint32_t lo, hi; atomic_uint next; atomic_int err;
                 int input_fd, output_fd, fqthreads; size_t maxraw, maxcompressed; uint64_t clip_lo, clip_hi;
                 uint64_t memcap; const uint32_t *order; } djob_t;   /* order: blocks lo..hi, longest first (review40 #7) */   /* memcap: the call's limit (the governor's cap; old frames' blocks are checked against it) */

/* Rule 1 (docs/MEMORY-MODEL.md): a block's working set, from its index entry and the head of its payload, checked
 * against what the frame declares before the block allocates anything. Returns the bytes to reserve, or an error. */
static int64_t blk_dec_admit(const parsed_t *P, uint32_t i, const uint8_t *pay, uint64_t memcap) {
    const uint8_t *e = P->idx + (size_t)i * IDX; size_t raw = get32(e), cl = (size_t)(P->poff[i + 1] - P->poff[i]);
    int codec = e[12], tflag = e[13];
    uint64_t need = codec == C_STORE ? blk_dec_floor(raw) : blk_dec_need(codec, tflag, raw, pay, cl, P->ver, 0);
    if (!need) return KOLMO_ERR_CORRUPT;                       /* a context-mixing header that no encoder writes */
    if (P->memd) {
        if (need > P->memd) return KOLMO_ERR_CORRUPT;          /* more than the frame declares: forged or damaged */
        if (codec == C_GRAPH && P->mode == KOLMO_ULTRA) {       /* leaves may be context mixing (ultra level 2) */
            uint64_t w = blk_dec_need(codec, tflag, raw, pay, cl, P->ver, 1); need = w < P->memd ? w : P->memd;
        }
    } else if (need > memcap) return KOLMO_ERR_MEMORY;           /* a frame that declares nothing (byte 11 zero) */
    return (int64_t)need;
}

/* fqt: the model jobs a FASTQ family block may run at once (reserved by the caller, fq_threads) */
static int decode_block(const djob_t *J, uint32_t i, codec_ctx *x, tls_t *S, const uint8_t *pay, uint8_t *tbuf, uint8_t *out, int fqt) {
    const parsed_t *P = J->P; const uint8_t *e = P->idx + (size_t)i * IDX;
    size_t len = get32(e), cl = get32(e + 4); int codec = e[12], tflag = e[13];
    size_t meta = 0;
    x->allow_nplane = 0; x->allow_f2 = 0;
    /* graph leaves only (FORMAT.md 8.1): no encoder writes them as a block's codec (rANS and khuf included) */
    if (codec == C_NPLANE || codec == C_PLANES || codec == C_ZRUN || codec == C_RANS || codec == C_KHUF) return KOLMO_ERR_CORRUPT;
    plan_t pl = P->pl;
    if (P->psum) {                                             /* rt: the checksum covers the stored bytes; checked first */
        if (cl < 4 || (uint32_t)kolmo_khash64(pay + 4, cl - 4, 0x637a6e31u ^ get32(pay)) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
    }
    {                                                          /* per-block plan */
        if (cl < 4) return KOLMO_ERR_CORRUPT;
        pl.t = pay[0]; pl.a = pay[1]; pl.b = pay[2]; pl.c = pay[3]; pay += 4; cl -= 4;
        if (pl.t > T_A64 || (pl.t == T_A64 && (pl.b > 15 || pl.c))) return KOLMO_ERR_CORRUPT;
        /* a stored block is its bytes: plan 0, 0, 0, 0 and no transform, as every encoder writes it (FORMAT.md 7.1) */
        if (codec == C_STORE && (pl.t || pl.a || pl.b || pl.c || tflag)) return KOLMO_ERR_CORRUPT;
    }
    x->threads = J->fqthreads;
    if (codec == C_FQ) {                                        /* FASTQ family block (version 10) */
        if (pl.t != T_NONE || pl.a || pl.b || pl.c || tflag) return KOLMO_ERR_CORRUPT;
        if (kf_fastq_decompress(pay, cl, out, len, fqt)) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (codec == C_VID) return KOLMO_ERR_CORRUPT;                /* video frames are decoded whole, by run_decode */
    if (codec == C_AUD) {                                       /* audio (kaud) block (version 11) */
        if (pl.t != T_NONE || pl.a > 1 || pl.b || pl.c || tflag) return KOLMO_ERR_CORRUPT;
        if (aud_decompress(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
        if (pl.a == 1) {                                        /* plan prefix 0, 1: big-endian samples (AIFF), swapped back */
            const uint64_t h = get32(pay + 37), fr = (uint64_t)get32(pay + 22) | (uint64_t)get32(pay + 26) << 32, ch = pay[6];
            if (h > len || ch < 1 || ch > 2 || fr > (len - h) / (2 * ch)) return KOLMO_ERR_CORRUPT;
            aud_swap16(out + h, (size_t)(fr * 2 * ch));
        }
        if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (codec == C_KIMG) {                                      /* kimg image block (version 11) */
        if (pl.t != T_NONE || pl.a || pl.b || pl.c || tflag) return KOLMO_ERR_CORRUPT;
        if (kimg_decompress(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (codec == C_GR6) {                                       /* GR6+ block (version 11) */
        if (pl.t != T_NONE || pl.a || pl.b || pl.c || tflag) return KOLMO_ERR_CORRUPT;
        int r = gr6_decompress(pay, cl, out, len); if (r) return r;
        if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (codec == C_GRAPH || pl.t == T_KG) {                     /* kolmo graph block (version 7) */
        if (codec != C_GRAPH || pl.t != T_KG || tflag) return KOLMO_ERR_CORRUPT;
        /* context-mixing leaves (planes and zero-run sub-streams included) are written by ultra level 2 only: in a frame
         * whose header says another mode they are refused, so the header's mode bounds the decode cost class */
        x->allow_nplane = 1; x->allow_f2 = 1; x->deny_cm = P->mode != KOLMO_ULTRA;
        int r = kg_decode(x, pay, cl, out, len); x->allow_nplane = 0; x->allow_f2 = 0; x->deny_cm = 0; if (r) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
#ifdef KOLMO_FRONTIER
    if (pl.t == T_TBL && (tflag == 10 || tflag == 11)) {   /* experimental wide table / lag (the log plan's flag 10 is minified JSON) */
        if (P->psum || pl.t != T_TBL || codec != C_KLZH2 || cl < 4) return KOLMO_ERR_CORRUPT;
        size_t bytes = get32(pay);
        if (bytes > len + TSLACK || codec_decompress(x, codec, pay + 4, cl - 4, tbuf, bytes)) return KOLMO_ERR_CORRUPT;
        int error = tflag == 10 ? frontier_table_decode(tbuf, bytes, (uint8_t)pl.a, out, len)
                                : frontier_lag_decode(tbuf, bytes, out, len);
        if (error) return KOLMO_ERR_CORRUPT;
        return blk_sum(out, len) == get32(e + 8) ? 0 : KOLMO_ERR_CHECKSUM;
    }
#endif
    if (pl.t == T_TBL && tflag == 13) {                         /* column-split table (transforms/table_cols.c) */
        int64_t tn = tblc_decompress(x, codec, pay, cl, tbuf, len + TSLACK);
        if (tn < 0 || tbl2_decode(tbuf, tn, (uint8_t)pl.a, out, len)) return KOLMO_ERR_CORRUPT;
        if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
        return 0;
    }
    if (tflag && pl.t == T_TP) meta = 4 * (size_t)(pl.c + 1);
    if ((tflag >= 3 && tflag <= 7) || tflag == 9) meta = 4;
    if (tflag == 10) meta = 8;
    if (tflag == 12) meta = 4;
    int maxf = pl.t == T_TP ? 2 : pl.t == T_TBL ? 4 : pl.t == T_LOG ? 10 : pl.t == T_IMG ? 6 : pl.t == T_REC ? 7 : pl.t == T_X86 ? 8 : pl.t == T_WORD ? 9 : pl.t == T_LINES ? 12 : pl.t == T_A64 ? 14 : 1;
    if (tflag > maxf || (pl.t == T_LINES && tflag && tflag != 12) || (pl.t == T_TBL && tflag && tflag < 3) || (pl.t == T_LOG && tflag && tflag != 5 && tflag != 10)
        || (pl.t == T_IMG && tflag && tflag != 6) || (pl.t == T_REC && tflag && tflag != 7) || (pl.t == T_X86 && tflag && tflag != 8) || (pl.t == T_A64 && tflag && tflag != 14) || (tflag == 14 && pl.t != T_A64) || (pl.t == T_WORD && tflag && tflag != 9)
        || (pl.t == T_NONE && tflag)) return KOLMO_ERR_CORRUPT;
    if (meta > cl) return KOLMO_ERR_CORRUPT;
    uint8_t *tgt = (tflag && tflag != 8 && tflag != 14) ? tbuf : out;
    size_t dlen = len;
    if ((tflag >= 3 && tflag <= 7) || tflag == 9 || tflag == 10 || tflag == 12) { dlen = get32(pay); if (dlen > len + (tflag == 4 ? TBL_SLACK(len) : TSLACK)) return KOLMO_ERR_CORRUPT; }
#ifdef KOLMO_FRONTIER
    if (codec == C_PCM) {
        if (tflag || pl.t != T_NONE || frontier_pcm_decode(pay, cl, out, len)) return KOLMO_ERR_CORRUPT;
    } else if (codec == C_DNA || codec == C_DNA_PREFIX || codec == C_DNA_RUNS) {
        if (tflag || pl.t != T_NONE) return KOLMO_ERR_CORRUPT;
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
    else if (tflag == 14) a64_inv(out, len, (uint32_t)(pl.a | (pl.b & 15) << 8));   /* ARM64 branches; a, b: the block's page phase */
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
    if (!P->psum && blk_sum(out, len) != get32(e + 8)) return KOLMO_ERR_CHECKSUM;
    return 0;
}

/* A FASTQ family block decodes its stream slices as model jobs on up to J->fqthreads threads, and each job holds a
 * model as large as the one job the frame declares (memneed.c: 4 raw + 32 MiB besides the buffers). The first job
 * is the block's own reservation; each further one is reserved with the governor only if it fits now, so the call
 * stays under its limit (-M) and runs one job at a time when memory is short. The output does not depend on it.
 * Returns the jobs granted; *extra jobs of *each bytes were reserved (kmem_release(*each) once per extra job). */
static int fq_threads(const djob_t *J, uint32_t i, int *extra, uint64_t *each) {
    *extra = 0; *each = 0;
    if (J->P->idx[(size_t)i * IDX + 12] != C_FQ || J->fqthreads <= 1) return J->fqthreads;
    uint64_t raw = get32(J->P->idx + (size_t)i * IDX); *each = 4 * raw + 32 * KMEM_MIB;
    while (1 + *extra < J->fqthreads && kmem_try_reserve(*each, J->memcap)) (*extra)++;
    return 1 + *extra;
}
static void *decompress_worker(void *arg) {
    djob_t *J = arg; tls_t *S = tls_begin();
    if (!S) { atomic_store(&J->err, KOLMO_ERR_ALLOC); return NULL; }
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->maxraw + TBL_SLACK(J->maxraw) + 64);
    int buffered = J->clip_hi || J->output_fd >= 0;
    /* a range read decodes a block that lies wholly inside the range straight into the caller's buffer; the block
     * buffer (rbuf) is taken only for a block cut by the range or written to a descriptor */
    uint8_t *rbuf = NULL;
    uint8_t *packed = J->input_fd >= 0 ? malloc(J->maxcompressed ? J->maxcompressed : 1) : NULL;
    if ((J->input_fd >= 0 && !packed) || !tbuf) { free(packed); atomic_store(&J->err, KOLMO_ERR_ALLOC); tls_end(S); return NULL; }
    codec_ctx *xp = &S->x;
    xp->threads = J->fqthreads;                                            /* threads left over per block (BWT) */
    /* a context-mixing model larger than the frame declares (or, undeclared, than the limit) is refused unallocated */
    kmem_tls_dec_limit = J->P->memd ? J->P->memd : J->memcap == KMEM_UNLIMITED ? 0 : J->memcap;
    for (;;) {
        uint32_t k = atomic_fetch_add(&J->next, 1); if (k >= J->hi - J->lo) break;
        uint32_t i = J->order ? J->order[k] : J->lo + k;   /* every block is written at its own offset: any order */
        if (atomic_load(&J->err)) break;
        int r;
        const uint8_t *payload;
        if (packed) {
            size_t bytes = J->P->poff[i + 1] - J->P->poff[i];
            if (read_block(J->input_fd, packed, bytes, J->P->poff[i])) { atomic_store(&J->err, KOLMO_ERR_IO); break; }
            payload = packed;
        } else payload = J->src + J->P->poff[i];
        /* the governor: this block's working set is reserved before it decodes (fewer blocks at once when memory is
         * short, one always); a figure above the frame's declaration is refused */
        int64_t need = blk_dec_admit(J->P, i, payload, J->memcap);
        if (need < 0) { atomic_store(&J->err, (int)need); break; }
        kmem_reserve((uint64_t)need, J->memcap); kmem_tls_job = 1;
        int fq_extra; uint64_t fq_each; int fqt = fq_threads(J, i, &fq_extra, &fq_each);
        const uint64_t b0 = J->P->roff[i], b1 = J->P->roff[i + 1];
        if (!buffered) r = decode_block(J, i, xp, S, payload, tbuf, J->dst + b0, fqt);
        else if (J->output_fd < 0 && J->clip_lo <= b0 && b1 <= J->clip_hi)   /* whole block inside the range */
            r = decode_block(J, i, xp, S, payload, tbuf, J->dst + (b0 - J->clip_lo), fqt);
        else if (!rbuf && !(rbuf = S->rbuf = grow(S->rbuf, &S->rbcap, J->maxraw + 64))) r = KOLMO_ERR_ALLOC;
        else {                                               /* range read: decode block, copy the overlap */
            r = decode_block(J, i, xp, S, payload, tbuf, rbuf, fqt);
            if (!r && J->output_fd >= 0) {
                r = write_block(J->output_fd, rbuf, J->P->roff[i + 1] - J->P->roff[i], J->P->roff[i]);
            } else if (!r) {
                uint64_t a = J->clip_lo > b0 ? J->clip_lo : b0, z = J->clip_hi < b1 ? J->clip_hi : b1;
                memcpy(J->dst + (a - J->clip_lo), rbuf + (a - b0), z - a);
            }
        }
        while (fq_extra--) kmem_release(fq_each);
        kmem_tls_job = 0; kmem_release((uint64_t)need);
        if (r) atomic_store(&J->err, r);
    }
    kmem_tls_dec_limit = 0;
    free(packed);
    tls_end(S);
    return NULL;
}

static int64_t run_decode(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, int threads, uint64_t lo, uint64_t hi, int range,
                          const kolmo_decode_limits *limits);
/* the single codec-20 block of a frame: the payload's plan bytes are 0, the rest is a Kolmo frame of the kvid layout */
static int64_t vid_decode_frame(const parsed_t *P, const uint8_t *src, int input_fd, uint8_t *dst, int output_fd, int threads,
                                uint64_t lo, uint64_t hi, int range, const kolmo_decode_limits *limits, uint64_t memcap) {
    const uint8_t *e = P->idx; size_t raw = get32(e), cl = (size_t)(P->poff[1] - P->poff[0]);
    if (P->psum || e[13] || cl < 4 + HDR + IDX || raw != P->orig) return KOLMO_ERR_CORRUPT;
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
    /* Memory (docs/MEMORY-MODEL.md): the layout (tn), the output and the inverse's copy (2 raw) are this frame's own
     * buffers, as the encoder counts them in the figure it declares (core/compress.c); the nested frame decodes under
     * what is left of the limit. The governor is not used here: the nested blocks reserve with it themselves. */
    uint64_t own = 2 * (uint64_t)raw + (uint64_t)tn;
    if (own > memcap) { ret = KOLMO_ERR_MEMORY; goto out; }
    t = malloc((size_t)tn); full = !range && output_fd < 0 ? dst : malloc(raw ? raw : 1);
    if (!t || !full) { ret = KOLMO_ERR_ALLOC; goto out; }
    {   /* the caller's limits apply inside (the size limit to the outer frame, already checked: tn is bounded by raw) */
        kolmo_decode_limits in = limits ? *limits : (kolmo_decode_limits){0}; in.max_output_bytes = 0;
        if (memcap != KMEM_UNLIMITED) in.max_memory = memcap - own > 0 ? memcap - own : 1;
        int64_t d = run_decode(pay + 4, -1, cl - 4, t, -1, threads, 0, 0, 0, &in);
        if (d != tn) { if (d == KOLMO_ERR_LIMIT || d == KOLMO_ERR_MEMORY || d == KOLMO_ERR_ALLOC) ret = d; goto out; }
    }
    if (kv_inverse(t, (size_t)tn, full, raw, threads > 0 ? threads : default_threads())) goto out;
    if (blk_sum(full, raw) != get32(e + 8)) { ret = KOLMO_ERR_CHECKSUM; goto out; }
    if (range) { if (hi > raw || lo > hi) goto out; memcpy(dst, full + lo, (size_t)(hi - lo)); ret = (int64_t)(hi - lo); }
    else if (output_fd >= 0) { int w = write_block(output_fd, full, raw, 0); ret = w ? w : (int64_t)raw; }
    else ret = (int64_t)raw;
out:
    free(pb); free(t); if (full != dst) free(full); return ret;
}
/* Execute a block span already identified against this frame's checked index. The owner holds the offsets
 * through the worker join, so a stream can use the same index for successive output windows. */
static int64_t run_decode_parsed(const parsed_t *P, const uint8_t *src, int input_fd, uint8_t *dst, int output_fd,
                                 int threads, uint32_t first, uint32_t last, uint64_t lo, uint64_t hi, int range,
                                 const kolmo_decode_limits *limits, uint64_t cap, uint64_t metadata) {
    uint64_t codec_cap = frame_codec_budget(cap, metadata);
    int r = frame_admit(P, P->maxraw, codec_cap);
    if (r) return r;
    if (P->nb == 1 && P->idx[12] == C_VID)                      /* format 2: video is decoded whole */
        return vid_decode_frame(P, src, input_fd, dst, output_fd, threads, lo, hi, range, limits, codec_cap);
    djob_t J = {0}; J.src = src; J.P = P; J.input_fd = input_fd; J.dst = dst; J.output_fd = output_fd;
    J.maxraw = P->maxraw; J.maxcompressed = P->maxcompressed; J.memcap = codec_cap;
    J.lo = first; J.hi = last; J.clip_lo = lo; J.clip_hi = hi;
    atomic_init(&J.next, 0); atomic_init(&J.err, 0);
    int nt = threads > 0 ? threads : default_threads(), nblk = (int)(J.hi - J.lo < 256 ? J.hi - J.lo : 256);
    J.fqthreads = nblk > 0 && nt / nblk > 0 ? nt / nblk : 1;    /* threads left over per block (FASTQ family blocks) */
    uint32_t *storage = NULL;
    size_t m = J.hi - J.lo;
    if (nt > 1 && m > 1) {
        uint64_t sort_cap = frame_codec_budget(cap, metadata + 2 * (uint64_t)m * sizeof *storage);
        /* Without a declaration, the index floor cannot prove that every codec still fits. */
        if ((P->memd || sort_cap == codec_cap) && !frame_admit(P, P->maxraw, sort_cap))
            storage = array_alloc(m, 2 * sizeof *storage, 0);
        if (storage) {
            J.memcap = sort_cap;
            /* Longest first. A stable merge sort keeps equal-length blocks in input order. */
            uint32_t *order = storage, *tmp = storage + m;
            for (size_t k = 0; k < m; k++) order[k] = J.lo + (uint32_t)k;
#define DLEN(b) (P->roff[(b) + 1] - P->roff[(b)])
            for (size_t w = 1; w < m;) {
                for (size_t a = 0; a < m;) {
                    size_t b = a + (w < m - a ? w : m - a), e = b + (w < m - b ? w : m - b);
                    size_t x = a, y = b;
                    for (size_t t = a; t < e; t++)
                        tmp[t] = y >= e || (x < b && DLEN(order[x]) >= DLEN(order[y])) ? order[x++] : order[y++];
                    a = e;
                }
                uint32_t *sw = order; order = tmp; tmp = sw;
                if (w > m / 2) break;
                w *= 2;
            }
#undef DLEN
            J.order = order;
        }
    }
    run_pool(decompress_worker, &J, nt, J.hi - J.lo);
    free(storage);
    r = atomic_load(&J.err);
    return r ? r : (int64_t)(range ? hi - lo : P->orig);
}
static int64_t run_decode(const uint8_t *src, int input_fd, size_t n, uint8_t *dst, int output_fd, int threads, uint64_t lo, uint64_t hi, int range,
                          const kolmo_decode_limits *limits) {
    parsed_t P;
    int r = frame_scan(src, n, &P);
    if (r) return r;
    if (P.end > n) return KOLMO_ERR_CORRUPT;
    uint64_t metadata = frame_offset_bytes(P.nb);
    if (input_fd >= 0) metadata += HDR + (uint64_t)P.nb * IDX;
    uint64_t cap = kmem_limit_of(limits ? limits->max_memory : 0);
    r = frame_prepare(&P, limits, frame_codec_budget(cap, metadata));
    if (r) return r;
    uint32_t first = 0, last = P.nb;
    if (range) {
        while (first < P.nb && P.roff[first + 1] <= lo) first++;
        last = first;
        while (last < P.nb && P.roff[last] < hi) last++;
    }
    int64_t result = run_decode_parsed(&P, src, input_fd, dst, output_fd, threads, first, last, lo, hi, range,
                                       limits, cap, metadata);
    free(P.poff); free(P.roff);
    return result;
}
