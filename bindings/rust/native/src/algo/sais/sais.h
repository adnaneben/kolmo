/* Suffix sorting, BWT and inverse BWT: the subset of libsais 2.12.1 that Kolmo uses, taken over into src/algo/sais/.
   Copyright (c) 2021-2026 Ilya Grebnov <ilya.grebnov@gmail.com>; changes Copyright 2026 Adnane Aqartit and
   contributors. Apache License 2.0 with LLVM Exceptions (see LICENSE in this directory). README.md lists the changes.

   All three take `threads` (<= 1: the caller's thread only); the output never depends on it. */
#ifndef KOLMO_ALGO_SAIS_H
#define KOLMO_ALGO_SAIS_H
#include <stdint.h>

/* libsais_bwt_aux: BWT of T[0..n) into U (T == U allowed), A is n int32 of scratch. r (a power of two >= 2) is the
   sampling interval of the auxiliary indexes: I[i] = rank of the suffix starting at i * r, for i <= (n - 1) / r;
   I[0] is the primary index. Returns 0, -1 for bad arguments, -2 when out of memory. */
int32_t kolmo_sais_bwt_aux(const uint8_t *T, uint8_t *U, int32_t *A, int32_t n, int32_t r, int32_t *I, int32_t threads);

/* libsais_unbwt_aux: inverse of the above (T == U allowed), A is n + 1 int32 of scratch. The indexes let several
   threads decode at once. Returns 0, -1 for bad arguments or indexes out of range, -2 when out of memory. */
int32_t kolmo_sais_unbwt_aux(const uint8_t *T, uint8_t *U, int32_t *A, int32_t n, int32_t r, const int32_t *I, int32_t threads);

/* libsais_unbwt with primary index i: kolmo_sais_unbwt_aux with r = n. */
int32_t kolmo_sais_unbwt(const uint8_t *T, uint8_t *U, int32_t *A, int32_t n, int32_t i, int32_t threads);
#endif
