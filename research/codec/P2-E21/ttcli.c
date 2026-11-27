/* P2-E21 ttcli: research ENCODER writing a genuine CZN1 frame whose payload is the PRODUCTION table graph
 * (kg_encode_table, separator , decimal .) serialized with a chosen leaf codec; production decoder (kolmo -d).
 * usage: ttcli tt CODEC(zstd|bsc|brotli|lzma|cm|lz4hc) LEVEL SRC DST   (falls back to a production zstd-3 frame) */
#include "../P2-E11/p2e11.c"
__attribute__((constructor)) static void tt(int argc, char **argv, char **envp) {
    (void)envp; if (argc < 6 || strcmp(argv[1], "tt")) return;     /* tt CODEC LEVEL SRC DST : production table graph, given leaf codec */
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    const char *k = argv[2]; int lv = atoi(argv[3]), cc = C_ZSTD;
    if (!strcmp(k, "bsc")) cc = C_BSC; else if (!strcmp(k, "lzma")) cc = C_LZMA; else if (!strcmp(k, "brotli")) cc = C_BROTLI; else if (!strcmp(k, "cm")) cc = C_CM; else if (!strcmp(k, "lz4hc")) cc = C_LZ4HC;
    size_t n; uint8_t *src = readfile(argv[4], &n); kolmo_params kp = {KOLMO_FAST, 0, 1, 0, 0}; size_t cap = kolmo_bound(n, &kp) + n / 8 + 65536;
    uint8_t *f = malloc(cap); size_t fn = 0; codec_ctx x = {0};
    size_t pn = n >= 64 ? kg_encode_table(&x, cc, lv, src, n, ',', '.', f + HDR + IDX + 4, cap - HDR - IDX - 4) : 0;
    if (pn) { uint8_t *tmp = malloc(pn); memcpy(tmp, f + HDR + IDX + 4, pn); fn = tp_wrap(src, n, 0, 0, tmp, pn, f); f[HDR + IDX + 1] = ','; f[HDR + IDX + 2] = '.';
              uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, IDX); put32(f + 28, hc); free(tmp); }
    if (!fn) fn = tp_fallback(src, n, f, cap);
    if (!fn) DIE("ttcli: encode failed");
    FILE *o = fopen(argv[5], "wb"); if (!o || fwrite(f, 1, fn, o) != fn || fclose(o)) DIE("ttcli: write");
    codec_ctx_free(&x); free(f); free(src); exit(0);
}
