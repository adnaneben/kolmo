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
#endif

#endif
