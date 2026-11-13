/* tfcli: research command-line tool for the P2-E2 type-first pipelines (research only; not the kolmo CLI).
 *   tfcli -p CANDIDATE [-T1] SRC DST     compress SRC with a P2-E2 candidate (e.g. tf_planes_huf, tf_prod19)
 *   tfcli -d [-T1] SRC DST                decompress (the candidate is recorded in the file)
 * File: "KTF1" magic, candidate index byte, then the exact P2-E2 research container produced by p2e2.c's enc()
 * (includes p2e2.c unmodified; this tool runs from a constructor and exits before p2e2's main). Never a production
 * Kolmo frame: the magic is not accepted by kolmo, and the format may change between research commits. One thread;
 * -T1 is accepted for command-line compatibility with matched harnesses and ignored. */
#include "p2e2.c"

static int write_all(const char *path, const uint8_t *a, size_t an, const uint8_t *b, size_t bn) {
    FILE *f = fopen(path, "wb"); if (!f) return -1;
    int bad = fwrite(a, 1, an, f) != an || (bn && fwrite(b, 1, bn, f) != bn);
    return fclose(f) || bad ? -1 : 0;
}
__attribute__((constructor)) static void tfcli_entry(int argc, char **argv, char **envp) {
    (void)envp; const char *cand = NULL; int decode = 0, i = 1;
    if (argc < 2 || (strcmp(argv[1], "-p") && strcmp(argv[1], "-d"))) return;     /* not a tfcli invocation */
    { uint16_t one = 1; if (*(uint8_t *)&one != 1) DIE("little-endian host required"); }
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-d")) decode = 1; else if (!strcmp(argv[i], "-p") && i + 1 < argc) cand = argv[++i];
        else if (strcmp(argv[i], "-T1")) DIE("tfcli: unknown option %s", argv[i]);
    }
    if (argc - i != 2) DIE("usage: tfcli -p CANDIDATE [-T1] SRC DST | tfcli -d [-T1] SRC DST");
    for (int c = 0; c < NC; c++) CONF[c] = CAND[c].leaf ? conf_by_name(CAND[c].leaf) : -1;
    size_t n; uint8_t *src = readfile(argv[i], &n); if (!src) DIE("tfcli: cannot read %s", argv[i]);
    if (!decode) {
        int ci = -1; for (int c = 0; c < NC; c++) if (cand && !strcmp(cand, CAND[c].name)) ci = c;
        if (ci < 0 || CAND[ci].kind == 'Z') DIE("tfcli: unknown or non-research candidate %s (use the zstd CLI for rivals)", cand ? cand : "(none)");
        if (!n) DIE("tfcli: empty input is not supported");
        uint8_t hdr[5] = {'K', 'T', 'F', '1', (uint8_t)ci}, *f; size_t fn = enc(src, n, ci, &f);
        if (write_all(argv[i + 1], hdr, 5, f, fn)) DIE("tfcli: write failed");
        free(f); free(src); exit(0);
    }
    if (n < 5 || memcmp(src, "KTF1", 4) || src[4] >= NC || CAND[src[4]].kind == 'Z') DIE("tfcli: not a KTF1 research file");
    int ci = src[4]; size_t i2 = 1; uint64_t len;
    if (n < 6 || kg_getuv(src + 5, n - 5, &i2, &len) || len > ((uint64_t)1 << 40)) DIE("tfcli: corrupt header");
    uint8_t *out = malloc((size_t)len + 64); if (!out) DIE("tfcli: allocation");
    if (dec(src + 5, n - 5, ci, out, (size_t)len)) DIE("tfcli: corrupt or mismatched data");
    if (write_all(argv[i + 1], out, (size_t)len, NULL, 0)) DIE("tfcli: write failed");
    free(out); free(src); exit(0);
}
