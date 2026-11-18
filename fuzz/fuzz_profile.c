/* libFuzzer target for trained profiles (KPF1, src/plan/profile.c), profile frames (CZN1 with a profile id and the
 * compact CZC1 form, decoded with the profile's dictionaries) and the --target parser, calibration table and cost
 * formula evaluator (src/plan/target.c).
 *
 * Input: sel u8 | L u16 | profile[L] | rest.
 *   sel bit 0: recompute the profile's id and CRC (fuzz/fuzz_fixup.h) so that mutations reach the section parsers.
 *   sel bit 1: also recompute the header CRC of a CZN1 frame in `rest`.
 *   sel bit 2: target mode instead: `rest` up to the first NUL is a --target expression, the bytes after it a
 *              calibration table; the parsed formula is evaluated on edge values (0, huge, negative, NaN inputs).
 * With a profile that loads: describe it, decode `rest` as a profile frame (bounded and exact buffers), and compress
 * the first 16 KiB of `rest` with the profile (its own setting when that is not ultra, else balanced) and require an
 * exact round trip (a mismatch traps). */
#include "../src/kolmo.c"
#include "fuzz_fixup.h"

static void fz_target(const uint8_t *d, size_t n) {
    size_t z = 0; while (z < n && d[z]) z++;
    if (z > 4096) return;
    char *spec = malloc(z + 1); if (!spec) return;
    memcpy(spec, d, z); spec[z] = 0;
    kolmo_target *t = NULL;
    if (kolmo_target_parse(spec, &t) == 0 && t) {
        if (z < n) kolmo_target_set_calibration(t, (const char *)d + z + 1, n - z - 1);
        static const double v[] = { 0, 1, 1e-300, 1e300, -1, 4096, 1e9, 0.5 };
        for (int a = 0; a < 8; a++)
            for (int b = 0; b < 8; b += 3) { volatile double r = kt_eval(t, v[a], v[(a + b) % 8], v[b], v[7 - a]); (void)r; }
        for (int s = 0; s < KT_NSET; s++)
            for (int c = 0; c < KT_NCLS; c++) { volatile double r = kt_enc_time(t, s, c, 1e6) + kt_dec_time(t, s, c, 0); (void)r; }
        kolmo_target_free(t);
    }
    for (int s = 0; s < 12; s++) (void)kolmo_target_setting_name(s);
    free(spec);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3 || size > (4u << 20)) return 0;
    uint8_t sel = data[0];
    if (sel & 4) { fz_target(data + 1, size - 1); return 0; }
    size_t L = (size_t)data[1] | (size_t)data[2] << 8;
    data += 3; size -= 3;
    if (L > size) L = size;
    uint8_t *pb = malloc(L ? L : 1), *rest = malloc(size - L + 1);
    if (!pb || !rest) { free(pb); free(rest); return 0; }
    memcpy(pb, data, L); memcpy(rest, data + L, size - L);
    size_t rn = size - L;
    if (sel & 1) fz_fix_profile(pb, L);
    if (sel & 2) fz_fix_frame(rest, rn);
    kolmo_profile *P = NULL;
    if (kolmo_profile_load(pb, L, &P) == 0 && P) {
        char desc[2048]; kolmo_profile_describe(P, desc, (sel & 8) ? 16 : sizeof desc);
        kolmo_params kp; kolmo_profile_params(P, &kp); (void)kolmo_profile_id(P);
        uint8_t small[1 << 16]; kolmo_decompress_profile(rest, rn, small, sizeof small, 1, P);
        int64_t orig = kolmo_original_size(rest, rn);
        if (orig >= 0 && orig <= (int64_t)(16u << 20)) {
            uint8_t *o = malloc((size_t)orig + 1);
            if (o) kolmo_decompress_profile(rest, rn, o, (size_t)orig, 1 + (sel >> 7), P);
            free(o);
        }
        /* round trip with the profile (never ultra: too slow for fuzzing) */
        size_t m = rn > 16384 ? 16384 : rn;
        kolmo_params p = kp; if (p.mode == KOLMO_ULTRA || p.mode > KOLMO_ULTRA) p.mode = KOLMO_FAST;
        p.threads = 1; if (p.level < 0 || p.level > 22) p.level = 0;
        size_t cap = kolmo_bound(m, &p) + 1024; uint8_t *c = malloc(cap), *r = malloc(m + 1);
        if (c && r && m) {
            int64_t k = kolmo_compress_profile(rest, m, c, cap, &p, P);
            if (k > 0) {
                int64_t d = kolmo_decompress_profile(c, (size_t)k, r, m, 1, P);
                if (d != (int64_t)m || memcmp(r, rest, m)) __builtin_trap();   /* lossy round trip with a loaded profile */
            }
        }
        free(c); free(r);
        kolmo_profile_free(P);
    }
    uint32_t id; kolmo_frame_profile_id(rest, rn, &id);
    free(pb); free(rest);
    return 0;
}
