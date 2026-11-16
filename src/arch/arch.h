/* CPU-specific kernels: the switches that select them and the entry points Kolmo's portable code calls.
 *
 * Every kernel has a portable C reference, which is the specification; a test compares the two (tests/test_arch.c,
 * `make check-arch`). Kernels change speed, never output. NEON is part of the arm64 baseline, so the NEON kernels are
 * used whenever the build targets arm64; `make NO_ASM=1` (-DKOLMO_NO_ASM) builds the C references only.
 * kolmo_force_c = 1 sends every call to its C reference at run time (tests and A/B timing only). */
#ifndef KOLMO_ARCH_H
#define KOLMO_ARCH_H
#include <stddef.h>
#include <stdint.h>

#if defined(__aarch64__) && defined(__ARM_NEON) && !defined(KOLMO_NO_ASM)
#define KOLMO_NEON 1
#else
#define KOLMO_NEON 0
#endif

extern int kolmo_force_c;

#if KOLMO_NEON
/* word transform decoder (src/word.inc, word_decode): decodes whole tokens (a literal run, then a word code) while
 * 80 bytes of input and 2048 bytes of output room remain, and stops at a token boundary, where the C loop goes on.
 * It also stops in front of a token the C loop rejects, so errors are reported by the reference. */
void kolmo_wd_dec_neon(const uint8_t **pp, const uint8_t *end, uint8_t **po, uint8_t *oe, const uint8_t *arena,
                       const uint8_t *wl, uint32_t nw, const int16_t *pidx, uint8_t capb, const uint8_t *special);
/* byte-plane split / merge with optional per-channel delta (shuffle_fwd / shuffle_inv in src/kolmo.c): words of 2, 4
 * or 8 bytes and 1 to 4 channels; returns 0 for other shapes, which take the C reference */
int kolmo_shuffle_fwd_neon(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out);
int kolmo_shuffle_inv_neon(const uint8_t *in, int64_t rows, int w, int c, int delta, uint8_t *out);
/* image prediction (src/arch/aarch64/image_neon.c): IMG8 inverse and forward residuals (kg_img8_back / kg_img8_fwd;
 * return 0 when the row is too short and the reference runs), IMG16 forward residuals from sample i > W on (returns
 * where it stopped), and the sum of absolute differences of 16-bit samples x[k * step], y[k * step], k < n (step 1
 * or 4; reads at most x[step * (n - 1)]) */
int kolmo_img8_back_neon(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);
int kolmo_img8_fwd_neon(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r);
int64_t kolmo_img16_fwd_neon(const uint16_t *a, int64_t i, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi);
uint64_t kolmo_sad_u16_neon(const uint16_t *x, const uint16_t *y, int64_t n, int step);
/* integer graph inverse for 32-bit elements (fi_inverse in src/kgraph.inc, planar residuals of 1/2/4/8 bytes, any
 * order and factor); returns 0 for other shapes */
int kolmo_int_inv4_neon(const uint8_t *r, size_t m, int ow, int ord, uint64_t g, uint64_t bias, uint64_t v1, uint64_t v2, uint8_t *o);
/* table decoder helpers (src/tbl2.inc): nonzero-length mask of up to 64 fields, indices of the numbers in a chunk
 * (writes up to 8 bytes past the last index), and the kind counts of a column (returns 1 if a kind is above 2) */
uint64_t kolmo_row_nonempty_neon(const uint16_t *rl, int n);
int kolmo_compact_rows_neon(const uint8_t *kd, int R, uint8_t *rows);
int kolmo_kind_count_neon(const uint8_t *kind, uint32_t cnt, uint32_t *n1, uint32_t *n2);
/* IMG16 inverse, left predictor (mode 0), from sample i >= 1 */
void kolmo_img16_left_neon(const uint8_t *lo, const uint8_t *hi, int64_t i, int64_t n, uint16_t *out);
/* DEC64 inverse (kg_dec64_inv): 8-byte values of one column from stream index q, 16 at a time; returns how many it
 * wrote (a multiple of 16) and updates the column's running values and exception pointer */
size_t kolmo_dec64_run_neon(const uint8_t *tag, const uint8_t *const *pl, int np, size_t q, size_t n, double P, int isint,
                            uint64_t dmask, uint64_t d2mask, uint64_t *prev, uint64_t *pd, uint8_t *w, size_t step,
                            uint64_t miss, const uint8_t **xp);
#endif

#endif
