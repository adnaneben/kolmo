/* Block splitting, slot sizes, and the block encoder run by the pool (compress_worker). */
/* block boundaries: newline-aligned for transposed text, element-aligned for shuffled arrays */
static uint32_t make_blocks(const uint8_t *s, size_t n, uint32_t B, plan_t pl, uint64_t **starts_out,
                            int fd, uint64_t offset, int64_t *error) {
    size_t cap = n / (B / 2 + 1) + 2; uint64_t *st = array_alloc(cap, sizeof(uint64_t), 0); uint32_t nb = 0; size_t pos = 0;
    *starts_out = NULL;
    if (!st) return 0;
    if (pl.t == T_SHUF) { uint32_t u = pl.a * pl.b; B = B / u * u; if (!B) B = u; }
    if (pl.t == T_IMG) B &= ~1u;
    if (pl.t == T_A64) B = B >= 4096 ? B & ~4095u : 4;             /* ARM64 code: instruction words stay aligned in every block */
    if (pl.t == T_REC) { B = B / pl.rec->P * pl.rec->P; if (!B) B = pl.rec->P; }
    int kgnum = pl.t == T_KG && pl.a == 0;                     /* integer graph: width-aligned blocks after the head */
    if (kgnum) {
        uint32_t kw = pl.b & 96 ? pl.kgw : (uint32_t)(pl.b & 15);     /* IMG8, DEC64: whole rows; INT/FLOAT: whole words */
        B = B / kw * kw; if (!B) B = kw;
    }
    while (pos < n) {
        size_t end = B < n - pos ? pos + B : n;
        if (pl.t == T_REC && pos == 0) end = (size_t)pl.rec->h + B < n ? (size_t)pl.rec->h + B : n;
        if (kgnum && pos == 0) end = (size_t)pl.c + B < n ? (size_t)pl.c + B : n;
        if ((pl.t == T_TP || pl.t == T_TBL || pl.t == T_LOG || pl.t == T_LINES || (pl.t == T_KG && !kgnum) || pl.experimental_table || (pl.lines && !kgnum)) && end < n) {
            size_t lo = pos + B / 2, e = end;
            if (fd >= 0 && (pl.wide || pl.experimental_table)) {
                /* Avoid faulting a sparse page from the planning map at every
                 * boundary of a large file. Search the same bytes backwards. */
                uint8_t tail[4096];
                while (e > lo) {
                    size_t count = e - lo < sizeof(tail) ? e - lo : sizeof(tail);
                    if (read_block(fd, tail, count, offset + e - count)) {
                        free(st); *error = KOLMO_ERR_IO; return 0;
                    }
                    size_t at = count;
                    while (at && tail[at - 1] != '\n') at--;
                    e -= count - at;
                    if (at) break;
                }
            } else while (e > lo && s[e - 1] != '\n') e--;
            if (e > lo) end = e;
        }
        if (nb == UINT32_MAX - 1) { free(st); return 0; }
        if ((size_t)nb + 1 >= cap) {
            if (cap > SIZE_MAX / 2) { free(st); return 0; }
            uint64_t *grown = array_resize(st, cap * 2, sizeof(uint64_t));
            if (!grown) { free(st); return 0; }
            st = grown; cap *= 2;
        }
        st[nb++] = pos; pos = end;
    }
    if (n == 0) st[nb++] = 0;
    st[nb] = n;
    *starts_out = st; return nb;
}

static size_t slot_size(uint32_t B, int codec) { size_t a = codec_bound(codec, (size_t)B + TBL_SLACK(B)), b = codec_bound(C_BWT, (size_t)B + TBL_SLACK(B)); return (a > b ? a : b) + 256 * 4 + 64; }

/* Speed-tier (rt/fast) default block size: aim for ~8 balanced blocks so decode and
   compress parallelise across cores, with a 1 MiB floor (below it ratio drops faster
   than speed rises) and the 4 MiB ceiling (so large inputs are unchanged). A pure
   function of n, so output stays byte-identical across machines regardless of cores. */
static uint32_t kolmo_fast_block(size_t n) {
    uint64_t b = (uint64_t)n / 8;
    if (b < (1u << 20)) b = 1u << 20;
    if (b > (4u << 20)) b = 4u << 20;
    return (uint32_t)b;
}

/* ------------------------------------------------------------------ parallel driver */
typedef struct {
    const uint8_t *src; int input_fd; uint8_t *dst; const uint64_t *starts; uint32_t nb; size_t slot; uint8_t *idx;
    int codec, level, default_level, output_fd, native_leaf, fqthreads, columns; plan_t pl; atomic_uint next; atomic_int err; size_t B;
    const plan_t *plans; const uint32_t *bplan; const uint64_t *rstart;   /* per-block plan (container v2) */
    const uint64_t *poff;   /* decode: payload offsets */
    uint64_t rlo, rhi; uint8_t *rdst;   /* range read */
    uint8_t **out;          /* compressed block copies */
    size_t *outlen;         /* their sizes */
    pthread_mutex_t wmu; uint32_t wnext; size_t wpos, cap; int werr;   /* blocks are written to dst in order as they finish */
    int psum;               /* checksums over stored bytes (rt) instead of decoded bytes */
    int aud_prof, aud_ch, aud_be; uint64_t aud_off, aud_end;   /* aud_be: AIFF, big-endian samples (swapped for kaud; plan prefix 0, 1) */
    int img_on; uint32_t img_w, img_ch; uint64_t img_off, img_end;   /* kimg (codec 31): 8-bit image rows of img_w bytes, pixels [off, end) */
      /* format 2: 16-bit PCM WAV (kaud profile, channels, sample bytes [off, end)); aud_prof < 0: none */
    uint64_t memcap;        /* the call's limit for the governor (explicit, auto or unlimited) */
    uint64_t xcap;          /* the explicit cap only (0: none): the one memory figure format decisions may read */
    _Atomic uint64_t dneed; /* the largest block decode figure (blk_dec_need), declared in header byte 11 */
    int cm_leaves;          /* ultra level 2: graph leaves may be context mixing */
} job_t;
static void job_note_need(job_t *j, uint64_t v) {
    uint64_t cur = atomic_load_explicit(&j->dneed, memory_order_relaxed);
    while (v > cur && !atomic_compare_exchange_weak_explicit(&j->dneed, &cur, v, memory_order_relaxed, memory_order_relaxed)) {}
}
static void append_bytes(job_t *j, const uint8_t *bytes, size_t size) {
    if (j->werr) return;
    if (size > j->cap - j->wpos) { j->werr = KOLMO_ERR_DSTSIZE; return; }
    if (j->output_fd >= 0) j->werr = write_block(j->output_fd, bytes, size, j->wpos);
    else memcpy(j->dst + j->wpos, bytes, size);
    j->wpos += size;
}

static void drain_blocks(job_t *j) {
    while (j->wnext < j->nb && j->out[j->wnext]) {
        uint32_t next = j->wnext++;
        append_bytes(j, j->out[next], j->outlen[next]);
        free(j->out[next]); j->out[next] = NULL;
    }
}

/* Serialize the next payload. Copy out-of-order results outside the lock. */
static void emit_block(job_t *j, uint32_t i, const uint8_t *header, const uint8_t *body, size_t size) {
    pthread_mutex_lock(&j->wmu);
    if (i == j->wnext) {
        if (!header) append_bytes(j, body, size);
        else { append_bytes(j, header, 4); append_bytes(j, body, size - 4); }
        j->wnext++;
        drain_blocks(j);
        pthread_mutex_unlock(&j->wmu);
        return;
    }
    pthread_mutex_unlock(&j->wmu);
    uint8_t *saved = malloc(size);
    if (!saved) { atomic_store(&j->err, KOLMO_ERR_ALLOC); return; }
    if (!header) memcpy(saved, body, size);
    else { memcpy(saved, header, 4); memcpy(saved + 4, body, size - 4); }
    pthread_mutex_lock(&j->wmu);
    j->out[i] = saved; j->outlen[i] = size;
    drain_blocks(j);
    pthread_mutex_unlock(&j->wmu);
}


static void *compress_worker(void *arg) {
    job_t *J = arg; tls_t *S = tls_begin();
    if (!S) { atomic_store(&J->err, KOLMO_ERR_ALLOC); return NULL; }
    codec_ctx *xp = &S->x;
    xp->threads = J->fqthreads;                                            /* threads left over per block (BWT) */
    uint8_t *tbuf = S->tbuf = grow(S->tbuf, &S->tcap, J->B + TBL_SLACK(J->B) + 64);
    uint8_t *slot = S->slot = grow(S->slot, &S->scap, J->slot + 4);
    uint8_t *input = J->input_fd >= 0 ? malloc(J->B ? J->B : 1) : NULL;
    if (!tbuf || !slot || (J->input_fd >= 0 && !input)) { free(input); atomic_store(&J->err, KOLMO_ERR_ALLOC); tls_end(S); return NULL; }
    uint64_t held = 0;
    for (;;) {
#ifdef KOLMO_GPU
        uint32_t nx = atomic_load(&J->next);
        if (!kolmo_gpu_take(nx < J->nb ? J->nb - nx : 0)) break;          /* GPU worker: only while the CPU has a backlog */
#endif
        uint32_t i = atomic_fetch_add(&J->next, 1); if (i >= J->nb) break;
        const plan_t pl = J->plans[J->bplan[i]]; const uint64_t rs = J->rstart[i];
        size_t len = J->starts[i + 1] - J->starts[i];
        /* the governor: this block's working set is reserved before it starts (it waits when memory is short; one
         * block always runs). Changes when blocks run, never what they write. */
        const uint64_t resv = kmem_enc_block(J->codec, J->level, len, J->slot);
        kmem_reserve(resv, J->memcap); held = resv;
        kmem_tls_job = 1; kmem_tls_cap = J->xcap; kmem_tls_raw = len;
        const uint8_t *raw;
        if (input) {
            if (read_block(J->input_fd, input, len, J->starts[i])) { atomic_store(&J->err, KOLMO_ERR_IO); break; }
            raw = input;
        } else raw = J->src + J->starts[i];
        const uint8_t *in = raw; size_t meta = 0; int tflag = 0; unsigned modes = 0;
        /* a block of WAV samples is coded by the audio codec when that is smaller (it was on all 43 WAV files measured,
         * by 17% or more); the rest of the block's path keeps rc2's leaf rule, without the format-2 leaf trials */
        xp->no_f2 = J->aud_prof >= 0 && J->starts[i] < J->aud_end && J->starts[i + 1] > J->aud_off;
        xp->used_bwt = 0;
        uint8_t *sl = slot + 4;                                             /* slot: plan (4 bytes) | meta | codec data */
        slot[0] = (uint8_t)pl.t; slot[1] = (uint8_t)pl.a; slot[2] = (uint8_t)pl.b; slot[3] = (uint8_t)pl.c;
#ifdef KOLMO_FRONTIER
        size_t trial_len = 0;
        if (J->default_level) {
            if (pl.experimental_table)
                trial_len = frontier_table_encode(raw, len, pl.experimental_table, tbuf, len + TSLACK);
            else if (pl.lags.near)
                trial_len = frontier_lag_encode(raw, len, &pl.lags, tbuf, len + TSLACK);
        }
        if (trial_len) {
            put32(sl, (uint32_t)trial_len);
            slot[0] = T_TBL; slot[1] = (uint8_t)pl.experimental_table;
            slot[2] = slot[3] = 0; tflag = pl.experimental_table ? 10 : 11;
            meta = 4; in = tbuf; modes |= 4;
        } else
#endif
        if (pl.t == T_TP && len) {
            int k = pl.c + 1; int64_t colsz[256];
            int quoted = pl.b && (memchr(raw, '"', len) || memchr(raw, '[', len) || memchr(raw, ']', len));
            if (quoted) { transpose_fwd(raw, len, (uint8_t)pl.a, k, pl.b, tbuf, colsz); tflag = 1; }
            else if (pl.wide) { tpq_wide_fwd(raw, len, (uint8_t)pl.a, tbuf, colsz); tflag = 2; }
            else {
                uint32_t *pos = S->pos = grow(S->pos, &S->pcap, (J->B + 1) * sizeof(uint32_t));
                if (!pos) { atomic_store(&J->err, KOLMO_ERR_ALLOC); break; }
                tpq_fwd(raw, len, (uint8_t)pl.a, k, tbuf, colsz, pos); tflag = 2;
            }
            for (int j = 0; j < k; j++) put32(sl + 4 * j, (uint32_t)colsz[j]);
            meta = 4 * k; in = tbuf;
        } else if (pl.t == T_TBL && len) {
            int64_t tn = tbl2_encode(raw, len, (uint8_t)pl.a, pl.c + 1, pl.b, pl.fs, tbuf, &modes);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 4; if ((size_t)tn > len + TSLACK) modes |= 128; }
            else modes = 0;
        } else if (pl.t == T_LOG && len) {
            uint8_t *sp = pl.a == 2 && !memchr(raw, '\n', len) ? malloc(2 * len + 64) : NULL;
            if (sp) {                                   /* minified JSON: records split onto lines (block flag 10) */
                size_t sn = json_split(raw, len, sp); int64_t tn = log_encode_m(sp, sn, tbuf, len + TSLACK, 1);
                if (tn > 0) { put32(sl, (uint32_t)tn); put32(sl + 4, (uint32_t)(sn - len)); meta = 8; in = tbuf; tflag = 10; modes |= 4; }
                free(sp);
            } else {
                int64_t tn = log_encode_m(raw, len, tbuf, len + TSLACK, pl.a >= 1);
                if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 5; }
            }
        } else if (pl.t == T_LINES && len) {
            int64_t tn = lines_encode(raw, len, tbuf, len + TSLACK);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 12; modes |= 16; }
        } else if (pl.t == T_IMG && len) {
            int64_t tn = img16_encode(raw, len, rs, &pl.img, tbuf);
            if (tn > 0 && (size_t)tn <= len + TSLACK - 64) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 6; }
        } else if (pl.t == T_REC && len) {
            int64_t tn = rec_encode(raw, len, rs, pl.rec, tbuf, J->B + TSLACK);
            if (tn > 0 && (size_t)tn <= len + TSLACK - 64) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 7; }
        } else if (pl.t == T_SHUF && len) {
            size_t u = pl.a * pl.b, rows = len / u;
            shuffle_fwd(raw, rows, pl.a, pl.b, pl.c, tbuf);
            memcpy(tbuf + rows * u, raw + rows * u, len - rows * u);
            in = tbuf; tflag = 1;
        } else if (pl.t == T_WORD && len && !(J->codec == C_CM ||
                   (J->codec == R_LZMA && ((pl.wide && J->default_level ? 1 : J->level) == 1 || looks_text(raw, len))))) {
            /* (ratio/max and ultra drop the word transform of a text block below, coding the plain bytes with block
             * sorting or context mixing: it is not computed for them; it cost 10% of ratio's encode time on text) */
            int64_t tn = word_encode(raw, len, tbuf, len + TSLACK);
            if (tn > 0) { put32(sl, (uint32_t)tn); meta = 4; in = tbuf; tflag = 9; }
        } else if (pl.t == T_X86 && len) {
            memcpy(tbuf, raw, len); e8e9_fwd(tbuf, len); in = tbuf; tflag = 8;
        } else if (pl.t == T_A64 && len) {                                  /* ARM64 code: the block's own page phase in a, b */
            const uint32_t ph = (uint32_t)(pl.a | pl.b << 8) + (uint32_t)rs;
            slot[1] = (uint8_t)ph; slot[2] = (uint8_t)((ph >> 8) & 15); slot[3] = 0;
            memcpy(tbuf, raw, len); a64_fwd(tbuf, len, ph & 0xFFF); in = tbuf; tflag = 14; modes |= 128;
        }
        size_t inlen = (tflag >= 3 && tflag <= 7) || (tflag >= 9 && tflag != 14) ? get32(sl) : len, cl = 0;
        int codec = J->codec, level = (pl.wide || (tflag >= 10 && pl.t == T_TBL)) && J->default_level ? 1 : J->level;
        /* trials run beside context mixing (ultra, core/trials.c), and whether this block is FASTQ for the family coder */
        enum { XT_ALT, XT_ALTS, XT_A1, XT_FQ, XT_ALTB, XT_N }; xtrial_t xt[XT_N]; memset(xt, 0, sizeof xt);
        const int xfq = J->codec == C_CM && J->level != 1 && len >= 64 && kf_fastq_like(raw, len);
        if (pl.packed && meta_incompressible(raw, len)) { cl = 0; goto coded; }   /* compressed content: stored */
        if (xfq && len >= (64u << 10)) {
            /* ultra, a FASTQ block: the family coder first, with every thread the block has. It has beaten context
             * mixing on every FASTQ input measured (by 10 to 20%), so when it applies the context-mixing, LZMA2 and
             * block-sorting trials are not run (the frame is unchanged; encoding is about 20 times faster). Blocks under
             * 64 KiB still try both. */
            size_t fz = 0; uint8_t *fq = kf_fastq_compress(raw, len, J->fqthreads, kf_chunk_for(len), KF_BCHUNK, &fz);
            if (fq && fz < len && fz <= J->slot) {
                memcpy(sl, fq, fz); cl = fz; codec = C_FQ; tflag = 0; meta = 0; modes = 64; in = raw; inlen = len;
                slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; free(fq); goto coded;
            }
            free(fq);
        }
        if (pl.t == T_KG && len) {                                          /* kolmo graph: the block is a program + leaves */
            /* max, wide table: the column leaves are text, so they take BWT like any text block at max (LZMA2 on the
             * leaves was 3% smaller and ten times slower to encode). Float and decimal arrays too: on float planes
             * BWT is the same size and four to six times faster; on decimal planes LZMA2 is 10 to 12% smaller at 25
             * to 30 times the encode time, 18 KB saved per CPU second where the cost model breaks even at 54 (data kept
             * a year) or 652 (30 days). Integer arrays keep LZMA2 (2 to 9% smaller, small inputs). */
            /* max -l 10 keeps LZMA2 on decimal planes: the smallest output without context mixing, whatever it costs */
            int kgc = codec == R_LZMA && (pl.a || (!(pl.b & 32) && ((pl.b & 16) || ((pl.b & 64) && level < 10)))) ? C_BWT : codec, kgl = kgc != codec ? 9 : level;
            /* hc: zstd 19 over float planes gives the size of zstd 9 (the planes are coded by Huffman or rANS when that
             * is smaller) at up to 16 times the time. Decimal planes keep zstd 19: zstd 12 was 3 to 36% larger. */
            if (codec == R_LZH && level > 9 && !pl.a && !(pl.b & (32 | 64)) && (pl.b & 16)) kgl = 9;
            xp->native_leaf = J->native_leaf; xp->used_native_leaf = 0; xp->used_f2 = 0; xp->used_cmt = 0;
            cl = pl.a ? kg_encode_table(xp, kgc, kgl, raw, len, (uint8_t)pl.a, (uint8_t)pl.b, sl, J->slot)
                      : pl.b & 32 ? kg_encode_img8(xp, kgc, kgl, raw, len, pl.kgw, (unsigned)(pl.b & 15), rs == 0 ? (size_t)pl.c : 0, sl, J->slot)
                      : pl.b & 64 ? kg_encode_dec64(xp, kgc, kgl, raw, len, pl.b & 1 ? 0 : pl.b & 2 ? (pl.kgw / 4) | KG_DEC_F32 : pl.kgw / 8, rs == 0 ? (size_t)pl.c : 0, sl, J->slot)
                      : pl.b & 16 ? kg_encode_float(xp, kgc, kgl, raw, len, pl.b & 15, rs == 0 ? (size_t)pl.c : 0, sl, J->slot)
                      : kg_encode_num(xp, kgc, kgl, raw, len, pl.b, rs == 0 ? (size_t)pl.c : 0, sl, J->slot);
            if (cl && (pl.b & 32) && !pl.a && (codec == R_LZMA || codec == C_CM)) {
                /* 8-bit images under the slow coders: block-sorted leaves are 5 to 10% smaller than LZMA2 leaves on
                 * colour photographs (the residual planes are noise-like, with no matches to find). Both are coded. */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t c2 = alt ? kg_encode_img8(xp, C_BWT, 9, raw, len, pl.kgw, (unsigned)(pl.b & 15), rs == 0 ? (size_t)pl.c : 0, alt, J->slot) : 0;
                if (c2 && c2 < cl) { memcpy(sl, alt, c2); cl = c2; }
            }
            if (cl && (pl.b & 32) && !pl.a && codec == C_CM && level != 1 && len < ((size_t)1 << 28)) {
                /* ultra, 8-bit image: context mixing with the image model (the pixel's neighbours as contexts) over
                 * the plain pixels; 12 to 20% smaller than the predictor graph on photographs */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t c2 = alt ? cm_compress_img(raw, len, pl.kgw, (unsigned)(pl.b & 15), rs == 0 ? (uint32_t)pl.c : 0, alt, J->slot) : 0;
                if (c2 && c2 < cl) { memcpy(sl, alt, c2); cl = c2; codec = C_CMI; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; goto coded; }
            }
            if (cl && (len < (256u << 10) || xp->used_native_leaf)) {      /* small block (or a native leaf, scored on a slice): the block's
                                                                             * own coder on the plain bytes may still be smaller */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t pc = alt ? codec_compress(xp, codec, level, raw, len, alt, J->slot) : 0;
                if (pc && pc < cl) cl = 0;
            }
            if (cl && !pl.a && len >= (256u << 10) && (pl.verify || codec == C_CM || ((pl.b & 64) && (codec == R_LZMA || codec == C_BWT)))) {
                /* decimal doubles under the slow coders: the plan was scored with zstd, and on small-integer tables
                 * block sorting or context mixing over the plain bytes can still be smaller. Marked plans (the input
                 * is not one kind of data throughout): this block may be the part the graph does not fit. Both are
                 * coded; the smaller one is kept. */
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4); int bc = 0; size_t bz = cl;
                if (alt && codec != R_LZH && codec != R_SPEED) {
                    /* the slow coders: block sorting of a slice from the middle tells whether plain coding is within
                     * reach (a slice sorts worse than the block, so the limit is loose); otherwise the graph stands */
                    size_t sn = len < (512u << 10) ? len : (512u << 10), so = ((len - sn) / 2) & ~(size_t)63;
                    /* ultra: every numeric graph block comes here, and context mixing over the plain bytes costs 20
                     * times the graph's time. The slice is coded by the coder in question: a 256 KiB slice under
                     * context mixing (a slice codes worse than the block, so within 10% of the graph's rate is close
                     * enough). Block sorting was the probe before rc3w2 and missed the cases that matter: on 10 MB of
                     * float64 sensor data plain context mixing is 33% smaller than the graph, block sorting is not
                     * within reach (benchmark/rc3w2/twopass/RESULTS.md). */
                    if (codec == C_CM) sn = len < (256u << 10) ? len : (256u << 10), so = ((len - sn) / 2) & ~(size_t)63;
                    size_t cs = codec == C_CM ? cm_compress_auto(raw + so, sn, alt, J->slot) : codec_compress(xp, C_BWT, 9, raw + so, sn, alt, J->slot);
                    if (!cs || (double)cs / sn > (codec == C_CM ? 1.10 : 1.35) * (double)cl / len) alt = NULL;
                }
                if (alt && (codec == R_LZH || codec == R_SPEED) && len >= ((size_t)1 << 20)) {
                    /* the fast coders: three 64 KiB slices (start, middle, end) coded plain; the full plain pass costs
                     * as much as the graph, so it runs only when the slices come within 35% of the graph's rate */
                    const size_t sn = 64u << 10; size_t cs = 0;
                    for (int k = 0; k < 3; k++) {
                        size_t z = codec_compress(xp, codec, level, raw + (k == 0 ? 0 : k == 1 ? ((len - sn) / 2) & ~(size_t)63 : len - sn), sn, alt, J->slot);
                        cs += z && z < sn ? z : sn;
                    }
                    if ((double)cs / (double)(3 * sn) > 1.35 * (double)cl / (double)len) alt = NULL;
                }
                size_t a1 = alt ? codec_compress(xp, codec, level, raw, len, alt, J->slot) : 0;
                if (a1 && a1 < bz) { memcpy(sl, alt, a1); bz = a1; bc = codec; }
                size_t a2 = alt && codec == R_LZMA ? codec_compress(xp, C_BWT, 9, raw, len, alt, J->slot) : 0;
                if (a2 && a2 < bz) { memcpy(sl, alt, a2); bz = a2; bc = C_BWT; }
                if (bc) { cl = bz; codec = bc; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; goto coded; }
            }
            if (cl) { codec = C_GRAPH; tflag = 0; meta = 0; modes |= 8 | (xp->used_native_leaf ? 32 : 0) | (xp->used_f2 || xp->used_cmt ? 128 : 0); goto coded; }
            slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0;               /* did not apply: plain block */
        }
        if (codec == C_CM && tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; }                    /* ultra: CM models words itself */
        if (codec == R_LZMA && level == 1) { codec = C_BWT; if (tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; } }   /* max -l 1: BWT for every block */
        else if (codec == R_LZMA && (tflag == 10 || (tflag == 5 && pl.t == T_LOG && pl.a == 1))) codec = C_BWT;            /* max, JSON token streams (minified or one record per line): BWT won over LZMA2 by 10%; coding it first skips the LZMA2 pass */
        else if (codec == R_LZMA && (!tflag || tflag == 9) && looks_text(raw, len)) { codec = C_BWT; if (tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; } }   /* max: text to BWT */
        else if (KOLMO_ECO_TEXT_BWT && codec == R_ECO && J->default_level && len >= (64u << 10)
                 && (tflag == 10 || (tflag == 5 && pl.t == T_LOG && pl.a == 1))) codec = C_BWT, level = 9;   /* eco, JSON token streams: as max */
        else if (KOLMO_ECO_TEXT_BWT && codec == R_ECO && J->default_level && (!tflag || tflag == 9) && len >= (64u << 10)
                 && !pl.packed && looks_text8(raw, len)) {   /* eco, text: block sorting of the plain bytes (w4: smaller than brotli -11 on
                                               * prose and markup, at 10-20x its encode speed; decodes at block-sorting speed) */
            codec = C_BWT; level = 9; if (tflag == 9) { in = raw; inlen = len; tflag = 0; meta = 0; }
        }
#ifdef KOLMO_FRONTIER
#ifdef KOLMO_FRONTIER_PATCHED_PCM
        if (pl.experimental_pcm && J->default_level) {
            cl = frontier_pcm_encode(raw, len, pl.experimental_pcm, sl, J->slot);
            if (cl) {
                codec = C_PCM; tflag = 0; meta = 0; modes |= 4;
                memset(slot, 0, 4);
            }
        }
#endif
        if (pl.experimental_dna && J->default_level) {
            size_t packed = frontier_dna_encode(raw, len, tbuf, J->B + TSLACK);
            if (packed) {
                cl = frontier_runs_encode(tbuf, packed, sl, J->slot);
                if (cl) codec = C_DNA_RUNS;
                else {
                    cl = frontier_prefix_encode(tbuf, packed, sl, J->slot);
                    if (cl) codec = C_DNA_PREFIX;
                    else { memcpy(sl, tbuf, packed); cl = packed; codec = C_DNA; }
                }
                modes |= 4;
            }
        }
#endif
#ifdef KOLMO_FRONTIER_FAST_PCM
        if (pl.experimental_pcm && J->default_level && codec == R_LZH) level = 1;
#endif
        if (!cl && tflag == 4 && J->columns) {      /* KOLMO_T_COLUMNS: one coded segment per column (flag 13) */
            cl = tblc_compress(xp, codec, level, in, inlen, sl, J->slot);
            if (cl) { tflag = 13; meta = 0; goto coded; }
        }
        /* ultra with threads to spare: the alternative coders start now, beside context mixing (core/trials.c) */
        if (codec == C_CM && level != 1 && !cl && J->fqthreads > 1) {
            const int text = !tflag && looks_text(in, inlen), ak = text ? C_BWT : R_LZMA;
            xtrial_start(&xt[XT_ALT], ak, 9, in, inlen, J->slot - meta, 1);
            if (inlen > (2u << 20) && ak == R_LZMA) xtrial_start(&xt[XT_ALTS], ak, 9, in + (inlen - (1u << 20)) / 2, 1u << 20, J->slot - meta, 1);   /* only LZMA2 is gated by a slice */
            if (in != raw && len >= (64u << 10)) xtrial_start(&xt[XT_A1], C_BWT, 9, raw, len, J->slot, 1);
            if (xfq) xtrial_start(&xt[XT_FQ], C_FQ, 0, raw, len, 0, J->fqthreads - 1);
            if (!text) xtrial_start(&xt[XT_ALTB], C_BWT, 9, in, inlen, J->slot - meta, 1);   /* binary blocks: block sorting too */
        }
        int cmp_used = 0;
        if (!cl && codec == C_CM && pl.cmp && pl.t == T_TP && tflag && inlen == len) {
            /* ultra, a table chosen as columns: one context-mixing model per column, in parallel (codec 25) */
            if (pl.cmpz && rs == 0 && len == pl.cmpn && pl.cmpzn <= J->slot - meta) { memcpy(sl + meta, pl.cmpz, pl.cmpzn); cl = pl.cmpzn; }
            else {
                size_t parts[256]; int np = 0;
                for (int j = 0; j < pl.c + 1; j++) { size_t v = get32(sl + 4 * j); if (v) parts[np++] = v; }
                cl = cmp_compress_model(in, inlen, parts, np, sl + meta, J->slot - meta, J->fqthreads, level == 1 ? 1 : 2);
            }
            cmp_used = cl != 0;
        }
        if (!cl && codec == C_CM && level != 1 && inlen >= CMP_SPLIT_MIN && inlen < ((size_t)1 << 29) && !looks_text(in, inlen)) {
            /* ultra, a large single stream that is not text: two halves, each with its own model at half the table
             * size, coded and decoded in parallel (codec 25), 2x faster both ways. From 32 MiB: the measured cost was
             * 0.14% on 100 MB of binary data, but 0.14 to 2.3% for 5 MB halves (CSV, binary, Silesia osdb); text
             * costs 2.8 to 8.7% and stays whole (benchmark/rc3/xmode/RESULTS.md). */
            size_t parts[2] = {inlen / 2, inlen - inlen / 2};
            cl = cmp_compress_cap(in, inlen, parts, 2, sl + meta, J->slot - meta, J->fqthreads, 0, cm_bits_for(parts[1]) - 1);
            cmp_used = cl != 0;
        }
        int slice_verdict = 0;                        /* max, binary block over 8 MiB: 1 BWT clearly wins, -1 skip BWT */
        if (codec == R_LZMA && !cl && level != 1 && tflag != 8 && tflag != 14 && tflag != 4 && tflag != 5 && inlen > (8u << 20)) {
            /* A slice from the middle, coded both ways (BWT against LZMA2 at its fastest preset), predicts the outcome
             * (see below). Where the slice already shows BWT at most 90% of LZMA2, BWT won on the whole block by more
             * (osdb: slice 0.84, block 0.78) and the full LZMA2 pass, the slowest step of this mode, is skipped. */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            size_t sn = inlen / 8 < (512u << 10) ? inlen / 8 : (512u << 10), so = ((inlen - sn) / 2) & ~(size_t)63;
            size_t sb = alt ? codec_compress(xp, C_BWT, 9, in + so, sn, alt, J->slot - meta) : 0, slz = sb ? codec_compress(xp, R_LZMA, 1, in + so, sn, alt, J->slot - meta) : 0;
            if (!sb || !slz || (uint64_t)sb * 100 > (uint64_t)slz * 103) slice_verdict = -1;
            else if ((uint64_t)sb * 100 <= (uint64_t)slz * KOLMO_BWT_SKIP_LZMA_PCT && (uint64_t)slz * 100 <= (uint64_t)sn * 35) { codec = C_BWT; level = 9; slice_verdict = 1; }
        }
        if (!cl) cl = codec_compress(xp, codec, level, in, inlen, sl + meta, J->slot - meta);
        if (pl.verify && tflag == 1 && pl.t == T_SHUF && cl && len >= (64u << 10)) {   /* marked plan: this block may not suit the shuffle */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            size_t a1 = alt ? codec_compress(xp, codec, level, raw, len, alt, J->slot) : 0;
            if (a1 && a1 < cl) { memcpy(sl, alt, a1); cl = a1; in = raw; inlen = len; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; cmp_used = 0; }
        }
        if (codec == R_LZMA && cl) {                  /* max, binary block: BWT too, kept only when clearly smaller */
            /* BWT decodes about half as fast as LZMA2 here, so it must pay for that: at least KOLMO_BWT_GAIN_PCT smaller. */
            /* BWT is tried only where it has won in tests: not on x86 or ARM64 code (tflag 14 treated as 8, unmeasured), typed table columns or log token streams (4 to 12% larger), and only when LZMA2 already reaches
             * 35% or less (on less compressible binary data BWT was 1.5 to 8% larger) */
            uint8_t *alt = tflag == 8 || tflag == 14 || tflag == 4 || tflag == 5 || (uint64_t)cl * 100 > (uint64_t)inlen * 35 ? NULL : (S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4));
            if (slice_verdict < 0) alt = NULL;              /* the slice above already ruled BWT out */
            else if (alt && inlen > (8u << 20) && !slice_verdict) {   /* large blocks only: on small ones the slice costs as much as the attempt */
                /* a short slice from the middle, coded both ways (BWT against LZMA2 at its fastest preset), predicts the
                 * outcome: where BWT won on the whole block the slice ratio was at most 1.02, where it lost by 5% or
                 * more it was 1.04 or higher. Above 1.03 the full attempt is skipped. */
                size_t sn = inlen / 8 < (512u << 10) ? inlen / 8 : (512u << 10), so = ((inlen - sn) / 2) & ~(size_t)63;
                size_t sb = codec_compress(xp, C_BWT, 9, in + so, sn, alt, J->slot - meta), slz = sb ? codec_compress(xp, R_LZMA, 1, in + so, sn, alt, J->slot - meta) : 0;
                if (!sb || !slz || (uint64_t)sb * 100 > (uint64_t)slz * 103) alt = NULL;
            }
            size_t c2 = alt ? codec_compress(xp, C_BWT, 9, in, inlen, alt, J->slot - meta) : 0;
            if (c2 && (uint64_t)c2 * 100 <= (uint64_t)cl * (100 - KOLMO_BWT_GAIN_PCT)) { memcpy(sl + meta, alt, c2); cl = c2; codec = C_BWT; }
        }
        if ((codec == R_LZMA || codec == C_BWT) && J->codec == R_LZMA && cl && in != raw && len >= (64u << 10)) {
            /* max, transformed block: the transform was chosen by its zstd score, and under block sorting or LZMA2 the
             * plain bytes can be smaller (Silesia nci: template tokens 1.53 MB, plain block sorting 1.23 MB). Block
             * sorting of the plain block is tried when a slice from the middle comes within 10% of the transformed
             * block's ratio (25% for text transforms); blocks up to 2 MiB also try plain LZMA2. A plain result must be 3% smaller to be kept. */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            int try_bsc = alt != NULL, bc = 0; size_t bz = cl + meta;
            if (alt && len > (2u << 20)) {
                size_t sn = 512u << 10, so = ((len - sn) / 2) & ~(size_t)63, cs = codec_compress(xp, C_BWT, 9, raw + so, sn, alt, J->slot);
                /* a slice sorts worse than the whole block, most of all on repetitive text (nci: slice 5.2%, block 3.7%) */
                const double tol = tflag == 4 || tflag == 5 || tflag == 12 ? 1.25 : 1.10;
                if (!cs || (double)cs / sn > tol * (double)bz / len) try_bsc = 0;
            }
            size_t a1 = try_bsc ? codec_compress(xp, C_BWT, 9, raw, len, alt, J->slot) : 0;
            if (a1 && (uint64_t)a1 * 100 <= (uint64_t)bz * 97) { memcpy(sl, alt, a1); bz = a1; bc = C_BWT; }
            /* plain LZMA2 only where plain block sorting came within 25% of the transformed block */
            size_t a2 = alt && len <= (2u << 20) && a1 && (double)a1 <= 1.25 * (double)(cl + meta) ? codec_compress(xp, R_LZMA, level, raw, len, alt, J->slot) : 0;
            if (a2 && (uint64_t)a2 * 100 <= (uint64_t)(cl + meta) * 97 && a2 < bz) { memcpy(sl, alt, a2); bz = a2; bc = R_LZMA; }
            if (bc) { cl = bz; codec = bc; in = raw; inlen = len; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; }
        }
        if (codec == C_CM && level != 1) {            /* ultra: best of context mixing, LZMA2 and (text) BWT per block */
            uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
            if (alt) {
                int alts[2] = {R_LZMA, C_BWT};
                for (int k = 0; k < 2; k++) {
                    int text = !tflag && looks_text(in, inlen);
                    if (alts[k] == R_LZMA && text) continue;  /* text: BWT always beat LZMA2 in tests; saves ~25% of the time */
                    int hit;
                    /* large block: LZMA2 (1-2 MB/s, a tenth of CM's time) is coded in full only when a 1 MiB slice from
                     * the middle comes within 25% of CM's rate. Slices are pessimistic: on a table stream (owid-energy)
                     * the slice was 22% worse and the whole block 1% smaller than CM. Block sorting (10-25 MB/s, a few
                     * percent of CM's time) is always coded in full, on binary blocks too: on Silesia osdb its slice
                     * was 11% worse than CM and the whole block 1% smaller (benchmark/rc3/router/RESULTS.md).
                     * The 25% reach applies to transformed streams (tflag), where it was measured; raw blocks keep rc3's
                     * gate (the slice 3% below CM): on raw video residual planes LZMA2 came 13% short and the 25% reach
                     * coded it in full for nothing, +25% -x time at one thread (kvid -m x, akiyo: 83 -> 104 s). */
                    if (cl && inlen > (2u << 20) && alts[k] == R_LZMA) {
                        size_t sn = 1u << 20, so = (inlen - sn) / 2;
                        size_t cs = xtrial_take(&xt[XT_ALTS], alts[k], in + so, sn, &hit);
                        if (!hit) cs = codec_compress(xp, alts[k], 9, in + so, sn, alt, J->slot - meta);
                        if (!cs || (double)cs / sn > (tflag ? 1.25 : 0.97) * (double)cl / inlen) continue;   /* not within reach of CM */
                    }
                    size_t c2 = xtrial_take(&xt[alts[k] == C_BWT && !text ? XT_ALTB : XT_ALT], alts[k], in, inlen, &hit);
                    if (hit) { if (c2) memcpy(alt, xt[alts[k] == C_BWT && !text ? XT_ALTB : XT_ALT].out, c2); }
                    else c2 = codec_compress(xp, alts[k], 9, in, inlen, alt, J->slot - meta);
#if defined(KOLMO_TRACE) || defined(KOLMO_PROF)
                    fprintf(stderr, "len %zu t%d text %d cm %zu alt%d %zu\n", (size_t)inlen, tflag, looks_text(in, inlen), cl, alts[k], c2);
#endif
                    if (c2 && (!cl || c2 < cl)) { memcpy(sl + meta, alt, c2); cl = c2; codec = alts[k]; }
                }
                if (cl && in != raw && len >= (64u << 10)) {
                    /* ultra, transformed block: the transform was chosen by its zstd score, and the plain bytes can be
                     * smaller under the strong coders (a 1 MB image array was larger at ultra than at max). Plain block
                     * sorting is quick; when it comes within 2% of the transformed block, plain context mixing and,
                     * up to 2 MiB, plain LZMA2 are tried as well. The smallest is kept. */
                    int bc = 0, hit; size_t bz = cl + meta, ref = cl + meta;
                    size_t a1 = xtrial_take(&xt[XT_A1], C_BWT, raw, len, &hit);
                    if (hit) { if (a1) memcpy(alt, xt[XT_A1].out, a1); }
                    else a1 = codec_compress(xp, C_BWT, 9, raw, len, alt, J->slot);
                    if (a1 && a1 < bz) { memcpy(sl, alt, a1); bz = a1; bc = C_BWT; }
                    if (a1 && (double)a1 <= 1.02 * (double)ref) {
                        /* plain LZMA2 (up to 2 MiB) beside plain context mixing when a thread is free */
                        xtrial_t *x3 = &xt[XT_A1]; xtrial_free(x3);
                        if (len <= (2u << 20) && J->fqthreads > 1) xtrial_start(x3, R_LZMA, 9, raw, len, J->slot, 1);
                        size_t a2 = codec_compress(xp, C_CM, level, raw, len, alt, J->slot);
                        if (a2 && a2 < bz) { memcpy(sl, alt, a2); bz = a2; bc = C_CM; }
                        size_t a3 = xtrial_take(x3, R_LZMA, raw, len, &hit);
                        if (hit) { if (a3) memcpy(alt, x3->out, a3); }
                        else a3 = len <= (2u << 20) ? codec_compress(xp, R_LZMA, 9, raw, len, alt, J->slot) : 0;
                        if (a3 && a3 < bz) { memcpy(sl, alt, a3); bz = a3; bc = R_LZMA; }
                    }
                    if (bc) { cl = bz; codec = bc; in = raw; inlen = len; tflag = 0; meta = 0; slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; cmp_used = 0; }
                }
                /* GR6+ (codec 26: fields as decisions, column predictors and formulas, CM over 6-bit units) on the
                 * plain block, kept when smaller than the best so far (codecs/gr6.c; benchmark/rc3/gr6/RESULTS.md) */
                if (gr6_applies(raw, len)) {
                    size_t g = codec_compress(xp, C_GR6, level, raw, len, alt, J->slot);
                    if (g && (!cl || g < cl + meta)) {
                        memcpy(sl, alt, g); cl = g; codec = C_GR6; in = raw; inlen = len; tflag = 0; meta = 0; modes = 128;
                        slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0;
                    }
                }
            }
        }
        /* the payload is still codec 25's when no later trial replaced it (they clear cmp_used or change codec) */
        if (cmp_used && codec == C_CM) { codec = C_CMP; modes |= 128; }
coded:;
        if (xfq && codec != C_FQ) {
            /* ultra (not level 1), a block of complete FASTQ records: the family coder (codec 16: layout, names,
             * bases, base exceptions and qualities, each with its own model) when it is smaller than the block as
             * coded above. Anything that is not complete FASTQ fails the split and leaves the block as it is. */
            size_t fz = 0, cur = cl && cl + meta < len ? cl + meta : len; int hit;
            uint8_t *fq = NULL; fz = xtrial_take(&xt[XT_FQ], C_FQ, raw, len, &hit);
            if (hit) { fq = xt[XT_FQ].out; xt[XT_FQ].out = NULL; }
            else fq = kf_fastq_compress(raw, len, J->fqthreads, kf_chunk_for(len), KF_BCHUNK, &fz);
            if (fq && fz < cur && fz <= J->slot) {
                memcpy(sl, fq, fz); cl = fz; codec = C_FQ; tflag = 0; meta = 0; modes = 64;
                slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0;
            }
            free(fq);
        }
        if (J->aud_prof >= 0) {                                         /* format 2: audio block (codec 19) */
            const uint64_t fs = 2 * (uint64_t)J->aud_ch, s0 = J->starts[i], s1 = J->starts[i + 1];
            const uint64_t a = s0 > J->aud_off ? s0 : J->aud_off, b = s1 < J->aud_end ? s1 : J->aud_end;
            const uint64_t f0 = a < b ? (a - J->aud_off + fs - 1) / fs : 0, f1 = a < b ? (b - J->aud_off) / fs : 0;
            if (f1 > f0 + 1024) {
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t cur = cl && cl + meta < len ? cl + meta : len;
                const size_t ho = (size_t)(J->aud_off + f0 * fs - s0); const uint8_t *ain = raw; uint8_t *sw = NULL;
                if (J->aud_be && alt && (sw = malloc(len))) {          /* AIFF: the samples little-endian, as kaud reads them */
                    memcpy(sw, raw, len); aud_swap16(sw + ho, (size_t)(f1 - f0) * (size_t)fs); ain = sw;
                }
                size_t az = alt && (!J->aud_be || sw) ? aud_compress(J->aud_prof, J->aud_ch, ain, len, ho, f1 - f0, alt, J->slot) : 0;
                free(sw);
                if (az && az < cur) {
                    memcpy(sl, alt, az); cl = az; codec = C_AUD; tflag = 0; meta = 0; modes = 128;
                    slot[0] = T_NONE; slot[1] = (uint8_t)J->aud_be; slot[2] = slot[3] = 0;
                }
            }
        }
        if (J->img_on && J->default_level) {                         /* kimg: the block's image pixels (codec 31) */
            const uint64_t s0 = J->starts[i], s1 = J->starts[i + 1];
            const uint64_t p0 = s0 > J->img_off ? s0 : J->img_off, p1 = s1 < J->img_end ? s1 : J->img_end;
            if (p1 > p0 + (64u << 10)) {
                uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, J->slot + 4);
                size_t cur = cl && cl + meta < len ? cl + meta : len;
                size_t kz = alt ? kimg_compress(raw, len, J->img_w, J->img_ch, (uint32_t)((p0 - J->img_off) % J->img_w), (size_t)(p0 - s0),
                                                (size_t)(p1 - p0), alt, J->slot) : 0;
                if (kz && kz < cur) {
                    memcpy(sl, alt, kz); cl = kz; codec = C_KIMG; tflag = 0; meta = 0; modes = 128;
                    slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0;
                }
            }
        }
        (void)modes;   /* the format features this block used (once the frame's version); every frame is version 12 */
        for (int k = 0; k < XT_N; k++) xtrial_free(&xt[k]);
        size_t tot = 4 + meta + cl;
        uint8_t *e = J->idx + (size_t)i * IDX;
        if (cl == 0 || cl + meta >= len) {                                  /* store raw */
            codec = C_STORE; tflag = 0; tot = 4 + len;
            static const uint8_t h0[4] = {T_NONE, 0, 0, 0};
            emit_block(J, i, h0, raw, tot);
        } else {
            emit_block(J, i, NULL, slot, tot);
        }
        put32(e, (uint32_t)len); put32(e + 4, (uint32_t)tot);
        if (J->psum) put32(e + 8, codec == C_STORE ? (uint32_t)kolmo_khash64(raw, len, 0x637a6e31u ^ (uint32_t)T_NONE)
                                                   : (uint32_t)kolmo_khash64(slot + 4, tot - 4, 0x637a6e31u ^ get32(slot)));
        else put32(e + 8, blk_sum(raw, len));
        e[12] = codec_wire(xp, codec); e[13] = (uint8_t)tflag; e[14] = e[15] = 0;
        /* what decoding this block will need, from the payload as written (the frame declares the largest) */
        job_note_need(J, codec == C_STORE ? blk_dec_floor(len) : blk_dec_need(codec_wire(xp, codec), tflag, len, slot, tot, KOLMO_MAX_VERSION, J->cm_leaves));   /* the codec as written (native ids) */
        kmem_tls_job = 0; kmem_tls_cap = 0; kmem_tls_raw = 0;
        kmem_release(resv); held = 0;
    }
    if (held) { kmem_tls_job = 0; kmem_tls_cap = 0; kmem_tls_raw = 0; kmem_release(held); }   /* left the loop on an error */
    free(input);
    tls_end(S);
    return NULL;
}
