/* E02C correctness suite (included by e02c.c). Run under ASan/UBSan/LSan via `make -f research/codec/E02C/Makefile check`.
 * Counts printed here are results of THIS experiment, not of the planner-ownership validation. */
static size_t T_planner_cases_int, T_roundtrips, T_truncations, T_mutations, T_malformed, T_prod_cases, T_prod_na, T_prod_recon, T_planner_blocks, T_planner_recon, T_golden;
#define CHECK(c, ...) do { if (!(c)) { fprintf(stderr, "SELFTEST FAIL: " __VA_ARGS__); fputc('\n', stderr); exit(1); } } while (0)

static FILE *golden_out; static char *golden_in; static size_t golden_in_n;
/* one line per case: digest over every configuration's blob (size and bytes), in configuration order */
static void golden(const char *cs, uint64_t digest, size_t bytes) {
    char line[200]; snprintf(line, sizeof line, "%s %zu %016llx\n", cs, bytes, (unsigned long long)digest);
    if (golden_out) fputs(line, golden_out);
    if (golden_in) {
        char key[128]; snprintf(key, sizeof key, "\n%s ", cs); char *h = strstr(golden_in, key); if (h) h++;
        CHECK(h, "golden vector missing for %s", key);
        CHECK(!strncmp(h, line, strlen(line)), "golden vector changed: %s", line);
        T_golden++;
    }
}
static void gen(uint64_t *z, size_t m, int ow, int pat, uint64_t seed) {
    uint64_t top = ow == 8 ? ~0ull : ((1ull << (8 * ow)) - 1); xs = seed * 2654435761u + 1;
    for (size_t i = 0; i < m; i++) {
        uint64_t r = xr();
        switch (pat) {
            case 0: z[i] = 0; break;                                    /* zero */
            case 1: z[i] = top; break;                                  /* constant at the extreme */
            case 2: z[i] = r & top; break;                              /* full width random */
            case 3: z[i] = (r & 7) | ((r >> 8) % 97 == 0 ? top : 0); break;   /* sparse outliers */
            case 4: z[i] = (i / 200) % 3 == 1 && (i % 200) < 40 ? (r & top) : (r & 3); break;   /* clustered outliers */
            case 5: z[i] = i & 1 ? top : 0; break;                      /* alternating extremes */
            default: z[i] = i + 1 == m ? top : 0; break;                /* single outlier in the tail */
        }
    }
}
static void rt_case(codec_ctx *x, int ow, int pln, size_t m, int pat, int heavy, const char *dir) {
    uint64_t *z = malloc((m ? m : 1) * 8); uint8_t *r = malloc(m * (size_t)ow + 1); gen(z, m, ow, pat, m * 31 + (size_t)pat * 7 + (size_t)ow);
    z_to_res(z, m, ow, pln, r); char cs[64], cf[32]; uint64_t dg = 1469598103934665603ull, dgp = dg, dge = dg, dgl = dg; size_t gb = 0, gbp = 0, gbe = 0, gbl = 0; snprintf(cs, sizeof cs, "ow%d_p%d_m%zu_pat%d", ow, pln, m, pat);
    for (int c = 0; c < NCONF; c++) {
        kbuf b = {0}, b2 = {0}, d = {0}; estat st; conf_name(c, cf);
        CHECK(!conf_encode(x, c, r, m, ow, pln, &b, &st), "encode %s %s", cs, cf);
        CHECK(!conf_encode(x, c, r, m, ow, pln, &b2, NULL) && b2.n == b.n && !memcmp(b.p, b2.p, b.n), "nondeterministic %s %s", cs, cf);
        CHECK(!e2c_decode(x, b.p, b.n, m, ow, pln, m * (size_t)ow + 64, &d) && d.n == m * (size_t)ow && !memcmp(d.p, r, d.n), "round trip %s %s", cs, cf);
        T_roundtrips++;
        for (int q = 0; q < st.ns; q++) CHECK(st.s[q].actual == C_STORE || st.s[q].coded < st.s[q].raw, "coded not smaller %s", cs);
        if (c >= NFIX + 2 * B_N + 3) { dgl = (dgl ^ fnv(b.p, b.n) ^ b.n) * 1099511628211ull; gbl += b.n; }
        else if (c >= NFIX + 2 * B_N) { dge = (dge ^ fnv(b.p, b.n) ^ b.n) * 1099511628211ull; gbe += b.n; }
        else if (conf_rep(c) == R_PLANES) { dgp = (dgp ^ fnv(b.p, b.n) ^ b.n) * 1099511628211ull; gbp += b.n; }   /* appended configs: own line */
        else { dg = (dg ^ fnv(b.p, b.n) ^ b.n) * 1099511628211ull; gb += b.n; }
        if (dir && conf_rep(c) >= 0 && conf_be(c) == B_STORE && (pat == 3 || pat == 5 || pat == 6) && (m == 129 || m == 1000)) {   /* vectors for the independent reference */
            char path[512]; snprintf(path, sizeof path, "%s/%s_%s.blob", dir, cs, RN[conf_rep(c)]);
            FILE *f = fopen(path, "wb"); fwrite(b.p, 1, b.n, f); fclose(f);
            snprintf(path, sizeof path, "%s/%s.res", dir, cs); f = fopen(path, "wb"); fwrite(r, 1, m * (size_t)ow, f); fclose(f);
        }
        if (heavy) {
            for (size_t t = 0; t < b.n; t++) { CHECK(e2c_decode(x, b.p, t, m, ow, pln, m * (size_t)ow + 64, &d), "truncated blob accepted %s %s len %zu", cs, cf, t); T_truncations++; }
            uint8_t *mu = malloc(b.n + 1);
            for (int k = 0; k < 64; k++) {                            /* mutations: must never crash (outcome free) */
                memcpy(mu, b.p, b.n); size_t at = (size_t)(xr() % b.n); mu[at] ^= (uint8_t)(1u << (xr() % 8));
                if (!e2c_decode(x, mu, b.n, m, ow, pln, m * (size_t)ow + 64, &d)) CHECK(d.n == m * (size_t)ow, "mutated size");
                T_mutations++;
            }
            free(mu);
            if (!e2c_decode(x, b.p, b.n, m + 1, ow, pln, (m + 1) * (size_t)ow + 64, &d))   /* the count is context (KG_INT params); */
                CHECK(d.n == (m + 1) * (size_t)ow, "wrong count size %s", cs);           /* zero-width packing legitimately omits it */
            uint8_t *tr = malloc(b.n + 1); memcpy(tr, b.p, b.n); tr[b.n] = 0;
            CHECK(e2c_decode(x, tr, b.n + 1, m, ow, pln, m * (size_t)ow + 64, &d), "trailing byte accepted %s", cs); free(tr);
            T_malformed += 2;
        }
        free(b.p); free(b2.p); free(d.p);
    }
    golden(cs, dg, gb);
    { char cp[80]; snprintf(cp, sizeof cp, "%s_planes", cs); golden(cp, dgp, gbp); snprintf(cp, sizeof cp, "%s_ent", cs); golden(cp, dge, gbe); snprintf(cp, sizeof cp, "%s_lvl", cs); golden(cp, dgl, gbl); }
    free(z); free(r);
}
static void expect_reject(codec_ctx *x, const uint8_t *p, size_t n, size_t m, int ow, size_t maxs, const char *what) {
    kbuf d = {0}; CHECK(e2c_decode(x, p, n, m, ow, ow > 1, maxs, &d), "malformed accepted: %s", what); free(d.p); T_malformed++;
}
static void malformed(codec_ctx *x) {
    /* tag / representation */
    expect_reject(x, (const uint8_t[]){0x00}, 1, 0, 1, 64, "non-experimental tag");
    expect_reject(x, (const uint8_t[]){0xE6}, 1, 0, 1, 64, "unknown representation");
    expect_reject(x, (const uint8_t[]){0xD1, 'G', 1, 0, 1, 1, 0, 0, 0}, 9, 0, 1, 64, "production-rule framing on a packed representation");
    expect_reject(x, (const uint8_t[]){0xD0, 'G', 1, 1, 1, 1, 0, 2, 2, 1, 2}, 11, 2, 1, 64, "production-rule framing with a node");
    expect_reject(x, (const uint8_t[]){0xD0, 'G', 1, 0, 1, 2, 0, 2, 2, 1, 2}, 11, 2, 1, 64, "production-rule framing: wrong leaf id");
    expect_reject(x, (const uint8_t[]){0xD0, 'G', 1, 0, 1, 1, 6, 2, 2, 1, 2}, 11, 2, 1, 64, "production-rule framing: disallowed codec");
    expect_reject(x, (const uint8_t[]){0xD5, 'G', 1, 0, 1, 1, 0, 2, 2, 1, 2}, 11, 2, 2, 64, "production-rule framing: too few planes");
    expect_reject(x, (const uint8_t[]){0xE5, 0, 2, 2, 1, 2}, 6, 2, 2, 64, "planes: too few plane streams");
    expect_reject(x, (const uint8_t[]){0}, 0, 0, 1, 64, "empty blob");
    /* stream framing: invalid codec id, raw length mismatch, coded > available, coded >= raw for entropy codec */
    expect_reject(x, (const uint8_t[]){0xE0, 7, 2, 2, 1, 2}, 6, 2, 1, 64, "invalid codec id");
    expect_reject(x, (const uint8_t[]){0xE0, 0, 3, 3, 1, 2, 3}, 7, 2, 1, 64, "raw length mismatch");
    expect_reject(x, (const uint8_t[]){0xE0, 0, 2, 9, 1, 2}, 6, 2, 1, 64, "coded length past end");
    expect_reject(x, (const uint8_t[]){0xE0, 13, 2, 2, 1, 2}, 6, 2, 1, 64, "entropy stream not smaller");
    expect_reject(x, (const uint8_t[]){0xE0, 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 13, 2, 1, 64, "overlong varint");
    /* invalid Huffman / rANS / zstd payloads (garbage tables) */
    expect_reject(x, (const uint8_t[]){0xE0, 13, 8, 4, 2, 8, 0xFF, 0xFF}, 8, 8, 1, 64, "invalid Huffman table");
    expect_reject(x, (const uint8_t[]){0xE0, 14, 8, 5, 8, 0xFF, 0xFF, 0xFF, 0xFF}, 9, 8, 1, 64, "invalid rANS table");
    expect_reject(x, (const uint8_t[]){0xE0, 3, 8, 4, 0x28, 0xB5, 0x2F, 0xFD}, 8, 8, 1, 64, "truncated zstd frame");
    /* GBP: width above the output width; nonzero padding bits */
    expect_reject(x, (const uint8_t[]){0xE1, 9, 0, 2, 2, 0xFF, 0x01}, 7, 1, 1, 64, "global width > 8*ow");
    expect_reject(x, (const uint8_t[]){0xE1, 1, 0, 1, 1, 0x02}, 6, 1, 1, 64, "nonzero padding bits");
    /* LBP: segment width too large */
    expect_reject(x, (const uint8_t[]){0xE2, 0, 1, 1, 17, 0, 0, 0}, 8, 1, 2, 64, "local width > 8*ow");
    /* GPP: exception count above m; exception with zero high part; exception position past m; overflowing high */
    expect_reject(x, (const uint8_t[]){0xE3, 1, 5, 0, 1, 1, 0, 0, 2, 2, 0, 1, 0, 0, 0, 0}, 16, 2, 1, 64, "exceptions > m");
    expect_reject(x, (const uint8_t[]){0xE3, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0}, 15, 2, 1, 64, "zero high part");
    expect_reject(x, (const uint8_t[]){0xE3, 1, 1, 0, 1, 1, 0, 0, 1, 1, 5, 0, 1, 1, 1}, 15, 2, 1, 64, "position past m");
    expect_reject(x, (const uint8_t[]){0xE3, 2, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0x7F}, 15, 2, 1, 64, "high overflows ow");
    expect_reject(x, (const uint8_t[]){0xE3, 8, 1, 0, 1, 1, 0}, 7, 1, 1, 64, "exceptions at full width");
    /* LPP: exception count above segment length; repeated position */
    expect_reject(x, (const uint8_t[]){0xE4, 0, 1, 1, 1, 0, 1, 1, 3, 0, 0, 0}, 12, 2, 1, 64, "segment exceptions > length");
    expect_reject(x, (const uint8_t[]){0xE4, 0, 1, 1, 0, 0, 1, 1, 2, 0, 1, 1, 0, 0, 2, 2, 0, 0, 0, 2, 2, 1, 1}, 23, 2, 1, 64, "repeated exception position");
    /* resource limits: count above the limit; raw length far above the limit (rejected before allocation) */
    expect_reject(x, (const uint8_t[]){0xE0, 0, 2, 2, 1, 2}, 6, 1000000, 8, 64, "m * ow above limit");
    expect_reject(x, (const uint8_t[]){0xE3, 1, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F}, 7, 16, 1, 64, "huge exception count");
    expect_reject(x, (const uint8_t[]){0xE3, 1, 1, 0, 2, 2, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0}, 15, 16, 1, 64, "huge gap stream raw length");
}
/* typed integer arrays through the unchanged kg_encode_num: signed/unsigned extremes, wrapping deltas, gcd, outliers */
static size_t fill_typed(uint8_t *a, int w, int pat, size_t cnt, size_t head, size_t tail, uint64_t seed) {
    xs = seed; for (size_t i = 0; i < head; i++) a[i] = (uint8_t)(0x40 + i);
    uint64_t v = 0, top = w == 8 ? ~0ull : (1ull << (8 * w)) - 1;
    for (size_t i = 0; i < cnt; i++) {
        uint64_t r = xr();
        switch (pat) {
            case 0: v = 7; break;                                                    /* constant */
            case 1: v = i & 1 ? top : top >> 1; break;                               /* unsigned max / signed max */
            case 2: v = i & 1 ? (top >> 1) + 1 : top >> 1; break;                    /* signed min / max: wrapping deltas */
            case 3: v += (uint64_t)((int64_t)(r % 41) - 20) * 12; break;             /* random walk, gcd 12 (wraps) */
            case 4: v = (r % 9) + ((r >> 20) % 251 == 0 ? top - (r & 255) : 0); break;   /* sparse outliers */
            case 5: v = ((i / 512) & 1) ? (r & top) : 1000 + (r & 15); break;        /* clustered outliers */
            case 6: v = (uint64_t)i * 0x9E3779B97F4A7C15ull; break;                  /* full-width wrapping ramp */
            default: v = (uint64_t)(int64_t)(int32_t)(r >> 32) / 5 * 5; break;       /* signed, gcd 5 */
        }
        for (int b = 0; b < w; b++) a[head + i * (size_t)w + (size_t)b] = (uint8_t)(v >> (8 * b));
    }
    for (size_t i = 0; i < tail; i++) a[head + cnt * (size_t)w + i] = (uint8_t)(0xA0 + i);
    return head + cnt * (size_t)w + tail;
}
static void prod_cases(codec_ctx *x, const char *dir) {
    static const size_t counts[] = {16, 17, 129, 4096, 70001}; uint8_t *a = malloc(70001 * 8 + 64), *pay = malloc(70001 * 9 + 8192), *o = malloc(70001 * 8 + 64);
    char nb[32]; int dumped = 0;
    for (int w = 2; w <= 8; w *= 2) for (int pat = 0; pat < 8; pat++) for (size_t ci = 0; ci < 5; ci++) for (int lv = 0; lv < 2; lv++) {
        size_t head = (pat * 3 + ci) % 5, tail = (pat + ci) % (size_t)w, n = fill_typed(a, w, pat, counts[ci], head, tail, 1000 + (uint64_t)pat * 17 + ci);
        size_t pn = kg_encode_num(x, C_ZSTD, lv ? 19 : 3, a, n, w, head, pay, 70001 * 9 + 8192);
        T_prod_cases++;
        if (!pn) { T_prod_na++; continue; }
        intg g; CHECK(!ig_parse(x, pay, pn, n, 1, &g), "capture w%d pat%d", w, pat);
        kbuf r = {0}; CHECK(!kg_int_inv(g.par, g.plen, &g.res, &g.rest, n * 4 + (1u << 20), &r) && r.n == n && !memcmp(r.p, a, n), "production inverse w%d pat%d", w, pat); free(r.p);
        for (int c = 0; c < NCONF; c++) {
            kbuf b = {0}; CHECK(!conf_encode(x, c, g.res.p, g.m, g.ow, g.pln, &b, NULL), "enc");
            memset(o, 0x5A, n); CHECK(!ig_decode_exp(x, pay, pn, b.p, b.n, o, n) && !memcmp(o, a, n), "complete-input reconstruction w%d pat%d n%zu %s", w, pat, n, conf_name(c, nb));
            T_prod_recon++; free(b.p);
        }
        if (dir && counts[ci] == 4096 && lv == 0 && dumped < 12) {            /* inputs + production payloads for the reference */
            char path[512]; snprintf(path, sizeof path, "%s/typed_w%d_pat%d.in", dir, w, pat); FILE *f = fopen(path, "wb"); fwrite(a, 1, n, f); fclose(f);
            snprintf(path, sizeof path, "%s/typed_w%d_pat%d.par", dir, w, pat); f = fopen(path, "wb"); fwrite(g.par, 1, g.plen, f); fclose(f);
            snprintf(path, sizeof path, "%s/typed_w%d_pat%d.res", dir, w, pat); f = fopen(path, "wb"); fwrite(g.res.p, 1, g.res.n, f); fclose(f);
            snprintf(path, sizeof path, "%s/typed_w%d_pat%d.rest", dir, w, pat); f = fopen(path, "wb"); fwrite(g.rest.p, 1, g.rest.n, f); fclose(f);
            dumped++;
        }
        ig_free(&g);
    }
    free(a); free(pay); free(o);
}
/* complete production planner: the earlier planner-ownership .npy int32 random-walk fixture, plus raw int16 and u64 */
static void planner_cases(codec_ctx *x) {
    for (int kind = 0; kind < 3; kind++) {
        size_t cnt = 300000, n = 0; uint8_t *a = malloc(cnt * 8 + 256);
        xs = 88172645463325252ull + (uint64_t)kind;
        if (kind == 0) {
            static const char header[] = "\x93NUMPY\x01\x00\x76\x00{'descr': '<i4', 'fortran_order': False, 'shape': (300000,), }";
            memset(a, ' ', 128); memcpy(a, header, sizeof(header) - 1); a[127] = '\n'; uint32_t v = 0;
            for (size_t i = 0; i < cnt; i++) { v += (uint32_t)((int32_t)(xr() % 21) - 10); memcpy(a + 128 + 4 * i, &v, 4); }
            n = 128 + 4 * cnt;
        } else if (kind == 1) {
            int16_t v = 0; for (size_t i = 0; i < cnt; i++) { v = (int16_t)(v + (int16_t)((int)(xr() % 61) - 30)); memcpy(a + 2 * i, &v, 2); } n = 2 * cnt;
        } else {
            uint64_t v = 1700000000000000000ull; for (size_t i = 0; i < cnt / 2; i++) { v += 1000 + xr() % 5000 + (xr() % 1000 == 0 ? 1ull << 40 : 0); memcpy(a + 8 * i, &v, 8); } n = 8 * (cnt / 2) + 3; a[n - 3] = 1; a[n - 2] = 2; a[n - 1] = 3;
        }
        for (int mo = 0; mo < 2; mo++) {
            kolmo_params p = mode_params(mo ? "hc" : "fast"); size_t cap = kolmo_bound(n, &p); uint8_t *fr = malloc(cap), *o = malloc(n + 64);
            int64_t fn = kolmo_compress(a, n, fr, cap, &p); CHECK(fn > 0, "compress");
            fparsed F; CHECK(!frame_walk(x, fr, (size_t)fn, &F), "walk"); int nint = 0;
            for (uint32_t i = 0; i < F.P.nb; i++) nint += F.B[i].isint;
            printf("planner case kind=%d mode=%s blocks=%u int_blocks=%d\n", kind, mo ? "hc" : "fast", F.P.nb, nint);
            T_planner_blocks += (size_t)nint; T_planner_cases_int += nint > 0;
            for (int c = 0; c < NCONF; c++) {
                kbuf *bl = calloc(F.P.nb, sizeof(kbuf));
                for (uint32_t i = 0; i < F.P.nb; i++) if (F.B[i].isint) CHECK(!conf_encode(x, c, F.B[i].g.res.p, F.B[i].g.m, F.B[i].g.ow, F.B[i].g.pln, &bl[i], NULL), "enc");
                memset(o, 0xC3, n); CHECK(!seq_decode(&F, bl, o) && !memcmp(o, a, n), "planner complete-input reconstruction kind %d config %d", kind, c);
                T_planner_recon++;
                for (uint32_t i = 0; i < F.P.nb; i++) free(bl[i].p); free(bl);
            }
            frame_free(&F); free(fr); free(o);
        }
        free(a);
    }
}
static int selftest(const char *dir, const char *goldpath) {
    codec_ctx x = {0};
    if (goldpath) {
        FILE *f = fopen(goldpath, "rb");
        if (f) { fseek(f, 0, SEEK_END); golden_in_n = (size_t)ftell(f); fseek(f, 0, SEEK_SET); golden_in = calloc(golden_in_n + 2, 1); golden_in[0] = '\n';
                 if (fread(golden_in + 1, 1, golden_in_n, f) != golden_in_n) return 2; fclose(f); }
        else golden_out = fopen(goldpath, "w");
    }
    static const size_t lens[] = {0, 1, 2, 127, 128, 129, 255, 256, 257, 1000, 4097};
    for (int ow = 1; ow <= 8; ow *= 2) for (int pln = 0; pln <= 1; pln++) for (size_t li = 0; li < sizeof lens / sizeof *lens; li++) for (int pat = 0; pat < 7; pat++)
        rt_case(&x, ow, pln, lens[li], pat, lens[li] <= 257 && lens[li] > 0 && (pat == 3 || pat == 4 || pat == 6), dir);
    malformed(&x);
    prod_cases(&x, dir);
    planner_cases(&x);
    CHECK(T_planner_cases_int >= 2, "complete planner selected the INT graph in fewer than two cases");
    const char *gs = golden_in ? "compared" : golden_out ? "written" : "none";
    if (golden_out) fclose(golden_out);
    free(golden_in); codec_ctx_free(&x);
    printf("E02C SELFTEST PASSED roundtrips=%zu truncations=%zu mutations=%zu malformed=%zu production_typed_cases=%zu production_typed_not_applicable=%zu "
           "production_typed_reconstructions=%zu planner_int_blocks=%zu planner_reconstructions=%zu golden_checked=%zu golden=%s\n",
           T_roundtrips, T_truncations, T_mutations, T_malformed, T_prod_cases, T_prod_na, T_prod_recon, T_planner_blocks, T_planner_recon, T_golden, gs);
    return 0;
}
