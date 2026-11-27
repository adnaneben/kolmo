/* P2-E30 rep: research ENCODER for audio/typed arrays. A content-defined repeat detector routes each input to ONE
 * production-format frame:
 *   anchors = 2-byte-aligned positions whose 4-byte window hashes to (h >> 24) == 0 (about 1 in 256 positions);
 *   key     = a 64-bit hash of the 32 bytes after the anchor; a table of keys counts anchors already seen;
 *   repeat fraction = repeated anchors / anchors. Exact repeats at any distance (looped recordings) show up here,
 *   at any 2-byte offset, which a 16 KiB probe cannot see.
 *   repeat fraction >= REP_T (0.02) -> tp_prod3 (type-first program, production zstd-3 leaves: LZ keeps the repeats)
 *   otherwise                        -> tp_phuf_pg16 (type-first per-plane Huffman, 16 KiB pre-guard)
 * Frames are decoded by the unmodified production kolmo -d. usage: rep enc SRC DST | rep frac SRC */
#include "../P2-E13/p2e13.c"
#define REP_T 0.02
static double rep_frac(const uint8_t *s, size_t n) {
    if (n < 4096) return 0.0;
    size_t cap = 1; while (cap < n / 64) cap <<= 1; uint64_t *T = calloc(cap, 8); if (!T) return 0.0;
    size_t anchors = 0, hits = 0;
    for (size_t i = 0; i + 36 <= n; i += 2) {
        uint32_t w; memcpy(&w, s + i, 4); uint32_t h = w * 2654435761u;
        if (h >> 24) continue;
        uint64_t a, b, c, d; memcpy(&a, s + i + 4, 8); memcpy(&b, s + i + 12, 8); memcpy(&c, s + i + 20, 8); memcpy(&d, s + i + 28, 8);
        uint64_t k = (a * 0x9E3779B97F4A7C15ull) ^ (b * 0xC2B2AE3D27D4EB4Full) ^ (c * 0x165667B19E3779F9ull) ^ (d * 0x27D4EB2F165667C5ull) ^ w; k |= 1;
        size_t j = (size_t)(k >> 7) & (cap - 1); anchors++;
        for (;;) { if (!T[j]) { T[j] = k; break; } if (T[j] == k) { hits++; break; } j = (j + 1) & (cap - 1); }
        if (anchors * 2 >= cap) break;                     /* table full enough: decide on what was seen */
    }
    free(T); return anchors ? (double)hits / (double)anchors : 0.0;
}
static int rep_entry(int argc, char **argv);
__attribute__((constructor(101))) static void rep_ctor(int argc, char **argv, char **envp) { (void)envp; if (argc >= 3 && (!strcmp(argv[1], "enc") || !strcmp(argv[1], "frac"))) exit(rep_entry(argc, argv)); }
static int rep_entry(int argc, char **argv) {
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    size_t n; uint8_t *src = readfile(argv[2], &n); if (!src || !n) DIE("rep: read");
    double fr = rep_frac(src, n);
    if (!strcmp(argv[1], "frac")) { printf("%.4f\n", fr); free(src); return 0; }
    if (argc != 4) DIE("usage: rep enc SRC DST");
    int ci = -1; const char *want = fr >= REP_T ? "tp_prod3" : "tp_phuf_pg16";
    for (int c = 0; c < N13; c++) if (!strcmp(C13[c].name, want)) ci = c;
    uint8_t *f; size_t fn = enc13(src, n, ci, &f);
    FILE *o = fopen(argv[3], "wb"); if (!o || fwrite(f, 1, fn, o) != fn || fclose(o)) DIE("rep: write");
    free(f); free(src); return 0;
}
