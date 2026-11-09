#include "frontier_table.h"
#include <stdlib.h>
#include <string.h>
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

enum { MAX_COLS = 1024, MAX_SUFFIX = 8, HEADER = 8 };
typedef struct {
    size_t bytes;
    uint8_t suffix[MAX_SUFFIX];
    unsigned suffix_size, examples, hits[MAX_SUFFIX];
    uint8_t *cursor;
} Column;

static void store32(uint8_t *dst, uint32_t x) {
    for (unsigned i = 0; i < 4; ++i) dst[i] = (uint8_t)(x >> (i * 8));
}
static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int frontier_table_probe(const uint8_t *src, size_t n, uint8_t *delim) {
    if (n < 8192) return 0;
    size_t limit = n < 16384 ? n : 16384;
    const uint8_t *first = memchr(src, '\n', limit);
    if (!first) return 0;
    size_t offset = (size_t)(first - src) + 1;
    limit = n - offset < 16384 ? n - offset : 16384;
    const uint8_t *end = memchr(src + offset, '\n', limit);
    if (!end) return 0;
    const uint8_t choices[] = {',', ';', '\t', '|'};
    for (unsigned s = 0; s < sizeof(choices); ++s) {
        size_t left = 0, right = 0;
        for (const uint8_t *p = src; p < first; ++p) left += *p == choices[s];
        if (left < 15 || left >= MAX_COLS) continue;
        for (const uint8_t *p = first + 1; p < end; ++p) right += *p == choices[s];
        if (left == right) { *delim = choices[s]; return 1; }
    }
    return 0;
}

/* All sample comparisons refer to the tail of the first nonempty data value.
 * Choose the number of shared suffix bytes that saves most source bytes. */
static void observe(Column *c, const uint8_t *p, size_t n) {
    if (!n || c->examples == 32) return;
    if (!c->examples) {
        c->suffix_size = n < MAX_SUFFIX ? (unsigned)n : MAX_SUFFIX;
        memcpy(c->suffix, p + n - c->suffix_size, c->suffix_size);
    }
    ++c->examples;
    for (unsigned i = 0; i < c->suffix_size && i < n; ++i) {
        if (c->suffix[c->suffix_size - 1 - i] != p[n - i - 1]) break;
        c->hits[i]++;
    }
}

static void choose_suffix(Column *c) {
    unsigned best = 0, savings = 0;
    for (unsigned i = 1; i < c->suffix_size; ++i) {
        unsigned score = (i + 1) * c->hits[i];
        if (c->hits[i] >= 3 && score > savings) { savings = score; best = i + 1; }
    }
    memmove(c->suffix, c->suffix + c->suffix_size - best, best);
    c->suffix_size = best;
}

/* Caller owns a complete line. The end is already known so memchr never
 * crosses an input boundary, including the last unterminated line. */
static const uint8_t *separator(const uint8_t *p, const uint8_t *end, uint8_t delim) {
    if ((size_t)(end - p) >= 16) {
#if defined(__aarch64__) && defined(__ARM_NEON)
        uint8x16_t eq = vceqq_u8(vld1q_u8(p), vdupq_n_u8(delim));
        uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
        if (mask) return p + (__builtin_ctzll(mask) >> 2);
        p += 16;
#elif defined(__SSE2__)
        __m128i v = _mm_loadu_si128((const __m128i *)p);
        unsigned mask = _mm_movemask_epi8(_mm_cmpeq_epi8(v, _mm_set1_epi8((char)delim)));
        if (mask) return p + __builtin_ctz(mask);
        p += 16;
#endif
    }
    const uint8_t *hit = memchr(p, delim, (size_t)(end - p));
    return hit ? hit : end;
}

size_t frontier_table_encode(const uint8_t *src, size_t n, uint8_t delim, uint8_t *dst, size_t cap) {
    if (!n || n > UINT32_MAX || cap < HEADER) return 0;
    const uint8_t *end = src + n, *first = memchr(src, '\n', n);
    if (!first) return 0;
    unsigned cols = 1;
    for (const uint8_t *p = src; p < first; ++p) cols += *p == delim;
    if (cols < 16 || cols > MAX_COLS) return 0;
    Column *c = calloc(cols, sizeof(*c));
    if (!c) return 0;
    size_t rows = 0, result = 0;
    for (const uint8_t *p = src; p < end && rows <= 64;) {
        const uint8_t *line = memchr(p, '\n', (size_t)(end - p));
        if (!line) line = end;
        for (unsigned j = 0; j < cols; ++j) {
            const uint8_t *q = separator(p, line, delim);
            if ((q == line) != (j + 1 == cols) || q - p > 127) goto cleanup;
            if (rows > 0 && rows <= 64) observe(c + j, p, (size_t)(q - p));
            p = q + (q != end);
        }
        ++rows;
    }
    rows = src[n - 1] != '\n';
    for (const uint8_t *p = src; p < end;) {
        const uint8_t *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;
        ++rows; p = nl + 1;
    }
    if (rows < 4 || rows > (cap - HEADER) / cols) goto cleanup;
    for (unsigned j = 0; j < cols; ++j) choose_suffix(c + j);
    /* Write one code per cell and count the stored bytes per column. */
    dst[0] = (uint8_t)cols; dst[1] = (uint8_t)(cols >> 8);
    dst[2] = src[n - 1] == '\n'; dst[3] = 0;
    store32(dst + 4, (uint32_t)rows);
    size_t head = HEADER;
    for (unsigned j = 0; j < cols; ++j) head += 5 + c[j].suffix_size;
    if (head > cap || rows * cols > cap - head) goto cleanup;
    uint8_t *tags = dst + head;
    size_t index = 0;
    for (const uint8_t *p = src; p < end;) {
        const uint8_t *line = memchr(p, '\n', (size_t)(end - p));
        if (!line) line = end;
        for (unsigned j = 0; j < cols; ++j) {
            const uint8_t *q = separator(p, line, delim);
            if ((q == line) != (j + 1 == cols) || q - p > 127) goto cleanup;
            size_t len = (size_t)(q - p), s = c[j].suffix_size;
            int shared = s && s <= len && !memcmp(q - s, c[j].suffix, s);
            if (shared) len -= s;
            tags[index++] = (uint8_t)(len + (shared ? 128 : 0));
            c[j].bytes += len;
            p = q + (q != end);
        }
    }
    size_t data = head + index, hdr = HEADER;
    for (unsigned j = 0; j < cols; ++j) {
        if (c[j].bytes > cap - data) goto cleanup;
        store32(dst + hdr, (uint32_t)c[j].bytes);
        dst[hdr + 4] = (uint8_t)c[j].suffix_size;
        memcpy(dst + hdr + 5, c[j].suffix, c[j].suffix_size);
        hdr += 5 + c[j].suffix_size;
        c[j].cursor = dst + data;
        data += c[j].bytes;
    }
    const uint8_t *p = src;
    for (size_t r = 0; r < rows; ++r) for (unsigned j = 0; j < cols; ++j) {
        uint8_t tag = *tags++;
        size_t len = tag & 127;
        memcpy(c[j].cursor, p, len);
        c[j].cursor += len;
        p += len + ((tag & 128) ? c[j].suffix_size : 0);
        if (p < end) ++p;
    }
    result = data;
cleanup:
    free(c);
    return result;
}

typedef struct {
    const uint8_t *p, *end;
    uint64_t suffix;
    unsigned suffix_size;
} Cursor;

int frontier_table_decode(const uint8_t *src, size_t n, uint8_t delim, uint8_t *dst, size_t raw) {
    if (n < HEADER) return -1;
    unsigned cols = src[0] | ((unsigned)src[1] << 8), final_lf = src[2];
    size_t rows = load32(src + 4);
    if (cols < 16 || cols > MAX_COLS || !rows || final_lf > 1 || src[3] || rows > n / cols) return -1;
    Cursor c[MAX_COLS];
    size_t head = HEADER, bytes = 0;
    for (unsigned j = 0; j < cols; ++j) {
        if (n - head < 5) return -1;
        size_t len = load32(src + head), suffix = src[head + 4];
        if (suffix > MAX_SUFFIX || suffix > n - head - 5 || len > n - bytes) return -1;
        c[j].suffix = 0;
        memcpy(&c[j].suffix, src + head + 5, suffix);
        c[j].suffix_size = (unsigned)suffix;
        head += 5 + suffix;
        bytes += len;
    }
    size_t cells = rows * cols;
    if (cells > n - head || bytes != n - head - cells || cells - (1 - final_lf) > raw) return -1;
    const uint8_t *tag = src + head, *data = tag + cells;
    head = HEADER;
    for (unsigned j = 0; j < cols; ++j) {
        c[j].p = data;
        data += load32(src + head);
        c[j].end = data;
        head += 5 + c[j].suffix_size;
    }
    size_t used = 0;
    for (size_t r = 0; r < rows; ++r) for (unsigned j = 0; j < cols; ++j) {
        unsigned t = *tag++, len = t & 127, suffix = t >> 7 ? c[j].suffix_size : 0;
        size_t sep = j + 1 < cols || r + 1 < rows || final_lf;
        if ((size_t)(c[j].end - c[j].p) < len || raw - used < len + suffix + sep) return -1;
        if (len <= 16 && raw - used >= 24 && (size_t)(src + n - c[j].p) >= 16) {
            memcpy(dst + used, c[j].p, 16);
            memcpy(dst + used + len, &c[j].suffix, 8);
        } else {
            memcpy(dst + used, c[j].p, len);
            memcpy(dst + used + len, &c[j].suffix, suffix);
        }
        c[j].p += len;
        used += len + suffix;
        if (sep) dst[used++] = j + 1 < cols ? delim : '\n';
    }
    for (unsigned j = 0; j < cols; ++j) if (c[j].p != c[j].end) return -1;
    return used == raw ? 0 : -1;
}
