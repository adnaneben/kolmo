/* P2-E2 malformed-input checks (research only). Includes p2e2.c unmodified (main renamed).
 *   fuzz container FILE CANDIDATE ITER SEED  mutate a valid research container (flips, truncation, length edits,
 *                                             splices); decoding must never crash, and any accepted output must be
 *                                             the original (the checksum makes acceptance of a wrong output unlikely;
 *                                             it is counted, not assumed)
 *   fuzz inverse ITER SEED                    random KG_INT parameters/streams: fi_inverse and production kg_int_inv
 *                                             must agree on accept/reject and, when accepted, on every output byte
 * Build with ASan/UBSan; any sanitizer report or disagreement exits non-zero. */
#include "p2e2.c"      /* its main() stays; the fuzzer runs from a glibc constructor (gets argc/argv) and exits */

static uint64_t fr_s = 1;
static uint64_t fr(void) { fr_s ^= fr_s << 13; fr_s ^= fr_s >> 7; fr_s ^= fr_s << 17; return fr_s; }

static int fuzz_container(const char *path, const char *cand, long iters) {
    for (int c = 0; c < NC; c++) CONF[c] = CAND[c].leaf ? conf_by_name(CAND[c].leaf) : -1;
    int ci = -1; for (int c = 0; c < NC; c++) if (!strcmp(cand, CAND[c].name)) ci = c; if (ci < 0) DIE("candidate");
    size_t n; uint8_t *src = readfile(path, &n); if (!src) DIE("read");
    uint8_t *f; size_t fn = enc(src, n, ci, &f), cap = fn + 64; uint8_t *m = malloc(cap), *out = malloc(n + 64);
    long rejected = 0, accepted_exact = 0, accepted_wrong = 0;
    for (long it = 0; it < iters; it++) {
        memcpy(m, f, fn); size_t mn = fn; int k = (int)(fr() % 5);
        if (k == 0) { int flips = 1 + (int)(fr() % 4); for (int q = 0; q < flips; q++) m[fr() % mn] ^= (uint8_t)(1 + fr() % 255); }
        else if (k == 1) mn = (size_t)(fr() % fn);                                   /* truncation */
        else if (k == 2) { size_t p = fr() % (mn < 48 ? mn : 48); m[p] = (uint8_t)fr(); }  /* header / length fields */
        else if (k == 3) { size_t a = fr() % mn, b = fr() % mn, l = 1 + fr() % 64; if (a + l <= mn && b + l <= mn) memmove(m + a, m + b, l); }
        else { size_t p = fr() % mn; m[p] = (uint8_t)(fr() & 1 ? 0xFF : 0x80); }        /* varint continuation */
        memset(out, 0, n);
        int rc = dec(m, mn, ci, out, n);
        if (rc) rejected++; else if (!memcmp(out, src, n)) accepted_exact++; else accepted_wrong++;
    }
    printf("{\"kind\":\"p2e2_fuzz_container\",\"input\":\"%s\",\"candidate\":\"%s\",\"iterations\":%ld,\"rejected\":%ld,\"accepted_exact\":%ld,\"accepted_wrong\":%ld}\n",
           path, cand, iters, rejected, accepted_exact, accepted_wrong);
    free(f); free(m); free(out); free(src);
    return accepted_wrong ? 3 : 0;
}

static int fuzz_inverse(long iters) {
    long agree_rej = 0, agree_acc = 0;
    for (long it = 0; it < iters; it++) {
        static const int W[] = {1, 2, 4, 8, 3}, OW[] = {1, 2, 4, 8, 5};
        int w = W[fr() % 5], ow = OW[fr() % 5], ord = (int)(fr() % 4), sg = (int)(fr() & 1), pln = (int)(fr() & 1);
        if (fr() % 4 == 0) pln = ow > 1;
        uint64_t cnt = fr() % 300, h = fr() % 8, g = fr() % 5 == 0 ? fr() % 1000 : 1, bz = fr() % 3 ? fr() % 70000 : fr();
        kbuf par = {0}; kb_byte(&par, (uint8_t)w); kb_byte(&par, (uint8_t)(sg | ord << 1 | pln << 3 | (fr() % 50 == 0 ? 16 : 0))); kb_byte(&par, (uint8_t)ow);
        kb_uv(&par, h); kb_uv(&par, cnt); kb_uv(&par, g); kb_uv(&par, bz);
        for (int q = 0; q < ord && q < 2; q++) kb_uv(&par, fr() % 3 ? fr() % 100000 : fr());
        if (fr() % 20 == 0 && par.n) par.n -= 1;                                    /* truncated parameters */
        if (fr() % 20 == 0) kb_byte(&par, 7);                                       /* trailing byte */
        size_t m = cnt >= (uint64_t)ord ? (size_t)(cnt - (uint64_t)ord) : 0, rn = m * (size_t)ow;
        if (fr() % 10 == 0) rn = (size_t)(fr() % 600);                              /* wrong residual length */
        kbuf res = {0}, rest = {0}; kb_res(&res, rn + 1); for (size_t q = 0; q < rn; q++) res.p[q] = (uint8_t)fr(); res.n = rn;
        size_t tn = (size_t)(fr() % 12); kb_res(&rest, tn + 1); for (size_t q = 0; q < tn; q++) rest.p[q] = (uint8_t)fr(); rest.n = tn;
        size_t maxs = fr() % 10 == 0 ? (size_t)(fr() % 1000) : 1u << 20;
        kbuf a = {0}, b = {0};
        int ra = kg_int_inv(par.p, par.n, &res, &rest, maxs, &a), rb = fi_inverse(par.p, par.n, &res, &rest, maxs, &b);
        if (!!ra != !!rb) DIE("accept/reject disagreement at iteration %ld (production %d, fast %d)", it, ra, rb);
        if (!ra) { if (a.n != b.n || memcmp(a.p, b.p, a.n)) DIE("output disagreement at iteration %ld", it); agree_acc++; } else agree_rej++;
        free(par.p); free(res.p); free(rest.p); free(a.p); free(b.p);
    }
    printf("{\"kind\":\"p2e2_fuzz_inverse\",\"iterations\":%ld,\"both_rejected\":%ld,\"both_accepted_identical\":%ld}\n", iters, agree_rej, agree_acc);
    return 0;
}

__attribute__((constructor)) static void fuzz_entry(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc >= 6 && !strcmp(argv[1], "container")) { fr_s = (uint64_t)atoll(argv[5]) * 2654435761ull + 1; exit(fuzz_container(argv[2], argv[3], atol(argv[4]))); }
    if (argc >= 4 && !strcmp(argv[1], "inverse")) { fr_s = (uint64_t)atoll(argv[3]) * 2654435761ull + 1; exit(fuzz_inverse(atol(argv[2]))); }
    DIE("usage: fuzz container FILE CANDIDATE ITER SEED | fuzz inverse ITER SEED");
}
