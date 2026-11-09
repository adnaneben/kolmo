#ifndef KOLMO_FRONTIER_PCM_H
#define KOLMO_FRONTIER_PCM_H
#include <stdint.h>
#include <stddef.h>
int frontier_pcm_probe(const uint8_t *, size_t);
size_t frontier_pcm_encode(const uint8_t *, size_t, unsigned, uint8_t *, size_t);
int frontier_pcm_decode(const uint8_t *, size_t, uint8_t *, size_t);
#endif
