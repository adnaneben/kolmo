#ifndef KOLMO_FRONTIER_RUNS_H
#define KOLMO_FRONTIER_RUNS_H
#include <stdint.h>
#include <stddef.h>
size_t frontier_runs_encode(const uint8_t *,size_t,uint8_t *,size_t);
int frontier_runs_decode(const uint8_t *,size_t,uint8_t *,size_t);
#endif
