/* libFuzzer target for the container extensions of rc3 (SPEC.md, "Container extensions"): the KEXT trailer and
 * section walk (src/core/extension.c), the IDX1 index and its query engine (src/core/index.c, column-split table
 * blocks of flag 13 in src/transforms/table_cols.c) and DDP1 deduplicating archives (src/core/dedup.c).
 *
 * Input: sel u8 | q u8 | frame (with or without extension sections).
 *   sel bit 0: recompute the frame header CRC, the KEXT trailer and section CRCs and the DDP1 table frame's header CRC
 *              before parsing (fuzz/fuzz_fixup.h), so that mutations reach the parsers behind the checksums.
 *   q picks the query: rows range, column (none, "#1".."#3", a name taken from kolmo_index_info), a fixed pattern
 *   taken from the frame bytes or one of a few regular expressions, the KOLMO_Q_* flags and 1 or 2 threads.
 * Invariants (traps):
 *   - a query run with and without the index (KOLMO_Q_NOINDEX) must give the same rows when both succeed and the
 *     checksums were not recomputed (SPEC: only a forged index with a valid CRC may change the answer);
 *   - an archive member extracted alone must equal the same member from kolmo_archive_extract_all, and its size the
 *     size kolmo_archive_stat reports. */
#include "../src/kolmo.c"
#include "fuzz_fixup.h"

typedef struct { uint8_t *p; size_t n, cap; } fz_out;
static int fz_sink(void *ctx, const uint8_t *p, size_t n) {
    fz_out *o = ctx;
    if (o->n + n > o->cap) {                     /* keep at most 4 MiB of output; then stop the query */
        if (o->n + n > (4u << 20)) return 1;
        size_t c = o->cap ? o->cap * 2 : 4096; while (c < o->n + n) c *= 2;
        uint8_t *q = realloc(o->p, c); if (!q) return 1; o->p = q; o->cap = c;
    }
    memcpy(o->p + o->n, p, n); o->n += n; return 0;
}

typedef struct { const uint8_t *src; size_t n; uint64_t i; int bad; } fz_arc;
static int fz_each(void *ctx, const char *name, const uint8_t *data, uint64_t size, uint32_t mode, int64_t mtime) {
    fz_arc *a = ctx; (void)name; (void)mode; (void)mtime;
    if (a->i < 4 && size <= (4u << 20)) {
        uint8_t *b = malloc(size ? size : 1);
        if (b) {
            int64_t r = kolmo_archive_extract(a->src, a->n, a->i, b, size, 1);
            if (r >= 0 && ((uint64_t)r != size || memcmp(b, data, size))) a->bad = 1;
            free(b);
        }
    }
    a->i++;
    return a->i > 64;                             /* stop after 64 members */
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3 || size > (8u << 20)) return 0;
    uint8_t sel = data[0], q = data[1];
    size_t n = size - 2;
    uint8_t *s = malloc(n); if (!s) return 0;
    memcpy(s, data + 2, n);
    int fixed = sel & 1;
    if (fixed) { fz_fix_kext(s, n); fz_fix_frame(s, n); }
    /* only frames whose decoded size is modest: the query and the archive decode whole blocks */
    int64_t orig = kolmo_original_size(s, n);
    if (orig > (int64_t)(32u << 20)) { free(s); return 0; }

    /* IDX1 */
    int kind = 0; uint8_t delim = 0; char names[512];
    int64_t rows = kolmo_index_info(s, n, &kind, &delim, names, sizeof names);
    {
        kolmo_query Q; memset(&Q, 0, sizeof Q);
        Q.threads = 1 + (q >> 7);
        Q.row_first = (q & 3) == 1 ? (uint64_t)(sel >> 4) : 0;
        Q.row_count = (q & 3) == 2 ? (uint64_t)(sel >> 1 & 7) : 0;
        char col[64];
        switch (q >> 2 & 3) {
        case 0: Q.column = NULL; break;
        case 1: snprintf(col, sizeof col, "#%d", 1 + (sel >> 5 & 3)); Q.column = col; break;
        case 2: { size_t L = 0; if (rows > 0) { while (L < sizeof col - 1 && names[L] && names[L] != '\n') { col[L] = names[L]; L++; } }
                  col[L] = 0; Q.column = L ? col : kind == KOLMO_IDX_JSON ? ((sel & 8) ? "user" : "id") : "#2"; break; }
        default: Q.column = "nosuchcolumn"; break;
        }
        char pat[16];
        static const char *const rx[] = { "a.c", "^[0-9]+,", "(x|y)+z$", "[[:alpha:]]{3}" };
        switch (q >> 4 & 3) {
        case 0: Q.pattern = NULL; break;
        case 1: { size_t L = n < 24 ? 0 : 3 + (sel >> 3 & 7); size_t o = n > 40 ? (size_t)sel * 7 % (n - 20) : 0;
                  for (size_t k = 0; k < L; k++) { uint8_t c = s[o + k]; pat[k] = (char)(c >= 32 && c < 127 ? c : 'a'); }
                  pat[L] = 0; Q.pattern = pat; Q.flags |= KOLMO_Q_FIXED; break; }
        case 2: Q.pattern = rx[sel >> 6]; break;
        default: Q.pattern = "the"; Q.flags |= KOLMO_Q_FIXED | KOLMO_Q_WORD; break;
        }
        if (q & 64) Q.flags |= KOLMO_Q_LINENO;
        if ((q & 0x30) && (sel & 2)) Q.flags |= KOLMO_Q_ICASE;
        if (sel & 4) Q.flags |= KOLMO_Q_COUNT;
        fz_out a = {0}, b = {0}; kolmo_query_stats st;
        int64_t ra = kolmo_query_run(s, n, &Q, fz_sink, &a, &st);
        Q.flags |= KOLMO_Q_NOINDEX;
        int64_t rb = kolmo_query_run(s, n, &Q, fz_sink, &b, NULL);
        if (!fixed && rows > 0 && ra >= 0 && rb >= 0 && (ra != rb || a.n != b.n || (a.n && memcmp(a.p, b.p, a.n)))) __builtin_trap();
        free(a.p); free(b.p);
    }

    /* DDP1 */
    int64_t cnt = kolmo_archive_count(s, n);
    if (cnt > 0) {
        for (uint64_t i = 0; i < (uint64_t)cnt && i < 4; i++) {
            char name[64]; uint64_t fsz = 0; uint32_t mode = 0; int64_t mt = 0;
            if (kolmo_archive_stat(s, n, i, name, (q & 8) ? 4 : sizeof name, &fsz, &mode, &mt)) continue;
            if (fsz > (4u << 20)) continue;
            uint8_t *b = malloc(fsz ? fsz : 1); if (!b) continue;
            int64_t r = kolmo_archive_extract(s, n, i, b, (q & 1) && fsz ? fsz - 1 : fsz, 1 + (q >> 7));
            if (!(q & 1) && r >= 0 && (uint64_t)r != fsz) __builtin_trap();
            free(b);
        }
        fz_arc A = { s, n, 0, 0 };
        int64_t r = kolmo_archive_extract_all(s, n, fz_each, &A, 1);
        if (!fixed && r >= 0 && A.bad) __builtin_trap();
    }
    /* the plain decoder must accept or reject the same frame, whatever follows it */
    if (orig >= 0 && orig <= (int64_t)(8u << 20)) {
        uint8_t *o = malloc((size_t)orig + 1);
        if (o) kolmo_decompress(s, n, o, (size_t)orig, 1);
        free(o);
    }
    uint32_t pid; kolmo_frame_profile_id(s, n, &pid);
    free(s);
    return 0;
}
