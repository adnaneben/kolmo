/* Container extensions: tagged sections after the last block payload, found from a fixed trailer at the end of the
 * file (SPEC.md, "Container extensions"). A reader that does not know them ignores them: block decoding stops at the
 * last payload, so a frame with extensions decodes with every reader of its version.
 *
 *   frame | section * count | trailer
 *   section := tag[4] | length u64 | body[length] | crc32c u32 (of tag, length and body)
 *   trailer := "KEXT" | version u8 (1) | 0 u8 * 3 | count u32 | sections length u64 | crc32c u32 (of the 20 bytes before)
 *
 * Licence: Apache-2.0. */
#include "features.h"
#include <stdlib.h>
#include <string.h>

#define KX_HDR 32
#define KX_IDX 16

int kx_frame_open(const uint8_t *s, size_t n, kx_frame *f) {
    memset(f, 0, sizeof *f);
    /* the decoder's header and index check (frame_head: version, flags, checksum, block sizes): a frame the decoder
     * would refuse is refused here too, before its block sizes are trusted for allocations, and kolmo_original_size
     * answers for every frame this accepts (found by fuzz_ext: a version-13 header, then a version-12 header with
     * invalid flags, each with a valid CRC, made kolmo_query_run allocate a block buffer of 2.8 and 4 GB) */
    { uint64_t o, e; int v = frame_head(s, n, &o, &e); if (v) return v; }
    if (n < KX_HDR) return KOLMO_ERR_CORRUPT;
    f->ver = s[4]; f->orig = kx_get64(s + 12); f->nb = kx_get32(s + 20);
    if ((uint64_t)f->nb * KX_IDX + KX_HDR > n) return KOLMO_ERR_CORRUPT;
    uint32_t hc = kolmo_crc32c(0, s, 28); hc = kolmo_crc32c(hc, s + KX_HDR, (size_t)f->nb * KX_IDX);
    if (hc != kx_get32(s + 28)) return KOLMO_ERR_CHECKSUM;
    f->idx = s + KX_HDR;
    f->roff = malloc(sizeof(uint64_t) * ((size_t)f->nb + 1)); f->poff = malloc(sizeof(uint64_t) * ((size_t)f->nb + 1));
    if (!f->roff || !f->poff) { kx_frame_close(f); return KOLMO_ERR_ALLOC; }
    uint64_t po = KX_HDR + (uint64_t)f->nb * KX_IDX, ro = 0;
    for (uint32_t i = 0; i < f->nb; i++) {
        uint32_t raw = kx_get32(f->idx + (size_t)i * KX_IDX), cl = kx_get32(f->idx + (size_t)i * KX_IDX + 4);
        f->roff[i] = ro; f->poff[i] = po;
        if (cl > n - po || raw > f->orig - ro) { kx_frame_close(f); return KOLMO_ERR_CORRUPT; }
        po += cl; ro += raw;
    }
    f->roff[f->nb] = ro; f->poff[f->nb] = po; f->end = po;
    if (ro != f->orig) { kx_frame_close(f); return KOLMO_ERR_CORRUPT; }
    return 0;
}
void kx_frame_close(kx_frame *f) { free(f->roff); free(f->poff); f->roff = f->poff = NULL; }

const uint8_t *kx_find(const uint8_t *s, size_t n, uint64_t fe, const char tag[4], uint64_t *len, int *err) {
    *err = 0; *len = 0;
    if (fe > n || n - fe < KX_TRAILER) return NULL;                   /* no trailer: a frame without extensions */
    const uint8_t *t = s + n - KX_TRAILER;
    if (memcmp(t, "KEXT", 4)) return NULL;
    if (t[4] != 1 || t[5] || t[6] || t[7] || kolmo_crc32c(0, t, 20) != kx_get32(t + 20)) { *err = KOLMO_ERR_CORRUPT; return NULL; }
    uint32_t count = kx_get32(t + 8); uint64_t total = kx_get64(t + 12);
    if (total != n - KX_TRAILER - fe) { *err = KOLMO_ERR_CORRUPT; return NULL; }
    uint64_t p = fe, e = fe + total; const uint8_t *hit = NULL;
    for (uint32_t i = 0; i < count; i++) {
        if (e - p < 16) { *err = KOLMO_ERR_CORRUPT; return NULL; }
        uint64_t l = kx_get64(s + p + 4);
        if (l > e - p - 16) { *err = KOLMO_ERR_CORRUPT; return NULL; }
        if (!memcmp(s + p, tag, 4)) {
            if (kolmo_crc32c(0, s + p, 12 + (size_t)l) != kx_get32(s + p + 12 + l)) { *err = KOLMO_ERR_CHECKSUM; return NULL; }
            if (!hit) { hit = s + p + 12; *len = l; }
        }
        p += 16 + l;
    }
    if (p != e) { *err = KOLMO_ERR_CORRUPT; return NULL; }
    return hit;
}

uint64_t kx_bound(int count, const uint64_t *lens) {
    uint64_t t = KX_TRAILER; for (int i = 0; i < count; i++) t += 16 + lens[i]; return t;
}

int64_t kx_append(uint8_t *dst, size_t cap, uint64_t fe, int count, const char (*tags)[4],
                  const uint8_t *const *bodies, const uint64_t *lens) {
    uint64_t need = kx_bound(count, lens);
    if (fe > cap || need > cap - fe) return KOLMO_ERR_DSTSIZE;
    uint64_t p = fe;
    for (int i = 0; i < count; i++) {
        memcpy(dst + p, tags[i], 4); kx_put64(dst + p + 4, lens[i]);
        if (lens[i]) memcpy(dst + p + 12, bodies[i], (size_t)lens[i]);
        kx_put32(dst + p + 12 + lens[i], kolmo_crc32c(0, dst + p, 12 + (size_t)lens[i]));
        p += 16 + lens[i];
    }
    uint8_t *t = dst + p;
    memcpy(t, "KEXT", 4); t[4] = 1; t[5] = t[6] = t[7] = 0; kx_put32(t + 8, (uint32_t)count); kx_put64(t + 12, p - fe);
    kx_put32(t + 20, kolmo_crc32c(0, t, 20));
    return (int64_t)(p + KX_TRAILER);
}

int kx_grow(kx_buf *b, size_t add) {
    if (b->oom) return -1;
    if (b->cap - b->n >= add) return 0;
    size_t c = b->cap ? b->cap : 256;
    while (c - b->n < add) { if (c > SIZE_MAX / 2) { b->oom = 1; return -1; } c *= 2; }
    uint8_t *p = realloc(b->p, c); if (!p) { b->oom = 1; return -1; }
    b->p = p; b->cap = c; return 0;
}
void kx_bytes(kx_buf *b, const void *s, size_t n) { if (n && !kx_grow(b, n)) { memcpy(b->p + b->n, s, n); b->n += n; } }
void kx_uv(kx_buf *b, uint64_t v) {
    uint8_t t[10]; int k = 0;
    do { t[k++] = (uint8_t)(v | (v >= 128 ? 128 : 0)); v >>= 7; } while (v);
    kx_bytes(b, t, (size_t)k);
}
int kx_getuv(const uint8_t *p, size_t n, size_t *i, uint64_t *v) {
    uint64_t r = 0;
    for (int s = 0; s < 64; s += 7) {
        if (*i >= n) return -1;
        uint8_t c = p[(*i)++]; r |= (uint64_t)(c & 127) << s;
        if (!(c & 128)) { if (s == 63 && c > 1) return -1; *v = r; return 0; }
    }
    return -1;
}
