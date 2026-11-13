/* tpcli: research ENCODER writing genuine Kolmo CZN1 frames with the P2-E11 type-first pipelines.
 *   tpcli -p CANDIDATE [-T1] SRC DST     (tp_prod3, tp_phuf, tp_phuf_pg16)
 * Decoding is the unchanged production CLI: `kolmo -d [-T1] SRC DST`. Includes p2e11.c unmodified and runs from a
 * constructor (exits before p2e11's main). */
#include "p2e11.c"
static int wr(const char *path, const uint8_t *a, size_t an) { FILE *f = fopen(path, "wb"); if (!f) return -1; int bad = fwrite(a, 1, an, f) != an; return fclose(f) || bad ? -1 : 0; }
__attribute__((constructor)) static void tpcli_entry(int argc, char **argv, char **envp) {
    (void)envp; const char *cand = NULL; int i = 1;
    if (argc < 2 || strcmp(argv[1], "-p")) return;
    for (; i < argc && argv[i][0] == '-'; i++) { if (!strcmp(argv[i], "-p") && i + 1 < argc) cand = argv[++i]; else if (strcmp(argv[i], "-T1")) DIE("tpcli: unknown option %s", argv[i]); }
    if (argc - i != 2) DIE("usage: tpcli -p CANDIDATE [-T1] SRC DST");
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    int ci = -1; for (int c = 0; c < NC; c++) if (cand && !strcmp(cand, CAND[c].name) && CAND[c].kind == 'P') ci = c;
    if (ci < 0) DIE("tpcli: unknown candidate %s", cand ? cand : "(none)");
    size_t n; uint8_t *src = readfile(argv[i], &n); if (!src || !n) DIE("tpcli: cannot read %s", argv[i]);
    uint8_t *f; size_t fn = enc(src, n, ci, &f);
    if (wr(argv[i + 1], f, fn)) DIE("tpcli: write failed");
    free(f); free(src); exit(0);
}
