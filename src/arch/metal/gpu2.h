/* rc3 wave 2, gpu2 lane: GPU kernels for the "speed tier" candidates, and their CPU references. Apache-2.0, Kolmo
 * contributors. Research code: not part of the default build (benchmark/rc3w2/gpu2/build.sh builds it).
 *
 * Every GPU function has a portable C reference in gpu2_ref.c that is its specification; the GPU result is the same
 * bytes (benchmark/rc3w2/gpu2/gpu2bench check). Integer arithmetic only. Measurements: benchmark/rc3w2/gpu2/RESULTS.md.
 *
 *  table front end  field split + FNV-1a hash + decimal parse (one pass per line), then same-as-above flags and delta
 *                   residuals per column (one pass per field). Output column-major (index c * rows + r).
 *  hist             byte histograms of many segments at once, and an integer (Q16) order-0 entropy estimate of each.
 *  hist o1          order-1 (previous byte, byte) histogram, 65536 bins.
 *  krw              wide interleaved rANS: per segment one 12-bit frequency table and L lanes; symbol j of a segment
 *                   belongs to lane j mod L. 32-bit states, 16-bit renormalisation words.
 *  formulas         the map of GR6+'s formula search (src/codecs/gr6.c, g6_formulas): screen and score every
 *                   candidate, the reduce (best per target, in the CPU's order) is done by the caller. */
#ifndef KOLMO_ARCH_METAL_GPU2_H
#define KOLMO_ARCH_METAL_GPU2_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- table front end ---------------- */
enum { KG2_CSV = 0, KG2_JSONL = 1 };
#define KG2_LINE_TILE 4096u          /* bytes per line-index tile */
typedef struct {
    uint32_t rows, cols;             /* rows = lines; cols = K (fields kept per line) */
    uint32_t cap_rows;               /* rows the arrays below have room for (the column stride is rows) */
    uint32_t *line_start;            /* rows + 1: start of each line; [rows] = n, or n + 1 when d[n-1] != '\n' */
    uint32_t *off;                   /* cols * rows: field start (absolute) */
    uint16_t *len;                   /* field length, clamped to 65535 */
    uint32_t *hash;                  /* FNV-1a 32 of the field bytes */
    int64_t *val;                    /* parsed value (0 when not a number) */
    uint8_t *fmt;                    /* bit 7: number; bits 0-4: decimals */
    uint8_t *rowflag;                /* rows: bit 0 field count != cols */
    uint8_t *fl;                     /* bit 0 number, bit 1 same as above, bit 2 delta valid, bits 3-7 decimals */
    int64_t *res;                    /* delta from the field above when valid, else the value */
} kg2_table;

/* number of lines of d[0..n) (a final line without '\n' counts) */
uint32_t kg2_ref_count_lines(const uint8_t *d, size_t n);
/* line starts of lines whose start lies in [b, e) (lines start at 0 and after every '\n' that is not the last byte);
 * writes them to out, returns how many */
uint32_t kg2_ref_line_starts(const uint8_t *d, size_t n, size_t b, size_t e, uint32_t *out);
/* pass 1 (fused split + hash + parse) for rows [r0, r1); t->line_start must be complete */
void kg2_ref_fields(const uint8_t *d, size_t n, int mode, uint8_t sep, kg2_table *t, uint32_t r0, uint32_t r1);
/* pass 2 (same-as-above flags and residuals) for rows [r0, r1) of every column */
void kg2_ref_resid(const uint8_t *d, kg2_table *t, uint32_t r0, uint32_t r1);
/* field count of the first line (the header of a CSV, the first record of JSON lines) */
uint32_t kg2_ref_first_nf(const uint8_t *d, size_t n, int mode, uint8_t sep);

/* ---------------- histograms and entropy ---------------- */
/* Q16 log2 of x (0 for x = 0) from the 1024-entry table kg2_log_table() fills (the GPU gets the same table) */
void kg2_log_table(uint32_t tab[1024]);
uint64_t kg2_ref_log2q16(const uint32_t tab[1024], uint64_t x);
/* histogram of segment s = [off[s], off[s+1]) into h + 256 * s, and its order-0 cost in Q16 bits into bits[s] */
void kg2_ref_hist_segs(const uint8_t *d, const uint64_t *off, uint32_t s0, uint32_t s1, uint32_t *h, uint64_t *bits,
                       const uint32_t tab[1024]);
/* order-1 histogram of d[b..e) (pairs (d[i-1], d[i]) for i in [max(b,1), e)) added into h[65536] */
void kg2_ref_hist_o1(const uint8_t *d, size_t b, size_t e, uint32_t *h);

/* ---------------- wide interleaved rANS (krw) ---------------- */
#define KRW_PROB 12u
#define KRW_M (1u << KRW_PROB)
#define KRW_LOW (1u << 16)
/* frequencies (sum 4096, every present symbol >= 1) from counts; all zero when n = 0 */
void krw_norm(const uint32_t cnt[256], uint64_t n, uint16_t f[256]);
/* bytes of the coded segment of length m with L lanes, given its word count */
static inline uint64_t krw_seg_header(uint32_t L) { return 512u + 8u * (uint64_t)L; }
/* encode segment src[0..m) with L lanes and table f into: states[L], counts[L] and the words, lane by lane, in decode
 * order. scratch holds at least m + 2 * L words. Returns the total words. */
uint64_t krw_ref_encode_seg(const uint8_t *src, uint32_t m, uint32_t L, const uint16_t f[256], uint32_t *states,
                            uint32_t *counts, uint16_t *words, uint16_t *scratch);
/* whole-buffer form used by the benchmarks (with krw_fast_encode_seg): segments of seg bytes (the last may be shorter), each coded as
 * [f: 256 x u16][states: L x u32][counts: L x u32][words]. Encode returns the coded size (0 on error), out must hold
 * krw_bound(n, seg, L) bytes. seg_out (nseg + 1) receives every segment's offset in out. */
uint64_t krw_bound(uint64_t n, uint32_t seg, uint32_t L);
uint64_t krw_ref_encode_range(const uint8_t *in, uint64_t n, uint32_t seg, uint32_t L, uint32_t s0, uint32_t s1,
                              uint8_t *seg_buf, uint64_t seg_cap, uint64_t *seg_size);
int krw_ref_decode_seg(const uint8_t *code, uint64_t code_n, uint32_t L, uint8_t *dst, uint32_t m);
/* the same bytes, organised for a CPU (rows of lanes; L <= 1024; scratch holds L * (m / L + 3) words) */
uint64_t krw_fast_encode_seg(const uint8_t *src, uint32_t m, uint32_t L, const uint16_t f[256], uint32_t *states,
                             uint32_t *counts, uint16_t *words, uint16_t *scratch);
int krw_fast_decode_seg(const uint8_t *code, uint64_t code_n, uint32_t L, uint8_t *dst, uint32_t m);

/* ---------------- GPU (Metal) side ---------------- */
int kg2_metal_available(void);
double kg2_metal_setup_seconds(void);
double kg2_metal_last_gpu_seconds(void);          /* GPU time of the last call's command buffers */
void *kg2_metal_alloc(size_t n);                  /* page-aligned host memory the GPU can map without a copy */
void kg2_metal_free(void *p, size_t n);

/* table front end over d[0..n) (n < 2^32 - 1); fills every array of t (allocated by the caller with kg2_metal_alloc
 * for zero-copy, or any memory, which is then copied); t->rows must be kg2_ref_count_lines(). Returns 0 or < 0. */
int kg2_metal_table(const uint8_t *d, size_t n, int mode, uint8_t sep, kg2_table *t);
/* histograms and Q16 bits of every segment, as kg2_ref_hist_segs over all segments */
int kg2_metal_hist_segs(const uint8_t *d, size_t n, const uint64_t *off, uint32_t nseg, uint32_t *h, uint64_t *bits);
int kg2_metal_hist_o1(const uint8_t *d, size_t n, uint32_t *h);
/* krw encode of segments given by off[0..nseg] (any lengths up to 2^24), L lanes (32..1024, a multiple of 32); out
 * receives the segments back to back, seg_out[s] their offsets (seg_out[nseg] = total). Returns 0 or < 0. */
int kg2_metal_krw_encode(const uint8_t *d, size_t n, const uint64_t *off, uint32_t nseg, uint32_t L, uint8_t *out,
                         size_t cap, uint64_t *seg_out);
/* code must be 4-byte aligned (in a kg2_metal_alloc block: at a multiple of 4 from its start) */
int kg2_metal_krw_decode(const uint8_t *code, size_t code_n, const uint64_t *seg_out, const uint64_t *off,
                         uint32_t nseg, uint32_t L, uint8_t *dst, size_t n);
/* video strip prediction (kvid, src/arch/metal/kvid.metal) without copies: file must come from kg2_metal_alloc */
int kg2_metal_kvid(const uint8_t *file, size_t n, const uint64_t *frame_at, uint32_t frames, int np, const size_t *w,
                   const size_t *h, uint8_t *res, size_t res_n, uint8_t *flags, size_t flags_n);

/* formula search map (GR6+, src/codecs/gr6.c g6_formulas). The row sample is column-major: V[c * R + r], D[c * R + r]
 * (decimals), OK[c * R + r] (parsed), rows <= R. A candidate is (target t, inputs a and b, type ty, scale index q);
 * one-input types have b = a. Screen: g6_formulas' first loop (up to 64 usable rows among the first 3000); out[i] is
 * its (hit, seen), seen = -1 when the candidate fails the screen. Score: g6_formulas' second loop over rows [1, lim)
 * with its exact early exit (cannot reach one half), plus a second exact exit: the candidate cannot reach the rate
 * bound[t] (a rate some other candidate of t reaches): out[i] = (-1, -1) then; bound[t].seen = 0 means none. Taking,
 * in the CPU's order, the first candidate with seen >= 32, 2 hit > seen and the highest hit / seen gives exactly
 * g6_formulas' choice (benchmark/rc3w2/gpu2/g6wrap.c does that reduce). */
typedef struct { uint16_t t, a, b, tq; } kg2_cand;                /* tq = ty << 8 | q */
typedef struct { int32_t hit, seen; } kg2_score;
int kg2_metal_formula_screen(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t K, uint32_t R,
                             uint32_t rows, const kg2_cand *cand, uint32_t nc, kg2_score *out);
int kg2_metal_formula_score(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t K, uint32_t R,
                            uint32_t rows, uint32_t lim, const kg2_cand *cand, uint32_t nc, const kg2_score *bound,
                            kg2_score *out);
/* a new sample follows (the GPU's row-major copy of the last one, GPU2_FM_ROWMAJOR=1, is stale) */
void kg2_metal_formula_reset(void);
/* the same on the CPU, for candidates [i0, i1): the reference of both kernels */
void kg2_ref_formula_screen(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t R, uint32_t rows,
                            const kg2_cand *cand, uint32_t i0, uint32_t i1, kg2_score *out);
void kg2_ref_formula_score(const int64_t *V, const uint8_t *D, const uint8_t *OK, uint32_t R, uint32_t lim,
                           const kg2_cand *cand, uint32_t i0, uint32_t i1, const kg2_score *bound, kg2_score *out);
/* g6_fpred of src/codecs/gr6.c, written with 64-bit halves instead of __int128 (the GPU's form); same results */
int kg2_fpred(int type, int ke, int64_t va, int da, int okl, int64_t vla, int dla, int64_t vb, int db, int oklt,
              int64_t vlt, int dlt, int dt, int64_t *out);

#ifdef __cplusplus
}
#endif
#endif
