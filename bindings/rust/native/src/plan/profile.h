/* Trained profiles (kolmo train, kolmo -P): the interface between the core (src/kolmo.c) and src/plan/profile.c.
 *
 * Until the structure lane splits src/kolmo.c into separate translation units, src/plan/profile.c,
 * src/plan/target.c and src/api/features.c are compiled as part of src/kolmo.c (included at its end, like the .inc
 * files), so every existing build (Makefile, the tests that include kolmo.c, the Python and Rust bindings) picks them
 * up unchanged. This header declares what the core calls; the definitions follow later in the same unit.
 *
 * File format: SPEC.md, "Trained profiles (KPF1)". Frames that use a profile: SPEC.md, "Profile frames". */
#ifndef KOLMO_PLAN_PROFILE_H
#define KOLMO_PLAN_PROFILE_H
#include <stddef.h>
#include <stdint.h>

struct kolmo_profile;

/* The profile's id (low 32 bits of XXH3-64 of its sections), written into every frame that uses it. */
static uint32_t kp_id(const struct kolmo_profile *P);
/* Block codecs that take the profile's dictionaries: zstd (magicless frames, the ZDIC dictionary), Brotli and LZ4
 * (the RDIC raw dictionary). Return 0 when the codec is not one of them (the caller codes as usual) and set *handled. */
static size_t kp_codec_compress(const struct kolmo_profile *P, void **zc, int codec, int level, const uint8_t *in,
                                size_t n, uint8_t *out, size_t cap, int *handled);
static int kp_codec_decompress(const struct kolmo_profile *P, void **zd, int codec, const uint8_t *in, size_t cn,
                               uint8_t *out, size_t n, int *handled);
/* Compact single-block profile frame ("CZC1"): original size, or a negative error. */
static int64_t kp_compact_original_size(const uint8_t *src, size_t n);

#endif
