/* The memory model (docs/MEMORY-MODEL.md): the frame's decode declaration, the governor and its hysteresis, a budget
 * lowered in the middle of a run (the job count falls and recovers), forced budgets that must not change a byte,
 * forged frames refused before allocation, explicit caps and floors. Includes the library to reach its internals. */
#include "../src/kolmo.c"
#include <assert.h>
#include <stdio.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
static const uint64_t MiB = 1u << 20;

static uint8_t *text_input(size_t n, uint32_t seed) {
    uint8_t *b = malloc(n); size_t o = 0; uint32_t s = seed;
    static const char *w[] = {"memory", "changes", "speed", "never", "bytes", "kolmo", "block", "budget", "the", "a", "of", "frame"};
    while (o < n) {
        s = s * 1103515245u + 12345u; const char *x = w[(s >> 16) % 12]; size_t l = strlen(x);
        for (size_t i = 0; i < l && o < n; i++) b[o++] = (uint8_t)x[i];
        if (o < n) b[o++] = (s >> 8) % 11 ? ' ' : '\n';
    }
    return b;
}
static uint8_t *binary_input(size_t n, uint32_t seed) {
    uint8_t *b = malloc(n); uint32_t s = seed, v = 0;
    for (size_t i = 0; i < n; i++) { s = s * 1664525u + 1013904223u; v += (s >> 28) - 7; b[i] = (uint8_t)(i % 4 ? v : s >> 24); }
    return b;
}

static void declaration_coding(void) {
    uint64_t prev = 0;
    for (int v = 1; v < 256; v++) { uint64_t d = kmem_declared((uint8_t)v); CHECK(d > prev); prev = d; }
    for (uint64_t need = 1; need < ((uint64_t)1 << 50); need = need * 3 + 7) {
        uint8_t c = kmem_code(need); CHECK(kmem_declared(c) >= need);
        if (c > 1) CHECK(kmem_declared((uint8_t)(c - 1)) < need);
    }
    CHECK(kmem_declared(0) == 0);
    uint64_t v;
    CHECK(!kolmo_mem_parse("512M", &v) && v == 512 * MiB);
    CHECK(!kolmo_mem_parse("8G", &v) && v == ((uint64_t)8 << 30));
    CHECK(!kolmo_mem_parse("1.5GiB", &v) && v == (uint64_t)(1.5 * (1 << 30)));
    CHECK(!kolmo_mem_parse("auto", &v) && v == 0);
    CHECK(!kolmo_mem_parse("max", &v) && v == KOLMO_MEM_MAX);
    CHECK(!kolmo_mem_parse("50%", &v) && v > 0 && v <= kmem_phys() / 2 + 1);
    CHECK(kolmo_mem_parse("12Q", &v) && kolmo_mem_parse("", &v) && kolmo_mem_parse("150%", &v) && kolmo_mem_parse("-1G", &v));
    puts("declaration coding and sizes: ok");
}

/* hysteresis: a lower sample (or pressure) lowers the budget at once; a higher one is taken only after 1 s of calm */
static uint64_t fake_avail; static int fake_pressure;
static uint64_t fake_sample(int *pressure) { *pressure = fake_pressure; return fake_avail; }
static uint64_t budget_now(void) {
    pthread_mutex_lock(&kmem_mu); kmem_last_sample_ns = 0; uint64_t b = kmem_budget_locked(); pthread_mutex_unlock(&kmem_mu); return b;
}
static void hysteresis(void) {
    kmem_force_budget(0); kmem_sample_hook = fake_sample;
    fake_avail = 8000 * MiB; fake_pressure = 0;
    pthread_mutex_lock(&kmem_mu); kmem_budget_cur = 0; kmem_last_pressure_ns = 0; pthread_mutex_unlock(&kmem_mu);
    CHECK(budget_now() == 8000 * MiB);
    fake_avail = 1000 * MiB; CHECK(budget_now() == 1000 * MiB);              /* drops at once */
    fake_avail = 6000 * MiB; CHECK(budget_now() == 1000 * MiB);              /* not yet */
    struct timespec t = {0, 400000000}; nanosleep(&t, NULL);
    CHECK(budget_now() == 1000 * MiB);
    t.tv_nsec = 700000000; nanosleep(&t, NULL);
    CHECK(budget_now() == 6000 * MiB);                                        /* after 1 s of calm */
    kmem_reserve(300 * MiB, 0);                                               /* pressure: no room beyond what is held */
    fake_pressure = 1; CHECK(budget_now() == 300 * MiB);
    fake_pressure = 0; CHECK(budget_now() == 300 * MiB);
    CHECK(!kmem_try_reserve(10 * MiB, 0));                                    /* optional work waits */
    kmem_release(300 * MiB);
    t.tv_sec = 1; t.tv_nsec = 100000000; nanosleep(&t, NULL);
    CHECK(budget_now() == 6000 * MiB);
    /* one job always runs, whatever its size */
    fake_avail = 1; CHECK(budget_now() == 1);
    kmem_reserve(4000 * MiB, 0); CHECK(!kmem_try_reserve(1, 0)); kmem_release(4000 * MiB);
    kmem_sample_hook = NULL; kmem_force_budget(0);
    pthread_mutex_lock(&kmem_mu); kmem_budget_cur = 0; pthread_mutex_unlock(&kmem_mu);
    puts("governor hysteresis, pressure and the one-job floor: ok");
}

/* a budget lowered in the middle of a run: the number of jobs in flight falls to one and recovers */
typedef struct { const uint8_t *in; size_t n; uint8_t *out; size_t cap; int64_t r; kolmo_params p; } run_arg;
static void *run_main(void *a) { run_arg *r = a; r->r = kolmo_compress(r->in, r->n, r->out, r->cap, &r->p); return NULL; }
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static void mid_run(void) {
    size_t n = 48 * MiB; uint8_t *in = text_input(n, 7);
    kolmo_params p = {KOLMO_HC, 0, 4, 1u << 20, 0, 0};     /* 48 blocks of 1 MiB, zstd 19, four threads */
    size_t cap = kolmo_bound(n, &p); uint8_t *ref = malloc(cap), *out = malloc(cap);
    kmem_force_budget(KMEM_UNLIMITED);
    int64_t rl = kolmo_compress(in, n, ref, cap, &p); CHECK(rl > 0);
    kmem_stats_reset_peak();
    run_arg ra = {in, n, out, cap, 0, p}; pthread_t th; CHECK(!pthread_create(&th, NULL, run_main, &ra));
    int jobs, hi1 = 0, hi_low = 0, hi2 = 0; double t0 = now_s();
    while (now_s() - t0 < 30) { kmem_stats(&jobs, NULL, NULL); if (jobs > hi1) hi1 = jobs; if (hi1 >= 3) break; }
    kmem_force_budget(1);                                   /* almost nothing: one job at a time */
    double t1 = now_s();
    while (now_s() - t1 < 10) { kmem_stats(&jobs, NULL, NULL); if (jobs <= 1) break; }   /* running jobs finish */
    double t2 = now_s(); int low_samples = 0;
    while (now_s() - t2 < 1.0) { kmem_stats(&jobs, NULL, NULL); if (jobs > hi_low) hi_low = jobs; low_samples++; }
    kmem_force_budget(KMEM_UNLIMITED);
    double t3 = now_s();
    while (now_s() - t3 < 30) { kmem_stats(&jobs, NULL, NULL); if (jobs > hi2) hi2 = jobs; if (hi2 >= 3) break; }
    pthread_join(th, NULL);
    printf("mid-run budget: jobs before %d, while low %d (%d samples), after %d\n", hi1, hi_low, low_samples, hi2);
    CHECK(hi1 >= 3 && hi_low == 1 && hi2 >= 3);
    CHECK(ra.r == rl && !memcmp(out, ref, (size_t)rl));     /* the same frame */
    kmem_force_budget(0);
    free(in); free(ref); free(out);
    puts("budget lowered mid-run: jobs fell to one and recovered, same frame: ok");
}

/* forced budgets never change a byte (every mode, 1 and 4 threads) */
static void forced_budgets(void) {
    struct { kolmo_mode m; int level; size_t n; } cases[] = {
        {KOLMO_RT, 0, 6 * MiB}, {KOLMO_FAST, 0, 6 * MiB}, {KOLMO_HC, 0, 3 * MiB}, {KOLMO_MAX, 0, 3 * MiB}, {KOLMO_ULTRA, 1, 300000}, {KOLMO_ULTRA, 0, 300000}};
    uint64_t budgets[] = {KMEM_UNLIMITED, 64 * MiB, 512 * MiB, (uint64_t)4 << 30, 1};
    for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++) for (int kind = 0; kind < 2; kind++) {
        size_t n = cases[c].n; uint8_t *in = kind ? binary_input(n, 3) : text_input(n, 5);
        uint8_t *ref = NULL; int64_t rl = 0;
        for (size_t b = 0; b < sizeof budgets / sizeof budgets[0]; b++) for (int t = 1; t <= 4; t += 3) {
            kolmo_params p = {cases[c].m, cases[c].level, t, cases[c].m == KOLMO_ULTRA ? 0 : (512u << 10), 1, 0};
            size_t cap = kolmo_bound(n, &p); uint8_t *out = malloc(cap);
            kmem_force_budget(budgets[b]);
            int64_t r = kolmo_compress(in, n, out, cap, &p); CHECK(r > 0);
            if (!ref) { ref = out; rl = r; } else { CHECK(r == rl && !memcmp(out, ref, (size_t)r)); free(out); }
        }
        uint8_t *back = malloc(n); kmem_force_budget(64 * MiB);
        CHECK(kolmo_decompress(ref, (size_t)rl, back, n, 4) == (int64_t)n && !memcmp(back, in, n));
        free(back); free(ref); free(in);
    }
    kmem_force_budget(0);
    puts("forced budgets 1 B, 64 MiB, 512 MiB, 4 GiB, unlimited: identical frames in every mode: ok");
}

static void fix_crc(uint8_t *f, uint32_t nb) { uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, (size_t)nb * IDX); put32(f + 28, hc); }

/* forged frames: refused before anything is allocated */
static void forged(void) {
    /* 1. the fuzzing case: a stream whose frame claims 8 GiB, with no limit at all */
    uint8_t s[12 + 8 + HDR + 2 * IDX + 16] = {'C', 'Z', 'N', 'S', 1, 0, 0, 0}; put32(s + 8, 64u << 20);
    uint8_t *f = s + 20; put64(s + 12, HDR + 2 * IDX + 16);
    memcpy(f, "CZN1", 4); f[4] = 3; f[5] = 1; put64(f + 12, (uint64_t)8 << 30); put32(f + 20, 2); put32(f + 24, 1u << 30);
    for (int i = 0; i < 2; i++) { uint8_t *e = f + HDR + i * IDX; put32(e, (uint32_t)((4ull << 30) - (i ? 0 : 0)) ); put32(e + 4, 8); e[12] = C_STORE; }
    put32(f + HDR, 0xFFFFFFFFu); put32(f + HDR + IDX, (uint32_t)((8ull << 30) - 0xFFFFFFFFull));
    char path[] = "/tmp/kolmo-memdyn-XXXXXX"; int fd = mkstemp(path); CHECK(fd >= 0); unlink(path);
    kolmo_set_memlimit(KOLMO_MEM_MAX);
    for (int crc = 0; crc < 2; crc++) {
        if (crc) fix_crc(f, 2); else put32(f + 28, 0x12345678);   /* garbage CRC, then a valid one with stored blocks */
        int in = memfd_create("in", 0); CHECK(in >= 0); CHECK(write(in, s, sizeof s) == (ssize_t)sizeof s); lseek(in, 0, SEEK_SET);
        int64_t r = kolmo_stream_decompress(in, fd, 1);
        CHECK(r == (crc ? KOLMO_ERR_CORRUPT : KOLMO_ERR_CHECKSUM));
        close(in);
    }
    /* 2. a context-mixing block whose stream asks for 2^22-bucket tables (5 GiB) in a frame that declares 16 MiB */
    size_t raw = 1u << 20; uint8_t g[HDR + IDX + 4 + 16] = {0};
    memcpy(g, "CZN1", 4); g[4] = 3; g[5] = KOLMO_ULTRA; g[11] = kmem_code(16 * MiB); put64(g + 12, raw); put32(g + 20, 1); put32(g + 24, (uint32_t)raw);
    uint8_t *e = g + HDR; put32(e, (uint32_t)raw); put32(e + 4, 4 + 16); e[12] = C_CM;
    uint8_t *pay = g + HDR + IDX + 4; put32(pay, (uint32_t)raw | 8u << 28); pay[4] = (uint8_t)(3 << 4 | (22 - 15));
    fix_crc(g, 1);
    uint8_t *out = malloc(raw); long made = atomic_load(&cm_models_made);
    CHECK(kolmo_decompress(g, sizeof g, out, raw, 1) == KOLMO_ERR_CORRUPT);
    CHECK(atomic_load(&cm_models_made) == made);            /* refused before the model was made */
    /* 3. the same block in a frame that declares nothing (written before rc3): held to the limit instead */
    g[11] = 0; fix_crc(g, 1);
    kolmo_decode_limits lim = {0, 0, 0, 1u << 30};
    CHECK(kolmo_decompress_limited(g, sizeof g, out, raw, 1, &lim) == KOLMO_ERR_MEMORY);
    CHECK(atomic_load(&cm_models_made) == made);
    /* 4. a frame whose index claims a block larger than its declaration */
    g[11] = kmem_code(2 * MiB); fix_crc(g, 1);
    CHECK(kolmo_check_frame(g, sizeof g, NULL) == KOLMO_ERR_CORRUPT);
    kolmo_set_memlimit(0);
    free(out); close(fd);
    puts("forged frames (8 GiB claim, oversized model, undeclared model, index above the declaration): refused unallocated: ok");
}

/* an explicit cap: -x tables and blocks shrink so the frame decodes within it; a cap below the floor is refused */
static void explicit_cap(void) {
    size_t n = 6 * MiB; uint8_t *in = text_input(n, 11);
    kolmo_params p = {KOLMO_ULTRA, 1, 4, 0, 1, 256 * MiB};
    size_t cap = kolmo_bound(n, &p); uint8_t *out = malloc(cap), *back = malloc(n);
    int64_t r = kolmo_compress(in, n, out, cap, &p); CHECK(r > 0);
    int64_t need = kolmo_decode_memory(out, (size_t)r);
    printf("-x -l 1 under a 256 MiB cap: %lld bytes, declares %.0f MiB\n", (long long)r, need / (double)MiB);
    CHECK(need > 0 && (uint64_t)need <= 256 * MiB);
    kolmo_decode_limits lim = {0, 0, 0, 256 * MiB};
    CHECK(kolmo_decompress_limited(out, (size_t)r, back, n, 4, &lim) == (int64_t)n && !memcmp(back, in, n));
    lim.max_memory = (uint64_t)need - 1;
    CHECK(kolmo_decompress_limited(out, (size_t)r, back, n, 4, &lim) == KOLMO_ERR_MEMORY);
    CHECK(kolmo_check_frame(out, (size_t)r, &lim) == KOLMO_ERR_MEMORY);
    p.mem_limit = 128 * MiB; CHECK(kolmo_compress(in, n, out, cap, &p) == KOLMO_ERR_MEMORY);   /* floor 256 MiB */
    p.mode = KOLMO_MAX; p.level = 0; p.mem_limit = 512 * MiB; CHECK(kolmo_compress(in, n, out, cap, &p) == KOLMO_ERR_MEMORY);
    p.mode = KOLMO_FAST; p.mem_limit = 64 * MiB; CHECK(kolmo_compress(in, n, out, cap, &p) > 0);
    CHECK(kolmo_mem_floor(KOLMO_MAX) == 1024 * MiB && kolmo_mem_floor(KOLMO_FAST) == 64 * MiB);
    free(in); free(out); free(back);
    puts("explicit cap and floors: ok");
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    int quick = argc > 1 && !strcmp(argv[1], "--quick");
    declaration_coding();
    hysteresis();
    forged();
    explicit_cap();
    forced_budgets();
    if (!quick) mid_run();
    puts("MEMDYN TESTS PASSED");
    return 0;
}
