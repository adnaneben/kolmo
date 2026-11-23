/* The level table (kolmo_params_for_level, docs/LEVELS.md): every level 1..9 and ultra round-trips on several kinds
 * of input, gives the same bytes on repeated calls and with 1 and 4 threads, keeps the caller's threads, block and
 * memory limit, rejects other levels, writes no table block 0.0.2 cannot read (a field over 65535 bytes), and level 1 is exactly the 0.0.1 CLI's "-m speed" (rt, transforms 3). */
#include "kolmo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)rs; }

static int64_t enc(const uint8_t *s, size_t n, const kolmo_params *p, uint8_t **out) {
    size_t cap = kolmo_bound(n, p); *out = malloc(cap);
    return *out ? kolmo_compress(s, n, *out, cap, p) : KOLMO_ERR_ALLOC;
}

int main(void) {
    /* inputs: text, CSV, integers, random bytes, empty and one byte */
    size_t tn = 300000; uint8_t *text = malloc(tn);
    static const char *w[] = {"level ", "kolmo ", "the ", "scale ", "fast ", "small ", "block\n", "of "};
    for (size_t i = 0; i < tn;) { const char *x = w[rnd() % 8]; for (; *x && i < tn; x++) text[i++] = (uint8_t)*x; }
    size_t cn = 0, ccap = 400000; uint8_t *csv = malloc(ccap);
    cn += (size_t)snprintf((char *)csv, ccap, "id,x,y\n");
    for (int i = 0; cn + 40 < ccap; i++) cn += (size_t)snprintf((char *)csv + cn, ccap - cn, "%d,%u,%u.%02u\n", i, rnd() % 1000, rnd() % 100, rnd() % 100);
    size_t in_ = 200000; uint8_t *ints = malloc(in_);
    for (size_t i = 0; i < in_ / 4; i++) { uint32_t v = 1000 + (uint32_t)i * 3 + rnd() % 7; memcpy(ints + 4 * i, &v, 4); }
    size_t rn = 50000; uint8_t *rb = malloc(rn); for (size_t i = 0; i < rn; i++) rb[i] = (uint8_t)rnd();
    /* aligned CSV (every row 3 fields) with one 70,000-byte field, in a text column and in a numeric column: 0.0.2 wrote
     * table blocks for these at levels 3-9 that no decoder of 0.0.2 could read (CHANGELOG, Unreleased) */
    uint8_t *lcsv[2]; size_t lcn[2];
    for (int v = 0; v < 2; v++) {
        size_t cap = 200000, m = 0; uint8_t *d = lcsv[v] = malloc(cap);
        for (int i = 0; i < 3000; i++) {
            m += (size_t)snprintf((char *)d + m, cap - m, "%d,%s,", i, w[rnd() % 6]);
            if (i == 2500 - 700 * v) { for (int t = 0; t < 70000; t++) d[m++] = (uint8_t)"kolmo field "[t % 12]; }
            else m += (size_t)snprintf((char *)d + m, cap - m, "%u.%02u", rnd() % 100, rnd() % 100);
            d[m++] = '\n';
        }
        lcn[v] = m;
    }
    struct { const char *name; const uint8_t *p; size_t n; } in[] = {
        {"text", text, tn}, {"csv", csv, cn}, {"ints", ints, in_}, {"random", rb, rn}, {"empty", (const uint8_t *)"", 0},
        {"one", (const uint8_t *)"x", 1}, {"csv-long-field", lcsv[0], lcn[0]}, {"csv-long-number-field", lcsv[1], lcn[1]}};

    /* the table itself */
    kolmo_params p; memset(&p, 0, sizeof p);
    CHECK(kolmo_params_for_level(0, &p) == KOLMO_ERR_PARAM, "level 0 accepted");
    CHECK(kolmo_params_for_level(11, &p) == KOLMO_ERR_PARAM, "level 11 accepted");
    CHECK(kolmo_params_for_level(-1, &p) == KOLMO_ERR_PARAM, "level -1 accepted");
    CHECK(kolmo_params_for_level(3, NULL) == KOLMO_ERR_PARAM, "NULL params accepted");
    p.threads = 7; p.block = 1u << 20; p.mem_limit = 123456789; p.mode = KOLMO_MAX;
    CHECK(kolmo_params_for_level(0, &p) && p.mode == KOLMO_MAX, "a rejected level changed the params");
    CHECK(!kolmo_params_for_level(KOLMO_LEVEL_FASTEST, &p), "level 1 rejected");
    CHECK(p.mode == KOLMO_RT && p.level == 0 && p.transforms == 3, "level 1 is not the 0.0.1 '-m speed' (rt, transforms 3)");
    CHECK(p.threads == 7 && p.block == (1u << 20) && p.mem_limit == 123456789, "level changed threads, block or mem_limit");
    CHECK(!kolmo_params_for_level(KOLMO_LEVEL_DEFAULT, &p) && p.mode == KOLMO_FAST && p.level == 0 && p.transforms == 1,
          "the default level is not 0.0.1 balanced");
    CHECK(KOLMO_LEVEL_DEFAULT == KOLMO_LEVEL_LIGHT && KOLMO_LEVEL_MAX_RATIO == KOLMO_LEVEL_MAX && KOLMO_LEVEL_ULTRA == 10,
          "level constants");
    CHECK(!kolmo_params_for_level(KOLMO_LEVEL_ULTRA, &p) && p.mode == KOLMO_ULTRA, "ultra is not context mixing");
    CHECK(strstr(kolmo_strerror(KOLMO_ERR_PARAM), "level") != NULL, "KOLMO_ERR_PARAM message");

    for (int lv = KOLMO_LEVEL_MIN; lv <= KOLMO_LEVEL_ULTRA; lv++) {
        for (size_t k = 0; k < sizeof in / sizeof in[0]; k++) {
            if (lv == KOLMO_LEVEL_ULTRA && in[k].n > 100000) continue;   /* keep the context-mixing tier quick */
            kolmo_params q = {0}; q.threads = 1;
            CHECK(!kolmo_params_for_level(lv, &q), "level %d rejected", lv);
            uint8_t *a = NULL, *b = NULL, *c = NULL;
            int64_t na = enc(in[k].p, in[k].n, &q, &a), nb = enc(in[k].p, in[k].n, &q, &b);
            q.threads = 4; int64_t nc = enc(in[k].p, in[k].n, &q, &c);
            CHECK(na >= 0, "level %d %s: compress: %s", lv, in[k].name, kolmo_strerror(na));
            if (na >= 0) {
                CHECK(na == nb && !memcmp(a, b, (size_t)na), "level %d %s: two calls differ", lv, in[k].name);
                CHECK(na == nc && !memcmp(a, c, (size_t)na), "level %d %s: 1 and 4 threads differ", lv, in[k].name);
                CHECK(na >= 5 && !memcmp(a, "CZN1\x0c", 5), "level %d %s: not container version 12", lv, in[k].name);
                uint8_t *o = malloc(in[k].n + 1);
                int64_t r = kolmo_decompress(a, (size_t)na, o, in[k].n, 2);
                CHECK(r == (int64_t)in[k].n && !memcmp(o, in[k].p, in[k].n), "level %d %s: round trip", lv, in[k].name);
                if (!strncmp(in[k].name, "csv-long", 8) && na >= 32) {   /* no table block (tflag 4): 0.0.2 reads it */
                    uint32_t nbk; memcpy(&nbk, a + 20, 4);
                    for (uint32_t i = 0; i < nbk && 32 + 16 * (size_t)i + 14 <= (size_t)na; i++)
                        CHECK(a[32 + 16 * i + 13] != 4, "level %d %s: block %u is a table block with a field over 65535 bytes",
                              lv, in[k].name, i);
                }
                free(o);
            }
            free(a); free(b); free(c);
        }
    }
    free(lcsv[0]); free(lcsv[1]);
    /* each level differs from its neighbours somewhere (the table has ten distinct rows) */
    int distinct = 0;
    for (int lv = KOLMO_LEVEL_MIN; lv < KOLMO_LEVEL_ULTRA; lv++) {
        kolmo_params x = {0}, y = {0};
        kolmo_params_for_level(lv, &x); kolmo_params_for_level(lv + 1, &y);
        distinct += x.mode != y.mode || x.level != y.level || x.transforms != y.transforms;
    }
    CHECK(distinct == KOLMO_LEVEL_ULTRA - 1, "neighbouring levels with the same settings");
    free(text); free(csv); free(ints); free(rb);
    printf("test_levels: %s\n", fails ? "FAILED" : "OK (levels 1-9 and ultra: round trips, deterministic)");
    return fails != 0;
}
