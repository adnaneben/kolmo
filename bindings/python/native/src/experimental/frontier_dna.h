#ifndef KOLMO_FRONTIER_DNA_H
#define KOLMO_FRONTIER_DNA_H
#include <stddef.h>
#include <stdint.h>

/* A bounded independent nucleotide block. Zero from the encoder declines
 * the recipe, leaving the caller free to encode using its existing codec. */
int frontier_dna_probe(const uint8_t *src, size_t n);
size_t frontier_dna_encode(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
int frontier_dna_decode(const uint8_t *src, size_t n, uint8_t *dst, size_t raw);
#endif
