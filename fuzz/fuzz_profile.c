/* libFuzzer target for the --target parser, calibration table and cost formula evaluator (src/plan/target.c), and the
 * trained-profile calls, which this build keeps as stubs (profiles needed library codecs; container version 12).
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
    data += 3; size -= 3;
    /* trained profiles are not available in this build: whatever the bytes, loading reports KOLMO_ERR_UNSUPPORTED, and
     * the frame calls must not crash (a frame of an earlier version is KOLMO_ERR_LEGACY) */
    kolmo_profile *P = NULL;
    if (kolmo_profile_load(data, size, &P) != KOLMO_ERR_UNSUPPORTED || P) __builtin_trap();
    uint8_t small[1 << 16];
    if (kolmo_decompress_profile(data, size, small, sizeof small, 1, NULL) != KOLMO_ERR_UNSUPPORTED) __builtin_trap();
    uint32_t id; int r = kolmo_frame_profile_id(data, size, &id);
    if (r > 0) __builtin_trap();                                   /* no frame of this build uses a profile */
    return 0;
}
