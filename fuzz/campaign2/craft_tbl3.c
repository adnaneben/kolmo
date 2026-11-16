/* Seed crafter (campaign 2): the current encoder never writes the version-2 table transform (tflag 3, tbl_decode in
 * src/kolmo.c): only its planner trial calls tbl_encode. This tool takes a CZN1 frame made by build/kolmo from a CSV and
 * the raw CSV, and rewrites every block whose plan is T_TBL into the tflag-3 form the decoder accepts: payload =
 * plan prefix | u32 transformed length | zstd(tbl_encode(block)). The block checksum covers the decoded bytes, which
 * do not change; the header CRC is recomputed. The output must decode to the raw input (checked here).
 * Build: clang -O1 -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -Isrc -Ivendor/zstd -Ivendor/lz4
 *        -Ivendor/libbsc -Ivendor/brotli/include fuzz/campaign2/craft_tbl3.c build/libvendor.a -llzma -lpthread -lm -lstdc++
 * Usage: craft_tbl3 frame.kolmo raw.csv out.kolmo */
#include "../../src/kolmo.c"
static uint8_t *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)l + 1); if (fread(b, 1, (size_t)l, f) != (size_t)l) { fclose(f); free(b); return NULL; } fclose(f); *n = (size_t)l; return b;
}
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: %s frame raw out\n", argv[0]); return 2; }
    size_t fn, rn; uint8_t *f = slurp(argv[1], &fn), *raw = slurp(argv[2], &rn);
    if (!f || !raw || fn < 32 || memcmp(f, "CZN1", 4) || f[4] < 2 || (f[10] & 1)) { fprintf(stderr, "bad input\n"); return 1; }
    uint32_t nb = get32(f + 20); size_t ip = 32, pp = 32 + (size_t)nb * 16, ro = 0;
    uint8_t *o = malloc(fn * 4 + rn * 2 + 1024); memcpy(o, f, pp); size_t op = pp; int changed = 0;
    for (uint32_t i = 0; i < nb; i++) {
        uint8_t *e = o + ip + (size_t)i * 16; uint32_t len = get32(e), cl = get32(e + 4); const uint8_t *pay = f + pp; pp += cl;
        if (cl >= 4 && pay[0] == T_TBL && e[13] == 4 && e[12] == C_ZSTD) {
            uint8_t *t = malloc((size_t)len * 3 + TSLACK + 1024);
            int64_t tn = tbl_encode(raw + ro, len, pay[1], pay[3] + 1, pay[2], t);
            if (tn > 0 && (size_t)tn <= len + TSLACK) {
                memcpy(o + op, pay, 4); put32(o + op + 4, (uint32_t)tn);
                size_t z = ZSTD_compress(o + op + 8, ZSTD_compressBound((size_t)tn), t, (size_t)tn, 3);
                if (!ZSTD_isError(z)) { put32(e + 4, (uint32_t)(8 + z)); e[13] = 3; op += 8 + z; ro += len; free(t); changed++; continue; }
            }
            free(t);
        }
        memcpy(o + op, pay, cl); op += cl; ro += len;
    }
    uint32_t hc = kolmo_crc32c(0, o, 28); hc = kolmo_crc32c(hc, o + 32, (size_t)nb * 16); put32(o + 28, hc);
    uint8_t *chk = malloc(rn + 1); int64_t d = kolmo_decompress(o, op, chk, rn, 1);
    if (d != (int64_t)rn || memcmp(chk, raw, rn)) { fprintf(stderr, "crafted frame does not decode (%lld), %d blocks changed\n", (long long)d, changed); return 1; }
    FILE *w = fopen(argv[3], "wb"); fwrite(o, 1, op, w); fclose(w);
    fprintf(stderr, "%s: %d of %u blocks rewritten to tflag 3, %zu bytes\n", argv[3], changed, nb, op);
    return changed ? 0 : 1;
}
