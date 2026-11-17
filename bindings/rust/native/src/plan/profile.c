/* Trained profiles: the .kpf file format, pinned plans, and the dictionary codecs of profile frames.
 *
 * Part of the src/kolmo.c translation unit for now (see src/plan/profile.h): it uses the core's plan_t, make_plan
 * (through plan_for) and kolmo_xxh3, and the core calls kp_codec_compress, kp_codec_decompress, kp_plan_pinned and
 * kp_id. The public entry points are in src/api/features.c.
 *
 * Format (SPEC.md, "Trained profiles (KPF1)"), all integers little-endian:
 *   "KPF1" | version u16 = 1 | flags u16 = 0 | profile id u32 | section count u32 | sections | CRC32C u32
 *   section: tag[4] | flags u32 (bit 0: critical) | length u32 | data
 *   profile id = low 32 bits of XXH3-64(sections, seed 0x4B504631)
 * A reader rejects the file on any mismatch, on an unknown or repeated critical section, on a critical section it
 * cannot use, and skips unknown non-critical sections. Sections of version 1:
 *   PARM (not critical) setting: mode u8 | level i8 | analysis u8 | 0 u8 | block u32
 *   PLAN (not critical) pinned plans, see kp_pins_parse
 *   ZDIC (critical)     zstd dictionary (ZDICT format) for the zstd leaves of profile frames (a leaf of at most 64 KiB
 *                       is one raw zstd block, a larger one a magicless frame without content size or checksum)
 *   RDIC (critical)     raw dictionary for the Brotli leaves (as a raw prefix) and the LZ4 leaves (its last 64 KiB)
 *   DLIM (critical)     u64: only leaves of at most this many bytes use the dictionaries (absent: 256 KiB); the
 *                       trainer picks it on held-out samples (a dictionary helps small leaves, can cost large ones)
 *   INFO (not critical) UTF-8 summary of the training run
 *   CMPR (critical, reserved) context-mixing priors; no writer yet, so this reader rejects files that carry it */
#include <math.h>
#define ZDICT_STATIC_LINKING_ONLY
#include "zdict.h"
/* zstd's raw block functions (static API, kept although marked deprecated upstream): a profile leaf of at most
 * KP_ZBLOCK bytes is one raw zstd block, without the 5 to 6 bytes of frame and block header the container repeats */
#define ZSTD_STATIC_LINKING_ONLY
#define ZSTD_DISABLE_DEPRECATE_WARNINGS
#include "zstd.h"

#define KP_MAGIC "KPF1"
#define KP_VERSION 1
#define KP_ID_SEED 0x4B504631ull
#define KP_HDR 16
#define KP_CRIT 1u
#define KP_KEY 32                     /* bytes of the normalized first line that key a pinned plan */
#define KP_MAX_PINS 256
#define KP_ZSTD_LEVELS 32             /* prepared zstd dictionaries, by level + 8 (levels -7 .. 22) */
#define KP_LZ4_DICT (64u << 10)
#define KP_DLIM_DEFAULT ((uint64_t)256 << 10)
#define KP_ZBLOCK (64u << 10)

typedef struct {
    uint8_t key[KP_KEY], keylen, binary, mode, transforms, band;
    int32_t t, a, b, c, wide, verify; uint32_t kgw;
    int32_t img_be, img_off, img_mode; int64_t img_W;
    int has_fs; int32_t fs_n, fs_gcol; tformula f[256];
} kp_pin;

struct kolmo_profile {
    uint32_t id;
    int has_parm; kolmo_params parm;
    uint64_t dlim;                                        /* leaves of at most dlim bytes take the dictionaries */
    uint8_t *zdict, *rdict; size_t zdictlen, rdictlen;
    uint32_t npin; kp_pin *pins;
    char *info;
    pthread_mutex_t mu;                                   /* guards the dictionaries prepared on first use */
    ZSTD_CDict *cdict[KP_ZSTD_LEVELS];
    ZSTD_DDict *ddict;
    BrotliEncoderPreparedDictionary *bdict;               /* prepared on first use */
    LZ4_stream_t *l4;                                     /* LZ4 dictionary state, loaded on first use, attached per call */
    int bdict_failed, l4_failed;
};

static uint32_t kp_id(const struct kolmo_profile *P) { return P->id; }

/* ---------------------------------------------------------------- byte writer */
typedef struct { uint8_t *p; size_t n, cap; int err; } kp_buf;
static void kp_put(kp_buf *b, const void *d, size_t n) {
    if (b->err) return;
    if (n > b->cap - b->n) {
        size_t c = b->cap ? b->cap : 256;
        while (c - b->n < n) { if (c > SIZE_MAX / 2) { b->err = 1; return; } c *= 2; }
        uint8_t *q = realloc(b->p, c); if (!q) { b->err = 1; return; }
        b->p = q; b->cap = c;
    }
    memcpy(b->p + b->n, d, n); b->n += n;
}
static void kp_u8(kp_buf *b, uint8_t v) { kp_put(b, &v, 1); }
static void kp_u16(kp_buf *b, uint16_t v) { uint8_t x[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; kp_put(b, x, 2); }
static void kp_u32(kp_buf *b, uint32_t v) { uint8_t x[4]; put32(x, v); kp_put(b, x, 4); }
static void kp_u64(kp_buf *b, uint64_t v) { uint8_t x[8]; put64(x, v); kp_put(b, x, 8); }
static void kp_f64(kp_buf *b, double v) { uint64_t u; memcpy(&u, &v, 8); kp_u64(b, u); }
static void kp_section(kp_buf *b, const char tag[4], uint32_t flags, const void *d, size_t n) {
    if (n > UINT32_MAX) { b->err = 1; return; }
    kp_put(b, tag, 4); kp_u32(b, flags); kp_u32(b, (uint32_t)n); kp_put(b, d, n);
}

/* byte reader: every read is bounds-checked; a short read sets err */
typedef struct { const uint8_t *p; size_t n, i; int err; } kp_rd;
static const uint8_t *kp_take(kp_rd *r, size_t n) {
    if (r->err || n > r->n - r->i) { r->err = 1; return NULL; }
    const uint8_t *q = r->p + r->i; r->i += n; return q;
}
static uint8_t kp_g8(kp_rd *r) { const uint8_t *q = kp_take(r, 1); return q ? q[0] : 0; }
static uint16_t kp_g16(kp_rd *r) { const uint8_t *q = kp_take(r, 2); return q ? (uint16_t)(q[0] | q[1] << 8) : 0; }
static uint32_t kp_g32(kp_rd *r) { const uint8_t *q = kp_take(r, 4); return q ? get32(q) : 0; }
static uint64_t kp_g64(kp_rd *r) { const uint8_t *q = kp_take(r, 8); return q ? get64(q) : 0; }
static double kp_gf64(kp_rd *r) { uint64_t u = kp_g64(r); double v; memcpy(&v, &u, 8); return v; }

/* ---------------------------------------------------------------- pinned plans
 * A pinned plan is keyed by the shape of the file's first line: digit runs become '0', letter runs 'a', other bytes
 * stay (so the header of a CSV export, or the timestamp and level of a log line, match across files of one source),
 * at most 32 bytes. Binary files are keyed by their first 32 bytes as they are (a NumPy or image header). A key
 * match is necessary, not sufficient: table plans are checked against the field count of the input's second line. */
static int kp_is_binary(const uint8_t *s, size_t n) {
    size_t h = n < 4096 ? n : 4096, pr = 0;
    for (size_t i = 0; i < h; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 0xC2; }
    return pr * 100 < h * 95;
}
static size_t kp_key(const uint8_t *s, size_t n, uint8_t key[KP_KEY], int *binary) {
    *binary = kp_is_binary(s, n);
    if (*binary) { size_t k = n < KP_KEY ? n : KP_KEY; memcpy(key, s, k); return k; }
    size_t i = 0, o = 0, lim = n < 1024 ? n : 1024;
    while (i < lim && s[i] != '\n' && o < KP_KEY) {
        uint8_t c = s[i];
        if (c >= '0' && c <= '9') { while (i < lim && s[i] >= '0' && s[i] <= '9') i++; key[o++] = '0'; }
        else if ((c | 32) >= 'a' && (c | 32) <= 'z') { while (i < lim && (s[i] | 32) >= 'a' && (s[i] | 32) <= 'z') i++; key[o++] = 'a'; }
        else { key[o++] = c; i++; }
    }
    return o;
}
/* size band of an input: the analysis decides differently for small and large files of one source */
static uint8_t kp_band(size_t n) { return n < (64u << 10) ? 0 : n < (1u << 20) ? 1 : n < (16u << 20) ? 2 : 3; }
/* fields of line `line` (0-based) with delimiter d (outside double quotes when quotes) within the first 256 KiB;
 * -1 when the line is not there */
static int kp_fields(const uint8_t *s, size_t n, int line, uint8_t d, int quotes) {
    size_t lim = n < (256u << 10) ? n : (256u << 10), i = 0;
    for (int l = 0; l < line; l++) {
        const uint8_t *e = memchr(s + i, '\n', lim - i); if (!e) return -1;
        i = (size_t)(e - s) + 1;
    }
    const uint8_t *e = memchr(s + i, '\n', lim - i); if (!e) return -1;
    int f = 1, inq = 0;
    for (const uint8_t *q = s + i; q < e; q++) { if (quotes && *q == '"') inq ^= 1; else f += *q == d && !inq; }
    return f;
}
/* the transposition fits when lines 1 to 32 (those within 256 KiB) all have its field count */
static int kp_fields_fit(const uint8_t *s, size_t n, uint8_t d, int quotes, int k) {
    int seen = 0;
    for (int l = 1; l <= 32; l++) { int f = kp_fields(s, n, l, d, quotes); if (f < 0) break; if (f != k) return 0; seen++; }
    return seen > 0;
}
static int kp_pinnable(const plan_t *pl) {
    if (pl->experimental_dna || pl->experimental_table || pl->experimental_pcm) return 0;
    return pl->t != T_REC && pl->t <= T_LINES;            /* record plans carry per-file field tables: not pinned */
}
static void kp_pin_from_plan(kp_pin *k, const plan_t *pl) {
    k->t = pl->t; k->a = pl->a; k->b = pl->b; k->c = pl->c; k->wide = pl->wide; k->verify = pl->verify; k->kgw = pl->kgw;
    k->img_be = pl->img.be; k->img_off = pl->img.off; k->img_mode = pl->img.mode; k->img_W = pl->img.W;
    k->has_fs = pl->fs != NULL; k->fs_n = 0; k->fs_gcol = 0;
    if (pl->fs) {
        k->fs_n = pl->fs->n < 0 ? 0 : pl->fs->n > 256 ? 256 : pl->fs->n; k->fs_gcol = pl->fs->gcol;
        memcpy(k->f, pl->fs->f, (size_t)k->fs_n * sizeof(tformula));
    }
}
static int kp_same_plan(const kp_pin *x, const kp_pin *y) {
    if (x->t != y->t || x->a != y->a || x->b != y->b || x->c != y->c || x->wide != y->wide || x->verify != y->verify ||
        x->kgw != y->kgw || x->img_be != y->img_be || x->img_off != y->img_off || x->img_mode != y->img_mode ||
        x->img_W != y->img_W || x->has_fs != y->has_fs || x->fs_n != y->fs_n || x->fs_gcol != y->fs_gcol) return 0;
    for (int i = 0; i < x->fs_n; i++)
        if (x->f[i].y != y->f[i].y || x->f[i].a != y->f[i].a || x->f[i].b != y->f[i].b || x->f[i].form != y->f[i].form ||
            memcmp(&x->f[i].k, &y->f[i].k, sizeof(double))) return 0;
    return 1;
}
static int kp_pin_valid(const kp_pin *k) {
    if (k->t < 0 || k->t > T_LINES || k->t == T_REC || k->keylen > KP_KEY || k->fs_n < 0 || k->fs_n > 256) return 0;
    if ((k->t == T_TP || k->t == T_TBL) && (k->c < 0 || k->c > 255 || k->a < 0 || k->a > 255)) return 0;
    if (k->t == T_SHUF && (k->a < 1 || k->a > 8 || k->b < 1 || k->b > 64)) return 0;
    if (k->t == T_KG && (k->c < 0 || k->a < 0 || k->a > 255 || k->b < 0 || k->b > 255)) return 0;
    for (int i = 0; i < k->fs_n; i++) {
        const tformula *f = &k->f[i];
        if (f->form < FM_RATIO || f->form > FM_PCT || f->y > 255 || f->a > 255 || (f->b > 255 && f->b != FM_NONE) || !isfinite(f->k)) return 0;
    }
    return 1;
}
/* Called by plan_worker: the pinned plan for this input, if one matches. */
static int kp_plan_pinned(const struct kolmo_profile *P, const uint8_t *s, size_t n, const kolmo_params *p, plan_t *out) {
    if (!P->npin || !p->transforms || n < 4096) return 0;
    uint8_t key[KP_KEY]; int binary; size_t kl = kp_key(s, n, key, &binary);
    const kp_pin *k = NULL;
    for (uint32_t i = 0; i < P->npin && !k; i++) {
        const kp_pin *c = &P->pins[i];
        if (c->keylen == kl && c->binary == binary && c->mode == (uint8_t)p->mode && c->transforms == (uint8_t)p->transforms && c->band == kp_band(n) &&
            !memcmp(c->key, key, kl)) k = c;
    }
    if (!k) return 0;
    if ((k->t == T_TP || k->t == T_TBL) && !kp_fields_fit(s, n, (uint8_t)k->a, k->b != 0, k->c + 1)) return 0;
    if (k->t == T_KG && k->a && kp_fields(s, n, 1, (uint8_t)k->a, 0) <= 256) return 0;
    if (k->t == T_KG && !k->a && (size_t)k->c + 64 > n) return 0;
    if (k->t == T_IMG && (size_t)k->img_off + 64 > n) return 0;
    plan_t pl = {0};
    pl.t = k->t; pl.a = k->a; pl.b = k->b; pl.c = k->c; pl.wide = k->wide; pl.verify = k->verify; pl.kgw = k->kgw;
    pl.img.be = k->img_be; pl.img.off = k->img_off; pl.img.mode = k->img_mode; pl.img.W = k->img_W;
    if (k->has_fs) {
        pl.fs = malloc(sizeof(fset_t)); if (!pl.fs) return 0;
        memset(pl.fs, 0, sizeof(fset_t)); pl.fs->n = k->fs_n; pl.fs->gcol = k->fs_gcol;
        memcpy(pl.fs->f, k->f, (size_t)k->fs_n * sizeof(tformula));
    }
    *out = pl; return 1;
}
static void kp_pins_write(kp_buf *b, const kp_pin *pins, uint32_t npin) {
    kp_u32(b, npin);
    for (uint32_t i = 0; i < npin; i++) {
        const kp_pin *k = &pins[i];
        kp_u8(b, k->keylen); kp_put(b, k->key, k->keylen); kp_u8(b, k->binary); kp_u8(b, k->mode); kp_u8(b, k->transforms); kp_u8(b, k->band);
        kp_u32(b, (uint32_t)k->t); kp_u32(b, (uint32_t)k->a); kp_u32(b, (uint32_t)k->b); kp_u32(b, (uint32_t)k->c);
        kp_u32(b, (uint32_t)k->wide); kp_u32(b, (uint32_t)k->verify); kp_u32(b, k->kgw);
        kp_u32(b, (uint32_t)k->img_be); kp_u32(b, (uint32_t)k->img_off); kp_u32(b, (uint32_t)k->img_mode); kp_u64(b, (uint64_t)k->img_W);
        kp_u8(b, (uint8_t)k->has_fs); kp_u16(b, (uint16_t)k->fs_n); kp_u32(b, (uint32_t)k->fs_gcol);
        for (int j = 0; j < k->fs_n; j++) {
            kp_u16(b, k->f[j].y); kp_u16(b, k->f[j].a); kp_u16(b, k->f[j].b); kp_u8(b, k->f[j].form); kp_u8(b, 0); kp_f64(b, k->f[j].k);
        }
    }
}
static int kp_pins_parse(struct kolmo_profile *P, const uint8_t *d, size_t n) {
    kp_rd r = {d, n, 0, 0};
    uint32_t cnt = kp_g32(&r);
    if (r.err || cnt > KP_MAX_PINS) return -1;
    kp_pin *pins = calloc(cnt ? cnt : 1, sizeof(kp_pin)); if (!pins) return -2;
    for (uint32_t i = 0; i < cnt; i++) {
        kp_pin *k = &pins[i];
        k->keylen = kp_g8(&r); if (k->keylen > KP_KEY) r.err = 1;
        const uint8_t *key = kp_take(&r, k->keylen); if (key) memcpy(k->key, key, k->keylen);
        k->binary = kp_g8(&r); k->mode = kp_g8(&r); k->transforms = kp_g8(&r); k->band = kp_g8(&r);
        k->t = (int32_t)kp_g32(&r); k->a = (int32_t)kp_g32(&r); k->b = (int32_t)kp_g32(&r); k->c = (int32_t)kp_g32(&r);
        k->wide = (int32_t)kp_g32(&r); k->verify = (int32_t)kp_g32(&r); k->kgw = kp_g32(&r);
        k->img_be = (int32_t)kp_g32(&r); k->img_off = (int32_t)kp_g32(&r); k->img_mode = (int32_t)kp_g32(&r); k->img_W = (int64_t)kp_g64(&r);
        k->has_fs = kp_g8(&r); k->fs_n = kp_g16(&r); k->fs_gcol = (int32_t)kp_g32(&r);
        if (k->has_fs > 1 || k->fs_n > 256 || (!k->has_fs && k->fs_n)) r.err = 1;
        for (int j = 0; j < k->fs_n && !r.err; j++) {
            k->f[j].y = kp_g16(&r); k->f[j].a = kp_g16(&r); k->f[j].b = kp_g16(&r); k->f[j].form = kp_g8(&r);
            if (kp_g8(&r)) r.err = 1;
            k->f[j].k = kp_gf64(&r);
        }
        if (r.err || k->binary > 1 || k->mode > KOLMO_ULTRA || k->transforms > 3 || k->band > 3 || !kp_pin_valid(k)) { free(pins); return -1; }
    }
    if (r.i != n) { free(pins); return -1; }
    P->pins = pins; P->npin = cnt; return 0;
}

/* ---------------------------------------------------------------- codecs of profile frames */
static const ZSTD_CDict *kp_cdict(const struct kolmo_profile *Pc, int level) {
    struct kolmo_profile *P = (struct kolmo_profile *)Pc;     /* the prepared-dictionary cache is internal state */
    int i = level + 8; if (i < 0) i = 0; if (i >= KP_ZSTD_LEVELS) i = KP_ZSTD_LEVELS - 1;
    pthread_mutex_lock(&P->mu);
    if (!P->cdict[i]) P->cdict[i] = ZSTD_createCDict(P->zdict, P->zdictlen, i - 8);
    ZSTD_CDict *c = P->cdict[i];
    pthread_mutex_unlock(&P->mu);
    return c;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static const BrotliEncoderPreparedDictionary *kp_bdict(const struct kolmo_profile *Pc) {
    struct kolmo_profile *P = (struct kolmo_profile *)Pc;
    pthread_mutex_lock(&P->mu);
    if (!P->bdict && !P->bdict_failed) {
        P->bdict = BrotliEncoderPrepareDictionary(BROTLI_SHARED_DICTIONARY_RAW, P->rdictlen, P->rdict, BROTLI_MAX_QUALITY, NULL, NULL, NULL);
        P->bdict_failed = !P->bdict;
    }
    const BrotliEncoderPreparedDictionary *d = P->bdict;
    pthread_mutex_unlock(&P->mu);
    return d;
}
static const LZ4_stream_t *kp_l4(const struct kolmo_profile *Pc) {
    struct kolmo_profile *P = (struct kolmo_profile *)Pc;
    pthread_mutex_lock(&P->mu);
    if (!P->l4 && !P->l4_failed) {
        LZ4_stream_t *l = malloc(sizeof(LZ4_stream_t));
        if (l && LZ4_initStream(l, sizeof(LZ4_stream_t))) {
            size_t dl = P->rdictlen < KP_LZ4_DICT ? P->rdictlen : KP_LZ4_DICT;
            LZ4_loadDict(l, (const char *)P->rdict + P->rdictlen - dl, (int)dl);
            P->l4 = l;
        } else { free(l); P->l4_failed = 1; }
    }
    const LZ4_stream_t *d = P->l4;
    pthread_mutex_unlock(&P->mu);
    return d;
}
static size_t kp_codec_compress(const struct kolmo_profile *P, void **zcp, int codec, int level, const uint8_t *in,
                                size_t n, uint8_t *out, size_t cap, int *handled) {
    *handled = (codec == C_ZSTD || codec == C_BROTLI || codec == C_LZ4) && n <= P->dlim;
    if (!*handled) return 0;
    if (codec == C_ZSTD) {
        ZSTD_CCtx *zc = *zcp ? *zcp : (*zcp = ZSTD_createCCtx());
        if (!zc) return 0;
        const ZSTD_CDict *cd = P->zdictlen ? kp_cdict(P, level) : NULL;
        if (P->zdictlen && !cd) return 0;
        if (n <= KP_ZBLOCK) {                              /* one raw block; 0 (not compressible) stores the block */
            size_t e = cd ? ZSTD_compressBegin_usingCDict(zc, cd) : ZSTD_compressBegin(zc, level);
            if (ZSTD_isError(e) || !n || n > ZSTD_getBlockSize(zc)) return 0;
            size_t r = ZSTD_compressBlock(zc, out, cap, in, n);
            return ZSTD_isError(r) ? 0 : r;
        }
        ZSTD_CCtx_reset(zc, ZSTD_reset_session_and_parameters);
        size_t e = cd ? ZSTD_CCtx_refCDict(zc, cd) : ZSTD_CCtx_setParameter(zc, ZSTD_c_compressionLevel, level);
        /* magicless frames, no content size, no dictionary id, no checksum: the container already records and checks
         * all of these (format is ZSTD_c_experimentalParam2, ZSTD_f_zstd1_magicless = 1) */
        if (!ZSTD_isError(e)) e = ZSTD_CCtx_setParameter(zc, ZSTD_c_experimentalParam2, 1);
        if (!ZSTD_isError(e)) e = ZSTD_CCtx_setParameter(zc, ZSTD_c_contentSizeFlag, 0);
        if (!ZSTD_isError(e)) e = ZSTD_CCtx_setParameter(zc, ZSTD_c_dictIDFlag, 0);
        if (!ZSTD_isError(e)) e = ZSTD_CCtx_setParameter(zc, ZSTD_c_checksumFlag, 0);
        size_t r = ZSTD_isError(e) ? e : ZSTD_compress2(zc, out, cap, in, n);
        ZSTD_CCtx_reset(zc, ZSTD_reset_session_and_parameters);
        return ZSTD_isError(r) ? 0 : r;
    }
    if (codec == C_BROTLI) {
        const BrotliEncoderPreparedDictionary *bd = P->rdictlen ? kp_bdict(P) : NULL;
        if (P->rdictlen && !bd) return 0;
        BrotliEncoderState *s = BrotliEncoderCreateInstance(NULL, NULL, NULL);
        if (!s) return 0;
        size_t avail_in = n, avail_out = cap; const uint8_t *ni = in; uint8_t *no = out;
        int ok = BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, (uint32_t)level) &&
                 BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, 24) &&
                 BrotliEncoderSetParameter(s, BROTLI_PARAM_SIZE_HINT, n > (1u << 30) ? (1u << 30) : (uint32_t)n) &&
                 (!bd || BrotliEncoderAttachPreparedDictionary(s, bd)) &&
                 BrotliEncoderCompressStream(s, BROTLI_OPERATION_FINISH, &avail_in, &ni, &avail_out, &no, NULL) &&
                 BrotliEncoderIsFinished(s) && !avail_in;
        BrotliEncoderDestroyInstance(s);
        return ok ? cap - avail_out : 0;
    }
    if (codec == C_LZ4) {
        if (n > LZ4_MAX_INPUT_SIZE || cap > INT32_MAX) return 0;
        const LZ4_stream_t *l4 = P->rdictlen ? kp_l4(P) : NULL;
        if (P->rdictlen && !l4) return 0;
        LZ4_stream_t st;
        if (!LZ4_initStream(&st, sizeof st)) return 0;
        if (l4) LZ4_attach_dictionary(&st, l4);
        int r = LZ4_compress_fast_continue(&st, (const char *)in, (char *)out, (int)n, (int)cap, level > 0 ? level : 1);
        return r > 0 ? (size_t)r : 0;
    }
    return 0;
}
static int kp_codec_decompress(const struct kolmo_profile *P, void **zdp, int codec, const uint8_t *in, size_t cn,
                               uint8_t *out, size_t n, int *handled) {
    *handled = (codec == C_ZSTD || codec == C_BROTLI || codec == C_LZ4) && n <= P->dlim;
    if (!*handled) return -1;
    if (codec == C_ZSTD) {
        ZSTD_DCtx *zd = *zdp ? *zdp : (*zdp = ZSTD_createDCtx());
        if (!zd) return -1;
        if (n <= KP_ZBLOCK) {
            size_t e = P->ddict ? ZSTD_decompressBegin_usingDDict(zd, P->ddict) : ZSTD_decompressBegin(zd);
            size_t r = ZSTD_isError(e) || !cn ? (size_t)-1 : ZSTD_decompressBlock(zd, out, n, in, cn);
            return !ZSTD_isError(r) && r == n ? 0 : -1;
        }
        ZSTD_DCtx_reset(zd, ZSTD_reset_session_and_parameters);
        size_t e = ZSTD_DCtx_setParameter(zd, ZSTD_d_experimentalParam1, 1);   /* ZSTD_d_format = magicless */
        if (!ZSTD_isError(e) && P->ddict) e = ZSTD_DCtx_refDDict(zd, P->ddict);
        size_t r = ZSTD_isError(e) ? e : ZSTD_decompressDCtx(zd, out, n, in, cn);
        ZSTD_DCtx_reset(zd, ZSTD_reset_session_and_parameters);
        return !ZSTD_isError(r) && r == n ? 0 : -1;
    }
    if (codec == C_BROTLI) {
        BrotliDecoderState *s = BrotliDecoderCreateInstance(NULL, NULL, NULL);
        if (!s) return -1;
        size_t avail_in = cn, avail_out = n; const uint8_t *ni = in; uint8_t *no = out;
        int ok = (!P->rdictlen || BrotliDecoderAttachDictionary(s, BROTLI_SHARED_DICTIONARY_RAW, P->rdictlen, P->rdict)) &&
                 BrotliDecoderDecompressStream(s, &avail_in, &ni, &avail_out, &no, NULL) == BROTLI_DECODER_RESULT_SUCCESS &&
                 !avail_in && !avail_out;
        BrotliDecoderDestroyInstance(s);
        return ok ? 0 : -1;
    }
    if (codec == C_LZ4) {
        if (cn > INT32_MAX || n > INT32_MAX) return -1;
        size_t dl = P->rdictlen < KP_LZ4_DICT ? P->rdictlen : KP_LZ4_DICT;
        int r = dl ? LZ4_decompress_safe_usingDict((const char *)in, (char *)out, (int)cn, (int)n, (const char *)P->rdict + P->rdictlen - dl, (int)dl)
                   : LZ4_decompress_safe((const char *)in, (char *)out, (int)cn, (int)n);
        return r == (int)n ? 0 : -1;
    }
    return -1;
}

#pragma GCC diagnostic pop

/* ---------------------------------------------------------------- load, free */
static void kp_free(struct kolmo_profile *P) {
    if (!P) return;
    for (int i = 0; i < KP_ZSTD_LEVELS; i++) ZSTD_freeCDict(P->cdict[i]);
    ZSTD_freeDDict(P->ddict);
    if (P->bdict) BrotliEncoderDestroyPreparedDictionary(P->bdict);
    free(P->l4); free(P->zdict); free(P->rdict); free(P->pins); free(P->info);
    pthread_mutex_destroy(&P->mu);
    free(P);
}
static uint8_t *kp_dup(const uint8_t *d, size_t n) { uint8_t *q = malloc(n ? n : 1); if (q && n) memcpy(q, d, n); return q; }

static int kp_load(const uint8_t *s, size_t n, struct kolmo_profile **out) {
    *out = NULL;
    if (!s || n < KP_HDR + 4 || memcmp(s, KP_MAGIC, 4)) return KOLMO_ERR_PROFILE;
    if ((s[4] | s[5] << 8) != KP_VERSION || s[6] || s[7]) return KOLMO_ERR_PROFILE;
    if (kolmo_crc32c(0, s, n - 4) != get32(s + n - 4)) return KOLMO_ERR_PROFILE;
    uint32_t id = get32(s + 8), cnt = get32(s + 12);
    if ((uint32_t)kolmo_xxh3(s + KP_HDR, n - 4 - KP_HDR, KP_ID_SEED) != id) return KOLMO_ERR_PROFILE;
    struct kolmo_profile *P = calloc(1, sizeof *P);
    if (!P) return KOLMO_ERR_ALLOC;
    if (pthread_mutex_init(&P->mu, NULL)) { free(P); return KOLMO_ERR_ALLOC; }
    P->id = id; P->dlim = KP_DLIM_DEFAULT;
    kp_rd r = {s, n - 4, KP_HDR, 0};
    int ret = 0, seen = 0;                                 /* seen: bit per known section, to reject repeats */
    for (uint32_t i = 0; i < cnt && !ret; i++) {
        const uint8_t *tag = kp_take(&r, 4); uint32_t fl = kp_g32(&r), len = kp_g32(&r);
        const uint8_t *d = kp_take(&r, len);
        if (r.err || !tag || (fl & ~KP_CRIT)) { ret = KOLMO_ERR_PROFILE; break; }
        int bit = !memcmp(tag, "PARM", 4) ? 1 : !memcmp(tag, "PLAN", 4) ? 2 : !memcmp(tag, "ZDIC", 4) ? 4 :
                  !memcmp(tag, "RDIC", 4) ? 8 : !memcmp(tag, "INFO", 4) ? 16 : !memcmp(tag, "DLIM", 4) ? 32 : 0;
        if (!bit) { if (fl & KP_CRIT) ret = KOLMO_ERR_PROFILE; continue; }   /* unknown: skipped unless critical (CMPR) */
        if (seen & bit) { ret = KOLMO_ERR_PROFILE; break; }
        seen |= bit;
        if (bit == 1) {
            if (len != 8 || (fl & KP_CRIT) || d[0] > KOLMO_ULTRA || d[2] > 3 || d[3]) { ret = KOLMO_ERR_PROFILE; break; }
            P->has_parm = 1; P->parm = (kolmo_params){(kolmo_mode)d[0], (int8_t)d[1], 0, get32(d + 4), d[2]};
        } else if (bit == 2) {
            int e = kp_pins_parse(P, d, len); if (e) ret = e == -2 ? KOLMO_ERR_ALLOC : KOLMO_ERR_PROFILE;
        } else if (bit == 4 || bit == 8) {
            if (!(fl & KP_CRIT) || !len) { ret = KOLMO_ERR_PROFILE; break; }
            uint8_t *q = kp_dup(d, len); if (!q) { ret = KOLMO_ERR_ALLOC; break; }
            if (bit == 4) { P->zdict = q; P->zdictlen = len; } else { P->rdict = q; P->rdictlen = len; }
        } else if (bit == 32) {
            if (len != 8 || !(fl & KP_CRIT)) { ret = KOLMO_ERR_PROFILE; break; }
            P->dlim = get64(d);
        } else {
            P->info = malloc((size_t)len + 1); if (!P->info) { ret = KOLMO_ERR_ALLOC; break; }
            memcpy(P->info, d, len); P->info[len] = 0;
        }
    }
    if (!ret && (r.err || r.i != n - 4)) ret = KOLMO_ERR_PROFILE;
    if (!ret && P->zdictlen) {                             /* a dictionary zstd cannot load is a damaged profile */
        P->ddict = ZSTD_createDDict(P->zdict, P->zdictlen);
        if (!P->ddict || ZSTD_getDictID_fromDDict(P->ddict) != ZSTD_getDictID_fromDict(P->zdict, P->zdictlen)) ret = KOLMO_ERR_PROFILE;
    }
    if (ret) { kp_free(P); return ret; }
    *out = P; return 0;
}

/* Serialize: sections into b (after a 16-byte header), then the id, the count and the CRC. */
static void kp_finish(kp_buf *b, uint32_t nsec) {
    if (b->err || b->n < KP_HDR) { b->err = 1; return; }
    memcpy(b->p, KP_MAGIC, 4); b->p[4] = KP_VERSION; b->p[5] = 0; b->p[6] = b->p[7] = 0;
    put32(b->p + 8, (uint32_t)kolmo_xxh3(b->p + KP_HDR, b->n - KP_HDR, KP_ID_SEED)); put32(b->p + 12, nsec);
    kp_u32(b, kolmo_crc32c(0, b->p, b->n));
}

/* ---------------------------------------------------------------- training
 * Plans: every sample of at least 4 KiB is analysed as plan_worker would; samples are grouped by key, and each key
 * whose samples (by bytes) mostly got the same pinnable plan pins it. Dictionaries, over the samples cut into pieces of
 * at most 16 KiB (evenly spaced pieces when there are more than the cap): trainer 0 is ZDICT_trainFromBuffer, what
 * `zstd --train` runs (fastCover, d=8, 4 steps; 32 MiB cap); trainer 1 is COVER with its parameter search over
 * 2 steps on one thread (one thread, so the result does not depend on thread timing; 16 MiB cap). The raw
 * dictionary is the content part of the zstd dictionary. */
#define KP_PIECE (16u << 10)
#define KP_TRAIN_CAP ((size_t)32 << 20)
#define KP_COVER_CAP ((size_t)16 << 20)
#define KP_TRAINERS 2
static const char *const KP_TRAINER_NAME[KP_TRAINERS] = {"fastcover (zstd --train)", "cover"};
static int kp_train_dict(const uint8_t *samples, const size_t *sizes, size_t ns, size_t cap, int trainer, uint8_t **zd, size_t *zl) {
    *zd = NULL; *zl = 0;
    const size_t CAPB = trainer == 1 ? KP_COVER_CAP : KP_TRAIN_CAP;
    size_t total = 0, pieces = 0;
    for (size_t i = 0; i < ns; i++) { total += sizes[i]; pieces += (sizes[i] + KP_PIECE - 1) / KP_PIECE; }
    if (total < 4096 || pieces < 8) return 0;              /* too little to learn from: no dictionary */
    size_t step = total > CAPB ? (total + CAPB - 1) / CAPB : 1;   /* keep every step-th piece */
    size_t *psz = malloc(pieces * sizeof(size_t)); uint8_t *buf = malloc(total < CAPB ? total : CAPB + KP_PIECE);
    uint8_t *dict = malloc(cap);
    if (!psz || !buf || !dict) { free(psz); free(buf); free(dict); return KOLMO_ERR_ALLOC; }
    size_t np = 0, off = 0, k = 0, pos = 0;
    for (size_t i = 0; i < ns; i++) {
        for (size_t o = 0; o < sizes[i]; o += KP_PIECE, k++) {
            size_t m = sizes[i] - o < KP_PIECE ? sizes[i] - o : KP_PIECE;
            if (k % step == 0 && off + m <= (total < CAPB ? total : CAPB + KP_PIECE)) {
                memcpy(buf + off, samples + pos + o, m); psz[np++] = m; off += m;
            }
        }
        pos += sizes[i];
    }
    size_t r;
    if (trainer == 1) {
        ZDICT_cover_params_t cp; memset(&cp, 0, sizeof cp);
        cp.steps = 2; cp.nbThreads = 1; cp.zParams.compressionLevel = 3;
        r = ZDICT_optimizeTrainFromBuffer_cover(dict, cap, buf, psz, (unsigned)np, &cp);
    } else r = ZDICT_trainFromBuffer(dict, cap, buf, psz, (unsigned)np);
    free(psz); free(buf);
    if (ZDICT_isError(r)) { free(dict); return 0; }        /* the trainer found nothing to keep: no dictionary */
    *zd = dict; *zl = r; return 0;
}

static int64_t kp_train(const uint8_t *samples, const size_t *sizes, size_t ns, const kolmo_params *p, size_t dict_size,
                        int no_dict, int trainer, uint64_t dlim, const char *note, kp_buf *out) {
    kp_pin *pins = calloc(KP_MAX_PINS, sizeof(kp_pin)); uint64_t *pin_bytes = calloc(KP_MAX_PINS, sizeof(uint64_t));
    if (!pins || !pin_bytes) { free(pins); free(pin_bytes); return KOLMO_ERR_ALLOC; }
    /* candidates: per key, up to 4 distinct plans with their byte counts */
    typedef struct { kp_pin key, plan[4]; uint64_t bytes[4]; int np; uint64_t total; } cand_t;
    cand_t *cand = calloc(KP_MAX_PINS, sizeof(cand_t)); uint32_t ncand = 0;
    if (!cand) { free(pins); free(pin_bytes); return KOLMO_ERR_ALLOC; }
    size_t pos = 0, analysed = 0;
    for (size_t i = 0; i < ns; pos += sizes[i], i++) {
        const uint8_t *s = samples + pos; size_t len = sizes[i];
        if (len < 4096 || !p->transforms) continue;
        kp_pin k; memset(&k, 0, sizeof k); int bin;
        k.keylen = (uint8_t)kp_key(s, len, k.key, &bin); k.binary = (uint8_t)bin;
        k.mode = (uint8_t)p->mode; k.transforms = (uint8_t)p->transforms; k.band = kp_band(len);
        plan_t pl = plan_for(s, len, p, 1, 0);
        if (p->mode == KOLMO_RT && (pl.t == T_TP || pl.t == T_LOG)) pl.t = T_NONE;
        int ok = kp_pinnable(&pl);
        if (ok) kp_pin_from_plan(&k, &pl);
        free(pl.fs); free(pl.rec);
        analysed++;
        uint32_t c = 0;
        while (c < ncand && !(cand[c].key.keylen == k.keylen && cand[c].key.binary == k.binary && cand[c].key.band == k.band &&
                              !memcmp(cand[c].key.key, k.key, k.keylen))) c++;
        if (c == ncand) { if (ncand == KP_MAX_PINS) continue; cand[c].key = k; ncand++; }
        cand[c].total += len;
        if (!ok) continue;
        int j = 0; while (j < cand[c].np && !kp_same_plan(&cand[c].plan[j], &k)) j++;
        if (j == cand[c].np) { if (j == 4) continue; cand[c].plan[j] = k; cand[c].np++; }
        cand[c].bytes[j] += len;
    }
    uint32_t npin = 0;
    for (uint32_t c = 0; c < ncand; c++) {
        int best = -1; for (int j = 0; j < cand[c].np; j++) if (best < 0 || cand[c].bytes[j] > cand[c].bytes[best]) best = j;
        /* pin only a plan the analysis chose for at least 90% of the bytes under this key and size band: where it
         * decides file by file, it keeps deciding (a pinned plan that does not fit a file can cost 75%) */
        if (best >= 0 && cand[c].bytes[best] * 10 >= cand[c].total * 9) { pins[npin] = cand[c].plan[best]; pin_bytes[npin] = cand[c].bytes[best]; npin++; }
    }
    free(cand);
    uint8_t *zd = NULL; size_t zl = 0; int64_t ret = 0;
    if (!no_dict) ret = kp_train_dict(samples, sizes, ns, dict_size, trainer, &zd, &zl);
    if (ret) { free(pins); free(pin_bytes); return ret; }
    size_t content = 0;
    if (zl) { size_t h = ZDICT_getDictHeaderSize(zd, zl); content = ZSTD_isError(h) || h >= zl ? 0 : zl - h; }

    kp_buf b = {0}; uint8_t hdr[KP_HDR] = {0}; kp_put(&b, hdr, KP_HDR); uint32_t nsec = 0;
    uint8_t parm[8] = {(uint8_t)p->mode, (uint8_t)(int8_t)p->level, (uint8_t)p->transforms, 0}; put32(parm + 4, p->block);
    kp_section(&b, "PARM", 0, parm, 8); nsec++;
    if (npin) { kp_buf pb = {0}; kp_pins_write(&pb, pins, npin); if (pb.err) b.err = 1; else kp_section(&b, "PLAN", 0, pb.p, pb.n); free(pb.p); nsec++; }
    if (zl) { kp_section(&b, "ZDIC", KP_CRIT, zd, zl); nsec++; }
    if (content) { kp_section(&b, "RDIC", KP_CRIT, zd + zl - content, content); nsec++; }
    if (zl || content) { uint8_t x[8]; put64(x, dlim); kp_section(&b, "DLIM", KP_CRIT, x, 8); nsec++; }
    char info[1024];
    static const char *const MN[] = {"rt", "fast", "eco", "hc", "max", "ultra"};
    int il = snprintf(info, sizeof info, "trained on %zu samples, %llu bytes (%zu analysed); setting %s level %d analysis %d; "
                      "%u pinned plan%s; zstd dictionary %zu bytes (%s), raw dictionary %zu bytes%s%s",
                      ns, (unsigned long long)pos, analysed, MN[p->mode <= KOLMO_ULTRA ? p->mode : 1], p->level, p->transforms,
                      npin, npin == 1 ? "" : "s", zl, KP_TRAINER_NAME[trainer], content, note ? "; " : "", note ? note : "");
    if (il > 0) { kp_section(&b, "INFO", 0, info, (size_t)il < sizeof info ? (size_t)il : sizeof info - 1); nsec++; }
    kp_finish(&b, nsec);
    free(zd); free(pins); free(pin_bytes);
    if (b.err) { free(b.p); return KOLMO_ERR_ALLOC; }
    *out = b; return (int64_t)b.n;
}

#include <stdarg.h>
static void kp_out(char *o, size_t cap, size_t *w, const char *fmt, ...) {
    if (*w + 1 >= cap) return;
    va_list ap; va_start(ap, fmt); int k = vsnprintf(o + *w, cap - *w, fmt, ap); va_end(ap);
    if (k > 0) *w += (size_t)k < cap - *w ? (size_t)k : cap - *w - 1;
}
static size_t kp_describe(const struct kolmo_profile *P, char *o, size_t cap) {
    static const char *const MN[] = {"rt", "fast", "eco", "hc", "max", "ultra"};
    static const char *const TN[] = {"none", "transpose", "shuffle", "table", "log", "image16", "record", "x86", "word", "graph", "lines"};
    size_t w = 0;
    if (!cap) return 0;
    o[0] = 0;
    kp_out(o, cap, &w, "profile %08x\n", P->id);
    if (P->has_parm) kp_out(o, cap, &w, "setting: %s level %d analysis %d block %u\n", MN[P->parm.mode], P->parm.level, P->parm.transforms, P->parm.block);
    kp_out(o, cap, &w, "zstd dictionary: %zu bytes; raw dictionary (Brotli, LZ4): %zu bytes; used on leaves up to ", P->zdictlen, P->rdictlen);
    if (P->dlim == UINT64_MAX) kp_out(o, cap, &w, "any size\n"); else kp_out(o, cap, &w, "%llu bytes\n", (unsigned long long)P->dlim);
    kp_out(o, cap, &w, "pinned plans: %u\n", P->npin);
    for (uint32_t i = 0; i < P->npin; i++) {
        const kp_pin *k = &P->pins[i];
        static const char *const BN[] = {"<64K", "<1M", "<16M", ">=16M"};
        kp_out(o, cap, &w, "  [%s %s %s] ", k->binary ? "binary" : "text", MN[k->mode], BN[k->band]);
        for (int j = 0; j < k->keylen; j++) kp_out(o, cap, &w, k->key[j] >= 32 && k->key[j] < 127 ? "%c" : "\\x%02x", k->key[j]);
        kp_out(o, cap, &w, " -> %s a=%d b=%d c=%d%s%s\n", TN[k->t], k->a, k->b, k->c, k->wide ? " wide" : "", k->has_fs ? " formulas" : "");
    }
    if (P->info) kp_out(o, cap, &w, "%s\n", P->info);
    return w;
}
