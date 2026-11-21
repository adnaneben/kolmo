/* Trained profiles (kolmo train, -P) and --target: round trips, damaged profiles, damaged frames, wrong or missing
 * profiles, the direct small-input path against the general path, pinned plans on inputs they do not fit, target
 * parsing, calibration tables, chunk merging and concurrent use of one profile.
 * Built optimised and with sanitizers by `make check-profile` (part of `make check-rc`). */
#include "../src/kolmo.c"
#include <assert.h>

static int fails;
static double t_start;
static double tnow(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
#define SECTION(name) do { if (getenv("TP_TIMES")) fprintf(stderr, "%6.2f s  %s\n", tnow() - t_start, name); } while (0)
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint64_t rs = 88172645463325252ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)rs; }

/* samples: JSON messages, a CSV export, log lines */
static size_t gen_json(uint8_t *o, size_t cap, int i) {
    static const char *const EV[] = {"login", "logout", "view", "click", "purchase"};
    return (size_t)snprintf((char *)o, cap, "{\"id\":%d,\"user\":\"user%03u\",\"event\":\"%s\",\"value\":%u.%02u,\"region\":\"eu-west-%u\",\"ok\":true}\n",
                            i, rnd() % 200, EV[rnd() % 5], rnd() % 1000, rnd() % 100, rnd() % 3);
}
static size_t gen_csv(uint8_t *o, size_t cap, int rows, int cols) {
    size_t w = (size_t)snprintf((char *)o, cap, "id");
    for (int c = 1; c < cols; c++) w += (size_t)snprintf((char *)o + w, cap - w, ",col%d", c);
    w += (size_t)snprintf((char *)o + w, cap - w, "\n");
    for (int r = 0; r < rows && w + 256 < cap; r++) {
        w += (size_t)snprintf((char *)o + w, cap - w, "%d", r);
        for (int c = 1; c < cols; c++) w += (size_t)snprintf((char *)o + w, cap - w, ",%u.%u", (r * c) % 977 + rnd() % 3, rnd() % 10);
        o[w++] = '\n';
    }
    return w;
}

static uint8_t *train(const uint8_t *s, const size_t *sz, size_t n, kolmo_params p, int no_dict, const char *target, size_t *len) {
    kolmo_train_params tp = {p, 16u << 10, no_dict, target, 0};
    size_t cap = kolmo_profile_bound(&tp); uint8_t *o = malloc(cap);
    int64_t r = kolmo_profile_train(s, sz, n, &tp, o, cap);
    if (r < 0) { fprintf(stderr, "train: %s\n", kolmo_strerror(r)); free(o); return NULL; }
    *len = (size_t)r; return o;
}

/* re-seal a modified profile: recompute id and CRC */
static void reseal(uint8_t *b, size_t n) {
    put32(b + 8, (uint32_t)kolmo_xxh3(b + KP_HDR, n - 4 - KP_HDR, KP_ID_SEED));
    put32(b + n - 4, kolmo_crc32c(0, b, n - 4));
}

static void roundtrip(const uint8_t *s, size_t n, const kolmo_params *p, const kolmo_profile *P, const kolmo_profile *other, const char *what) {
    size_t cap = kolmo_bound(n, p) + 64; uint8_t *o = malloc(cap), *d = malloc(n + 1);
    int64_t z = kolmo_compress_profile(s, n, o, cap, p, P);
    CHECK(z > 0);
    if (z <= 0) { fprintf(stderr, "  %s n=%zu mode=%d: %lld\n", what, n, p->mode, (long long)z); free(o); free(d); return; }
    CHECK(kolmo_original_size(o, (size_t)z) == (int64_t)n);
    int64_t u = kolmo_decompress_profile(o, (size_t)z, d, n, 2, P);
    CHECK(u == (int64_t)n && !memcmp(d, s, n));
    if (u != (int64_t)n || memcmp(d, s, n)) fprintf(stderr, "  %s n=%zu mode=%d: decode %lld\n", what, n, p->mode, (long long)u);
    uint32_t id = 0;
    CHECK(kolmo_frame_profile_id(o, (size_t)z, &id) == 1 && id == kolmo_profile_id(P));
    CHECK(kolmo_decompress(o, (size_t)z, d, n, 1) == KOLMO_ERR_PROFILE);                 /* no profile */
    CHECK(kolmo_decompress_profile(o, (size_t)z, d, n, 1, NULL) == KOLMO_ERR_PROFILE);
    if (other) CHECK(kolmo_decompress_profile(o, (size_t)z, d, n, 1, other) == KOLMO_ERR_PROFILE);   /* another one */
    /* damaged frame: every byte of small frames, a sample of large ones; never wrong data reported as success */
    /* context mixing allocates its model per call: a few positions are enough there */
    size_t step = p->mode == KOLMO_ULTRA ? (size_t)z / 8 + 1 : z <= 4096 ? 1 : (size_t)z / 150 + 1;
    for (size_t i = 0; i < (size_t)z; i += step) {
        uint8_t keep = o[i]; o[i] ^= (uint8_t)(1u << (rnd() & 7));
        int64_t v = kolmo_decompress_profile(o, (size_t)z, d, n, 1, P);
        CHECK(v < 0 || (v == (int64_t)n && !memcmp(d, s, n)));
        o[i] = keep;
    }
    for (size_t t = 0; t < (size_t)z; t += p->mode == KOLMO_ULTRA ? (size_t)z / 4 + 1 : z <= 512 ? 1 : (size_t)z / 60 + 1) {   /* truncated */
        int64_t v = kolmo_decompress_profile(o, t, d, n, 1, P);
        CHECK(v < 0 || (v == (int64_t)n && !memcmp(d, s, n)));
    }
    free(o); free(d);
}

typedef struct { const uint8_t *s; size_t n; const kolmo_profile *P; int level; int ok; } conc_t;
static void *conc_worker(void *a) {
    conc_t *c = a; kolmo_params p = {c->level >= 10 ? KOLMO_HC : KOLMO_FAST, c->level, 1, 0, 1};
    size_t cap = kolmo_bound(c->n, &p); uint8_t *o = malloc(cap), *d = malloc(c->n);
    c->ok = 1;
    for (int k = 0; k < 20; k++) {
        size_t m = 50 + (size_t)k * 97 % (c->n - 50);
        int64_t z = kolmo_compress_profile(c->s, m, o, cap, &p, c->P);
        if (z <= 0 || kolmo_decompress_profile(o, (size_t)z, d, m, 1, c->P) != (int64_t)m || memcmp(d, c->s, m)) c->ok = 0;
    }
    free(o); free(d); return NULL;
}

int main(void) {
    t_start = tnow();
    /* --- samples */
    enum { NJ = 300 };
    uint8_t *js = malloc(NJ * 256); size_t jsz[NJ], jo = 0;
    for (int i = 0; i < NJ; i++) { jsz[i] = gen_json(js + jo, 256, i); jo += jsz[i]; }
    kolmo_params fast = {KOLMO_FAST, 0, 0, 0, 1};
    size_t pl1, pl2, pl3;
    uint8_t *p1 = train(js, jsz, NJ, fast, 0, NULL, &pl1);
    uint8_t *p1b = train(js, jsz, NJ, fast, 0, NULL, &pl2);
    CHECK(p1 && p1b && pl1 == pl2 && !memcmp(p1, p1b, pl1));             /* training is deterministic */
    free(p1b);
    kolmo_profile *P1 = NULL, *P2 = NULL, *Pn = NULL;
    CHECK(kolmo_profile_load(p1, pl1, &P1) == 0 && P1 && P1->zdictlen && P1->rdictlen);
    size_t nc = 0; uint8_t *csv = malloc(1 << 20); size_t csz[6];
    for (int i = 0; i < 6; i++) { csz[i] = gen_csv(csv + nc, (1u << 20) - nc, 400 + i * 50, 8); nc += csz[i]; }
    uint8_t *p2 = train(csv, csz, 6, fast, 0, NULL, &pl3);
    CHECK(p2 && kolmo_profile_load(p2, pl3, &P2) == 0 && P2->npin >= 1);
    size_t pln; uint8_t *pn = train(js, jsz, NJ, fast, 1, NULL, &pln);
    CHECK(pn && kolmo_profile_load(pn, pln, &Pn) == 0 && !Pn->zdictlen && !Pn->rdictlen);
    char desc[8192]; CHECK(kolmo_profile_describe(P2, desc, sizeof desc) > 0 && strstr(desc, "pinned plans"));

    SECTION("damaged profiles");
    /* --- damaged profiles: every bit flip and every truncation fails to load */
    for (size_t i = 0; i < pl1; i += pl1 > 20000 ? 7 : 1) {
        p1[i] ^= (uint8_t)(1u << (i & 7)); kolmo_profile *X = NULL;
        CHECK(kolmo_profile_load(p1, pl1, &X) == KOLMO_ERR_PROFILE && !X);
        p1[i] ^= (uint8_t)(1u << (i & 7));
    }
    for (size_t t = 0; t < pl1; t += t < 64 ? 1 : 997) { kolmo_profile *X = NULL; CHECK(kolmo_profile_load(p1, t, &X) == KOLMO_ERR_PROFILE && !X); }
    {   /* well-sealed but invalid: bad version, unknown critical section, repeated section, bad PARM, bad PLAN */
        uint8_t *b = malloc(pl3 + 64); kolmo_profile *X = NULL;
        memcpy(b, p2, pl3); b[4] = 2; reseal(b, pl3); CHECK(kolmo_profile_load(b, pl3, &X) == KOLMO_ERR_PROFILE);
        /* append sections: build after the last section */
        static const uint8_t crit[12] = {'C', 'M', 'P', 'R', 1, 0, 0, 0, 0, 0, 0, 0}, soft[12] = {'X', 'T', 'R', 'A', 0, 0, 0, 0, 0, 0, 0, 0};
        static const uint8_t parm[20] = {'P', 'A', 'R', 'M', 0, 0, 0, 0, 8, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0};
        const uint8_t *add[3] = {crit, soft, parm}; size_t al[3] = {12, 12, 20}; int expect[3] = {KOLMO_ERR_PROFILE, 0, KOLMO_ERR_PROFILE};
        for (int k = 0; k < 3; k++) {
            memcpy(b, p2, pl3 - 4); memcpy(b + pl3 - 4, add[k], al[k]); size_t m = pl3 + al[k];
            put32(b + 12, get32(p2 + 12) + 1); reseal(b, m); X = NULL;
            int r = kolmo_profile_load(b, m, &X);
            CHECK(r == expect[k]);
            kolmo_profile_free(X);
        }
        /* a pinned plan with an impossible plan type (the first pin's t field) */
        memcpy(b, p2, pl3);
        size_t at = KP_HDR; int found = 0;
        for (uint32_t k = 0; k < get32(p2 + 12); k++) {
            uint32_t len = get32(b + at + 8);
            if (!memcmp(b + at, "PLAN", 4)) { size_t q = at + 12 + 4; q += 1 + b[q] + 4; put32(b + q, 99); found = 1; break; }
            at += 12 + len;
        }
        CHECK(found);
        reseal(b, pl3); X = NULL; CHECK(kolmo_profile_load(b, pl3, &X) == KOLMO_ERR_PROFILE);
        free(b);
    }

    SECTION("round trips");
    /* --- round trips: sizes around every threshold (direct path, raw zstd block, dictionary limit, compact frame) */
    size_t sizes[] = {0, 1, 2, 63, 100, 1000, 4095, 4096, 5000, 65535, 65536, 65537, 300000, (1u << 20) + 7};
    uint8_t *big = malloc((2u << 20)); size_t bw = 0;
    while (bw + 300 < (2u << 20)) bw += gen_json(big + bw, 300, (int)bw);
    kolmo_mode modes[] = {KOLMO_RT, KOLMO_FAST, KOLMO_ECO, KOLMO_HC, KOLMO_MAX};
    for (size_t m = 0; m < sizeof modes / sizeof modes[0]; m++)
        for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
            if (modes[m] == KOLMO_MAX && sizes[k] > 70000) continue;
            kolmo_params p = {modes[m], 0, 0, 0, modes[m] == KOLMO_RT ? 3 : 1};
            roundtrip(big, sizes[k], &p, P1, P2, "json");
            roundtrip(big, sizes[k], &p, Pn, P1, "json, no dictionary");
        }
    { kolmo_params p = {KOLMO_ULTRA, 1, 0, 0, 1}; roundtrip(big, 3000, &p, P1, P2, "ultra"); roundtrip(big, 20000, &p, P1, NULL, "ultra"); }
    { kolmo_params p = {KOLMO_FAST, 0, 0, 0, 1}; roundtrip(csv, nc, &p, P2, P1, "csv"); }
    { kolmo_params p = {KOLMO_FAST, 0, 0, 0, 1}; for (int i = 0; i < 4096; i++) big[(1u << 20) + i] = (uint8_t)rnd(); roundtrip(big + (1u << 20), 4096, &p, P1, NULL, "random"); }

    SECTION("direct path");
    /* --- the direct path for inputs under 4 KiB writes what the general path writes */
    for (size_t m = 0; m < 4; m++)
        for (size_t n = 0; n < 4096; n += n < 64 ? 1 : 61) {
            kolmo_params p = {modes[m], 0, 1, 0, modes[m] == KOLMO_RT ? 3 : 1};
            size_t cap = kolmo_bound(n, &p) + 64; uint8_t *a = malloc(cap), *b = malloc(cap);
            int64_t za = kc_small_compress(big, n, a, cap, &p, P1);
            int64_t zb = compress_impl_p(big, -1, n, b, -1, cap, &p, NULL, 0, 0, P1);
            if (zb > 0) zb = (int64_t)kc_pack(b, (size_t)zb, big, n);
            CHECK(za > 0 && za == zb && !memcmp(a, b, (size_t)za));
            free(a); free(b);
        }

    SECTION("other inputs");
    /* --- frames without a profile still decode with one; files of other kinds under a CSV profile (pins that do not fit) */
    {
        kolmo_params p = {KOLMO_FAST, 0, 0, 0, 1}; size_t cap = kolmo_bound(nc, &p); uint8_t *o = malloc(cap), *d = malloc(nc);
        int64_t z = kolmo_compress(csv, nc, o, cap, &p);
        CHECK(z > 0 && kolmo_decompress_profile(o, (size_t)z, d, nc, 1, P2) == (int64_t)nc && !memcmp(d, csv, nc));
        CHECK(kolmo_frame_profile_id(o, (size_t)z, NULL) == 0);
        free(o); free(d);
        size_t m2 = gen_csv(big, 1u << 20, 3000, 5);              /* same header shape, other column count */
        roundtrip(big, m2, &p, P2, NULL, "csv, other columns");
        memcpy(big, csv, 200); roundtrip(big, 500000, &p, P2, NULL, "csv header, then other data");
        for (size_t i = 0; i < 300000; i++) big[i] = (uint8_t)(rnd() % 7 + 'a');
        memcpy(big, csv, 64); roundtrip(big, 300000, &p, P2, NULL, "csv header, then letters");
    }

    SECTION("concurrency");
    /* --- concurrent use of one profile, with zstd levels prepared lazily */
    {
        pthread_t th[6]; conc_t c[6];
        for (int i = 0; i < 6; i++) { c[i] = (conc_t){big, 4000, P1, i < 3 ? 1 + i : 17 + i % 3, 0}; pthread_create(&th[i], NULL, conc_worker, &c[i]); }
        for (int i = 0; i < 6; i++) { pthread_join(th[i], NULL); CHECK(c[i].ok); }
    }

    SECTION("targets");
    /* --- targets */
    {
        const char *good[] = {"size", "decode>=1GB/s", "encode>=50MB/s", "decode>=800Mbit", "cost:size+enc*1e6", "cost:cloud",
                              "cost:cloud=12", "cost:link=1Gbit", "cost:(size/1e9)*0.023 + (enc + 3.3*dec)*0.054/3600",
                              "size,decode>=500MB/s", "cost:cloud,decode>=2GiB/s", "cost:-(-size)", "cost:1.5e3*size", "decode>=1000GB/s"};
        const char *bad[] = {"", "fast", "decode>1GB/s", "decode>=", "decode>=1PB/s", "decode>=-1GB/s", "cost:", "cost:size+",
                             "cost:siz", "cost:(size", "cost:size)", "cost:cloud=0", "cost:link=0", "size,size", "cost:size,cost:size",
                             "cost:sizes", "cost:enc dec"};
        for (size_t i = 0; i < sizeof good / sizeof good[0]; i++) { kolmo_target *t = NULL; int r = kolmo_target_parse(good[i], &t); CHECK(r == 0 && t); if (r) fprintf(stderr, "  '%s'\n", good[i]); kolmo_target_free(t); }
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) { kolmo_target *t = NULL; int r = kolmo_target_parse(bad[i], &t); CHECK(r == KOLMO_ERR_TARGET && !t); if (!r) fprintf(stderr, "  '%s' accepted\n", bad[i]); kolmo_target_free(t); }
        kolmo_target *t = NULL; CHECK(kolmo_target_parse("cost:size*2 + enc*3 + dec*5 - n/n", &t) == 0);
        CHECK(fabs(kt_eval(t, 10, 7, 1, 1) - (20 + 3 + 5 - 1)) < 1e-9);
        const char *tab_ok = "kolmo-calibration 1\n# comment\nspeed binary 9000 9000\nratio text 1.5 2.5 # x\n";
        const char *tab_bad[] = {"speed binary 1 1\n", "kolmo-calibration 2\n", "kolmo-calibration 1\nspeed binary 0 1\n",
                                 "kolmo-calibration 1\nturbo text 1 1\n", "kolmo-calibration 1\nspeed prose 1 1\n", "kolmo-calibration 1\nspeed text 1\n",
                                 "kolmo-calibration 1\nspeed text nan 1\n"};
        CHECK(kolmo_target_set_calibration(t, tab_ok, strlen(tab_ok)) == 0 && t->enc[0][5] == 9000e6 && t->dec[6][0] == 2.5e6);
        for (size_t i = 0; i < sizeof tab_bad / sizeof tab_bad[0]; i++) CHECK(kolmo_target_set_calibration(t, tab_bad[i], strlen(tab_bad[i])) == KOLMO_ERR_TARGET);
        kolmo_target_free(t);

        /* mixed input in 32 KiB chunks: text and binary chunks get different settings; speed (rt) chunks checksum
         * their stored bytes, so merging recomputes their checksums */
        size_t n = 1u << 19; uint8_t *mix = malloc(n + 256), *o, *d = malloc(n);
        for (size_t i = 0; i < n; i += 32768) {
            if ((i >> 15) % 2) for (size_t j = 0; j < 32768; j++) mix[i + j] = (uint8_t)(j % 251 == 0 ? rnd() : (j * 7) >> 8);
            else { size_t w = 0; while (w < 32768) { size_t l = gen_json(mix + i + w, 256, (int)w); w += l; } }
        }
        /* forced table: binary chunks only meet the floor with speed, text chunks only with balanced */
        char tab_force[2048]; size_t tl = (size_t)snprintf(tab_force, sizeof tab_force, "kolmo-calibration 1\n");
        for (int c = 0; c < KT_NCLS; c++)
            for (int st = 0; st < KT_NSET; st++)
                tl += (size_t)snprintf(tab_force + tl, sizeof tab_force - tl, "%s %s 100 %s\n", KT_SET[st].name, KT_CLS[c],
                                       st == (c >= 4 ? 0 : 2) ? "50000" : "1");
        const char *specs[] = {"decode>=10GB/s", "size", "cost:size + enc*1e6", "cost:cloud", "decode>=1000GB/s"};
        for (size_t k = 0; k < 4; k++) {
            kolmo_target *tt = NULL; CHECK(kolmo_target_parse(specs[k], &tt) == 0);
            if (k == 0) CHECK(kolmo_target_set_calibration(tt, tab_force, strlen(tab_force)) == 0);
            kolmo_params p = {KOLMO_FAST, 0, 2, 32u << 10, 1}; kolmo_target_report rep;
            size_t cap = kolmo_bound(n, &p) + (1u << 16); o = malloc(cap);
            for (int withp = 0; withp < 2; withp++) {
                if (k == 1 && withp) continue;                    /* size: -x on every chunk, once is enough */
                int64_t z = kolmo_compress_target(mix, n, o, cap, &p, tt, withp ? P1 : NULL, &rep);
                CHECK(z > 0);
                uint32_t sum = 0; for (int s = 0; s < 16; s++) sum += rep.per_setting[s];
                CHECK(rep.chunks >= 16 && sum == rep.chunks);
                int64_t u = withp ? kolmo_decompress_profile(o, (size_t)z, d, n, 3, P1) : kolmo_decompress(o, (size_t)z, d, n, 3);
                CHECK(u == (int64_t)n && !memcmp(d, mix, n));
                if (k == 0 && !withp) CHECK(rep.per_setting[0] >= 4 && rep.per_setting[0] + rep.per_setting[2] == rep.chunks);
                for (size_t i = 0; i < (size_t)z && !withp; i += 997) {   /* damaged merged frame */
                    o[i] ^= 0x10; int64_t v = kolmo_decompress(o, (size_t)z, d, n, 2); CHECK(v < 0 || (v == (int64_t)n && !memcmp(d, mix, n))); o[i] ^= 0x10;
                }
            }
            free(o); kolmo_target_free(tt);
        }
        /* a floor nothing meets: the fastest decoder, still a valid frame */
        kolmo_target *tt = NULL; CHECK(kolmo_target_parse(specs[4], &tt) == 0);
        kolmo_params p = {KOLMO_FAST, 0, 1, 0, 1}; size_t cap = kolmo_bound(n, &p) + 4096; o = malloc(cap);
        int64_t z = kolmo_compress_target(mix, n, o, cap, &p, tt, NULL, NULL);
        CHECK(z > 0 && kolmo_decompress(o, (size_t)z, d, n, 1) == (int64_t)n && !memcmp(d, mix, n));
        CHECK(kolmo_compress_target(mix, n, o, 100, &p, tt, NULL, NULL) == KOLMO_ERR_DSTSIZE);
        free(o); kolmo_target_free(tt);
        /* empty input */
        CHECK(kolmo_target_parse("size", &tt) == 0);
        o = malloc(256); z = kolmo_compress_target(mix, 0, o, 256, &p, tt, NULL, NULL);
        CHECK(z > 0 && kolmo_decompress(o, (size_t)z, d, 0, 1) == 0);
        free(o); kolmo_target_free(tt);
        free(mix); free(d);
    }

    SECTION("train target");
    /* --- training with a target chooses a setting and records it */
    {
        size_t l; uint8_t *pt = train(js, jsz, NJ, fast, 0, "cost:size*1e-6 + enc", &l);
        kolmo_profile *X = NULL;
        CHECK(pt && kolmo_profile_load(pt, l, &X) == 0 && X->has_parm && X->info && strstr(X->info, "chosen"));
        kolmo_profile_free(X); free(pt);
        pt = train(js, jsz, 3, fast, 0, "size", &l);           /* fewer than five samples: nothing to measure on */
        CHECK(!pt);
        CHECK(!train(js, jsz, NJ, fast, 0, "decode>=1000GB/s", &l));
    }

    SECTION("calibration");
    /* --- calibration on built-in data (short) produces a table the parser accepts */
    if (!getenv("KOLMO_SKIP_CALIBRATE")) {
        char *tab = malloc(1 << 16); int64_t r = kf_calibrate(NULL, NULL, 0, 0.001, tab, 1 << 16, 16u << 10);
        CHECK(r > 0);
        kolmo_target *t = NULL; CHECK(kolmo_target_parse("size", &t) == 0);
        CHECK(r > 0 && kolmo_target_set_calibration(t, tab, (size_t)r) == 0);
        kolmo_target_free(t); free(tab);
    }

    SECTION("end");
    kolmo_profile_free(P1); kolmo_profile_free(P2); kolmo_profile_free(Pn);
    free(p1); free(p2); free(pn); free(js); free(csv); free(big);
    if (fails) { fprintf(stderr, "test_profile: %d failure(s)\n", fails); return 1; }
    printf("test_profile: all checks passed\n");
    return 0;
}
