/* blosc2_kolmo: Kolmo as a Blosc2 codec plugin.
 *
 * Licence: Apache-2.0.
 *
 * CODEC ID: PLACEHOLDER. 244 is in Blosc2's user range (BLOSC2_USER_REGISTERED_CODECS_START 160 .. 255), which an
 * application registers locally with blosc2_register_codec and which is never assigned globally, so data written with
 * it is only readable by a program that registers the same number. A global ID (32-159) is requested in
 * REGISTRY-SUBMISSION.md; once granted, set BLOSC_CODEC_KOLMO to it (the decoder is the same for both).
 *
 * Settings:
 *   cparams.clevel         1..9: the Kolmo level of the same number (Kolmo's own scale, docs/LEVELS.md). Blosc2
 *                          stores clevel 0 uncompressed without calling the codec.
 *   cparams.compcode_meta  0 = use clevel; 1..10 = this Kolmo level instead (10 = ultra, the archive tier).
 *   cparams.typesize       recorded by Blosc2 in every chunk header; Kolmo finds numeric word widths itself.
 *   cparams.filters        Kolmo transforms numeric data itself (byte planes, deltas), so BLOSC_NOSHUFFLE is usually
 *                          as small and faster; see the benchmark in README.md.
 *   cparams.blocksize      Kolmo codes each Blosc2 block on its own; blocks of 256 KiB or more give it room to find
 *                          structure (0 = Blosc2's choice, often 32-256 KiB).
 * Each block is coded on one thread: Blosc2 runs blocks in parallel itself (cparams.nthreads).
 */
#ifndef BLOSC2_KOLMO_H
#define BLOSC2_KOLMO_H

#include <stdint.h>
#include "blosc2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLOSC_CODEC_KOLMO_USER 244     /* PLACEHOLDER from the user range 160-255: not registered */
#ifndef BLOSC_CODEC_KOLMO
#define BLOSC_CODEC_KOLMO BLOSC_CODEC_KOLMO_USER
#endif
#define BLOSC2_KOLMO_VERSION 1

int blosc2_kolmo_encoder(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len, uint8_t meta,
                         blosc2_cparams *cparams, const void *chunk);
int blosc2_kolmo_decoder(const uint8_t *input, int32_t input_len, uint8_t *output, int32_t output_len, uint8_t meta,
                         blosc2_dparams *dparams, const void *chunk);

/* Registers the codec under BLOSC_CODEC_KOLMO with name "kolmo" (call after blosc2_init). 0, or Blosc2's error. */
int blosc2_kolmo_register(void);

#ifdef __cplusplus
}
#endif
#endif
