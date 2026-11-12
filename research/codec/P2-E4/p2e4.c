/* P2-E4 (copy of P2-E2/p2e2.c + zero-run leaf, zrl.inc): type-first fast integer pipelines as real serialized research containers (PROTOCOL.md). Research only.
 * Includes the frozen E02C harness unmodified (main renamed) and production src/ through it, plus fastint.inc.
 *
 * Kinds (one input, one thread, in memory):
 *   Z  libzstd 1.5.7 (vendored) ZSTD_compress2, nbWorkers 0, at the candidate level (3 = designated speed rival)
 *   T  P2-E1's type-first production pipeline (kg_encode_num / kg_decode), for reference
 *   F  type-first fast path: typing from the input's own NumPy/WAV header -> fi_forward (byte-identical to production's
 *      INT program) -> residual stream coded by an E02C configuration (leaf) -> serialized container. Decode: parse ->
 *      experimental leaf decode -> fi_inverse (or production kg_int_inv when inv = 'p') -> checksum.
 *      guard = 1: a zstd probe on a <= 64 KiB middle sample of the input (same level) predicts whole-input zstd;
 *      when the typed container is larger than the prediction, whole-input zstd is written instead (all charged).
 * Container: flag u8 (0 opaque zstd, 1 typed, 2 typed rejected -> zstd), uvarint length, u32 production block checksum
 * of the original, then flag 1: uv plen, par, uv rest length, rest, uv blob length, blob; flags 0/2: zstd frame. */
#define main e02c_harness_main
#include "../E02C-harness/harness/e02c.c"
#undef main
#include "../P2-E2/fastint.inc"
#include "zrl.inc"

typedef struct { const char *name; char kind; int level; const char *leaf; char inv; int guard; } cand_t;
static const cand_t CAND[] = {   /* leaf "zrl": zero-run leaf; guard 6: zrl when >= 25% zero residuals, else the named leaf */
    {"zstd3", 'Z', 3, NULL, 0, 0},
    {"tf_planes_huf", 'F', 3, "planes+huf", 'f', 0}, {"tf_prod3", 'F', 3, "exist+prod3", 'f', 0},
    {"tf_zrl", 'F', 3, "zrl", 'f', 0}, {"tf_zrl_auto", 'F', 3, "planes+huf", 'f', 6},
    {"zstd19", 'Z', 19, NULL, 0, 0}, {"tf_planes_prod19", 'F', 19, "planes+prod19", 'f', 0},
};
#define NC ((int)(sizeof CAND / sizeof CAND[0]))
static int conf_by_name(const char *want) { char nb[32]; for (int c = 0; c < NCONF; c++) if (!strcmp(conf_name(c, nb), want)) return c; DIE("no config %s", want); }
static int CONF[NC];

static int type_of(const uint8_t *s, size_t n, int *w, size_t *h) {   /* identical to P2-E1 */
    if (n >= 12 && !memcmp(s, "\x93NUMPY", 6)) {
        size_t hl = s[6] == 1 ? (size_t)s[8] | (size_t)s[9] << 8 : (size_t)get32(s + 8), o = s[6] == 1 ? 10 : 12;
        if (o + hl > n || hl > 65536) return -1;
        const char *d = memmem(s + o, hl, "'descr':", 8); if (!d) return -1;
        const char *q = memchr(d + 8, '\'', (size_t)((const char *)s + o + hl - (d + 8))); if (!q || q + 4 >= (const char *)s + o + hl) return -1;
        if ((q[1] != '<' && q[1] != '|') || (q[2] != 'i' && q[2] != 'u')) return -1;
        int ww = atoi(q + 3); if (ww != 2 && ww != 4 && ww != 8) return -1;
        if (!memmem(s + o, hl, "'fortran_order': False", 22)) return -1;
        *w = ww; *h = o + hl; return 0;
    }
    if (n >= 12 && !memcmp(s, "RIFF", 4) && !memcmp(s + 8, "WAVE", 4)) {
        size_t i = 12; int bits = 0, fmt = 0;
        while (i + 8 <= n) {
            uint32_t cl = get32(s + i + 4);
            if (!memcmp(s + i, "fmt ", 4) && cl >= 16 && i + 8 + 16 <= n) { fmt = s[i + 8] | s[i + 9] << 8; bits = s[i + 22] | s[i + 23] << 8; }
            if (!memcmp(s + i, "data", 4)) { if (fmt != 1 || bits != 16) return -1; *w = 2; *h = i + 8; return 0; }
            if ((uint64_t)cl + 8 + (cl & 1) > n - i) return -1; i += 8 + cl + (cl & 1);
        }
    }
    return -1;
}
static uint8_t *put_uv(uint8_t *q, uint64_t v) { while (v >= 128) { *q++ = (uint8_t)(v | 128); v >>= 7; } *q++ = (uint8_t)v; return q; }

/* zstd probe on a <= 64 KiB middle sample, extrapolated to the whole input (bytes) */
static double zprobe_n(const uint8_t *p, size_t n, int level, size_t lim) {
    size_t sn = n < lim ? n : lim, off = (n - sn) / 2 & ~(size_t)7; uint8_t *zb = malloc(ZSTD_compressBound(sn));
    size_t zs = ZSTD_compress(zb, ZSTD_compressBound(sn), p + off, sn, level); free(zb); if (ZSTD_isError(zs)) DIE("probe");
    return (double)zs * (double)n / (double)sn;
}
static double zprobe(const uint8_t *p, size_t n, int level) { return zprobe_n(p, n, level, 65536); }
typedef struct { const uint8_t *src; size_t n; int level; size_t lim; } gctx_t;
static int pre_guard(void *v, double est_bits) { gctx_t *g = v; return est_bits / 8.0 > zprobe_n(g->src, g->n, g->level, g->lim); }

/* complete encode into a fresh buffer; returns length (caller frees *dst) */
static size_t enc(const uint8_t *src, size_t n, int ci, uint8_t **dst) {
    const cand_t *c = &CAND[ci];
    if (c->kind == 'Z') {
        size_t cap = ZSTD_compressBound(n); uint8_t *d = malloc(cap); ZSTD_CCtx *z = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(z, ZSTD_c_compressionLevel, c->level); ZSTD_CCtx_setParameter(z, ZSTD_c_nbWorkers, 0);
        size_t r = ZSTD_compress2(z, d, cap, src, n); if (ZSTD_isError(r)) DIE("zstd"); ZSTD_freeCCtx(z); *dst = d; return r;
    }
    size_t cap = n + n / 8 + ZSTD_compressBound(n) + 4096; uint8_t *d = malloc(cap), *q = d; int w; size_t h, out = 0;
    codec_ctx x = {0}; int typed = !type_of(src, n, &w, &h);
    *q++ = 0; q = put_uv(q, n); put32(q, blk_sum(2, src, n)); q += 4;
    if (typed && c->kind == 'T') {
        size_t pn = kg_encode_num(&x, C_ZSTD, c->level, src, n, w, h, q, cap - (size_t)(q - d));
        if (pn) { d[0] = 1; out = (size_t)(q - d) + pn; } else d[0] = 2;
    } else if (typed) {
        fiprog P; kbuf blob = {0}; gctx_t gc = {src, n, c->level, c->guard == 5 ? 16384 : 65536};
        int fr = fi_forward_g(src, n, w, h, &P, (c->guard == 2 || c->guard == 4 || c->guard == 5) ? pre_guard : NULL, &gc);
        int usez = CONF[ci] < 0;
        if (!fr && c->guard == 6) { size_t nz = 0; for (size_t i = 0; i < P.m; i++) nz += !zrl_get(P.res.p, i, P.m, P.ow, P.pln); usez = nz * 4 >= P.m; }
        if (!fr && !(usez ? zrl_encode(&x, P.res.p, P.m, P.ow, P.pln, &blob) : conf_encode(&x, CONF[ci], P.res.p, P.m, P.ow, P.pln, &blob, NULL))) {
            if (c->guard == 3 || c->guard == 4) {                 /* cheap leaf selection: zstd-3 probe on the residual */
                double est = zprobe(P.res.p, P.res.n, c->level) + 16;
                if (est < 0.95 * (double)blob.n) { kbuf b2 = {0}; if (conf_encode(&x, conf_by_name("exist+prod3"), P.res.p, P.m, P.ow, P.pln, &b2, NULL)) DIE("sel");
                                                   if (b2.n < blob.n) { free(blob.p); blob = b2; } else free(b2.p); }
            }
            uint8_t *s = q; s = put_uv(s, P.par.n); memcpy(s, P.par.p, P.par.n); s += P.par.n;
            s = put_uv(s, P.rest.n); memcpy(s, P.rest.p, P.rest.n); s += P.rest.n;
            s = put_uv(s, blob.n); memcpy(s, blob.p, blob.n); s += blob.n;
            size_t tl = (size_t)(s - d); d[0] = 1; out = tl;
            if (c->guard == 1) {                                  /* sampled zstd probe after the typed encode */
                size_t sn = n < 65536 ? n : 65536, off = (n - sn) / 2; uint8_t *zb = malloc(ZSTD_compressBound(sn));
                size_t zs = ZSTD_compress(zb, ZSTD_compressBound(sn), src + off, sn, c->level); free(zb);
                if (ZSTD_isError(zs)) DIE("probe");
                if ((double)tl > (double)zs * (double)n / (double)sn) { d[0] = 2; out = 0; }
            }
        } else d[0] = 2;
        fi_free(&P); free(blob.p);
    }
    if (!out) {                                                   /* opaque or rejected: whole-input zstd at the level */
        size_t r = ZSTD_compress(q, cap - (size_t)(q - d), src, n, c->level); if (ZSTD_isError(r)) DIE("zstd"); out = (size_t)(q - d) + r;
    }
    codec_ctx_free(&x); *dst = d; return out;
}
static int dec(const uint8_t *f, size_t fn, int ci, uint8_t *out, size_t n) {
    const cand_t *c = &CAND[ci];
    if (c->kind == 'Z') { ZSTD_DCtx *z = ZSTD_createDCtx(); size_t r = ZSTD_decompressDCtx(z, out, n + 64, f, fn); ZSTD_freeDCtx(z); return ZSTD_isError(r) || r != n; }
    size_t i = 1; uint64_t len, a, b, bl; int rc = -1; codec_ctx x = {0}; kbuf res = {0}, o = {0};
    if (fn < 6 || kg_getuv(f, fn, &i, &len) || len != n || i + 4 > fn) return -1;
    uint32_t sum = get32(f + i); i += 4;
    if (f[0] != 1) { size_t r = ZSTD_decompress(out, n + 64, f + i, fn - i); rc = ZSTD_isError(r) || r != n; }
    else if (c->kind == 'T') rc = kg_decode(&x, f + i, fn - i, out, n);
    else {
        const size_t maxs = n * 4 + (1u << 20);
        if (kg_getuv(f, fn, &i, &a) || a > fn - i) goto done; const uint8_t *par = f + i; size_t plen = (size_t)a; i += plen;
        if (kg_getuv(f, fn, &i, &b) || b > fn - i) goto done; kbuf rest = {(uint8_t *)(f + i), (size_t)b, (size_t)b}; i += b;
        if (kg_getuv(f, fn, &i, &bl) || bl != fn - i || plen < 3) goto done;
        size_t pi = 3; uint64_t hh, cnt; if (kg_getuv(par, plen, &pi, &hh) || kg_getuv(par, plen, &pi, &cnt)) goto done;
        int ord = (par[1] >> 1) & 3, ow = par[2], pln = (par[1] >> 3) & 1; if ((uint64_t)ord > cnt || cnt > maxs) goto done;
        if (bl && f[i] == 0xB0 ? zrl_decode(&x, f + i, (size_t)bl, (size_t)(cnt - (uint64_t)ord), ow, pln, &res)
                               : e2c_decode(&x, f + i, (size_t)bl, (size_t)(cnt - (uint64_t)ord), ow, pln, maxs, &res)) goto done;
        if ((c->inv == 'p' ? kg_int_inv : fi_inverse)(par, plen, &res, &rest, maxs, &o) || o.n != n) goto done;
        memcpy(out, o.p, n); rc = 0;
    }
done:
    free(res.p); free(o.p); codec_ctx_free(&x);
    return rc || blk_sum(2, out, n) != sum;
}

int main(int argc, char **argv) {
    { uint16_t one = 1; if (*(uint8_t *)&one != 1) DIE("little-endian host required"); }
    if (argc < 4) DIE("usage: p2e2 run FILE TAG ROUNDS SEED | p2e2 mem FILE CANDIDATE");
    for (int c = 0; c < NC; c++) CONF[c] = CAND[c].leaf && strcmp(CAND[c].leaf, "zrl") ? conf_by_name(CAND[c].leaf) : -1;
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("read");
    uint8_t *out = malloc(n + 64);
    if (!strcmp(argv[1], "mem")) {
        int ci = -1; for (int c = 0; c < NC; c++) if (!strcmp(argv[3], CAND[c].name)) ci = c; if (ci < 0) DIE("candidate");
        if (hwm_reset()) DIE("clear_refs");
        uint8_t *f; size_t fn = enc(src, n, ci, &f); memset(out, 0, n); if (dec(f, fn, ci, out, n) || memcmp(out, src, n)) DIE("mem verify");
        printf("{\"kind\":\"p2e4_mem\",\"input\":\"%s\",\"candidate\":\"%s\",\"stored\":%zu,\"vmhwm_kb\":%ld}\n", argv[2], CAND[ci].name, fn, status_kb("VmHWM:"));
        free(f); free(out); free(src); return 0;
    }
    const char *tag = argv[3]; int rounds = argc > 4 ? atoi(argv[4]) : 5; xs = (uint64_t)(argc > 5 ? atoll(argv[5]) : 1) * 2654435761ull + 7;
    /* identity with production: fi_forward vs a kg_encode_num capture; fi_inverse vs kg_int_inv */
    { int w; size_t h; codec_ctx x = {0};
      if (!type_of(src, n, &w, &h)) {
          size_t cap = n + n / 8 + 4096; uint8_t *pay = malloc(cap); size_t pn = kg_encode_num(&x, C_ZSTD, 3, src, n, w, h, pay, cap);
          fiprog P; int fr = fi_forward(src, n, w, h, &P);
          { size_t cnt = (n - h) / (size_t)w; int o1 = 0, s1 = 1, o2 = 0, s2 = 1;
            double c1 = kg_num_choose(src + h, cnt, w, &o1, &s1), c2 = fi_choose(src + h, cnt, w, &o2, &s2);
            if (o1 != o2 || s1 != s2 || c1 != c2) DIE("fi_choose differs from kg_num_choose for %s", tag); }
          if (pn) {
              intg g; if (ig_parse(&x, pay, pn, n, 1, &g)) DIE("capture");
              int same = !fr && g.plen == P.par.n && !memcmp(g.par, P.par.p, P.par.n) && g.res.n == P.res.n && !memcmp(g.res.p, P.res.p, P.res.n)
                         && g.rest.n == P.rest.n && !memcmp(g.rest.p, P.rest.p, P.rest.n);
              if (!same) DIE("fi_forward differs from production for %s", tag);
              kbuf a = {0}, b = {0}; const size_t maxs = n * 4 + (1u << 20);
              if (kg_int_inv(g.par, g.plen, &g.res, &g.rest, maxs, &a) || fi_inverse(P.par.p, P.par.n, &P.res, &P.rest, maxs, &b)
                  || a.n != n || b.n != n || memcmp(a.p, src, n) || memcmp(b.p, src, n)) DIE("fi_inverse differs from production for %s", tag);
              printf("{\"kind\":\"p2e4_identity\",\"input\":\"%s\",\"w\":%d,\"order\":%d,\"signed\":%d,\"ow\":%d,\"gcd\":%llu,\"choice_identical\":true,\"forward_identical\":true,\"inverse_identical\":true}\n",
                     tag, w, P.bo, P.bs, P.ow, (unsigned long long)P.g);
              free(a.p); free(b.p); ig_free(&g);
          } else printf("{\"kind\":\"p2e4_identity\",\"input\":\"%s\",\"production_graph\":\"not smaller\"}\n", tag);
          if (!fr) fi_free(&P); free(pay);
      } else printf("{\"kind\":\"p2e4_identity\",\"input\":\"%s\",\"typed\":false}\n", tag);
      codec_ctx_free(&x); }
    uint8_t *F[NC]; size_t FN[NC]; int Be[NC], Bd[NC], ord[NC];
    for (int c = 0; c < NC; c++) {
        FN[c] = enc(src, n, c, &F[c]); uint8_t *f2; size_t f2n = enc(src, n, c, &f2);
        if (f2n != FN[c] || memcmp(f2, F[c], FN[c])) DIE("%s not deterministic", CAND[c].name); free(f2);
        memset(out, 0, n); if (dec(F[c], FN[c], c, out, n) || memcmp(out, src, n)) DIE("%s does not reconstruct", CAND[c].name);
        printf("{\"kind\":\"p2e4_size\",\"input\":\"%s\",\"bytes\":%zu,\"candidate\":\"%s\",\"stored\":%zu,\"flag\":%d,\"exact\":true}\n",
               tag, n, CAND[c].name, FN[c], CAND[c].kind == 'Z' ? -1 : F[c][0]);
        uint64_t t0 = ns_of(CLOCK_MONOTONIC); { uint8_t *t; enc(src, n, c, &t); free(t); } uint64_t t1 = ns_of(CLOCK_MONOTONIC); dec(F[c], FN[c], c, out, n); uint64_t t2 = ns_of(CLOCK_MONOTONIC);
        uint64_t te = t1 - t0 ? t1 - t0 : 1, td = t2 - t1 ? t2 - t1 : 1;
        Be[c] = te >= 20000000 ? 1 : (int)(20000000 / te) + 1; Bd[c] = td >= 20000000 ? 1 : (int)(20000000 / td) + 1;
        if (Be[c] > 500) Be[c] = 500; if (Bd[c] > 500) Bd[c] = 500;
    }
    for (int r = 0; r < rounds; r++) {
        for (int c = 0; c < NC; c++) ord[c] = (c + r) % NC;
        shuffle(ord, NC);
        for (int oi = 0; oi < NC; oi++) {
            int c = ord[oi]; uint64_t w0, c0, w1, c1, w2, c2;
            w0 = ns_of(CLOCK_MONOTONIC); c0 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Be[c]; it++) { uint8_t *t; if (enc(src, n, c, &t) != FN[c]) DIE("encode drift"); free(t); }
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            double ew = (double)(w1 - w0) / Be[c], ec = (double)(c1 - c0) / Be[c];
            memset(out, 0, n);
            w1 = ns_of(CLOCK_MONOTONIC); c1 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            for (int it = 0; it < Bd[c]; it++) if (dec(F[c], FN[c], c, out, n)) DIE("decode");
            w2 = ns_of(CLOCK_MONOTONIC); c2 = ns_of(CLOCK_PROCESS_CPUTIME_ID);
            if (memcmp(out, src, n)) DIE("timed decode mismatch %s", CAND[c].name);
            printf("{\"kind\":\"p2e4_time\",\"input\":\"%s\",\"candidate\":\"%s\",\"pair_id\":\"r%d\",\"slot\":%d,\"enc_reps\":%d,\"dec_reps\":%d,"
                   "\"encode_wall_s\":%.9f,\"encode_cpu_s\":%.9f,\"decode_wall_s\":%.9f,\"decode_cpu_s\":%.9f}\n",
                   tag, CAND[c].name, r, oi, Be[c], Bd[c], ew * 1e-9, ec * 1e-9, (double)(w2 - w1) / Bd[c] * 1e-9, (double)(c2 - c1) / Bd[c] * 1e-9);
        }
    }
    for (int c = 0; c < NC; c++) free(F[c]);
    free(out); free(src); return 0;
}
