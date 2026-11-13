/* Research wrapper only. Frozen E02C decoder/capture and production leaf coders. */
#define main e02c_legacy_main
#include "../E02C/probe.c"
#undef main
#include "selector.h"

typedef struct {
    size_t baseline_bytes, baseline_payload, represented_bytes;
    unsigned baseline_codec, chosen_codec, chosen_method, attempted_method, entropy_calls;
    uint64_t base_ns, sample_ns, optional_ns;
    e03_sample sample;
} e03_stats;

/* Consider actual coding output; ties retain the earlier (cheaper to decode) choice. */
static int e03_try(const uint8_t *p, size_t n, unsigned method, unsigned policy,
                   ec_buf *best, size_t *best_rep, unsigned *best_method,
                   unsigned *best_codec, unsigned *calls) {
    ec_buf trial = {0}; codec_ctx ctx = {0};
    int status = -1;
    if (n > EC_LIMIT / 4) goto done;
    if (!best->p || n < best->n) {
        best->n = 0;
        if (ec_add(best, p, n)) goto done;
        *best_rep = n; *best_method = method; *best_codec = C_STORE;
    }
    if (ec_reserve(&trial, n * 2 + 8192)) goto done;
    const unsigned codecs[2] = {C_HUF, C_RANS};
    for (unsigned i = 0; i < 2; i++) {
        unsigned codec = codecs[i];
        if ((policy == 2 && codec != C_HUF) || (policy == 3 && codec != C_RANS)) continue;
        (*calls)++;
        size_t z = codec_compress(&ctx, (int)codec, 1, p, n, trial.p, trial.cap);
        /* Existing coders return zero when this leaf is not applicable. */
        if (!z || z >= best->n) continue;
        trial.n = z;
        ec_buf tmp = *best; *best = trial; trial = tmp;
        *best_rep = n; *best_method = method; *best_codec = codec;
        trial.n = 0;
        if (ec_reserve(&trial, n * 2 + 8192)) goto done;
    }
    status = 0;
 done:
    ec_free(&trial); codec_ctx_free(&ctx); return status;
}

static int e03_encode(const uint8_t *src, size_t n, unsigned width, size_t head,
                      unsigned policy, ec_buf *frame, e03_stats *s) {
    ec_buf best = {0}, rep = {0}; ec_meta m; uint64_t *values = NULL;
    unsigned method = 0, coder = 0; size_t repn = 0; int status = -1;
    memset(s, 0, sizeof *s); frame->n = 0;
    if (policy > 3 || ec_capture(src, n, width, head) ||
        ec_params(ec_par.p, ec_par.n, ec_rest.n, n, ec_res.n, &m)) goto done;
    uint64_t start = ec_ns(CLOCK_MONOTONIC);
    if (e03_try(ec_res.p, ec_res.n, 0, policy, &best, &repn, &method, &coder, &s->entropy_calls)) goto done;
    s->base_ns = ec_ns(CLOCK_MONOTONIC) - start;
    s->baseline_payload = best.n; s->baseline_codec = coder;
    s->baseline_bytes = 64 + ec_par.n + ec_rest.n + best.n;
    if (policy == 1) {
        start = ec_ns(CLOCK_MONOTONIC);
        if (e03_sample_method(ec_res.p, m.values, m.ow, m.planes, &s->sample)) goto done;
        s->sample_ns = ec_ns(CLOCK_MONOTONIC) - start;
        s->attempted_method = s->sample.method;
        if (s->attempted_method) {
            start = ec_ns(CLOCK_MONOTONIC);
            values = malloc((m.values ? m.values : 1) * sizeof *values);
            if (!values) goto done;
            ec_values(ec_res.p, &m, values);
            if (ec_pack(values, m.values, s->attempted_method, &rep) ||
                e03_try(rep.p, rep.n, s->attempted_method, 0, &best, &repn,
                        &method, &coder, &s->entropy_calls)) goto done;
            free(values); values = NULL; ec_free(&rep);
            s->optional_ns = ec_ns(CLOCK_MONOTONIC) - start;
        }
    }
    s->chosen_method = method; s->chosen_codec = coder; s->represented_bytes = repn;
    uint8_t hdr[64] = {0}; memcpy(hdr, "E2C1", 4); hdr[4] = 1;
    hdr[5] = (uint8_t)method; hdr[6] = hdr[7] = (uint8_t)coder;
    ec_u64(hdr + 8, n); ec_u64(hdr + 16, ec_par.n); ec_u64(hdr + 24, ec_rest.n);
    ec_u64(hdr + 32, repn); ec_u64(hdr + 40, best.n); ec_u64(hdr + 48, ec_res.n);
    ec_u32(hdr + 56, kolmo_crc32c(0, src, n));
    if (ec_add(frame, hdr, 64) || ec_add(frame, ec_par.p, ec_par.n) ||
        ec_add(frame, ec_rest.p, ec_rest.n) || ec_add(frame, best.p, best.n)) goto done;
    ec_u32(frame->p + 60, ec_frame_crc(frame->p, frame->n));
    if (frame->n > s->baseline_bytes || s->entropy_calls > 4 ||
        (policy == 0 && (s->entropy_calls != 2 || method))) goto done;
    status = 0;
 done:
    free(values); ec_free(&rep); ec_free(&best); return status;
}

static int e03_test(void) {
    size_t lengths[] = {16, 17, 127, 128, 129, 1024, 1025, 4097};
    uint8_t *src = malloc(3 + 4097 * 8 + 1);
    ec_buf frame = {0}, again = {0}, out = {0}, fixed = {0};
    size_t cases = 0, truncations = 0, mutations = 0, oracle_checks = 0;
    uint64_t ticks; unsigned coders[] = {0, 13, 14};
    if (!src) return 1;
    for (unsigned w = 2; w <= 8; w *= 2) for (unsigned kind = 0; kind < 6; kind++)
        for (size_t l = 0; l < sizeof lengths / sizeof *lengths; l++) {
            size_t count = lengths[l], n = 3 + count * w + 1;
            memcpy(src, "HDR", 3); src[n - 1] = 0xD3;
            for (size_t i = 0; i < count; i++) {
                uint64_t x = kind == 0 ? 0 : kind == 1 ? i * 1000 + (ec_rand() & 7) :
                    kind == 2 ? (i % 127 ? ec_rand() & 7 : ec_rand()) :
                    kind == 3 ? ec_rand() : kind == 4 ? ec_rand() & (i < count / 2 ? 7 : 65535) :
                    (i & 1 ? UINT64_MAX : 1ull << 63);
                for (unsigned j = 0; j < w; j++) src[3 + i * w + j] = (uint8_t)(x >> (8 * j));
            }
            for (unsigned policy = 0; policy < 4; policy++) {
                e03_stats s, t;
                if (e03_encode(src, n, w, 3, policy, &frame, &s) ||
                    ec_decode(frame.p, frame.n, &out, NULL, &ticks) || out.n != n || memcmp(out.p, src, n) ||
                    e03_encode(src, n, w, 3, policy, &again, &t) || again.n != frame.n ||
                    memcmp(frame.p, again.p, frame.n) || s.attempted_method != t.attempted_method) return 2;
                size_t expected = SIZE_MAX;
                for (unsigned q = 0; q < 2; q++) {
                    unsigned method = q ? s.attempted_method : 0;
                    if (q && !method) continue;
                    for (unsigned c = 0; c < 3; c++) {
                        if ((policy == 2 && coders[c] == 14) || (policy == 3 && coders[c] == 13)) continue;
                        if (ec_encode(src, n, w, 3, method, coders[c], &fixed, &ticks)) return 3;
                        if (fixed.n < expected) expected = fixed.n;
                        oracle_checks++;
                    }
                }
                if (frame.n != expected || frame.n > s.baseline_bytes) return 4;
                for (size_t cut = 0; cut < frame.n; cut += frame.n / 13 + 1) {
                    if (!ec_decode(frame.p, cut, &out, NULL, &ticks)) return 5;
                    truncations++;
                }
                for (unsigned k = 0; k < 12; k++) {
                    size_t at = ec_rand() % frame.n; uint8_t saved = frame.p[at];
                    uint32_t crc = ec_r32(frame.p + 60); frame.p[at] ^= (uint8_t)(1 + ec_rand() % 255);
                    if (k & 1) ec_u32(frame.p + 60, ec_frame_crc(frame.p, frame.n));
                    int rc = ec_decode(frame.p, frame.n, &out, NULL, &ticks);
                    if (!rc && (out.n != n || memcmp(out.p, src, n))) return 6;
                    frame.p[at] = saved; ec_u32(frame.p + 60, crc); mutations++;
                }
                cases++;
            }
        }
    free(src); ec_free(&frame); ec_free(&again); ec_free(&out); ec_free(&fixed); ec_clear_capture();
    printf("E03_FULL_SOURCE cases=%zu oracle_checks=%zu truncations=%zu mutations=%zu PASSED\n",
           cases, oracle_checks, truncations, mutations);
    return 0;
}

static int e03_same_choice(const e03_stats *a, const e03_stats *b) {
    return a->baseline_bytes == b->baseline_bytes && a->baseline_codec == b->baseline_codec &&
        a->chosen_method == b->chosen_method && a->chosen_codec == b->chosen_codec &&
        a->attempted_method == b->attempted_method && a->entropy_calls == b->entropy_calls &&
        a->sample.values == b->sample.values && a->sample.local_bytes == b->sample.local_bytes &&
        a->sample.patched_bytes == b->sample.patched_bytes;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "test")) return e03_test();
    if (argc != 8 || strcmp(argv[1], "bench")) return 2;
    unsigned long long w, head, policy, reps;
    if (ec_arg(argv[3], &w) || ec_arg(argv[4], &head) || ec_arg(argv[5], &policy) ||
        ec_arg(argv[6], &reps) || w > 8 || head > EC_INPUT || policy > 3 || reps < 1 || reps > 100) return 2;
    ec_buf in = {0}, frame = {0}, first = {0}, out = {0}, rep = {0};
    e03_stats s, initial; uint64_t dk; int fail = 1;
    if (ec_read(argv[2], &in, EC_INPUT) || e03_encode(in.p, in.n, (unsigned)w, (size_t)head,
        (unsigned)policy, &first, &initial) || ec_decode(first.p, first.n, &out, NULL, &dk) ||
        out.n != in.n || memcmp(out.p, in.p, in.n)) goto done;
    for (unsigned long long i = 0; i < reps; i++) {
        uint64_t cpu0 = ec_ns(CLOCK_PROCESS_CPUTIME_ID), t0 = ec_ns(CLOCK_MONOTONIC);
        if (e03_encode(in.p, in.n, (unsigned)w, (size_t)head, (unsigned)policy, &frame, &s)) goto done;
        uint64_t t1 = ec_ns(CLOCK_MONOTONIC), cpu1 = ec_ns(CLOCK_PROCESS_CPUTIME_ID);
        if (ec_decode(frame.p, frame.n, &out, NULL, &dk)) goto done;
        uint64_t t2 = ec_ns(CLOCK_MONOTONIC), cpu2 = ec_ns(CLOCK_PROCESS_CPUTIME_ID);
        if (frame.n != first.n || memcmp(frame.p, first.p, frame.n) || out.n != in.n ||
            memcmp(out.p, in.p, in.n) || !e03_same_choice(&s, &initial)) goto done;
        printf("{\"type\":\"timing\",\"repeat\":%llu,\"encode_ns\":%llu,\"decode_ns\":%llu,\"encode_cpu_ns\":%llu,\"decode_cpu_ns\":%llu,\"baseline_coding_ns\":%llu,\"sample_ns\":%llu,\"optional_ns\":%llu}\n",
            i, (unsigned long long)(t1 - t0), (unsigned long long)(t2 - t1),
            (unsigned long long)(cpu1 - cpu0), (unsigned long long)(cpu2 - cpu1),
            (unsigned long long)s.base_ns, (unsigned long long)s.sample_ns, (unsigned long long)s.optional_ns);
    }
    if (ec_decode(frame.p, frame.n, &out, &rep, &dk) || ec_write(argv[7], ".frame", frame.p, frame.n) ||
        ec_write(argv[7], ".represented", rep.p, rep.n) || ec_write(argv[7], ".decoded", out.p, out.n) ||
        ec_write(argv[7], ".params", ec_par.p, ec_par.n) || ec_write(argv[7], ".residual", ec_res.p, ec_res.n) ||
        ec_write(argv[7], ".rest", ec_rest.p, ec_rest.n)) goto done;
    FILE *f = fopen("/proc/self/status", "r"); char line[256]; unsigned long long hwm = 0;
    if (!f) goto done;
    while (fgets(line, sizeof line, f)) if (sscanf(line, "VmHWM: %llu kB", &hwm) == 1) break;
    if (fclose(f) || !hwm) goto done;
    unsigned modes[3] = {0};
    if (s.chosen_codec == C_HUF && ec_huf_modes(frame.p + 64 + ec_par.n + ec_rest.n,
        frame.n - 64 - ec_par.n - ec_rest.n, modes)) goto done;
    printf("{\"type\":\"summary\",\"input_bytes\":%zu,\"stored_bytes\":%zu,\"baseline_bytes\":%zu,\"baseline_codec\":%u,\"method\":%u,\"actual_codec\":%u,\"attempted_method\":%u,\"entropy_calls\":%u,\"sample_values\":%zu,\"sample_original_bytes\":%zu,\"sample_local_bytes\":%zu,\"sample_patched_bytes\":%zu,\"represented_bytes\":%zu,\"residual_bytes\":%zu,\"huf_stored_chunks\":%u,\"huf_rle_chunks\":%u,\"huf_coded_chunks\":%u,\"post_exec_image_peak_rss_kib\":%llu}\n",
        in.n, frame.n, s.baseline_bytes, s.baseline_codec, s.chosen_method, s.chosen_codec,
        s.attempted_method, s.entropy_calls, s.sample.values, s.sample.original_bytes,
        s.sample.local_bytes, s.sample.patched_bytes, s.represented_bytes, ec_res.n,
        modes[0], modes[1], modes[2], hwm);
    fail = 0;
 done:
    ec_free(&in); ec_free(&frame); ec_free(&first); ec_free(&out); ec_free(&rep); ec_clear_capture();
    if (fail) fprintf(stderr, "E03 operation failed\n"); return fail;
}
