/* Public entry points of the rc3 features: --target, kolmo calibrate, and the trained-profile calls, which this build
 * keeps as stubs.
 *
 * Trained profiles (kolmo train, -P) and the compact profile frame (CZC1) were built on zstd and Brotli dictionaries;
 * with the library coders removed (container version 12) they are not available. The calls stay so that programs
 * keep linking, and return KOLMO_ERR_UNSUPPORTED. Part of the src/kolmo.c translation unit. */
#include <time.h>

/* ---------------------------------------------------------------- profiles (not available in this build) */
size_t kolmo_profile_bound(const kolmo_train_params *tp) { (void)tp; return 0; }
int kolmo_profile_load(const void *buf, size_t n, kolmo_profile **out) { (void)buf; (void)n; if (out) *out = NULL; return KOLMO_ERR_UNSUPPORTED; }
void kolmo_profile_free(kolmo_profile *prof) { (void)prof; }
uint32_t kolmo_profile_id(const kolmo_profile *prof) { (void)prof; return 0; }
void kolmo_profile_params(const kolmo_profile *prof, kolmo_params *p) {
    kolmo_params d = {KOLMO_FAST, 0, 0, 0, 1}; (void)prof;
    if (p) *p = d;
}
size_t kolmo_profile_describe(const kolmo_profile *prof, char *out, size_t cap) { (void)prof; if (out && cap) out[0] = 0; return 0; }
int64_t kolmo_compress_profile(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                               const kolmo_profile *prof) {
    (void)src; (void)n; (void)dst; (void)cap; (void)p; (void)prof; return KOLMO_ERR_UNSUPPORTED;
}
int64_t kolmo_decompress_profile(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                                 const kolmo_profile *prof) {
    (void)src; (void)n; (void)dst; (void)cap; (void)threads; (void)prof; return KOLMO_ERR_UNSUPPORTED;
}
/* 0 for a frame of this build (version 12 frames never use a profile); KOLMO_ERR_LEGACY for a frame of an earlier Kolmo
 * (compact profile frames included), KOLMO_ERR_CORRUPT for anything else */
int kolmo_frame_profile_id(const uint8_t *src, size_t n, uint32_t *id) {
    if (id) *id = 0;
    if (!src) return KOLMO_ERR_CORRUPT;
    return frame_version_check(src, n);
}
int64_t kolmo_profile_train(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                            const kolmo_train_params *tp, uint8_t *dst, size_t cap) {
    (void)samples; (void)sizes; (void)nsamples; (void)tp; (void)dst; (void)cap; return KOLMO_ERR_UNSUPPORTED;
}

static double kf_now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

/* ---------------------------------------------------------------- targets */
int kolmo_target_parse(const char *spec, kolmo_target **out) { if (!out) return KOLMO_ERR_TARGET; return kt_parse(spec, out); }
int kolmo_target_set_calibration(kolmo_target *t, const char *table, size_t n) { return t && table ? kt_set_calibration(t, table, n) : KOLMO_ERR_TARGET; }
void kolmo_target_free(kolmo_target *t) { kt_free_target(t); }
const char *kolmo_target_setting_name(int setting) { return setting >= 0 && setting < KT_NSET ? KT_SET[setting].name : NULL; }

/* Merge the frames of consecutive chunks into one frame: indexes and payloads concatenated (version 12), checksums
 * over decoded bytes (a speed-mode frame checksums its stored bytes; those entries are recomputed). */
static int64_t kt_merge(uint8_t *const *fr, const size_t *fl, uint32_t k, const uint8_t *src, const uint64_t *starts,
                        uint8_t *dst, size_t cap, const int *chosen) {
    uint64_t nb = 0, pay = 0; int psum_all = 1; uint32_t B = 0; int modes[6] = {0}; uint8_t memd = 0;
    for (uint32_t c = 0; c < k; c++) {
        const uint8_t *f = fr[c]; uint32_t m = get32(f + 20);
        if (fl[c] < HDR || memcmp(f, "CZN1", 4) || f[4] != KOLMO_MAX_VERSION || (uint64_t)m * IDX + HDR > fl[c]) return KOLMO_ERR_CORRUPT;
        nb += m; pay += fl[c] - HDR - (size_t)m * IDX;
        if (f[11] > memd) memd = f[11];                    /* the largest chunk's decode working set (header byte 11) */
        psum_all &= f[10] & 1;
        if (get32(f + 24) > B) B = get32(f + 24);
        modes[f[5] <= 5 ? f[5] : 1] += (int)chosen[c] >= 0;
    }
    if (nb > UINT32_MAX - 1) return KOLMO_ERR_CORRUPT;
    uint64_t total = HDR + nb * IDX + pay;
    if (total > cap) return KOLMO_ERR_DSTSIZE;
    uint8_t *ix = dst + HDR, *po = dst + HDR + nb * IDX;
    for (uint32_t c = 0; c < k; c++) {
        const uint8_t *f = fr[c]; uint32_t m = get32(f + 20); int ps = f[10] & 1;
        uint64_t raw = starts[c];
        for (uint32_t i = 0; i < m; i++) {
            memcpy(ix, f + HDR + (size_t)i * IDX, IDX);
            uint32_t len = get32(ix);
            if (raw + len > starts[c + 1]) return KOLMO_ERR_CORRUPT;
            if (ps && !psum_all) put32(ix + 8, blk_sum(src + raw, len));
            raw += len; ix += IDX;
        }
        size_t pl = fl[c] - HDR - (size_t)m * IDX;
        memcpy(po, f + HDR + (size_t)m * IDX, pl); po += pl;
    }
    int mode = 0; for (int i = 1; i < 6; i++) if (modes[i] > modes[mode]) mode = i;
    memcpy(dst, "CZN1", 4); dst[4] = KOLMO_MAX_VERSION; dst[5] = (uint8_t)mode; memset(dst + 6, 0, 6);
    dst[10] = (uint8_t)psum_all; dst[11] = memd;
    put64(dst + 12, starts[k] - starts[0]); put32(dst + 20, (uint32_t)nb); put32(dst + 24, B);
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, (size_t)nb * IDX); put32(dst + 28, hc);
    return (int64_t)total;
}

#define KT_CHUNK ((size_t)16 << 20)
int64_t kolmo_compress_target(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                              const kolmo_target *t, const kolmo_profile *prof, kolmo_target_report *report) {
    if (prof) return KOLMO_ERR_UNSUPPORTED;               /* trained profiles: not available in this build */
    if (!t || (!src && n) || !dst) return KOLMO_ERR_TARGET;
    int threads = p && p->threads > 0 ? p->threads : default_threads();
    size_t B = p && p->block ? p->block : KT_CHUNK; if (B < 4096) B = 4096;
    if (!src) src = (const uint8_t *)"";
    int text = n && !kt_is_binary(src, n);
    /* A nonfinal text chunk can end at its midpoint. Include the final endpoint. */
    size_t min_chunk = text ? B / 2 : B;
    size_t max_chunks = n ? 1 + (n - 1) / min_chunk : 1;
    /* Workers use a uint32_t claim counter; leave room for up to 256 terminal claims. */
    if (max_chunks > UINT32_MAX - 256 || max_chunks >= SIZE_MAX / sizeof(uint64_t))
        return KOLMO_ERR_ALLOC;
    uint64_t *starts = malloc((max_chunks + 1) * sizeof(*starts));
    if (!starts) return KOLMO_ERR_ALLOC;
    uint32_t k = 0;
    for (size_t pos = 0; pos < n || k == 0; ) {
        starts[k++] = pos;
        size_t end = n - pos > B ? pos + B : n;
        if (text && end < n) { size_t e = end; while (e > pos + B / 2 && src[e - 1] != '\n') e--; if (src[e - 1] == '\n') end = e; }
        pos = end;
        if (!n) break;
    }
    starts[k] = n;
    kt_job J; memset(&J, 0, sizeof J);
    J.src = src; J.starts = starts; J.nchunks = k; J.inner_threads = k == 1 ? threads : 1; J.t = t;
    J.frames = calloc(k, sizeof(uint8_t *)); J.flen = calloc(k, sizeof(size_t)); J.chosen = calloc(k, sizeof(int));
    J.pcost = calloc(k, sizeof(double)); J.penc = calloc(k, sizeof(double)); J.pdec = calloc(k, sizeof(double)); J.psize = calloc(k, sizeof(uint64_t));
    int64_t ret = KOLMO_ERR_ALLOC;
    if (J.frames && J.flen && J.chosen && J.pcost && J.penc && J.pdec && J.psize) {
        atomic_init(&J.next, 0); atomic_init(&J.err, 0);
        run_pool(kt_worker, &J, threads, k);
        ret = atomic_load(&J.err);
        if (!ret) {
            if (k == 1) {
                if (J.flen[0] > cap) ret = KOLMO_ERR_DSTSIZE;
                else { memcpy(dst, J.frames[0], J.flen[0]); ret = (int64_t)J.flen[0]; }
            } else ret = kt_merge(J.frames, J.flen, k, src, starts, dst, cap, J.chosen);
        }
        if (!ret || ret > 0) {
            if (report) {
                memset(report, 0, sizeof *report); report->chunks = k;
                for (uint32_t c = 0; c < k; c++) {
                    report->per_setting[J.chosen[c]]++; report->predicted_cost += J.pcost[c];
                    report->predicted_enc_s += J.penc[c]; report->predicted_dec_s += J.pdec[c]; report->predicted_size += J.psize[c];
                }
            }
        }
    }
    if (J.frames) for (uint32_t c = 0; c < k; c++) free(J.frames[c]);
    free(J.frames); free(J.flen); free(J.chosen); free(J.pcost); free(J.penc); free(J.pdec); free(J.psize); free(starts);
    zs_trim();
    return ret;
}

/* ---------------------------------------------------------------- calibration
 * Built-in data, one corpus per class, from a fixed generator (the same bytes on every machine): prose from a
 * Zipf-distributed vocabulary, a CSV of sensor rows, application log lines, JSON event records, seven float32
 * random walks, and machine-code-like binary (a few hundred instruction patterns with random operands). */
typedef struct { uint64_t s; } kf_rng;
static uint32_t kf_next(kf_rng *r) { r->s ^= r->s << 13; r->s ^= r->s >> 7; r->s ^= r->s << 17; return (uint32_t)(r->s >> 11); }
static double kf_unif(kf_rng *r) { return (kf_next(r) & 0xFFFFFF) / 16777216.0; }
static void kf_word(kf_rng *r, char *w, int idx) {
    static const char *const SY[] = {"ka", "lo", "mi", "ne", "ru", "ta", "si", "po", "de", "ga", "en", "or", "ar", "is", "th", "an", "re", "on", "st", "al"};
    kf_rng q = {0x9E3779B97F4A7C15ull ^ (uint64_t)idx * 0x100000001B3ull}; (void)r;
    int len = 1 + (int)(kf_next(&q) % 3) + (idx > 50) + (idx > 400); w[0] = 0;
    for (int i = 0; i < len; i++) strcat(w, SY[kf_next(&q) % 20]);
}
static int kf_zipf(kf_rng *r, int n) { double u = kf_unif(r); return (int)(n * u * u * u); }
static size_t kf_corpus(int cls, uint8_t *o, size_t cap) {
    kf_rng r = {0x2545F4914F6CDD1Dull + (uint64_t)cls}; size_t w = 0; char line[512], wd[64], wd2[64];
    static const char *const LV[] = {"INFO", "INFO", "INFO", "DEBUG", "WARN", "ERROR"};
    static const char *const EV[] = {"login", "logout", "view", "click", "purchase", "search", "error"};
    double walk[7] = {20, 50, 1013, 3.5, 0.2, 100, 7};
    uint64_t t = 1763251200;
    for (int row = 0; w < cap; row++) {
        int l = 0;
        if (cls == 0) {                                  /* prose */
            int words = 6 + (int)(kf_next(&r) % 14);
            for (int i = 0; i < words && l < 400; i++) { kf_word(&r, wd, kf_zipf(&r, 3000)); if (!i) wd[0] = (char)(wd[0] - 32); l += snprintf(line + l, sizeof line - l, "%s%s", wd, i + 1 < words ? " " : ". "); }
            if (row % 4 == 3) line[l++] = '\n';
        } else if (cls == 1) {                           /* CSV */
            if (!row) l = snprintf(line, sizeof line, "id,time,station,temperature,humidity,pressure,status\n");
            else { t += 1 + kf_next(&r) % 30; walk[0] += (kf_unif(&r) - 0.5) * 0.3; walk[1] += (kf_unif(&r) - 0.5); walk[2] += (kf_unif(&r) - 0.5) * 0.2;
                   kf_word(&r, wd, kf_zipf(&r, 40));
                   l = snprintf(line, sizeof line, "%d,%llu,%s,%.2f,%d,%.1f,%s\n", row, (unsigned long long)t, wd, walk[0], (int)walk[1] & 127, walk[2], kf_next(&r) % 20 ? "ok" : "fault"); }
        } else if (cls == 2) {                           /* log */
            t += kf_next(&r) % 3; kf_word(&r, wd, kf_zipf(&r, 60)); kf_word(&r, wd2, kf_zipf(&r, 800));
            l = snprintf(line, sizeof line, "2026-11-16 %02llu:%02llu:%02llu,%03u %s [pool-%u-thread-%u] org.example.%s.Service: request %s finished in %u ms id=%u\n",
                         (unsigned long long)(t / 3600 % 24), (unsigned long long)(t / 60 % 60), (unsigned long long)(t % 60), kf_next(&r) % 1000,
                         LV[kf_next(&r) % 6], 1 + kf_next(&r) % 4, 1 + kf_next(&r) % 16, wd, wd2, kf_next(&r) % 900, 100000 + row * 7 + kf_next(&r) % 5);
        } else if (cls == 3) {                           /* JSON */
            t += kf_next(&r) % 5; kf_word(&r, wd, kf_zipf(&r, 500)); kf_word(&r, wd2, kf_zipf(&r, 30));
            l = snprintf(line, sizeof line, "{\"id\":%d,\"ts\":%llu,\"user\":\"%s\",\"event\":\"%s\",\"value\":%.3f,\"tags\":[\"%s\",\"beta\"],\"ok\":%s}\n",
                         row, (unsigned long long)t, wd, EV[kf_next(&r) % 7], kf_unif(&r) * 100, wd2, kf_next(&r) % 9 ? "true" : "false");
        } else if (cls == 4) {                           /* float32 columns */
            float v[7]; for (int c = 0; c < 7; c++) { walk[c] += (kf_unif(&r) - 0.5) * (c + 1) * 0.05; v[c] = (float)walk[c]; }
            memcpy(line, v, sizeof v); l = (int)sizeof v;
        } else {                                         /* machine-code-like */
            kf_rng q = {0xD1B54A32D192ED03ull + (uint64_t)kf_zipf(&r, 300)}; int len = 2 + (int)(kf_next(&q) % 7);
            for (int i = 0; i < len; i++) line[l++] = (char)kf_next(&q);
            if (kf_next(&r) % 2) { uint32_t imm = kf_next(&r) % 4096; memcpy(line + l, &imm, 4); l += 4; }
        }
        if (l <= 0) continue;
        size_t m = (size_t)l < cap - w ? (size_t)l : cap - w;
        memcpy(o + w, line, m); w += m;
    }
    return w;
}

static int64_t kf_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                            char *out, size_t cap, size_t CN);
int64_t kolmo_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                        char *out, size_t cap) {
    return kf_calibrate(files, sizes, nfiles, seconds_per_point, out, cap, 512u << 10);   /* 512 KiB per class */
}
static int64_t kf_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                            char *out, size_t cap, size_t CN) {
    if (!out || cap < 64) return KOLMO_ERR_DSTSIZE;
    if (!(seconds_per_point > 0)) seconds_per_point = 0.1;
    uint8_t *corp[KT_NCLS] = {0}; size_t clen[KT_NCLS] = {0}; int from_files[KT_NCLS] = {0};
    int64_t ret = 0;
    for (int c = 0; c < KT_NCLS; c++) if (!(corp[c] = malloc(CN))) ret = KOLMO_ERR_ALLOC;
    for (size_t i = 0; i < nfiles && !ret; i++) {
        if (!files[i] || !sizes[i]) continue;
        int c = kt_class(files[i], sizes[i]);
        size_t m = sizes[i] < CN - clen[c] ? sizes[i] : CN - clen[c];
        if (m) { memcpy(corp[c] + clen[c], files[i], m); clen[c] += m; from_files[c] = 1; }
    }
    for (int c = 0; c < KT_NCLS && !ret; c++) if (!from_files[c]) clen[c] = kf_corpus(c, corp[c], CN);
    size_t w = 0;
    w += (size_t)snprintf(out + w, cap - w, "kolmo-calibration 1\n# one thread; MB/s = 10^6 bytes per second; data: ");
    for (int c = 0; c < KT_NCLS && w < cap; c++) w += (size_t)snprintf(out + w, cap - w, "%s%s %s", c ? ", " : "", KT_CLS[c], from_files[c] ? "files" : "built-in");
    if (w < cap) w += (size_t)snprintf(out + w, cap - w, "\n# setting class encode_MBps decode_MBps encode_setup_ms decode_setup_ms\n");
    uint8_t *o = NULL, *b = NULL;
    if (!ret) { o = malloc(kolmo_bound(CN, NULL) + CN); b = malloc(CN); if (!o || !b) ret = KOLMO_ERR_ALLOC; }
    /* each point: the fastest of repeated runs at 1/8 of the corpus and at all of it; time = setup + bytes / rate */
    for (int st = 0; st < KT_NSET && !ret; st++)
        for (int c = 0; c < KT_NCLS && !ret; c++) {
            kolmo_params q = {KT_SET[st].mode, KT_SET[st].level, 1, 0, KT_SET[st].transforms};
            double te[2], td[2]; size_t len[2] = {clen[c] / 8, clen[c]};
            for (int k = 0; k < 2 && !ret; k++) {
                size_t oc = kolmo_bound(len[k], &q) + CN; double be = HUGE_VAL, bd = HUGE_VAL, spent = 0; int64_t z = 0;
                for (int rep = 0; rep < 1000 && (rep < 1 || spent < seconds_per_point); rep++) {
                    double t0 = kf_now(); z = kolmo_compress(corp[c], len[k], o, oc, &q); double t1 = kf_now();
                    if (z < 0) { ret = z; break; }
                    if (t1 - t0 < be) be = t1 - t0;
                    spent += t1 - t0;
                }
                spent = 0;
                for (int rep = 0; rep < 1000 && !ret && (rep < 1 || spent < seconds_per_point); rep++) {
                    double t0 = kf_now(); int64_t u = kolmo_decompress(o, (size_t)z, b, CN, 1); double t1 = kf_now();
                    if (u != (int64_t)len[k] || memcmp(b, corp[c], len[k])) { ret = KOLMO_ERR_CORRUPT; break; }
                    if (t1 - t0 < bd) bd = t1 - t0;
                    spent += t1 - t0;
                }
                te[k] = be > 1e-9 ? be : 1e-9; td[k] = bd > 1e-9 ? bd : 1e-9;
            }
            if (ret) break;
            double dn = (double)(len[1] - len[0]);
            double re = te[1] > te[0] ? dn / (te[1] - te[0]) : len[1] / te[1], rd = td[1] > td[0] ? dn / (td[1] - td[0]) : len[1] / td[1];
            double se = te[0] - len[0] / re, sd = td[0] - len[0] / rd;
            if (se < 0) se = 0;
            if (sd < 0) sd = 0;
            /* rates under 0.1 MB/s (-x on a small corpus, a loaded machine) keep four decimals: "%.1f" printed them as
             * 0.0, which the table parser rightly refuses (rates must be positive) */
            if (w < cap)
                w += (size_t)snprintf(out + w, cap - w, "%s %s %.*f %.*f %.3f %.3f\n", KT_SET[st].name, KT_CLS[c],
                                      re < 1e5 ? 4 : 1, re > 100 ? re / 1e6 : 1e-4, rd < 1e5 ? 4 : 1, rd > 100 ? rd / 1e6 : 1e-4,
                                      se * 1e3, sd * 1e3);
        }
    free(o); free(b);
    for (int c = 0; c < KT_NCLS; c++) free(corp[c]);
    if (ret) return ret;
    if (w >= cap) return KOLMO_ERR_DSTSIZE;
    return (int64_t)w;
}
