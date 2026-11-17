/* GR6+ formula search harness (g6wrap.c). Apache-2.0, Kolmo contributors. */
#ifndef G6WRAP_H
#define G6WRAP_H
#include <stddef.h>
#include <stdint.h>
#include "../../../src/arch/metal/gpu2.h"
#define G6W_R 20000
typedef struct { int t, a, b, ke, type; } g6w_formula;
typedef struct { int64_t *V; uint8_t *D, *OK; uint32_t K, rows; } g6w_sample_t;
uint8_t g6w_sep(const uint8_t *d, size_t n);
int g6w_reference(const uint8_t *d, size_t n, uint8_t sep, g6w_formula *out);     /* g6_formulas itself */
int g6w_sample(const uint8_t *d, size_t n, uint8_t sep, g6w_sample_t *S);           /* 1: formulas possible */
void g6w_sample_free(g6w_sample_t *S);
uint32_t g6w_candidates(const g6w_sample_t *S, kg2_cand **out);
int g6w_reduce(const kg2_cand *c, uint32_t nc, const kg2_score *sc, g6w_formula *out);
size_t kw_rans_compress(const uint8_t *in, size_t n, uint8_t *out, size_t cap);   /* Kolmo's rANS (rans.c) */
int kw_rans_decompress(const uint8_t *in, size_t cn, uint8_t *out, size_t n);
#endif
