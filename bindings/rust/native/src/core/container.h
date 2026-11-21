/* ------------------------------------------------------------------ container layout
 * header (32 B): "CZN1" | ver u8 | mode u8 | transform u8 | a u8 | b u8 | c u8 | flags u8 | reserved u8 |
 *                orig u64 | nblocks u32 | block u32 | crc32c(header[0..27] + index) u32
 * index: nblocks x { raw u32 | comp u32 | block checksum u32 | codec u8 | tflag u8 | reserved u16 }
 * block checksum: low 32 bits of kolmo_khash64(raw, seed 0x637a6e31) (src/algo/khash.c; version 12). Flags bit 0 (rt):
 * the checksum instead covers the stored payload after its 4 plan bytes, seeded with them, and is checked before decoding
 * blocks: payloads back to back; a transposed block starts with k x u32 column sizes */
#define HDR 32
#define IDX 16
static inline uint32_t blk_sum(const uint8_t *p, size_t n) { return (uint32_t)kolmo_khash64(p, n, 0x637a6e31); }

static inline void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static inline void put64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t get64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
