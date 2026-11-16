#ifndef KOLMO_FRONTIER_FASTQ_H
#define KOLMO_FRONTIER_FASTQ_H
#include <stddef.h>
#include <stdint.h>
int frontier_fastq_probe(const uint8_t *src,size_t n);
size_t frontier_fastq_encode(const uint8_t *src,size_t n,uint8_t *dst,size_t cap);
int frontier_fastq_decode(const uint8_t *src,size_t n,uint8_t *dst,size_t raw);
#endif
