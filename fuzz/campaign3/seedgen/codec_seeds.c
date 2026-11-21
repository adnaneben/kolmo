/* Seeds for fuzz/fuzz_codecs.c from real frames: for each block of a CZN1 frame whose codec is one fuzz_codecs decodes
 * directly (16, 17, 18, 19, 25, 26, the graph block 12 and codec 5), write sel | n (3 bytes) | flags | payload after the
 * plan bytes. For a YUV4MPEG2 input file (not a frame) write the kvid layout (decoder 2). Blocks over 1 MiB are skipped.
 * usage: codec_seeds OUTDIR FILE...   (build: see fuzz/campaign3/make_seeds.sh) */
#include "../../../src/kolmo.c"
#include <stdio.h>

static void emit(const char *dir, const char *tag, int k, int which, const uint8_t *p, size_t cn, size_t n) {
    if (n > (1u << 20) || cn > (2u << 20)) return;
    char path[4096]; snprintf(path, sizeof path, "%s/%s_b%d_d%d", dir, tag, k, which);
    FILE *f = fopen(path, "wb"); if (!f) return;
    uint8_t h[5] = {(uint8_t)which, (uint8_t)n, (uint8_t)(n >> 8), (uint8_t)(n >> 16), 0};
    fwrite(h, 1, 5, f); fwrite(p, 1, cn, f); fclose(f);
}
int main(int argc, char **argv) {
    for (int a = 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *s = malloc(L ? (size_t)L : 1); if (!s || fread(s, 1, (size_t)L, f) != (size_t)L) { fclose(f); free(s); continue; }
        fclose(f);
        const char *tag = strrchr(argv[a], '/'); tag = tag ? tag + 1 : argv[a];
        if (L > 10 && !memcmp(s, "YUV4MPEG2 ", 10)) {
            size_t tn; uint8_t *t = kv_transform(s, (size_t)L, &tn);
            if (t) emit(argv[1], tag, 0, 2, t, tn, (size_t)L);
            free(t); free(s); continue;
        }
        parsed_t P;
        if (parse(s, (size_t)L, &P, NULL)) { free(s); continue; }
        for (uint32_t i = 0; i < P.nb; i++) {
            const uint8_t *e = P.idx + (size_t)i * IDX; size_t len = get32(e); int codec = e[12];
            const uint8_t *pay = s + P.poff[i]; size_t cl = (size_t)(P.poff[i + 1] - P.poff[i]);
            if (P.ver >= 2) { if (cl < 4) continue; pay += 4; cl -= 4; }
            int which = codec == C_BWT ? 0 : codec == C_AUD ? 1 : codec == C_CMP ? 6 : codec == C_GR6 ? 7 : codec == C_CMI ? 8 :
                        codec == C_GRAPH ? 9 : codec == C_BSC5 ? 11 : codec == C_FQ ? 120 : -1;
            /* with a transform flag the codec stream follows 0, 4 or 8 bytes of transform metadata */
            if (which >= 0) for (size_t m = 0; m <= (e[13] ? 8 : 0); m += 4) if (m < cl) emit(argv[1], tag, (int)(i * 4 + m / 4), which, pay + m, cl - m, m == 4 ? get32(pay) : len);
            if (codec == C_VID && P.nb == 1) {      /* the nested frame of a video block */
                char path[4096]; snprintf(path, sizeof path, "%s/%s_nested", argv[1], tag);
                FILE *g = fopen(path, "wb"); if (g) { fwrite(pay, 1, cl, g); fclose(g); }
            }
        }
        free(P.poff); free(P.roff); free(s);
    }
    return 0;
}
