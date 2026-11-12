/* E02G-verify: decode-only timing of production / control / identical-control / candidate frames (PROTOCOL.md).
 * Includes the frozen harness unmodified; its main() is renamed. */
#define main e02c_harness_main
#include "../E02C-harness/harness/e02c.c"
#undef main

static int conf_by_name(const char *want) { char nb[32]; for (int c = 0; c < NCONF; c++) if (!strcmp(conf_name(c, nb), want)) return c; DIE("no config %s", want); }
static volatile uint8_t sink;
static void displace(uint8_t *buf, size_t n, uint64_t r) { for (size_t i = 0; i < n; i += 64) buf[i] = (uint8_t)(i + r); sink = buf[n / 2]; }

int main(int argc, char **argv) {
    if (argc < 6) DIE("usage: dtime FILE TAG fast|hc ROUNDS SEED");
    size_t n; uint8_t *src = readfile(argv[1], &n); if (!src) DIE("read");
    const char *tag = argv[2], *mode = argv[3]; int rounds = atoi(argv[4]); uint64_t seed = (uint64_t)atoll(argv[5]);
    kolmo_params p = mode_params(mode); size_t cap = kolmo_bound(n, &p); uint8_t *frame = malloc(cap), *out = malloc(n + 64);
    int64_t fn = kolmo_compress(src, n, frame, cap, &p); if (fn <= 0) DIE("compress");
    codec_ctx x = {0}; fparsed F; if (frame_walk(&x, frame, (size_t)fn, &F)) DIE("walk");
    int nint = 0; for (uint32_t i = 0; i < F.P.nb; i++) nint += F.B[i].isint;
    int hc = !strcmp(mode, "hc"), cc = conf_by_name(hc ? "exist+prod19" : "exist+prod3"), ck = conf_by_name(hc ? "planes+prod19" : "planes+prod3g");
    kbuf *bc = calloc(F.P.nb, sizeof(kbuf)), *bc2 = calloc(F.P.nb, sizeof(kbuf)), *bk = calloc(F.P.nb, sizeof(kbuf));
    size_t sc = (size_t)fn, sk = (size_t)fn;
    for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) {
        intg *g = &F.B[i].g;
        if (conf_encode(&x, cc, g->res.p, g->m, g->ow, g->pln, &bc[i], NULL) || conf_encode(&x, ck, g->res.p, g->m, g->ow, g->pln, &bk[i], NULL)) DIE("encode");
        if (conf_encode(&x, cc, g->res.p, g->m, g->ow, g->pln, &bc2[i], NULL) || bc2[i].n != bc[i].n || memcmp(bc2[i].p, bc[i].p, bc[i].n)) DIE("control not deterministic");
        sc = sc - g->rleaf_len + exp_leaf_bytes(g, bc[i].n); sk = sk - g->rleaf_len + exp_leaf_bytes(g, bk[i].n);
    }
    kbuf *pol[4] = {NULL, bc, bc2, bk}; const char *pname[4] = {"P", "C", "C2", "K"};
    for (int q = 0; q < 4; q++) { memset(out, 0, n); if (seq_decode(&F, nint ? pol[q] : NULL, out) || memcmp(out, src, n)) DIE("%s does not reconstruct", pname[q]); }
    printf("{\"kind\":\"dtime_frame\",\"input\":\"%s\",\"mode\":\"%s\",\"bytes\":%zu,\"int_blocks\":%d,\"production\":%lld,\"control\":%zu,\"candidate\":%zu}\n", tag, mode, n, nint, (long long)fn, sc, sk);
    const size_t DISP = 32u << 20; uint8_t *dbuf = malloc(DISP); memset(dbuf, 1, DISP);
    int batch = (int)((8u << 20) / (n ? n : 1)) + 1; if ((size_t)batch * n < (8u << 20) && n) batch++; if (batch < 1) batch = 1;
    for (int r = 0; r < rounds; r++) {
        int ord[4]; for (int q = 0; q < 4; q++) ord[q] = (q + r) % 4;
        xs = seed + (uint64_t)r * 0x9E3779B97F4A7C15ull + 1; shuffle(ord, 4);
        for (int reg = 0; reg < 2; reg++) for (int oi = 0; oi < 4; oi++) {
            int q = ord[oi]; kbuf *b = nint ? pol[q] : NULL; uint64_t w0, c0, w1, c1; int B = reg ? 1 : batch;
            if (reg) displace(dbuf, DISP, (uint64_t)r); else if (seq_decode(&F, b, out)) DIE("warm-up");
            w0 = ns_of(CLOCK_MONOTONIC); c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < B; it++) if (seq_decode(&F, b, out)) DIE("decode");
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            if (memcmp(out, src, n)) DIE("timed decode mismatch");
            printf("{\"kind\":\"dtime\",\"input\":\"%s\",\"mode\":\"%s\",\"round\":%d,\"regime\":\"%s\",\"slot\":%d,\"policy\":\"%s\",\"batch\":%d,\"wall_ns\":%.1f,\"cpu_ns\":%.1f}\n",
                   tag, mode, r, reg ? "displaced" : "warm", oi, pname[q], B, (double)(w1 - w0) / B, (double)(c1 - c0) / B);
        }
    }
    for (uint32_t i = 0; i < F.P.nb; i++) { free(bc[i].p); free(bc2[i].p); free(bk[i].p); }
    free(bc); free(bc2); free(bk); free(dbuf); frame_free(&F); codec_ctx_free(&x); free(frame); free(out); free(src);
    return 0;
}
