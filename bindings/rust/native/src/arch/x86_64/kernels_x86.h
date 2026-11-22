/* x86-64 kernels: declarations for the dispatch table (src/arch/cpu.c). */
#ifndef KOLMO_ARCH_X86_KERNELS_H
#define KOLMO_ARCH_X86_KERNELS_H
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
void kolmo_med8_inv_sse2(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);
void kolmo_med8_inv_sse41(const uint8_t *r, size_t m, size_t W, unsigned C, uint8_t *p);
void kolmo_planes_split_sse2(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out);
void kolmo_planes_merge_sse2(const uint8_t *in, size_t rows, unsigned w, int delta, uint8_t *out);
void kolmo_cm_dot_sse41(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]);
void kolmo_cm_dot_avx2(const int32_t *x, int n, const int32_t *w1, const int32_t *w2, const int32_t *w3, const int32_t *w4, int64_t s[4]);
void kolmo_cm_dot16_sse41(const int16_t *x, int n, const int16_t *w1, const int16_t *w2, const int16_t *w3, const int16_t *w4, int32_t s[4]);
void kolmo_cm_dot16_avx2(const int16_t *x, int n, const int16_t *w1, const int16_t *w2, const int16_t *w3, const int16_t *w4, int32_t s[4]);
void kolmo_cm_train16_sse41(const int16_t *x, int n, int16_t *w1, int16_t *w2, int16_t *w3, int16_t *w4, const int e[4]);
void kolmo_cm_train16_avx2(const int16_t *x, int n, int16_t *w1, int16_t *w2, int16_t *w3, int16_t *w4, const int e[4]);
void kolmo_cm_train_sse41(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift);
void kolmo_cm_train_avx2(const int32_t *x, int n, int32_t *w1, int32_t *w2, int32_t *w3, int32_t *w4, const int e[4], int shift);
void kolmo_med8_rows_sse2(const uint8_t *r, size_t W, unsigned C, uint8_t *p, size_t nrows);
void kolmo_med8_rows_sse41(const uint8_t *r, size_t W, unsigned C, uint8_t *p, size_t nrows);
int kolmo_vid_strip_enc_sse2(const uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, uint8_t *res);
void kolmo_vid_strip_dec_sse2(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res);
void kolmo_vid_strip_dec_sse41(uint8_t *cur, const uint8_t *prev, size_t w, size_t y0, size_t y1, int k, const uint8_t *res);
uint64_t kolmo_sad16_sse2(const uint16_t *x, const uint16_t *y, size_t n);
uint64_t kolmo_sad16_avx2(const uint16_t *x, const uint16_t *y, size_t n);
void kolmo_img16_fwd_sse2(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi);
void kolmo_img16_fwd_avx2(const uint16_t *a, int64_t n, int64_t W, int mode, uint8_t *lo, uint8_t *hi);
void kolmo_med8_fwd_sse2(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r);
void kolmo_med8_fwd_avx2(const uint8_t *p, size_t m, size_t W, unsigned C, uint8_t *r);
void kolmo_cm_hash_sse41(const uint32_t *h, int n, uint32_t add, uint32_t *out);
void kolmo_cm_hash_avx2(const uint32_t *h, int n, uint32_t add, uint32_t *out);
#endif
