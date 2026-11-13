/* P2-E13: "try both leaves" on ONE shared type-first forward pass, written as genuine CZN1 frames (research encoder;
 * decoding is the unmodified production kolmo_decompress). Stacking question (STACKING.md SQ-*): P2-E12 showed
 * tp_prod3 (production zstd-3 leaves) wins on smooth Covertype columns while tp_phuf (per-plane Huffman) wins on
 * CollegeMsg ids; launching both encoders separately doubles typing, forward and process start, so it never fits.
 * Here typing and fi_forward run once; only the leaf coding is duplicated. All work is charged inside the timed encode.
 *
 *   tb_best    : 16 KiB pre-guard (as tp_phuf_pg16) -> fi_forward -> build prod and phuf payloads -> keep smaller
 *   tb_best_fb : tb_best, then also a whole-input zstd-3 production frame (tp_fallback) -> keep the smallest of three
 *   tb_lazy    : pre-guard -> fi_forward -> phuf payload; build prod payload only if phuf saves < 30% of the residual
 *                (cheap order-0 signal that a context/match coder might help) -> keep smaller
 *   tb_pick    : pre-guard -> fi_forward -> PREDICT the leaf, code only one: per-plane order-0 entropy of the whole
 *                residual (Huffman estimate) vs zstd-3 on a <= 16 KiB residual sample taken evenly from every plane,
 *                extrapolated; prod leaf if its estimate is < 0.97 x the Huffman estimate, else phuf
 * Includes P2-E11's p2e11.c unmodified (entry via a glibc constructor) for the frozen encoders, harness and identity checks.
 * Archive-family additions (after the first P2-E13 measurement; speed candidates unchanged):
 *   tp_prans   : no guard -> fi_forward -> residual leaf C_RANS = per-plane p13_rans_compress_all chunks (a chunk never
 *                spans planes; singleton planes allowed by the R1 normalisation); rest stored
 *   tp_pent    : no guard -> build phuf and prans leaves, keep the smaller (C_HUF or C_RANS leaf; one codec per leaf)
 *   tp_arch19  : tp_pent, plus the production zstd-19 leaves (kg_serialize at 19) and a whole-input fallback frame;
 *                smallest of four
 *   p2e13 run FILE TAG ROUNDS SEED      p2e13 mem FILE CANDIDATE
 *   p2e13 feat SRC                      (P2-E16: encode-time features from the shared type-first pass, JSON line, with
 *                                        their own extraction time; no frame written)
 *   p2e13 enc CANDIDATE SRC DST         (CLI encoder writing the frame; decode with the production `kolmo -d`;
 *                                        added after the P2-E13 measurement at bcd9795, encoders unchanged) */
#include "../P2-E11/p2e11.c"
#include "rans1.inc"   /* production rANS encoder + R1 singleton normalisation (gen_rans1.py); production decoder */   /* its main() is never reached: p2e13 runs from a constructor and exits (as tpcli) */

typedef struct { const char *name; int e11; int mode; } c13_t;   /* e11 >= 0: reuse that P2-E11 candidate */
static const c13_t C13[] = {
    {"zstd3", 0, 0}, {"kolmo_fast", 1, 0}, {"tp_prod3", 2, 0}, {"tp_phuf", 3, 0}, {"tp_phuf_pg16", 4, 0},
    {"tb_best", -1, 1}, {"tb_best_fb", -1, 2}, {"tb_lazy", -1, 3}, {"tb_pick", -1, 4},
    {"tp_prans", -1, 5}, {"tp_pent", -1, 6}, {"tp_arch19", -1, 7},
};
#define N13 ((int)(sizeof C13 / sizeof C13[0]))

/* plane-aligned C_RANS residual leaf, stored rest leaf: tp_payload_phuf's syntax with the codec byte C_RANS */
static size_t tp_payload_prans(fiprog *P, uint8_t *dst, size_t cap) {
    kbuf o = {0}; size_t out = 0, zc = P->res.n + P->res.n / 4 + 4096 * (size_t)(P->ow + 1);
    uint8_t *hb = malloc(zc); if (!hb) return 0;
    size_t z = 0; int ok = 1, planes = P->pln ? P->ow : 1; size_t plen = P->pln ? P->m : P->res.n;
    for (int b = 0; b < planes && ok; b++) {
        size_t r = plen ? p13_rans_compress_all(P->res.p + (size_t)b * plen, plen, hb + z, zc - z) : 0;
        if (plen && !r) ok = 0; else z += r;
    }
    if (!ok || !P->res.n || z >= P->res.n) goto done;               /* caller keeps another leaf */
    if (kb_byte(&o, 'G') || kb_byte(&o, 1) || kb_uv(&o, 1)
        || kb_byte(&o, KG_INT) || kb_uv(&o, 1) || kb_uv(&o, 0) || kb_uv(&o, 2) || kb_uv(&o, P->par.n) || kb_put(&o, P->par.p, P->par.n)
        || kb_uv(&o, 2)
        || kb_uv(&o, 1) || kb_byte(&o, C_RANS) || kb_uv(&o, P->res.n) || kb_uv(&o, z) || kb_put(&o, hb, z)
        || kb_uv(&o, 2) || kb_byte(&o, C_STORE) || kb_uv(&o, P->rest.n) || kb_uv(&o, P->rest.n) || kb_put(&o, P->rest.p, P->rest.n)) goto done;
    if (o.n <= cap) { memcpy(dst, o.p, o.n); out = o.n; }
done:
    free(o.p); free(hb); return out;
}
static size_t enc13(const uint8_t *src, size_t n, int ci, uint8_t **dst) {
    const c13_t *c = &C13[ci];
    if (c->e11 >= 0) return enc(src, n, c->e11, dst);
    kolmo_params kp = {KOLMO_FAST, 0, 1, 0, 0}; size_t cap = kolmo_bound(n, &kp) + n / 8 + 65536;
    uint8_t *f = malloc(cap), *pa = malloc(cap), *pb = malloc(cap); size_t fn = 0; int w; size_t h; codec_ctx x = {0};
    if (!f || !pa || !pb) DIE("oom");
    if (!type_of(src, n, &w, &h)) {
        fiprog P; gctx_t gc = {src, n, 3, 16384};
        if (c->mode >= 5 && !fi_forward(src, n, w, h, &P)) {             /* archive family: no pre-guard */
            uint8_t *pc = malloc(cap); size_t best = 0; const uint8_t *bp = NULL;
            size_t nr = tp_payload_prans(&P, pa, cap);
            if (nr) { best = nr; bp = pa; }
            if (c->mode >= 6) { size_t nh = tp_payload_phuf(&P, pb, cap); if (nh && (!best || nh < best)) { best = nh; bp = pb; } }
            if (c->mode == 7 && pc) { size_t nz = tp_payload_prod(&x, &P, n, 19, pc, cap); if (nz && (!best || nz < best)) { best = nz; bp = pc; } }
            if (best) fn = tp_wrap(src, n, w, h, bp, best, f);
            free(pc); fi_free(&P); goto coded;
        }
        if (c->mode < 5 && !fi_forward_g(src, n, w, h, &P, pre_guard, &gc)) {
            if (c->mode == 4) {
                int planes = P.pln ? P.ow : 1; size_t plen = P.pln ? P.m : P.res.n; double hbits = 0, zest = 0;
                size_t per = planes ? 16384 / (size_t)planes : 0; uint8_t *zb = malloc(ZSTD_compressBound(16384) + 64);
                for (int b = 0; b < planes; b++) {
                    const uint8_t *q = P.res.p + (size_t)b * plen; uint32_t hc[256] = {0};
                    for (size_t k = 0; k < plen; k++) hc[q[k]]++;
                    for (int v = 0; v < 256; v++) if (hc[v]) hbits -= (double)hc[v] * log2((double)hc[v] / (double)plen);
                    size_t sn = plen < per ? plen : per, off = (plen - sn) / 2;
                    if (sn) { size_t z = ZSTD_compress(zb, ZSTD_compressBound(16384) + 64, q + off, sn, 3); if (ZSTD_isError(z)) DIE("probe"); zest += (double)z * (double)plen / (double)sn; }
                }
                free(zb);
                size_t pn = zest < 0.97 * (hbits / 8.0 + 256.0 * planes) ? tp_payload_prod(&x, &P, n, 3, pa, cap) : tp_payload_phuf(&P, pa, cap);
                if (pn) fn = tp_wrap(src, n, w, h, pa, pn, f);
                fi_free(&P); goto coded;
            }
            size_t na = tp_payload_phuf(&P, pa, cap), nb = 0;
            int need_b = c->mode != 3 || !na || (double)na > 0.70 * (double)(P.res.n + P.rest.n);
            if (need_b) nb = tp_payload_prod(&x, &P, n, 3, pb, cap);
            const uint8_t *pay = pa; size_t pn = na;
            if (nb && (!na || nb < na)) { pay = pb; pn = nb; }
            if (pn) fn = tp_wrap(src, n, w, h, pay, pn, f);
            fi_free(&P);
        }
    }
coded:
    codec_ctx_free(&x);
    if (c->mode == 2 || c->mode == 7) {
        uint8_t *g = malloc(cap); size_t gn = g ? tp_fallback(src, n, g, cap) : 0;
        if (gn && (!fn || gn < fn)) { memcpy(f, g, gn); fn = gn; }
        free(g);
    }
    if (!fn) fn = tp_fallback(src, n, f, cap);
    if (!fn) DIE("tb encode");
    free(pa); free(pb); *dst = f; return fn;
}
static int dec13(const uint8_t *f, size_t fn, int ci, uint8_t *out, size_t n) {
    if (C13[ci].e11 >= 0) return dec(f, fn, C13[ci].e11, out, n);
    return kolmo_decompress(f, fn, out, n + 64, 1) != (int64_t)n;          /* unmodified production decoder */
}

static int p2e13_main(int argc, char **argv);
__attribute__((constructor)) static void p2e13_entry(int argc, char **argv, char **envp) { (void)envp; exit(p2e13_main(argc, argv)); }
static int p2e13_main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "feat")) {
        size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("p2e13 feat: cannot read %s", argv[2]);
        uint64_t t0 = ns_of(CLOCK_MONOTONIC); int w = 0; size_t h = 0; int typed = !type_of(src, n, &w, &h), fwd = -1;
        double ent[8] = {0}, zero = 0; fiprog P; memset(&P, 0, sizeof P); int planes = 0;
        if (typed && !(fwd = fi_forward(src, n, w, h, &P))) {
            planes = P.pln ? P.ow : 1; size_t plen = P.pln ? P.m : P.res.n;
            for (int b = 0; b < planes && b < 8; b++) {
                const uint8_t *q = P.res.p + (size_t)b * plen; uint32_t hc[256] = {0};
                for (size_t k = 0; k < plen; k++) hc[q[k]]++;
                for (int v = 0; v < 256; v++) if (hc[v]) ent[b] -= (double)hc[v] / (double)plen * log2((double)hc[v] / (double)plen);
                zero += plen ? (double)hc[0] / (double)plen / planes : 0;
            }
        }
        uint64_t t1 = ns_of(CLOCK_MONOTONIC);
        printf("{\"kind\":\"p2e16_feat\",\"input_bytes\":%zu,\"typed\":%d,\"forward_ok\":%d,\"w\":%d,\"order\":%d,\"signed\":%d,\"ow\":%d,\"planes\":%d,\"gcd\":%llu,\"res_bytes\":%zu,\"rest_bytes\":%zu,\"zero_frac\":%.6f,\"ent\":[",
               n, typed, fwd == 0, w, P.bo, P.bs, P.ow, planes, (unsigned long long)P.g, P.res.n, P.rest.n, zero);
        for (int b = 0; b < planes && b < 8; b++) printf("%s%.6f", b ? "," : "", ent[b]);
        printf("],\"extract_s\":%.9f}\n", (double)(t1 - t0) * 1e-9);
        if (fwd == 0) fi_free(&P); free(src); return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "enc")) {
        int ci = -1; for (int c = 0; c < N13; c++) if (!strcmp(argv[2], C13[c].name) && (C13[c].e11 < 0 || CAND[C13[c].e11].kind == 'P')) ci = c;
        if (ci < 0) DIE("p2e13 enc: unknown frame candidate %s", argv[2]);
        for (int c = 0; c < NC; c++) CONF[c] = -1;
        size_t n; uint8_t *src = readfile(argv[3], &n); if (!src || !n) DIE("p2e13 enc: cannot read %s", argv[3]);
        uint8_t *f; size_t fn = enc13(src, n, ci, &f); FILE *o = fopen(argv[4], "wb");
        if (!o || fwrite(f, 1, fn, o) != fn || fclose(o)) DIE("p2e13 enc: write failed");
        free(f); free(src); return 0;
    }
    if (argc < 4) DIE("usage: p2e13 run FILE TAG ROUNDS SEED | p2e13 mem FILE CANDIDATE");
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("read");
    uint8_t *out = malloc(n + 64);
    if (!strcmp(argv[1], "mem")) {
        int ci = -1; for (int c = 0; c < N13; c++) if (!strcmp(argv[3], C13[c].name)) ci = c; if (ci < 0) DIE("candidate");
        if (hwm_reset()) DIE("clear_refs");
        uint8_t *f; size_t fn = enc13(src, n, ci, &f); memset(out, 0, n); if (dec13(f, fn, ci, out, n) || memcmp(out, src, n)) DIE("mem verify");
        printf("{\"kind\":\"p2e13_mem\",\"input\":\"%s\",\"candidate\":\"%s\",\"stored\":%zu,\"vmhwm_kb\":%ld}\n", argv[2], C13[ci].name, fn, status_kb("VmHWM:"));
        free(f); free(out); free(src); return 0;
    }
    const char *tag = argv[3]; int rounds = argc > 4 ? atoi(argv[4]) : 5; xs = (uint64_t)(argc > 5 ? atoll(argv[5]) : 1) * 2654435761ull + 7;
    uint8_t *F[N13]; size_t FN[N13]; int Be[N13], Bd[N13], ord[N13];
    for (int c = 0; c < N13; c++) {
        FN[c] = enc13(src, n, c, &F[c]); uint8_t *f2; size_t f2n = enc13(src, n, c, &f2);
        if (f2n != FN[c] || memcmp(f2, F[c], FN[c])) DIE("%s not deterministic", C13[c].name); free(f2);
        memset(out, 0, n); if (dec13(F[c], FN[c], c, out, n) || memcmp(out, src, n)) DIE("%s does not reconstruct", C13[c].name);
        printf("{\"kind\":\"p2e13_size\",\"input\":\"%s\",\"bytes\":%zu,\"candidate\":\"%s\",\"stored\":%zu,\"exact\":true}\n", tag, n, C13[c].name, FN[c]);
        uint64_t t0 = ns_of(CLOCK_MONOTONIC); { uint8_t *t; enc13(src, n, c, &t); free(t); } uint64_t t1 = ns_of(CLOCK_MONOTONIC); dec13(F[c], FN[c], c, out, n); uint64_t t2 = ns_of(CLOCK_MONOTONIC);
        uint64_t te = t1 - t0 ? t1 - t0 : 1, td = t2 - t1 ? t2 - t1 : 1;
        Be[c] = te >= 20000000 ? 1 : (int)(20000000 / te) + 1; Bd[c] = td >= 20000000 ? 1 : (int)(20000000 / td) + 1;
        if (Be[c] > 500) Be[c] = 500; if (Bd[c] > 500) Bd[c] = 500;
    }
    for (int r = 0; r < rounds; r++) {
        for (int c = 0; c < N13; c++) ord[c] = (c + r) % N13;
        shuffle(ord, N13);
        for (int oi = 0; oi < N13; oi++) {
            int c = ord[oi]; uint64_t w0, c0, w1, c1, w2, c2;
            w0 = ns_of(CLOCK_MONOTONIC); c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Be[c]; it++) { uint8_t *t; if (enc13(src, n, c, &t) != FN[c]) DIE("encode drift"); free(t); }
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            double ew = (double)(w1 - w0) / Be[c], ec = (double)(c1 - c0) / Be[c];
            memset(out, 0, n);
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Bd[c]; it++) if (dec13(F[c], FN[c], c, out, n)) DIE("decode");
            w2 = ns_of(CLOCK_MONOTONIC); c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            if (memcmp(out, src, n)) DIE("timed decode mismatch %s", C13[c].name);
            printf("{\"kind\":\"p2e13_time\",\"input\":\"%s\",\"candidate\":\"%s\",\"pair_id\":\"r%d\",\"slot\":%d,\"enc_reps\":%d,\"dec_reps\":%d,"
                   "\"encode_wall_s\":%.9f,\"encode_cpu_s\":%.9f,\"decode_wall_s\":%.9f,\"decode_cpu_s\":%.9f}\n",
                   tag, C13[c].name, r, oi, Be[c], Bd[c], ew * 1e-9, ec * 1e-9, (double)(w2 - w1) / Bd[c] * 1e-9, (double)(c2 - c1) / Bd[c] * 1e-9);
        }
    }
    for (int c = 0; c < N13; c++) free(F[c]);
    free(out); free(src); return 0;
}
