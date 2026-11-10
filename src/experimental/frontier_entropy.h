#ifndef KOLMO_FRONTIER_ENTROPY_H
#define KOLMO_FRONTIER_ENTROPY_H
#include <stddef.h>
#include <stdint.h>
enum { FRONTIER_BYTES=0, FRONTIER_QUALITY=1, FRONTIER_PLAIN=2 };
size_t frontier_entropy_encode(const uint8_t *src,size_t n,unsigned model,uint8_t *dst,size_t cap);
int frontier_entropy_decode(const uint8_t *src,size_t n,uint8_t *dst,size_t expected);
#endif
