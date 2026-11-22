/* Content segments: one plan per homogeneous run of a file that is not an archive (docs/TWOPASS.md).
 * A file that holds different kinds of data one after the other (prose, then a CSV export, then an array of doubles)
 * gets one plan from its analysis, chosen for whichever kind the planner's slice saw, and every other part is coded
 * with it. Here the whole file is read once, window by window (an 8 KiB probe of each 64 KiB), and each window gets a class: text, a
 * delimited table (its delimiter and field count), or binary. Runs shorter than 1 MiB are absorbed by their neighbours; each remaining
 * run of 1 MiB or more becomes a segment, with its own analysis and blocks, exactly as tar members do (no format
 * change: segments are a planning unit only). Measured on a file of four kinds (benchmark/rc3w2/twopass/RESULTS.md):
 * 4.2-13.3% smaller at balanced, hc and ratio; homogeneous files are left as one segment. The cuts depend on the input
 * bytes only. */
enum { SEG_W = 64u << 10, SEG_P = 8u << 10, SEG_MIN_W = 16 };   /* window; probe classified per window; shortest run kept (16 windows = 1 MiB) */
/* class of one window: 1 text, 2 binary, 256 + delimiter * 256 + fields: a delimited table. (A numeric-array class, the
 * word width whose byte planes beat the bytes, was tried: on 8 KiB probes it flickered on homogeneous float64 data and
 * split it, +1.9% at ratio on 100 MB; binary runs are left to the planner's own analysis.) */
static uint32_t seg_class(const uint8_t *s, size_t n) {
    size_t pr = 0, zero = 0;
    for (size_t i = 0; i < n; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 128; zero += !c; }
    if (pr * 100 >= n * 97 && zero * 1000 <= n) {
        static const uint8_t D[4] = {',', ';', '\t', '|'};
        for (int di = 0; di < 4; di++) {                  /* a table: most lines have the same number of fields (3 or more) */
            uint32_t cnt[65] = {0}, lines = 0; size_t i = 0;
            while (i < n) {
                const uint8_t *e = memchr(s + i, '\n', n - i); if (!e) break;
                uint32_t f = 1; for (const uint8_t *q = s + i; q < e; q++) f += *q == D[di];
                cnt[f < 64 ? f : 64]++; lines++; i = (size_t)(e - s) + 1;
            }
            int best = 0; for (int k = 3; k < 64; k++) if (cnt[k] > cnt[best]) best = k;
            if (lines >= 16 && best >= 3 && cnt[best] * 10 >= lines * 8) return 256u + (uint32_t)D[di] * 256u + (uint32_t)best;
        }
        return 1;
    }
    return 2;
}
/* Splits [0, n) into content segments; returns their count (1: the file stays whole). Only for inputs of 2 MiB or more. */
static uint32_t content_segments(const uint8_t *s, size_t n, uint64_t *ss, uint64_t *se, uint32_t maxs) {
    ss[0] = 0; se[0] = n;
    if (n < 2 * SEG_MIN_W * (size_t)SEG_W || maxs < 2) return 1;
    size_t nw = n / SEG_W;
    {   /* 32 probes spread over the file first: one class everywhere (most files) ends here */
        uint32_t c0 = seg_class(s, SEG_P); int differ = 0;
        for (int q = 1; q < 32 && !differ; q++) differ = seg_class(s + (size_t)q * (nw / 32) * SEG_W, SEG_P) != c0;
        if (!differ) return 1;
    }
    uint32_t *cl = malloc(nw * sizeof *cl), *rs = malloc((nw + 1) * sizeof *rs), *rc = malloc((nw + 1) * sizeof *rc);
    if (!cl || !rs || !rc) { free(cl); free(rs); free(rc); return 1; }
    for (size_t w = 0; w < nw; w++) cl[w] = seg_class(s + w * SEG_W, SEG_P);   /* the first 8 KiB of each window */
    /* runs of equal class; a run shorter than SEG_MIN_W windows joins the run before it (the first, the run after) */
    uint32_t nr = 0;
    for (size_t w = 0; w < nw; w++) { if (!nr || rc[nr - 1] != cl[w]) { rs[nr] = (uint32_t)w; rc[nr] = cl[w]; nr++; } }
    rs[nr] = (uint32_t)nw;
    for (int changed = 1; changed && nr > 1;) {
        changed = 0; uint32_t sh = 0, si = 0;                 /* the shortest run below the minimum goes first */
        for (uint32_t r = 0; r < nr; r++) { uint32_t len = rs[r + 1] - rs[r]; if (len < SEG_MIN_W && (!sh || len < sh)) { sh = len; si = r; } }
        if (!sh) break;
        uint32_t into = si ? si - 1 : si + 1;                 /* merge run si into a neighbour, then join equal neighbours */
        rc[si] = rc[into];
        uint32_t k = 0;
        for (uint32_t r = 0; r < nr; r++) { if (k && rc[k - 1] == rc[r]) continue; rs[k] = rs[r]; rc[k] = rc[r]; k++; }
        rs[k] = (uint32_t)nw; nr = k; changed = 1;
    }
    uint32_t k = 0;
    if (nr > 1) {
        size_t start = 0;
        for (uint32_t r = 1; r < nr && k + 1 < maxs; r++) {
            size_t cut = (size_t)rs[r] * SEG_W;
            /* a cut next to text moves to the line end at or after it (within one window), so no line is split */
            if (rc[r - 1] == 1 || rc[r - 1] >= 256 || rc[r] == 1 || rc[r] >= 256) {
                const uint8_t *e = memchr(s + cut, '\n', (cut + SEG_W < n ? SEG_W : n - cut)); if (e) cut = (size_t)(e - s) + 1;
            }
            if (cut <= start || cut >= n) continue;
            ss[k] = start; se[k] = cut; k++; start = cut;
        }
        ss[k] = start; se[k] = n; k++;
    }
    free(cl); free(rs); free(rc);
    if (k < 2) { ss[0] = 0; se[0] = n; return 1; }
    return k;
}
