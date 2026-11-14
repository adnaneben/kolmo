#include "../P2-E11/p2e11.c"
__attribute__((constructor)) static void gx(int argc, char **argv, char **envp) {
    (void)envp; if (argc < 2 || strcmp(argv[1], "gx")) return;
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    size_t n; uint8_t *src = readfile(argv[2], &n); int w; size_t h; if (type_of(src, n, &w, &h) || w != 4) DIE("type");
    size_t cnt = (n - h) / 4; int32_t mn = INT32_MAX; for (size_t i = 0; i < cnt; i++) { int32_t v = (int32_t)get32(src + h + 4 * i); if (v < mn) mn = v; }
    uint8_t *t2 = malloc(cnt * 2), *tp = malloc(cnt * 2), *out = malloc(n * 2 + 65536); codec_ctx x = {0};
    for (size_t i = 0; i < cnt; i++) { uint32_t v = (uint32_t)((int32_t)get32(src + h + 4 * i) - mn); t2[2 * i] = (uint8_t)v; t2[2 * i + 1] = (uint8_t)(v >> 8); tp[i] = (uint8_t)v; tp[cnt + i] = (uint8_t)(v >> 8); }
    int codecs[] = {C_CM, C_LZMA, C_BSC, C_ZSTD}; const char *nm[] = {"cm", "lzma", "bsc", "zstd19"}; int lv[] = {0, 9, 0, 19};
    printf("%s", argv[2] + strlen(argv[2]) - 18);
    for (int k = 0; k < 4; k++) {
        size_t a = codec_compress(&x, codecs[k], lv[k], t2, cnt * 2, out, n * 2 + 65536);
        size_t b = codec_compress(&x, codecs[k], lv[k], tp, cnt * 2, out, n * 2 + 65536);
        printf("  %s inter %zu planes %zu", nm[k], a, b);
    }
    printf("\n"); exit(0);
}
