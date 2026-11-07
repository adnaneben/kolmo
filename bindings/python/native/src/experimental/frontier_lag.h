#ifndef KOLMO_FRONTIER_LAG_H
#define KOLMO_FRONTIER_LAG_H
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t near, stride, period; } frontier_lags;
int frontier_lag_probe(const uint8_t *,size_t,frontier_lags *);
size_t frontier_lag_encode(const uint8_t *,size_t,const frontier_lags *,uint8_t *,size_t);
int frontier_lag_decode(const uint8_t *,size_t,uint8_t *,size_t);
#endif
