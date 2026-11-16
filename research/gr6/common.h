/* shared helpers for the GR6 prototypes: file I/O, timing, string interning, 6-bit packing */
#ifndef GR6_COMMON_H
#define GR6_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
static void *xmalloc(size_t n) { void *p = malloc(n ? n : 1); if (!p) { fprintf(stderr, "out of memory\n"); exit(1); } return p; }
static void *xrealloc(void *p, size_t n) { p = realloc(p, n ? n : 1); if (!p) { fprintf(stderr, "out of memory\n"); exit(1); } return p; }

static uint8_t *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = xmalloc(*n + 64); if (*n && fread(b, 1, *n, f) != *n) { perror(path); exit(1); } fclose(f); return b;
}
static void write_file(const char *path, const void *p, size_t n) {
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); exit(1); } fwrite(p, 1, n, f); fclose(f);
}

/* growable byte buffer */
typedef struct { uint8_t *p; size_t n, cap; } buf;
static inline void buf_reserve(buf *b, size_t extra) {
    if (b->n + extra > b->cap) { b->cap = (b->n + extra) * 2 + 1024; b->p = xrealloc(b->p, b->cap); }
}
static inline void buf_put(buf *b, uint8_t c) { buf_reserve(b, 1); b->p[b->n++] = c; }
static inline void buf_add(buf *b, const void *s, size_t n) { buf_reserve(b, n); memcpy(b->p + b->n, s, n); b->n += n; }

/* string interning: id = order of first insertion */
typedef struct { uint32_t *slot; uint32_t mask; uint32_t count, cap; uint64_t *off; uint32_t *len; uint32_t *hash; buf arena; } intern;
static uint32_t hash_bytes(const uint8_t *s, size_t n) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ n;
    while (n >= 8) { uint64_t v; memcpy(&v, s, 8); h = (h ^ v) * 0xff51afd7ed558ccdull; h ^= h >> 32; s += 8; n -= 8; }
    uint64_t v = 0; memcpy(&v, s, n); h = (h ^ v) * 0xc4ceb9fe1a85ec53ull; h ^= h >> 29;
    return (uint32_t)h;
}
static void intern_init(intern *t) {
    memset(t, 0, sizeof *t); t->mask = (1u << 16) - 1; t->slot = xmalloc(sizeof(uint32_t) << 16);
    memset(t->slot, 0xff, sizeof(uint32_t) << 16);
}
static void intern_grow(intern *t) {
    uint32_t nm = t->mask * 2 + 1; uint32_t *ns = xmalloc(sizeof(uint32_t) * (nm + 1)); memset(ns, 0xff, sizeof(uint32_t) * (nm + 1));
    for (uint32_t i = 0; i < t->count; i++) { uint32_t h = t->hash[i] & nm; while (ns[h] != UINT32_MAX) h = (h + 1) & nm; ns[h] = i; }
    free(t->slot); t->slot = ns; t->mask = nm;
}
/* returns id; *isnew set when inserted (insert=1) ; returns UINT32_MAX if absent and insert=0 */
static uint32_t intern_get(intern *t, const uint8_t *s, size_t n, int insert, int *isnew) {
    uint32_t h = hash_bytes(s, n), i = h & t->mask;
    if (isnew) *isnew = 0;
    for (;;) {
        uint32_t id = t->slot[i];
        if (id == UINT32_MAX) break;
        if (t->hash[id] == h && t->len[id] == n && memcmp(t->arena.p + t->off[id], s, n) == 0) return id;
        i = (i + 1) & t->mask;
    }
    if (!insert) return UINT32_MAX;
    if (t->count == t->cap) {
        t->cap = t->cap * 2 + 1024;
        t->off = xrealloc(t->off, t->cap * sizeof *t->off); t->len = xrealloc(t->len, t->cap * sizeof *t->len);
        t->hash = xrealloc(t->hash, t->cap * sizeof *t->hash);
    }
    uint32_t id = t->count++;
    t->off[id] = t->arena.n; t->len[id] = n; t->hash[id] = h; buf_add(&t->arena, s, n);
    t->slot[i] = id; if (isnew) *isnew = 1;
    if (t->count * 2 > t->mask) intern_grow(t);
    return id;
}
static inline const uint8_t *intern_str(const intern *t, uint32_t id) { return t->arena.p + t->off[id]; }

/* 64-bit key -> uint32 value map (open addressing) */
typedef struct { uint64_t *k; uint32_t *v; uint64_t mask, count; } map64;
static void map_init(map64 *m, int bits) {
    m->mask = (1ull << bits) - 1; m->count = 0;
    m->k = xmalloc(sizeof(uint64_t) << bits); m->v = xmalloc(sizeof(uint32_t) << bits);
    memset(m->k, 0xff, sizeof(uint64_t) << bits);
}
static inline uint64_t mix64(uint64_t x) { x ^= x >> 33; x *= 0xff51afd7ed558ccdull; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ull; x ^= x >> 33; return x; }
static uint32_t *map_slot(map64 *m, uint64_t key, int insert) {
    uint64_t i = mix64(key) & m->mask;
    for (;;) {
        if (m->k[i] == key) return &m->v[i];
        if (m->k[i] == UINT64_MAX) break;
        i = (i + 1) & m->mask;
    }
    if (!insert) return NULL;
    if ((m->count + 1) * 2 > m->mask) {
        map64 n; int bits = 0; while ((1ull << bits) <= m->mask) bits++;
        map_init(&n, bits + 1);
        for (uint64_t j = 0; j <= m->mask; j++) if (m->k[j] != UINT64_MAX) { uint32_t *s = map_slot(&n, m->k[j], 1); *s = m->v[j]; }
        free(m->k); free(m->v); *m = n;
        return map_slot(m, key, insert);
    }
    m->k[i] = key; m->v[i] = UINT32_MAX; m->count++;
    return &m->v[i];
}

/* move-to-front lists of uint32 ids, capped */
typedef struct { uint32_t *a; uint16_t n, cap; } mtf;
static inline int mtf_find(const mtf *l, uint32_t id) { for (int i = 0; i < l->n; i++) if (l->a[i] == id) return i; return -1; }
static inline void mtf_front(mtf *l, uint32_t id, int pos, int limit) {
    if (pos < 0) {
        if (l->n < limit) {
            if (l->n == l->cap) { l->cap = l->cap ? (l->cap * 2 > limit ? limit : l->cap * 2) : 4; l->a = xrealloc(l->a, l->cap * 4); }
            l->n++;
        }
        pos = l->n - 1;
    }
    memmove(l->a + 1, l->a, pos * 4); l->a[0] = id;
}

/* 6-bit packing, 4 units per 3 bytes */
static size_t pack6(const uint8_t *u, size_t k, uint8_t *out) {
    size_t o = 0, i = 0;
    for (; i + 4 <= k; i += 4) {
        uint32_t v = u[i] | u[i + 1] << 6 | u[i + 2] << 12 | (uint32_t)u[i + 3] << 18;
        out[o++] = v; out[o++] = v >> 8; out[o++] = v >> 16;
    }
    uint32_t v = 0; int r = k - i;
    for (int j = 0; j < r; j++) v |= (uint32_t)u[i + j] << (6 * j);
    for (int j = 0; j < (6 * r + 7) / 8; j++) out[o++] = v >> (8 * j);
    return o;
}
static void unpack6(const uint8_t *p, size_t k, uint8_t *u) {
    size_t i = 0, o = 0;
    for (; i + 4 <= k; i += 4, o += 3) {
        uint32_t v = p[o] | p[o + 1] << 8 | (uint32_t)p[o + 2] << 16;
        u[i] = v & 63; u[i + 1] = v >> 6 & 63; u[i + 2] = v >> 12 & 63; u[i + 3] = v >> 18;
    }
    uint32_t v = 0; int r = k - i;
    for (int j = 0; j < (6 * r + 7) / 8; j++) v |= (uint32_t)p[o + j] << (8 * j);
    for (int j = 0; j < r; j++) u[i + j] = v >> (6 * j) & 63;
}
#endif
