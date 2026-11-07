#ifndef KOLMO_FRONTIER_TABLE_H
#define KOLMO_FRONTIER_TABLE_H
#include <stddef.h>
#include <stdint.h>
/* Literal rectangular rows, split on published separator bytes. A row that
 * does not fit can always use an existing CZN block recipe. */
size_t frontier_table_encode(const uint8_t *, size_t, uint8_t, uint8_t *, size_t);
int frontier_table_decode(const uint8_t *, size_t, uint8_t, uint8_t *, size_t);
int frontier_table_probe(const uint8_t *, size_t, uint8_t *);
#endif
