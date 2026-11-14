/* P2-E30: audio follow-up (development). Includes P2-E13 unmodified (constructor entry, as p2e13) + fastaudio.inc.
 * New frame candidates (genuine CZN1 frames, production decoder):
 *   ta_phuf  : fx_forward_g (fast order-1 pass) + 16 KiB pre-guard + per-plane Huffman   (= tp_phuf_pg16 bytes)
 *   ta_rep   : repetition score >= 0.03 -> production zstd-3 leaves on the residual (tp_prod3 shape), else ta_phuf
 *   ta_rep_fb: as ta_rep but a repetitive input goes to the whole-input production zstd-3 fallback frame instead
 * usage: p2e30 enc CAND SRC DST | p2e30 verify FILE... (fx_forward == fi_forward byte for byte) | p2e30 score FILE... */
#include "../P2-E13/p2e13.c"
#include "fastaudio.inc"
static size_t enc30(const uint8_t *src, size_t n, const char *cand, uint8_t **dst) {
    kolmo_params kp = {KOLMO_FAST, 0, 1, 0, 0}; size_t cap = kolmo_bound(n, &kp) + n / 8 + 65536;
    uint8_t *f = malloc(cap), *pa = malloc(cap); size_t fn = 0; int w; size_t h; codec_ctx x = {0};
    if (!f || !pa) DIE("oom");
    int rep = strcmp(cand, "ta_phuf") && fx_rep_score(src, n) >= 0.03;
    if (rep && !strcmp(cand, "ta_rep_fb")) { fn = tp_fallback(src, n, f, cap); }
    else if (!type_of(src, n, &w, &h)) {
        fiprog P; gctx_t gc = {src, n, 3, 16384};
        if (!fx_forward_g(src, n, w, h, &P, rep ? NULL : pre_guard, &gc)) {
            size_t pn = rep ? tp_payload_prod(&x, &P, n, 3, pa, cap) : tp_payload_phuf(&P, pa, cap);
            if (pn) fn = tp_wrap(src, n, w, h, pa, pn, f);
            fi_free(&P);
        }
    }
    codec_ctx_free(&x);
    if (!fn) fn = tp_fallback(src, n, f, cap);
    if (!fn) DIE("p2e30 encode");
    free(pa); *dst = f; return fn;
}
__attribute__((constructor(150))) static void p2e30_entry(int argc, char **argv, char **envp) {
    (void)envp; if (argc < 3) return;
    if (!strcmp(argv[1], "enc") && argc == 5 && !strncmp(argv[2], "ta_", 3)) {
        for (int c = 0; c < NC; c++) CONF[c] = -1;
        size_t n; uint8_t *src = readfile(argv[3], &n); if (!src || !n) DIE("p2e30: read");
        uint8_t *f; size_t fn = enc30(src, n, argv[2], &f); FILE *o = fopen(argv[4], "wb");
        if (!o || fwrite(f, 1, fn, o) != fn || fclose(o)) DIE("p2e30: write");
        free(f); free(src); exit(0);
    }
    if (!strcmp(argv[1], "verify")) {
        int ok = 0, used = 0;
        for (int i = 2; i < argc; i++) {
            size_t n; uint8_t *src = readfile(argv[i], &n); int w; size_t h; fiprog A, B;
            if (!src || type_of(src, n, &w, &h)) { free(src); continue; }
            int ra = fi_forward(src, n, w, h, &A), rb = fx_forward_g(src, n, w, h, &B, NULL, NULL);
            if (ra != rb) DIE("verify rc %s", argv[i]);
            if (!ra) { if (A.par.n != B.par.n || memcmp(A.par.p, B.par.p, A.par.n) || A.res.n != B.res.n || memcmp(A.res.p, B.res.p, A.res.n) || A.rest.n != B.rest.n || memcmp(A.rest.p, B.rest.p, A.rest.n)) DIE("verify MISMATCH %s", argv[i]);
                       used += (w == 2 && B.bs == 1 && (B.bo == 1 || B.bo == 2)); fi_free(&A); fi_free(&B); }
            ok++; free(src);
        }
        printf("verify ok %d files (%d on the fast order-1 path)\n", ok, used); exit(0);
    }
    if (!strcmp(argv[1], "score")) { for (int i = 2; i < argc; i++) { size_t n; uint8_t *s = readfile(argv[i], &n); printf("%.3f %s\n", fx_rep_score(s, n), argv[i]); free(s); } exit(0); }
}
