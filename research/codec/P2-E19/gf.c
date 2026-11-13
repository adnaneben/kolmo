/* P2-E19 copy of P2-E18/gf.c (unchanged logic): KG_INT order 0, bias = min, 16-bit output, one leaf codec; production CZN1 frame. */
#include "../P2-E11/p2e11.c"
static uint64_t zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
__attribute__((constructor)) static void gf(int argc, char **argv, char **envp) {
    (void)envp; if (argc < 6 || strcmp(argv[1], "gf")) return;   /* gf CODEC(cm|bsc|lzma|rans|huf|zstd19) PLANES(0|1) SRC DST */
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    int planes = atoi(argv[3]); size_t n; uint8_t *src = readfile(argv[4], &n); int w; size_t h;
    if (type_of(src, n, &w, &h) || w != 4) DIE("type");
    size_t cnt = (n - h) / 4; int64_t mn = INT64_MAX, mx = INT64_MIN;
    for (size_t i = 0; i < cnt; i++) { int64_t v = (int32_t)get32(src + h + 4 * i); if (v < mn) mn = v; if (v > mx) mx = v; }
    if (mx - mn > 65535) DIE("range");
    uint8_t *res = malloc(cnt * 2 + 1); size_t rn = cnt * 2;
    for (size_t i = 0; i < cnt; i++) { uint32_t v = (uint32_t)((int32_t)get32(src + h + 4 * i) - mn);
        if (planes) { res[i] = (uint8_t)v; res[cnt + i] = (uint8_t)(v >> 8); } else { res[2 * i] = (uint8_t)v; res[2 * i + 1] = (uint8_t)(v >> 8); } }
    size_t tail = n - h - cnt * 4; uint8_t *rest = malloc(h + tail + 1); memcpy(rest, src, h); memcpy(rest + h, src + h + cnt * 4, tail); size_t restn = h + tail;
    kbuf par = {0}; kb_byte(&par, 4); kb_byte(&par, (uint8_t)(1 | (planes ? 8 : 0))); kb_byte(&par, 2); kb_uv(&par, h); kb_uv(&par, cnt); kb_uv(&par, 1); kb_uv(&par, zz(mn));
    int cc = C_CM, lv = 0; const char *k = argv[2];
    if (!strcmp(k, "bsc")) cc = C_BSC; else if (!strcmp(k, "lzma")) { cc = C_LZMA; lv = 9; } else if (!strcmp(k, "rans")) cc = C_RANS; else if (!strcmp(k, "huf")) cc = C_HUF; else if (!strcmp(k, "zstd19")) { cc = C_ZSTD; lv = 19; }
    codec_ctx x = {0}; size_t zc = rn * 2 + 65536; uint8_t *zb = malloc(zc); size_t z = codec_compress(&x, cc, lv, res, rn, zb, zc); if (!z) DIE("codec");
    kbuf o = {0};
    kb_byte(&o, 'G'); kb_byte(&o, 1); kb_uv(&o, 1); kb_byte(&o, KG_INT); kb_uv(&o, 1); kb_uv(&o, 0); kb_uv(&o, 2); kb_uv(&o, par.n); kb_put(&o, par.p, par.n);
    kb_uv(&o, 2); kb_uv(&o, 1); kb_byte(&o, (uint8_t)cc); kb_uv(&o, rn); kb_uv(&o, z); kb_put(&o, zb, z);
    kb_uv(&o, 2); kb_byte(&o, C_STORE); kb_uv(&o, restn); kb_uv(&o, restn); kb_put(&o, rest, restn);
    uint8_t *f = malloc(o.n + 4096); size_t fn = tp_wrap(src, n, w, h, o.p, o.n, f);
    FILE *of = fopen(argv[5], "wb"); if (!of || fwrite(f, 1, fn, of) != fn || fclose(of)) DIE("gf: write");
    codec_ctx_free(&x); free(f); free(o.p); free(zb); free(par.p); free(rest); free(res); free(src); exit(0);
}
