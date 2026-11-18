/* ---------------------------------------------------------------- working-set estimates (docs/MEMORY-MODEL.md)
 * Apache-2.0, Kolmo contributors. Part of the single translation unit built from src/kolmo.c (docs/LAYOUT.md); used by
 * core/trials.c, core/blocks.c, core/compress.c and core/decode.c.
 *
 * Two kinds of figures:
 *   - what decoding one block needs (blk_dec_need). The encoder computes it for every block from the payload it wrote
 *     and declares the largest in the frame header (byte 11, kmem_code); the decoder computes the same function from
 *     the same payload, refuses a block whose figure exceeds the declaration, and reserves it with the governor. It is
 *     exact for the context-mixing streams (their tables follow from the stream header) and a measured bound
 *     elsewhere (benchmark/rc3w2/memdyn/RESULTS.md).
 *   - what encoding one block or one trial needs (kmem_enc_*): estimates for the governor only. They decide how many
 *     jobs run at once, never the output.
 * Block-size and table-size choices under an explicit cap (kmem_block_for_cap) are format decisions: they read the
 * input size, the mode and the cap only. */
static uint64_t kmem_pow2_dict(uint64_t n) { uint64_t d = 4096; while (d < n && d < ((uint64_t)64 << 20)) d <<= 1; return d; }
static const uint64_t KMEM_MIB = (uint64_t)1 << 20;

/* encoding one codec call on n bytes at a level (the codec's own state; the block's buffers are counted apart) */
static uint64_t kmem_enc_codec(int codec, int level, uint64_t n) {
    switch (codec) {
        case C_STORE: return 0;
        case C_LZ4: return KMEM_MIB;
        case C_LZ4HC: return 2 * KMEM_MIB;
        case C_ZSTD: {                                          /* window, hash and chain tables of the level */
            uint64_t w = n < 2 * KMEM_MIB ? 2 * KMEM_MIB : n; if (w > 128 * KMEM_MIB) w = 128 * KMEM_MIB;
            return level >= 17 ? w * 6 + 8 * KMEM_MIB : level >= 10 ? w * 3 + 8 * KMEM_MIB : 12 * KMEM_MIB;
        }
        case C_BROTLI: { uint64_t w = n < (16 * KMEM_MIB) ? n : 16 * KMEM_MIB; return 8 * w + 8 * KMEM_MIB; }
        case C_LZMA: return 12 * kmem_pow2_dict(n) + 4 * KMEM_MIB;   /* preset 9 extreme: binary-tree match finder */
        case C_BWT: case C_BSC5: return 6 * n + 8 * KMEM_MIB;     /* suffix array, LZP, QLFC buffers */
        case C_CM: case C_CMI: return cm_need_bound((size_t)n) + n + 8;
        case C_FQ: return 5 * n + 32 * KMEM_MIB;
        case C_GR6: return 8 * n + 64 * KMEM_MIB;
        case C_AUD: return 4 * n + 16 * KMEM_MIB;
        case C_GRAPH: return 6 * n + 8 * KMEM_MIB;
    }
    return 2 * n + 4 * KMEM_MIB;
}

/* decoding one block: the payload p (cl bytes: 4 plan bytes, the meta bytes, the codec's stream) of a block of raw
 * bytes. cm_leaves: graph blocks may hold context-mixing leaves (frames of ultra level 2), whose tables are bounded
 * by the block. Returns 0 when a context-mixing header is not valid (the block is corrupt). */
static uint64_t blk_dec_need(int codec, int tflag, uint64_t raw, const uint8_t *p, size_t cl, int ver, int cm_leaves) {
    uint64_t base = kmem_buffers(raw), slack = raw + TBL_SLACK(raw);
    const uint8_t *s = p; size_t sn = cl;
    if (ver >= 2) { if (sn >= 4) { s += 4; sn -= 4; } else sn = 0; }   /* the plan prefix */
    size_t meta = 0;
    if ((tflag >= 3 && tflag <= 7) || tflag == 9 || tflag == 12) meta = 4;
    else if (tflag == 10) meta = 8;
    else if ((tflag == 1 || tflag == 2) && ver >= 2 && cl >= 4 && p[0] == T_TP) meta = 4 * ((size_t)p[3] + 1);   /* transpose: column sizes */
    if (meta > sn) meta = sn;
    const uint8_t *c = s + meta; size_t cn = sn - meta; uint64_t dn = tflag && tflag != 8 ? slack : raw;   /* the codec's output */
    uint64_t u;
    switch (codec) {
        case C_STORE: case C_LZ4: case C_LZ4HC: case C_HUF: case C_RANS: return base;
        case C_ZSTD: return base + 2 * KMEM_MIB;
        case C_BROTLI: return base + (dn < 16 * KMEM_MIB ? dn : 16 * KMEM_MIB) + 2 * KMEM_MIB;
        case C_LZMA: return base + kmem_pow2_dict(slack) + 2 * KMEM_MIB;
        case C_BWT: case C_BSC5: return base + 5 * slack + 4 * KMEM_MIB;
        case C_CM: u = cm_stream_need(c, cn, (size_t)dn); return u ? base + u : 0;
        case C_CMI: u = cm_image_need(c, cn); return u ? base + u : 0;
        case C_CMP: u = cmp_need(c, cn, (size_t)dn); return u ? base + u : 0;
        case C_FQ: return base + 4 * raw + 32 * KMEM_MIB;
        case C_GR6: return base + 6 * raw + 64 * KMEM_MIB;
        case C_AUD: return base + 3 * raw + 16 * KMEM_MIB;
        case C_GRAPH: return base + 6 * raw + 8 * KMEM_MIB + (cm_leaves ? cm_need_bound((size_t)(2 * raw + 65536)) : 0);
    }
    return base + 2 * raw + 4 * KMEM_MIB;
}
/* the part of blk_dec_need the index alone gives (no payload): the decoder's check before anything is read */
static uint64_t blk_dec_floor(uint64_t raw) { return kmem_buffers(raw); }

/* encoding one block of len bytes in a mode (codec and level of the mode, ultra's trials run in sequence on this
 * thread unless they reserve for themselves): the job's reservation */
static uint64_t kmem_enc_block(int codec, int level, uint64_t len, uint64_t slot) {
    uint64_t buf = 2 * (len + TBL_SLACK(len)) + 2 * slot + 4 * KMEM_MIB, w = kmem_enc_codec(codec, level, len);
    if (codec == C_LZMA) { uint64_t b = kmem_enc_codec(C_BWT, 9, len); if (b > w) w = b; }
    if (codec == C_CM && level != 1) {
        uint64_t a = kmem_enc_codec(C_LZMA, 9, len), b = kmem_enc_codec(C_BWT, 9, len);
        if (a > w) w = a;
        if (b > w) w = b;
    }
    return buf + w;
}
/* a trial started beside the block's coder (core/trials.c): its codec and its output buffer */
static uint64_t kmem_enc_trial(int codec, int level, uint64_t n, uint64_t cap) { return kmem_enc_codec(codec, level, n) + cap + KMEM_MIB; }

/* Rule 3: the least a mode needs; an explicit cap below it is refused (KOLMO_ERR_MEMORY). */
static uint64_t kmem_floor(int mode) {
    switch (mode) {
        case KOLMO_RT: case KOLMO_FAST: return 64 * KMEM_MIB;
        case KOLMO_ECO: case KOLMO_HC: return 256 * KMEM_MIB;
        case KOLMO_MAX: return 1024 * KMEM_MIB;
        default: return 256 * KMEM_MIB;                          /* ultra (-x), with tables sized to the cap */
    }
}
/* Under an explicit cap, the block size (a format decision: input size, mode and cap only). The default block of the
 * slow modes is halved until one block's encode estimate and decode figure fit the cap, down to 64 MiB for ratio and
 * 1 MiB for -x (whose tables then shrink to fit, cm_bits_for). Other modes keep their blocks (their floors cover them). */
static uint32_t kmem_block_for_cap(int mode, int codec, int level, uint32_t B, uint64_t cap) {
    if (!cap || cap == KMEM_UNLIMITED) return B;
    if (mode == KOLMO_MAX) {
        while (B > (64u << 20) && kmem_enc_block(codec, level, B, B + B / 8) > cap) B >>= 1;
    } else if (mode == KOLMO_ULTRA) {
        /* decode: buffers, the smallest tables and the match chain; encode: the same plus the alternative coders */
        while (B > (1u << 20) && (kmem_buffers(B) + cm_mem_for(16, 3) + 4 * ((uint64_t)B + 8) > cap
                                  || 2 * (B + TBL_SLACK(B)) + 2 * ((uint64_t)B + B / 8) + kmem_enc_codec(C_LZMA, 9, B) > cap)) B >>= 1;
    }
    return B;
}
