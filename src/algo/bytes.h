/* Small portable helpers shared by the algorithms in src/algo: little-endian loads and stores, bit scans.
   Copyright 2026 Adnane Aqartit and contributors. Apache-2.0. */
#ifndef KOLMO_ALGO_BYTES_H
#define KOLMO_ALGO_BYTES_H
#include <stdint.h>
#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define KOLMO_BIG_ENDIAN 1
#endif

static inline uint64_t kb_ld64(const void *p) { uint64_t v; memcpy(&v, p, 8);
#ifdef KOLMO_BIG_ENDIAN
    v = __builtin_bswap64(v);
#endif
    return v; }
static inline uint32_t kb_ld32(const void *p) { uint32_t v; memcpy(&v, p, 4);
#ifdef KOLMO_BIG_ENDIAN
    v = __builtin_bswap32(v);
#endif
    return v; }
static inline uint16_t kb_ld16(const void *p) { uint16_t v; memcpy(&v, p, 2);
#ifdef KOLMO_BIG_ENDIAN
    v = __builtin_bswap16(v);
#endif
    return v; }
static inline void kb_st32(void *p, uint32_t v) {
#ifdef KOLMO_BIG_ENDIAN
    v = __builtin_bswap32(v);
#endif
    memcpy(p, &v, 4); }
static inline void kb_st16(void *p, uint16_t v) {
#ifdef KOLMO_BIG_ENDIAN
    v = __builtin_bswap16(v);
#endif
    memcpy(p, &v, 2); }
static inline int kb_bsr32(uint32_t x) { return 31 ^ __builtin_clz(x); }        /* x != 0 */
static inline int kb_bsf64(uint64_t x) { return __builtin_ctzll(x); }          /* x != 0 */

/* Error codes of the block-sorting pipeline (the same values as libbsc's, so codec 5 maps one to one). */
enum { KB_OK = 0, KB_BAD_PARAMETER = -1, KB_NOT_ENOUGH_MEMORY = -2, KB_NOT_COMPRESSIBLE = -3, KB_NOT_SUPPORTED = -4,
       KB_UNEXPECTED_EOB = -5, KB_DATA_CORRUPT = -6 };
#endif
