/* Internal interface of the container extensions (SPEC.md, "Container extensions"): the KEXT trailer, the IDX1
 * column and row index (index.c) and the DDP1 deduplicating archive (dedup.c). Not installed. */
#ifndef KOLMO_CORE_FEATURES_H
#define KOLMO_CORE_FEATURES_H
#include "../kolmo.h"
#include <stddef.h>
#include <stdint.h>

/* ---- the CZN1 frame, as far as the extensions need it (extension.c) ---- */
typedef struct {
    uint32_t nb;              /* blocks */
    uint64_t orig;            /* decoded length */
    uint64_t end;             /* bytes of the frame (header, index, payloads); extensions start here */
    uint64_t *roff;           /* nb + 1 decoded offsets */
    uint64_t *poff;           /* nb + 1 payload offsets */
    const uint8_t *idx;       /* the block index (16 bytes per block) */
    int ver;
} kx_frame;
int  kx_frame_open(const uint8_t *src, size_t n, kx_frame *f);     /* 0 or a KOLMO_ERR_* code */
void kx_frame_close(kx_frame *f);

/* ---- extension sections ---- */
#define KX_TRAILER 24
/* the body of the section with this tag, or NULL (*err: 0 when there is no trailer or no such section,
 * KOLMO_ERR_CORRUPT / KOLMO_ERR_CHECKSUM when the trailer or a section is damaged) */
const uint8_t *kx_find(const uint8_t *src, size_t n, uint64_t frame_end, const char tag[4], uint64_t *len, int *err);
/* sections appended at dst + frame_end (tags[i], bodies[i], lens[i]), then the trailer; returns the new total */
int64_t kx_append(uint8_t *dst, size_t cap, uint64_t frame_end, int count, const char (*tags)[4],
                  const uint8_t *const *bodies, const uint64_t *lens);
uint64_t kx_bound(int count, const uint64_t *lens);

/* ---- hooks into the block decoder (kolmo.c) ---- */
/* Column col of block i when it is a column-split table block (flag 13): *out (malloc'd) holds one line per row of
 * the block. Returns 0, 1 when block i is not such a block (decode it whole instead), or a negative error.
 * *decoded receives the decoded bytes of the segments read and *read their stored bytes. */
int kolmo__block_column(const uint8_t *src, size_t n, uint32_t i, int col, uint8_t **out, size_t *outn,
                        uint64_t *decoded, uint64_t *read);

/* ---- little helpers shared by index.c and dedup.c ---- */
static inline void kx_put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static inline void kx_put64(uint8_t *p, uint64_t v) { kx_put32(p, (uint32_t)v); kx_put32(p + 4, (uint32_t)(v >> 32)); }
static inline uint32_t kx_get32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint64_t kx_get64(const uint8_t *p) { return kx_get32(p) | (uint64_t)kx_get32(p + 4) << 32; }

typedef struct { uint8_t *p; size_t n, cap; int oom; } kx_buf;
int  kx_grow(kx_buf *b, size_t add);
void kx_bytes(kx_buf *b, const void *s, size_t n);
void kx_uv(kx_buf *b, uint64_t v);
int  kx_getuv(const uint8_t *p, size_t n, size_t *i, uint64_t *v);   /* 0, or -1 when truncated or over 64 bits */

#include "../algo/khash.h"   /* kolmo_khash64 (src/algo/khash.c) */
#endif
