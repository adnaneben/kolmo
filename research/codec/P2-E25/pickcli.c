/* P2-E25 pickcli: research ENCODER for CSV archive mode. A near-free feature picks ONE pipeline per file:
 *   regularity = share of the (complete) lines in the first 64 KiB whose comma count equals the modal count
 *   (mode ties broken by first occurrence, as Python's statistics.mode), computed exactly as selector_csv.py.
 *   regularity > 0.9916  ->  production table program (kg_encode_table, ',' '.') with zstd-12 leaves, CZN1 frame
 *                            (= P2-E21 tt_zstd12); if the table encoder declines, production eco below
 *   otherwise            ->  production kolmo_compress with {KOLMO_ECO, 0, threads 1, 0, transforms 1} (= kolmo -m eco -T1)
 * Every output is a frame the unmodified production kolmo -d decodes. usage: pickcli pick SRC DST */
#include "../P2-E11/p2e11.c"
static double regularity(const uint8_t *s, size_t n) {
    if (n > 65536) n = 65536;
    uint32_t cnt[4096] = {0}, firstpos[4096]; size_t lines = 0; uint32_t c = 1; int big = 0;
    uint32_t *ks = malloc(sizeof(uint32_t) * (n + 1)); size_t nk = 0;
    for (size_t i = 0; i < n; i++) { if (s[i] == ',') c++; else if (s[i] == '\n') { ks[nk++] = c; c = 1; } }
    if (!nk) { free(ks); return 1.0; }                       /* one (partial) line only: Python's [s] case, one value */
    for (size_t i = 0; i < nk; i++) { uint32_t k = ks[i] < 4096 ? ks[i] : 4095; if (!cnt[k]) firstpos[k] = (uint32_t)i; cnt[k]++; if (ks[i] >= 4095) big = 1; }
    (void)big; uint32_t best = 0; for (uint32_t k = 1; k < 4096; k++) if (cnt[k] > cnt[best] || (cnt[k] == cnt[best] && cnt[k] && firstpos[k] < firstpos[best])) best = k;
    lines = nk; double r = (double)cnt[best] / (double)lines; free(ks); return r;
}
__attribute__((constructor)) static void pick(int argc, char **argv, char **envp) {
    (void)envp; if (argc < 4 || strcmp(argv[1], "pick")) return;
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("pickcli: read");
    kolmo_params kp = {KOLMO_ECO, 0, 1, 0, 1}; size_t cap = kolmo_bound(n, &kp) + n / 8 + 65536;
    uint8_t *f = malloc(cap), *tmp = malloc(cap); size_t fn = 0; codec_ctx x = {0};
    if (regularity(src, n) > 0.9916) {
        size_t pn = n >= 64 ? kg_encode_table(&x, C_ZSTD, 12, src, n, ',', '.', tmp, cap) : 0;
        if (pn) { fn = tp_wrap(src, n, 0, 0, tmp, pn, f); f[HDR + IDX + 1] = ','; f[HDR + IDX + 2] = '.';
                  uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, IDX); put32(f + 28, hc); }
    }
    if (!fn) { int64_t r = kolmo_compress(src, n, f, cap, &kp); if (r <= 0) DIE("pickcli: eco failed"); fn = (size_t)r; }
    FILE *o = fopen(argv[3], "wb"); if (!o || fwrite(f, 1, fn, o) != fn || fclose(o)) DIE("pickcli: write");
    codec_ctx_free(&x); free(tmp); free(f); free(src); exit(0);
}
