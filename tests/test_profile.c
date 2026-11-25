/* Trained profiles (kolmo train, -P) and --target. Profiles were built on zstd and Brotli dictionaries and are not
 * available in this build (container version 12, Kolmo's own coders only): every profile entry point must report
 * KOLMO_ERR_UNSUPPORTED (or its documented neutral value), and frames of earlier versions are KOLMO_ERR_LEGACY.
 * --target keeps its tests: parsing, calibration tables, chunk merging, floors, empty input.
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

/* A binary chunk misses the floor while a text chunk meets it with balanced.
 * Verify selection, the pre-merge byte total, and the selected formula for both. */
static void target_floor_reports(void) {
    uint8_t input[8192] = {0}, frame[16384], restored[8192];
    memset(input + 4096, 'a', 4096);
    const char *table = "kolmo-calibration 1\nspeed binary 1 90000\nbalanced text 2000000 2000000\n";
    const char *specs[] = {"cost:size*2+enc*3+dec*5,encode>=999999MB/s",
                           "cost:size*2+enc*3+dec*5,decode>=999999MB/s"};
    kolmo_params p = {KOLMO_FAST, 0, 2, 4096, 1};
    uint8_t *chunk = NULL;
    int64_t binary_bytes = kt_compress_setting(input, 4096, 0, 1, &chunk);
    CHECK(binary_bytes > 0); free(chunk); chunk = NULL;
    int64_t text_bytes = kt_compress_setting(input + 4096, 4096, 2, 1, &chunk);
    CHECK(text_bytes > 0); free(chunk);
    for (size_t i = 0; i < sizeof specs / sizeof specs[0]; i++) {
        kolmo_target *t = NULL;
        CHECK(kolmo_target_parse(specs[i], &t) == 0);
        CHECK(kolmo_target_set_calibration(t, table, strlen(table)) == 0);
        for (int chunks = 1; chunks <= 2; chunks++) {
            size_t n = (size_t)chunks * 4096;
            kolmo_target_report r; uint32_t fallbacks = 0;
            int64_t z = kolmo_compress_target_detail(input, n, frame, sizeof frame, &p, t, &r, &fallbacks);
            uint64_t expected = (uint64_t)binary_bytes + (chunks == 2 ? (uint64_t)text_bytes : 0);
            CHECK(fallbacks == 1 && r.chunks == (uint32_t)chunks);
            CHECK(r.per_setting[0] == 1 && r.per_setting[2] == (uint32_t)chunks - 1);
            CHECK(r.predicted_size == expected && z == (int64_t)(expected - (chunks - 1) * HDR));
            CHECK(fabs(r.predicted_cost - (2 * expected + 3 * r.predicted_enc_s + 5 * r.predicted_dec_s)) < 1e-8);
            CHECK(z > 0 && kolmo_decompress(frame, (size_t)z, restored, n, 2) == (int64_t)n && !memcmp(restored, input, n));
        }
        kolmo_target_free(t);
    }
}

int main(void) {
    t_start = tnow();
    SECTION("profiles: not available");
    {
        enum { NJ = 64 };
        uint8_t *js = malloc(NJ * 256); size_t jsz[NJ], jo = 0;
        for (int i = 0; i < NJ; i++) { jsz[i] = gen_json(js + jo, 256, i); jo += jsz[i]; }
        kolmo_train_params tp = {{KOLMO_FAST, 0, 0, 0, 1}, 16u << 10, 0, NULL, 0};
        uint8_t out[4096];
        CHECK(kolmo_profile_bound(&tp) == 0);
        CHECK(kolmo_profile_train(js, jsz, NJ, &tp, out, sizeof out) == KOLMO_ERR_UNSUPPORTED);
        kolmo_profile *P = (kolmo_profile *)(uintptr_t)1;
        CHECK(kolmo_profile_load(out, sizeof out, &P) == KOLMO_ERR_UNSUPPORTED && P == NULL);
        kolmo_profile_free(NULL);
        CHECK(kolmo_profile_id(NULL) == 0);
        kolmo_params q = {KOLMO_ULTRA, 7, 3, 9, 0}; kolmo_profile_params(NULL, &q);
        CHECK(q.mode == KOLMO_FAST && q.level == 0 && q.transforms == 1);
        char desc[16] = "x"; CHECK(kolmo_profile_describe(NULL, desc, sizeof desc) == 0 && desc[0] == 0);
        kolmo_params fast = {KOLMO_FAST, 0, 1, 0, 1};
        size_t cap = kolmo_bound(jo, &fast); uint8_t *f = malloc(cap), *b = malloc(jo + 1);
        CHECK(kolmo_compress_profile(js, jo, f, cap, &fast, NULL) == KOLMO_ERR_UNSUPPORTED);
        int64_t z = kolmo_compress(js, jo, f, cap, &fast);
        CHECK(z > 0 && f[4] == 12);
        CHECK(kolmo_decompress_profile(f, (size_t)z, b, jo, 1, NULL) == KOLMO_ERR_UNSUPPORTED);
        uint32_t id = 7; CHECK(kolmo_frame_profile_id(f, (size_t)z, &id) == 0 && id == 0);   /* no frame of this build uses one */
        f[4] = 11; CHECK(kolmo_frame_profile_id(f, (size_t)z, &id) == KOLMO_ERR_LEGACY);
        f[4] = 12;
        static const uint8_t czc1[16] = {'C', 'Z', 'C', '1'};                                /* compact profile frame (rc3) */
        CHECK(kolmo_frame_profile_id(czc1, sizeof czc1, &id) == KOLMO_ERR_LEGACY);
        CHECK(kolmo_decompress(czc1, sizeof czc1, b, jo, 1) == KOLMO_ERR_LEGACY);
        kolmo_target *t = NULL; CHECK(kolmo_target_parse("size", &t) == 0);
        CHECK(kolmo_compress_target(js, jo, f, cap, &fast, t, (const kolmo_profile *)f, NULL) == KOLMO_ERR_UNSUPPORTED);
        kolmo_target_free(t);
        /* dictionaries: the same */
        CHECK(kolmo_train_dict(js, jsz, NJ, out, sizeof out) == KOLMO_ERR_UNSUPPORTED);
        CHECK(kolmo_compress_dict(js, jo, f, cap, &fast, out, 1024) == KOLMO_ERR_UNSUPPORTED);
        CHECK(kolmo_decompress_dict(f, (size_t)z, b, jo, 1, out, 1024) == KOLMO_ERR_UNSUPPORTED);
        CHECK(kolmo_decompress_dict(f, (size_t)z, b, jo, 1, NULL, 0) == (int64_t)jo && !memcmp(b, js, jo));
        CHECK(!strcmp(kolmo_strerror(KOLMO_ERR_UNSUPPORTED), "not available in this build: dictionaries and trained profiles need library codecs"));
        free(f); free(b); free(js);
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
            for (int withp = 0; withp < 1; withp++) {             /* (profiles: not available in this build) */
                int64_t z = kolmo_compress_target(mix, n, o, cap, &p, tt, NULL, &rep);
                CHECK(z > 0 && o[4] == 12);
                uint32_t sum = 0; for (int s = 0; s < 16; s++) sum += rep.per_setting[s];
                CHECK(rep.chunks >= 16 && sum == rep.chunks);
                int64_t u = kolmo_decompress(o, (size_t)z, d, n, 3);
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
        kolmo_target_report fallback_report;
        int64_t z = kolmo_compress_target(mix, n, o, cap, &p, tt, NULL, &fallback_report);
        CHECK(z > 0 && fallback_report.predicted_size == (uint64_t)z && fallback_report.predicted_cost == (double)z);
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

    target_floor_reports();

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
    if (fails) { fprintf(stderr, "test_profile: %d failure(s)\n", fails); return 1; }
    printf("test_profile: all checks passed\n");
    return 0;
}
