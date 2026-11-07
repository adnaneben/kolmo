#include "frontier_dna.h"
#include <string.h>
#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

enum { HEADER = 20, MAX_LINE = 16384 };
static const uint8_t bases[2][16] = {"ACTG", "actg"};

static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void write32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

static unsigned code(uint8_t c) { return (c >> 1) & 3; }

int frontier_dna_probe(const uint8_t *src, size_t n) {
    size_t len = n < 8192 ? n : 8192;
    if (len < 512) return 0;
    size_t matches = 0;
    for (size_t i = 0; i < len; ++i)
        matches += (src[i] & 0xdf) == bases[0][code(src[i])] || (src[i] & 0xdf) == 'N';
    return matches * 100 >= len * 91;
}

/* A fixed line pitch avoids storing one position per LF. The content before
 * the regular lines and after the last complete line stays byte-exact. */
static int geometry(const uint8_t *p, size_t n, size_t *prefix, size_t *width, unsigned *ending) {
    size_t scan = n < MAX_LINE ? n : MAX_LINE;
    const uint8_t *first = memchr(p, '\n', scan);
    if (!first) {
        *prefix = 0; *width = 256; *ending = 0;
        return 1;
    }
    size_t start = (size_t)(first - p) + 1;
    scan = n - start < MAX_LINE ? n - start : MAX_LINE;
    const uint8_t *second = memchr(p + start, '\n', scan);
    if (!second || second == p + start) return 0;
    *ending = second[-1] == '\r' ? 2 : 1;
    *width = (size_t)(second - (p + start)) + 1 - *ending;
    *prefix = start;
    return *width > 0;
}

typedef struct {
    uint8_t *begin, *at, *limit;
} Exceptions;

/* Positions count nucleotide slots, so a run can cross a physical newline. */
static int add_exception(Exceptions *e, uint32_t pos, uint32_t count, unsigned flip, uint8_t value) {
    if (e->at != e->begin) {
        uint8_t *prev = e->at - 10;
        uint32_t start = read32(prev), length = read32(prev + 4);
        if (prev[8] == flip && prev[9] == value && (uint64_t)start + length == pos) {
            write32(prev + 4, length + count);
            return 1;
        }
    }
    if (e->limit - e->at < 10) return 0;
    write32(e->at, pos); write32(e->at + 4, count); e->at[8] = (uint8_t)flip; e->at[9] = value; e->at += 10;
    return 1;
}

static int pack_row(const uint8_t *p, size_t len, int lower, uint8_t *packed,
                    Exceptions *exceptions, uint32_t offset) {
    size_t i = 0;
#if defined(__aarch64__) && defined(__ARM_NEON)
    const uint8x16_t alphabet = vld1q_u8(bases[lower]);
    for (; len - i >= 64; i += 64) {
        uint8x16x4_t text = vld4q_u8(p + i);
        uint8x16_t v[4], mismatch = vdupq_n_u8(0);
        for (unsigned j = 0; j < 4; ++j) {
            v[j] = vandq_u8(vshrq_n_u8(text.val[j], 1), vdupq_n_u8(3));
            mismatch = vorrq_u8(mismatch, veorq_u8(text.val[j], vqtbl1q_u8(alphabet, v[j])));
        }
        uint8x16_t pair0 = vorrq_u8(v[0], vshlq_n_u8(v[1], 2));
        uint8x16_t pair1 = vorrq_u8(v[2], vshlq_n_u8(v[3], 2));
        vst1q_u8(packed + i / 4, vorrq_u8(pair0, vshlq_n_u8(pair1, 4)));
        if (vmaxvq_u8(mismatch)) {
            if (vminvq_u8(mismatch) == 32 && vmaxvq_u8(mismatch) == 32) {
                uint8x16_t all = vdupq_n_u8(255);
                for (unsigned j = 0; j < 4; ++j)
                    all = vandq_u8(all, vceqq_u8(veorq_u8(text.val[j], vqtbl1q_u8(alphabet, v[j])), vdupq_n_u8(32)));
                if (vminvq_u8(all)) {
                    if (!add_exception(exceptions, offset + i, 64, 1, 0)) return 0;
                    continue;
                }
            }
            uint8x16_t a = text.val[0];
            uint8x16_t different = vorrq_u8(veorq_u8(a, text.val[1]), veorq_u8(a, text.val[2]));
            if (!vmaxvq_u8(vorrq_u8(different, veorq_u8(a, text.val[3]))) && vmaxvq_u8(a) == vminvq_u8(a)) {
                if (!add_exception(exceptions, offset + i, 64, 0, p[i])) return 0;
                continue;
            }
            for (size_t j = i; j < i + 64; ++j) {
                if (bases[lower][code(p[j])] != p[j]) {
                    unsigned flip = (bases[lower][code(p[j])] ^ p[j]) == 32;
                    if (!add_exception(exceptions, offset + j, 1, flip, flip ? 0 : p[j])) return 0;
                }
            }
        }
    }
#endif
    for (; i < len; i += 4) {
        unsigned bits = 0;
        size_t count = len - i < 4 ? len - i : 4;
        for (size_t j = 0; j < count; ++j) {
            unsigned c = code(p[i + j]);
            bits |= c << (2 * j);
            if (bases[lower][c] != p[i + j]) {
                unsigned flip = (bases[lower][c] ^ p[i + j]) == 32;
                if (!add_exception(exceptions, offset + i + j, 1, flip, flip ? 0 : p[i + j])) return 0;
            }
        }
        packed[i / 4] = (uint8_t)bits;
    }
    return 1;
}

static size_t encode_regular(const uint8_t *src, size_t n, uint8_t *dst, size_t cap) {
    size_t prefix, width;
    unsigned ending;
    if (n < 512 || n > UINT32_MAX || !geometry(src, n, &prefix, &width, &ending)) return 0;
    size_t pitch = width + ending, rows = (n - prefix) / pitch;
    if (!rows) return 0;
    size_t suffix = n - prefix - rows * pitch, stride = (width + 3) / 4;
    uint64_t min_size = (uint64_t)HEADER + prefix + suffix + (uint64_t)rows * stride;
    if (min_size >= n || min_size > cap) return 0;
    size_t count_lower = 0, tally = width < 256 ? width : 256;
    for (size_t i = 0; i < tally; ++i) count_lower += (src[prefix + i] & 0x20) != 0;
    int lower = count_lower > tally / 2;
    write32(dst, prefix); write32(dst + 4, width); write32(dst + 8, rows); write32(dst + 12, suffix);
    dst[16] = lower; dst[17] = ending; dst[18] = 2; dst[19] = 0;
    memcpy(dst + HEADER, src, prefix);
    memcpy(dst + HEADER + prefix, src + n - suffix, suffix);
    uint8_t *packed = dst + HEADER + prefix + suffix, *exc = dst + (size_t)min_size;
    Exceptions exceptions = {exc, exc, dst + (cap < n ? cap : n)};
    for (size_t r = 0; r < rows; ++r) {
        size_t off = prefix + r * pitch;
        if (ending && (src[off + pitch - 1] != '\n' || (ending == 2 && src[off + width] != '\r'))) return 0;
        if (!pack_row(src + off, width, lower, packed + r * stride, &exceptions, (uint32_t)(r * width))) return 0;
    }
    return (size_t)(exceptions.at - dst);
}

static void unpack_row(const uint8_t *p, uint8_t *dst, size_t width, unsigned lower) {
    size_t i = 0;
#if defined(__aarch64__) && defined(__ARM_NEON)
    uint8x16_t alphabet = vld1q_u8(bases[lower]), mask = vdupq_n_u8(3);
    for (; width - i >= 64; i += 64) {
        uint8x16_t v = vld1q_u8(p + i / 4);
        uint8x16x4_t text;
        text.val[0] = vqtbl1q_u8(alphabet, vandq_u8(v, mask));
        text.val[1] = vqtbl1q_u8(alphabet, vandq_u8(vshrq_n_u8(v, 2), mask));
        text.val[2] = vqtbl1q_u8(alphabet, vandq_u8(vshrq_n_u8(v, 4), mask));
        text.val[3] = vqtbl1q_u8(alphabet, vshrq_n_u8(v, 6));
        vst4q_u8(dst + i, text);
    }
    static const uint8_t spread[16] = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3};
    static const int8_t shifts[16] = {0,-2,-4,-6,0,-2,-4,-6,0,-2,-4,-6,0,-2,-4,-6};
    for (; width - i >= 16; i += 16) {
        uint32_t group;
        memcpy(&group,p+i/4,4);
        uint8x16_t v=vreinterpretq_u8_u32(vdupq_n_u32(group));
        v=vshlq_u8(vqtbl1q_u8(v,vld1q_u8(spread)),vld1q_s8(shifts));
        vst1q_u8(dst+i,vqtbl1q_u8(alphabet,vandq_u8(v,mask)));
    }
#endif
    for (; i < width; ++i) dst[i] = bases[lower][(p[i / 4] >> (2 * (i % 4))) & 3];
}

static int decode_regular(const uint8_t *src, size_t n, uint8_t *dst, size_t raw) {
    if (n < HEADER || raw > UINT32_MAX) return -1;
    uint32_t prefix = read32(src), width = read32(src + 4), rows = read32(src + 8), suffix = read32(src + 12);
    unsigned lower = src[16], ending = src[17];
    if (!width || width > MAX_LINE || !rows || lower > 1 || ending > 2 || src[18] > 2 || src[19]) return -1;
    size_t stride = (width + 3) / 4, pitch = width + ending;
    if ((uint64_t)rows * pitch + prefix + suffix != raw) return -1;
    uint64_t extra = (uint64_t)HEADER + prefix + suffix + (uint64_t)rows * stride;
    if (extra > n || (n - (size_t)extra) % (src[18] == 2 ? 10 : src[18] ? 9 : 5)) return -1;
    memcpy(dst, src + HEADER, prefix);
    memcpy(dst + raw - suffix, src + HEADER + prefix, suffix);
    const uint8_t *packed = src + HEADER + prefix + suffix;
    for (size_t r = 0; r < rows; ++r) {
        uint8_t *out = dst + prefix + r * pitch;
        unpack_row(packed + r * stride, out, width, lower);
        if (ending) out[pitch - 1] = '\n';
        if (ending == 2) out[width] = '\r';
    }
    if (src[18]) {
        uint64_t total = (uint64_t)rows * width, next = 0;
        size_t record = src[18] == 2 ? 10 : 9;
        for (size_t p = (size_t)extra; p < n; p += record) {
            unsigned flip = record == 10 ? src[p + 8] : 0;
            uint8_t value = src[p + record - 1];
            if (flip > 1 || (flip && value)) return -1;
            uint32_t pos = read32(src + p), count = read32(src + p + 4);
            if (!count || pos < next || pos > total || count > total - pos) return -1;
            next = (uint64_t)pos + count;
            size_t row = pos / width, col = pos % width;
            while (count) {
                size_t part = count < width - col ? count : width - col;
                uint8_t *out = dst + prefix + row * pitch + col;
                if (flip) for (size_t i=0; i<part; ++i) out[i] ^= 32;
                else memset(out, value, part);
                count -= (uint32_t)part; ++row; col = 0;
            }
        }
        return 0;
    }
    size_t next = prefix;
    for (size_t p = (size_t)extra; p < n; p += 5) {
        uint32_t pos = read32(src + p);
        if (pos < next || pos >= raw - suffix) return -1;
        dst[pos] = src[p + 4];
        next = (size_t)pos + 1;
    }
    return 0;
}

/* Header tokens inside a block start independent FASTA records. Each record
 * may have its own wrapping width. Small or irregular records remain literal. */
static size_t next_header(const uint8_t *p, size_t n) {
    for (size_t i=1;i<n;) {
        const uint8_t *mark=memchr(p+i,'>',n-i);
        if (!mark) break;
        size_t at=(size_t)(mark-p);
        if (p[at-1]=='\n') return at;
        i=at+1;
    }
    return n;
}

size_t frontier_dna_encode(const uint8_t *src,size_t n,uint8_t *dst,size_t cap) {
    if (n>UINT32_MAX || cap<8) return 0;
    size_t part=next_header(src,n);
    if(part==n) return encode_regular(src,n,dst,cap);
    memcpy(dst,"DFR1",4);
    size_t at=0,w=8; unsigned count=0;
    while(at<n) {
        if (cap-w<8) return 0;
        size_t size=encode_regular(src+at,part,dst+w+8,cap-w-8);
        if(!size && part>cap-w-8) return 0;
        write32(dst+w,(uint32_t)part);write32(dst+w+4,(uint32_t)size);
        w+=8;
        if(size) w+=size;
        else { memcpy(dst+w,src+at,part);w+=part; }
        at+=part; ++count;
        if(at<n) part=next_header(src+at,n-at);
    }
    write32(dst+4,count);
    return w<n?w:0;
}

int frontier_dna_decode(const uint8_t *src,size_t n,uint8_t *dst,size_t raw) {
    if(n<8 || memcmp(src,"DFR1",4)) return decode_regular(src,n,dst,raw);
    size_t p=8,w=0;
    unsigned records=read32(src+4);
    if(records<2 || records>(n-8)/8) return -1;
    for(unsigned j=0;j<records;++j) {
        if(n-p<8) return -1;
        size_t len=read32(src+p),code=read32(src+p+4);
        p+=8;
        if(!len || len>raw-w || (code?code:len)>n-p) return -1;
        if(code) {
            if(decode_regular(src+p,code,dst+w,len)) return -1;
            p+=code;
        } else {memcpy(dst+w,src+p,len);p+=len;}
        w+=len;
    }
    return p==n && w==raw?0:-1;
}
