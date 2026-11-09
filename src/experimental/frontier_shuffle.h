#ifndef KOLMO_FRONTIER_SHUFFLE_H
#define KOLMO_FRONTIER_SHUFFLE_H
#include <stddef.h>
#include <stdint.h>
/* Return zero on machines or layouts that should use the portable transform. */
int frontier_shuffle16(const uint8_t *,size_t,unsigned,int,uint8_t *,int);
#endif
