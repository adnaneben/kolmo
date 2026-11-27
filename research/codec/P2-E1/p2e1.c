/* P2-E1: complete in-memory pipelines under the competitor-bounded policy (PROTOCOL.md). Research only.
 * Includes the frozen E02C harness unmodified (its main() is renamed) and production src/ through it.
 *
 * Candidate kinds (one input, one thread):
 *   K  production kolmo_compress / kolmo_decompress with the given mode/level/transforms (complete, real frames)
 *   Q  the same production frame decoded by the harness's sequential walk (like-for-like control for S's decode)
 *   S  E02G stacked pipeline on that frame as an EMULATED container: every KG_INT residual leaf replaced by the
 *      per-plane blob (planes+prod3g for speed modes, planes+prod19 for archive modes). Stored bytes are exact for
 *      leaf replacement. Encode is charged as production compress + frame walk + residual capture + per-plane
 *      re-encode (an upper bound for a realizable encoder). Decode: index/program walk + sequential decode.
 *   T  type-first pipeline: the declared element type is parsed from the input's own NumPy/WAV header inside the
 *      timed encode (no external descriptor), then production kg_encode_num at the mode's zstd level (3 speed,
 *      19 archive); if the graph is not applicable or not smaller, whole-input zstd at that level (the failed trial
 *      is charged). Undeclared/unsupported inputs are opaque and go straight to zstd. Container: 1 flag byte,
 *      uvarint length, 4-byte production block checksum of the original, payload; checksum verified on decode.
 *   U  T with the per-plane residual leaf (+guard at speed) as an EMULATED container (exact leaf-replacement
 *      bytes; encode charged as T + residual capture + per-plane encode; decode via the experimental leaf).
 *   Z  libzstd 1.5.7 (vendored), ZSTD_compress2, nbWorkers 0: level 3 is the designated speed rival; 19 informative.
 * Every decode is verified byte-exact outside the timed interval. Each timed sample is a mean over B repetitions
 * (B calibrated per input so a sample is >= ~20 ms); rounds rotate and shuffle the candidate order. */
#define main e02c_harness_main
#include "../E02C-harness/harness/e02c.c"
#undef main

typedef struct { const char *name; char kind; int mode, level, transforms; } cand_t;
static const cand_t CAND[] = {
    {"zstd3", 'Z', 0, 3, 0}, {"zstd19", 'Z', 0, 19, 0},
    {"kolmo_speed", 'K', KOLMO_RT, 0, 3}, {"kolmo_rt", 'K', KOLMO_RT, 0, 1},
    {"kolmo_fast", 'K', KOLMO_FAST, 0, 1}, {"kolmo_fast_q", 'K', KOLMO_FAST, 0, 2}, {"kolmo_fast_Q", 'K', KOLMO_FAST, 0, 3},
    {"kolmo_fast_n", 'K', KOLMO_FAST, 0, 0},
    {"kolmo_fast_seq", 'Q', KOLMO_FAST, 0, 1},
    {"stack_fast", 'S', KOLMO_FAST, 0, 1}, {"stack_fast_q", 'S', KOLMO_FAST, 0, 2}, {"stack_fast_Q", 'S', KOLMO_FAST, 0, 3},
    {"kolmo_eco", 'K', KOLMO_ECO, 0, 1}, {"kolmo_hc", 'K', KOLMO_HC, 0, 1}, {"kolmo_hc_seq", 'Q', KOLMO_HC, 0, 1},
    {"stack_hc", 'S', KOLMO_HC, 0, 1}, {"kolmo_max", 'K', KOLMO_MAX, 0, 1},
    {"typed_fast", 'T', KOLMO_FAST, 3, 0}, {"typed_stack_fast", 'U', KOLMO_FAST, 3, 0},
    {"typed_hc", 'T', KOLMO_HC, 19, 0}, {"typed_stack_hc", 'U', KOLMO_HC, 19, 0},
};
#define NC ((int)(sizeof CAND / sizeof CAND[0]))

static int conf_by_name(const char *want) { char nb[32]; for (int c = 0; c < NCONF; c++) if (!strcmp(conf_name(c, nb), want)) return c; DIE("no config %s", want); }
static kolmo_params cparams(const cand_t *c) { kolmo_params p = {(kolmo_mode)c->mode, c->level, 1, 0, c->transforms}; return p; }
static int speedmode(const cand_t *c) { return c->mode == KOLMO_RT || c->mode == KOLMO_FAST; }

/* decoder-side frame walk: parse the index and the INT program headers without decoding residual leaves */
static int frame_walk_dec(codec_ctx *x, uint8_t *frame, size_t fn, fparsed *F) {
    memset(F, 0, sizeof *F); F->frame = frame; F->fn = fn;
    if (parse(frame, fn, &F->P, NULL)) return -1;
    F->B = calloc(F->P.nb ? F->P.nb : 1, sizeof(binfo)); if (!F->B) return -1;
    for (uint32_t i = 0; i < F->P.nb; i++) {
        const uint8_t *e = F->P.idx + (size_t)i * IDX; binfo *b = &F->B[i];
        b->poff = F->P.poff[i]; b->cl = F->P.poff[i + 1] - F->P.poff[i]; b->roff = F->P.roff[i]; b->len = get32(e);
        if (b->len > F->maxraw) F->maxraw = b->len;
        const uint8_t *pay = frame + b->poff;
        if (F->P.ver >= 2 && b->cl >= 4 && e[12] == C_GRAPH && pay[0] == T_KG && pay[1] == 0 && !(pay[2] & 48)) {
            if (ig_parse(x, pay + 4, b->cl - 4, b->len, 0, &b->g)) return -1;
            b->isint = 1;
        }
    }
    return 0;
}

typedef struct { uint8_t *frame; size_t fn; kbuf *blobs; uint32_t nb; size_t stack_bytes; int nint; int tflag; uint32_t sum; } ref_t;

/* typing from the input's own declaration: 0 and (w, h) for little-endian 2/4/8-byte integer NumPy arrays and
 * 16-bit PCM WAV; -1 otherwise (opaque). Bounded, no allocation. */
static int type_of(const uint8_t *s, size_t n, int *w, size_t *h) {
    if (n >= 12 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? (size_t)s[8] | (size_t)s[9] << 8 : (size_t)get32(s + 8), o = s[6] == 1 ? 10 : 12;
        if (o + hl > n || hl > 65536) return -1;
        const char *d = memmem(s + o, hl, "'descr':", 8); if (!d) return -1;
        const char *q = memchr(d + 8, '\'', (size_t)((const char *)s + o + hl - (d + 8))); if (!q || q + 4 >= (const char *)s + o + hl) return -1;
        if ((q[1] != '<' && q[1] != '|') || (q[2] != 'i' && q[2] != 'u')) return -1;
        int ww = atoi(q + 3); if (ww != 2 && ww != 4 && ww != 8) return -1;
        if (!memmem(s + o, hl, "'fortran_order': False", 22)) return -1;
        *w = ww; *h = o + hl; return 0;
    }
    if (n >= 12 && !memcmp(s, "RIFF", 4) && !memcmp(s + 8, "WAVE", 4)) {
        size_t i = 12; int bits = 0, fmt = 0;
        while (i + 8 <= n) {
            uint32_t cl = get32(s + i + 4);
            if (!memcmp(s + i, "fmt ", 4) && cl >= 16 && i + 8 + 16 <= n) { fmt = s[i + 8] | s[i + 9] << 8; bits = s[i + 22] | s[i + 23] << 8; }
            if (!memcmp(s + i, "data", 4)) { if (fmt != 1 || bits != 16) return -1; *w = 2; *h = i + 8; return 0; }
            if ((uint64_t)cl + 8 + (cl & 1) > n - i) return -1; i += 8 + cl + (cl & 1);
        }
    }
    return -1;
}
static size_t thdr(size_t n) { return 1 + (size_t)uvlen(n) + 4; }
typedef struct { const uint8_t *src; size_t n; ref_t R[NC]; } ctx_t;

/* stacked leaves for a production frame: returns stored bytes, keeps blobs when keep != NULL */
static size_t stack_blobs(const cand_t *c, uint8_t *f, size_t fn, kbuf **keep, uint32_t *nbo, int *nint) {
    codec_ctx x = {0}; fparsed F; if (frame_walk(&x, f, fn, &F)) DIE("walk");
    int ck = conf_by_name(speedmode(c) ? "planes+prod3g" : "planes+prod19"); size_t out = fn; int ni = 0;
    kbuf *bl = keep ? calloc(F.P.nb ? F.P.nb : 1, sizeof(kbuf)) : NULL;
    for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) {
        intg *g = &F.B[i].g; kbuf b = {0};
        if (conf_encode(&x, ck, g->res.p, g->m, g->ow, g->pln, &b, NULL)) DIE("stack encode");
        out = out - g->rleaf_len + exp_leaf_bytes(g, b.n); ni++;
        if (bl) bl[i] = b; else free(b.p);
    }
    if (keep) { *keep = bl; *nbo = F.P.nb; }
    if (nint) *nint = ni;
    frame_free(&F); codec_ctx_free(&x); return out;
}
/* complete type-first encode; keeps the payload/blob in R when R != NULL */
static size_t typed_build(ctx_t *C, int ci, ref_t *R) {
    const cand_t *c = &CAND[ci]; int w; size_t h; uint32_t sum = blk_sum(2, C->src, C->n);
    size_t cap = C->n + C->n / 8 + 4096; uint8_t *pay = malloc(cap); size_t pn = 0, out; int flag = 0; codec_ctx x = {0}; kbuf b = {0};
    if (!type_of(C->src, C->n, &w, &h)) { pn = kg_encode_num(&x, C_ZSTD, c->level, C->src, C->n, w, h, pay, cap); flag = pn ? 1 : 2; }
    if (!pn) { size_t zc = ZSTD_compressBound(C->n); if (zc > cap) { pay = realloc(pay, zc); cap = zc; }
               pn = ZSTD_compress(pay, cap, C->src, C->n, c->level); if (ZSTD_isError(pn)) DIE("zstd"); }
    out = thdr(C->n) + pn;
    if (c->kind == 'U' && flag == 1) {
        intg g; if (ig_parse(&x, pay, pn, C->n, 1, &g)) DIE("typed capture");
        if (conf_encode(&x, conf_by_name(speedmode(c) ? "planes+prod3g" : "planes+prod19"), g.res.p, g.m, g.ow, g.pln, &b, NULL)) DIE("typed stack");
        out = out - g.rleaf_len + exp_leaf_bytes(&g, b.n); ig_free(&g);
    }
    if (R) { R->frame = pay; R->fn = pn; R->tflag = flag; R->sum = sum; R->stack_bytes = out; R->nint = flag == 1;
             if (b.p) { R->blobs = calloc(1, sizeof(kbuf)); R->blobs[0] = b; R->nb = 1; } }
    else { free(pay); free(b.p); }
    codec_ctx_free(&x); return out;
}
static int typed_dec(ctx_t *C, int ci, uint8_t *out) {
    ref_t *R = &C->R[ci]; codec_ctx x = {0}; int rc;
    if (R->tflag != 1) { size_t r = ZSTD_decompress(out, C->n + 64, R->frame, R->fn); rc = ZSTD_isError(r) || r != C->n; }
    else if (CAND[ci].kind == 'U') rc = ig_decode_exp(&x, R->frame, R->fn, R->blobs[0].p, R->blobs[0].n, out, C->n);
    else rc = kg_decode(&x, R->frame, R->fn, out, C->n);
    codec_ctx_free(&x);
    return rc || blk_sum(2, out, C->n) != R->sum;
}
static size_t enc(ctx_t *C, int ci) {
    const cand_t *c = &CAND[ci];
    if (c->kind == 'Z') {
        size_t cap = ZSTD_compressBound(C->n); uint8_t *d = malloc(cap); ZSTD_CCtx *z = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(z, ZSTD_c_compressionLevel, c->level); ZSTD_CCtx_setParameter(z, ZSTD_c_nbWorkers, 0);
        size_t r = ZSTD_compress2(z, d, cap, C->src, C->n); if (ZSTD_isError(r)) DIE("zstd"); ZSTD_freeCCtx(z); free(d); return r;
    }
    if (c->kind == 'T' || c->kind == 'U') return typed_build(C, ci, NULL);
    kolmo_params p = cparams(c); size_t cap = kolmo_bound(C->n, &p); uint8_t *f = malloc(cap);
    int64_t fn = kolmo_compress(C->src, C->n, f, cap, &p); if (fn <= 0) DIE("compress");
    size_t out = c->kind == 'S' ? stack_blobs(c, f, (size_t)fn, NULL, NULL, NULL) : (size_t)fn;
    free(f); return out;
}
static int dec(ctx_t *C, int ci, uint8_t *out) {
    const cand_t *c = &CAND[ci]; ref_t *R = &C->R[ci];
    if (c->kind == 'Z') { ZSTD_DCtx *z = ZSTD_createDCtx(); size_t r = ZSTD_decompressDCtx(z, out, C->n + 64, R->frame, R->fn); ZSTD_freeDCtx(z); return ZSTD_isError(r) || r != C->n; }
    if (c->kind == 'T' || c->kind == 'U') return typed_dec(C, ci, out);
    if (c->kind == 'K') return kolmo_decompress(R->frame, R->fn, out, C->n + 64, 1) != (int64_t)C->n;
    codec_ctx x = {0}; fparsed F; int rc = frame_walk_dec(&x, R->frame, R->fn, &F);
    if (!rc) rc = seq_decode(&F, c->kind == 'S' && R->nint ? R->blobs : NULL, out);
    frame_free(&F); codec_ctx_free(&x); return rc;
}
static size_t stored(ctx_t *C, int ci) { char k = CAND[ci].kind; return k == 'S' || k == 'T' || k == 'U' ? C->R[ci].stack_bytes : C->R[ci].fn; }
static void build_ref(ctx_t *C, int ci) {
    const cand_t *c = &CAND[ci]; ref_t *R = &C->R[ci];
    if (c->kind == 'Z') { size_t cap = ZSTD_compressBound(C->n); R->frame = malloc(cap); R->fn = ZSTD_compress(R->frame, cap, C->src, C->n, c->level); if (ZSTD_isError(R->fn)) DIE("zstd"); return; }
    if (c->kind == 'T' || c->kind == 'U') { typed_build(C, ci, R); return; }
    kolmo_params p = cparams(c); size_t cap = kolmo_bound(C->n, &p); R->frame = malloc(cap);
    int64_t fn = kolmo_compress(C->src, C->n, R->frame, cap, &p); if (fn <= 0) DIE("compress"); R->fn = (size_t)fn;
    if (c->kind == 'S') R->stack_bytes = stack_blobs(c, R->frame, R->fn, &R->blobs, &R->nb, &R->nint);
    else { int ni; stack_blobs(c, R->frame, R->fn, NULL, NULL, &ni); R->nint = ni; }
}
static void free_ref(ref_t *R) { if (R->blobs) for (uint32_t i = 0; i < R->nb; i++) free(R->blobs[i].p); free(R->blobs); free(R->frame); memset(R, 0, sizeof *R); }

int main(int argc, char **argv) {
    if (argc < 4) DIE("usage: p2e1 run FILE TAG ROUNDS SEED | p2e1 mem FILE CANDIDATE");
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("read");
    ctx_t C = {0}; C.src = src; C.n = n; uint8_t *out = malloc(n + 64);
    if (!strcmp(argv[1], "mem")) {   /* peak resident memory of one complete encode + verified decode (input buffer included) */
        int ci = -1; for (int c = 0; c < NC; c++) if (!strcmp(argv[3], CAND[c].name)) ci = c; if (ci < 0) DIE("candidate");
        build_ref(&C, ci); if (hwm_reset()) DIE("clear_refs");
        size_t s = enc(&C, ci); memset(out, 0, n); if (dec(&C, ci, out) || memcmp(out, src, n)) DIE("mem verify");
        printf("{\"kind\":\"p2e1_mem\",\"input\":\"%s\",\"candidate\":\"%s\",\"stored\":%zu,\"vmhwm_kb\":%ld}\n", argv[2], CAND[ci].name, s, status_kb("VmHWM:"));
        free_ref(&C.R[ci]); free(out); free(src); return 0;
    }
    const char *tag = argv[3]; int rounds = argc > 4 ? atoi(argv[4]) : 5; xs = (uint64_t)(argc > 5 ? atoll(argv[5]) : 1) * 2654435761ull + 7;
    for (int c = 0; c < NC; c++) {
        build_ref(&C, c);
        memset(out, 0, n); if (dec(&C, c, out) || memcmp(out, src, n)) DIE("%s does not reconstruct", CAND[c].name);
        if (enc(&C, c) != stored(&C, c)) DIE("%s stored bytes not deterministic", CAND[c].name);
        printf("{\"kind\":\"p2e1_size\",\"input\":\"%s\",\"bytes\":%zu,\"candidate\":\"%s\",\"stored\":%zu,\"int_blocks\":%d,\"typed\":%d,\"exact\":true}\n",
               tag, n, CAND[c].name, stored(&C, c), C.R[c].nint, C.R[c].tflag);
    }
    int Be[NC], Bd[NC], ord[NC];
    for (int c = 0; c < NC; c++) {
        uint64_t t0 = ns_of(CLOCK_MONOTONIC); enc(&C, c); uint64_t t1 = ns_of(CLOCK_MONOTONIC); dec(&C, c, out); uint64_t t2 = ns_of(CLOCK_MONOTONIC);
        uint64_t te = t1 - t0 ? t1 - t0 : 1, td = t2 - t1 ? t2 - t1 : 1;
        Be[c] = te >= 20000000 ? 1 : (int)(20000000 / te) + 1; Bd[c] = td >= 20000000 ? 1 : (int)(20000000 / td) + 1;
        if (Be[c] > 500) Be[c] = 500; if (Bd[c] > 500) Bd[c] = 500;
    }
    for (int r = 0; r < rounds; r++) {
        for (int c = 0; c < NC; c++) ord[c] = (c + r) % NC;
        shuffle(ord, NC);
        for (int oi = 0; oi < NC; oi++) {
            int c = ord[oi]; uint64_t w0, c0, w1, c1, w2, c2;
            w0 = ns_of(CLOCK_MONOTONIC); c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Be[c]; it++) if (enc(&C, c) != stored(&C, c)) DIE("encode drift");
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            double ew = (double)(w1 - w0) / Be[c], ec = (double)(c1 - c0) / Be[c];
            memset(out, 0, n);
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Bd[c]; it++) if (dec(&C, c, out)) DIE("decode");
            w2 = ns_of(CLOCK_MONOTONIC); c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            if (memcmp(out, src, n)) DIE("timed decode mismatch %s", CAND[c].name);
            printf("{\"kind\":\"p2e1_time\",\"input\":\"%s\",\"candidate\":\"%s\",\"pair_id\":\"r%d\",\"slot\":%d,\"enc_reps\":%d,\"dec_reps\":%d,"
                   "\"encode_wall_s\":%.9f,\"encode_cpu_s\":%.9f,\"decode_wall_s\":%.9f,\"decode_cpu_s\":%.9f}\n",
                   tag, CAND[c].name, r, oi, Be[c], Bd[c], ew * 1e-9, ec * 1e-9, (double)(w2 - w1) / Bd[c] * 1e-9, (double)(c2 - c1) / Bd[c] * 1e-9);
        }
    }
    for (int c = 0; c < NC; c++) free_ref(&C.R[c]);
    free(out); free(src); return 0;
}
