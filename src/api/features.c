/* Public entry points of the rc3 features: trained profiles (kolmo train, -P), --target, kolmo calibrate.
 *
 * Part of the src/kolmo.c translation unit for now (see src/plan/profile.h). Format of profile files and frames:
 * SPEC.md, "Trained profiles (KPF1)" and "Profile frames". */
#include <time.h>

/* ---------------------------------------------------------------- compact profile frame (CZC1)
 * A profile frame of one block whose original size is at most 1 MiB is written as
 *   "CZC1" | profile id u32 | A u8 | [block flag u8, when A bit 6] | original size (LEB128, 1-3 bytes) |
 *   checksum u32 | [block plan 4 bytes, when A bit 5] | block payload
 * A: bits 0-4 the block's codec, bit 5 plan bytes present (otherwise 0 0 0 0), bit 6 block flag present (otherwise
 * 0), bit 7 zero. The block is decoded as a block of a version 11 frame. The checksum is always the block checksum
 * over the decoded bytes (the low 32 bits of XXH3-64(raw, 0x637a6e31)), so every header field is covered by the
 * check of the decoded output. A plain block (plan 0, flag 0) is decoded directly; any other block through the
 * equivalent one-block CZN1 frame. */
#define KC_MAX ((size_t)1 << 20)
#define KC_MIN 14                                         /* magic, id, A, size >= 1 byte, checksum, 1 payload byte */
#define KC_VER 11

static int kc_parse(const uint8_t *s, size_t n, uint32_t *id, int *codec, int *tflag, uint64_t *orig,
                    uint32_t *sum, uint8_t plan[4], size_t *body) {
    if (n < KC_MIN - 1 || memcmp(s, "CZC1", 4)) return KOLMO_ERR_CORRUPT;
    *id = get32(s + 4); uint8_t A = s[8]; size_t i = 9;
    if (A & 0x80) return KOLMO_ERR_CORRUPT;
    *codec = A & 31; *tflag = 0;
    if (A & 0x40) { *tflag = s[i++]; if (!*tflag) return KOLMO_ERR_CORRUPT; }
    uint64_t v = 0; int sh = 0;
    for (;;) {
        if (i >= n || sh > 14) return KOLMO_ERR_CORRUPT;
        uint8_t b = s[i++]; v |= (uint64_t)(b & 127) << sh; sh += 7;
        if (!(b & 128)) break;
    }
    if (v > KC_MAX || n - i < 4) return KOLMO_ERR_CORRUPT;
    *orig = v; *sum = get32(s + i); i += 4;
    memset(plan, 0, 4);
    if (A & 0x20) {
        if (n - i < 4) return KOLMO_ERR_CORRUPT;
        memcpy(plan, s + i, 4); i += 4;
        if (!(plan[0] | plan[1] | plan[2] | plan[3])) return KOLMO_ERR_CORRUPT;
    }
    *body = i;
    return 0;
}
static int64_t kp_compact_original_size(const uint8_t *src, size_t n) {
    uint32_t id, sum; int codec, tflag; uint64_t orig; uint8_t plan[4]; size_t body;
    int r = kc_parse(src, n, &id, &codec, &tflag, &orig, &sum, plan, &body);
    return r ? r : (int64_t)orig;
}
static size_t kc_header(uint8_t *h, uint32_t id, int codec, int tflag, const uint8_t plan[4], size_t n, uint32_t sum) {
    int has_plan = plan[0] || plan[1] || plan[2] || plan[3];
    size_t k = 0;
    memcpy(h, "CZC1", 4); put32(h + 4, id);
    h[8] = (uint8_t)(codec | (has_plan ? 0x20 : 0) | (tflag ? 0x40 : 0)); k = 9;
    if (tflag) h[k++] = (uint8_t)tflag;
    uint64_t v = n; do { h[k++] = (uint8_t)((v & 127) | (v > 127 ? 128 : 0)); v >>= 7; } while (v);
    put32(h + k, sum); k += 4;
    if (has_plan) { memcpy(h + k, plan, 4); k += 4; }
    return k;
}
/* f: a one-block CZN1 profile frame of flen bytes made from src[0..n). Rewritten in place; returns the new length. */
static size_t kc_pack(uint8_t *f, size_t flen, const uint8_t *src, size_t n) {
    if (flen < HDR + IDX + 4 || get32(f + 20) != 1 || n > KC_MAX || get64(f + 12) != n || f[4] > KC_VER) return flen;
    const uint8_t *e = f + HDR;
    uint32_t comp = get32(e + 4);
    if ((size_t)comp + HDR + IDX != flen || comp < 4 || e[12] > 31) return flen;
    uint32_t sum = (f[10] & 1) ? blk_sum(3, src, n) : get32(e + 8);
    uint8_t h[32], plan[4]; memcpy(plan, f + HDR + IDX, 4);
    size_t k = kc_header(h, get32(f + 6), e[12], e[13], plan, n, sum), data = comp - 4;
    memmove(f + k, f + HDR + IDX + 4, data);
    memcpy(f, h, k);
    return k + data;
}
/* Direct path for inputs under 4 KiB at the zstd, Brotli and LZ4 settings: the analysis plans nothing below 4 KiB,
 * so the frame is one plain block, coded here as compress_worker codes it (stored when coding does not shrink it).
 * The output is identical to the general path (tests/test_profile.c checks it). Returns 0 when it does not apply. */
static int64_t kc_small_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *q,
                                 const struct kolmo_profile *prof) {
    if (n >= 4096 || q->mode > KOLMO_HC || cap < 32 + n) return 0;
    int codec, level; mode_codec(q, &codec, &level);
    tls_t *S = tls_begin(); if (!S) return KOLMO_ERR_ALLOC;
    codec_ctx *x = &S->x; x->dict = NULL; x->dictlen = 0; x->prof = prof;
    uint8_t *out = dst + 32; size_t room = cap - 32;
    size_t cl = codec_compress(x, codec, level, src, n, out, room);
    x->prof = NULL;
    tls_end(S);
    static const uint8_t plan0[4] = {0};
    if (cl == 0 || cl >= n) { codec = C_STORE; cl = n; out = (uint8_t *)src; }
    uint8_t h[32]; size_t k = kc_header(h, kp_id(prof), codec, 0, plan0, n, blk_sum(3, src, n));
    memmove(dst + k, out, cl);
    memcpy(dst, h, k);
    return (int64_t)(k + cl);
}
static int64_t kc_decode(const uint8_t *s, size_t n, uint8_t *dst, size_t cap, int threads, const struct kolmo_profile *prof) {
    uint32_t id, sum; int codec, tflag; uint64_t orig; uint8_t plan[4]; size_t body;
    int r = kc_parse(s, n, &id, &codec, &tflag, &orig, &sum, plan, &body);
    if (r) return r;
    if (!prof || kp_id(prof) != id) return KOLMO_ERR_PROFILE;
    if (orig > cap) return KOLMO_ERR_DSTSIZE;
    size_t data = n - body;
    if (!tflag && !(plan[0] | plan[1] | plan[2] | plan[3]) &&
        (codec == C_STORE || codec == C_ZSTD || codec == C_BROTLI || codec == C_LZ4)) {   /* plain block: directly */
        tls_t *S = tls_begin(); if (!S) return KOLMO_ERR_ALLOC;
        codec_ctx *x = &S->x; x->dict = NULL; x->dictlen = 0; x->prof = prof;
        int e = codec_decompress(x, codec, s + body, data, dst, (size_t)orig);
        x->prof = NULL;
        tls_end(S);
        if (e) return KOLMO_ERR_CORRUPT;
        return blk_sum(3, dst, (size_t)orig) == sum ? (int64_t)orig : KOLMO_ERR_CHECKSUM;
    }
    size_t fl = HDR + IDX + 4 + data;
    if (data > UINT32_MAX - 4) return KOLMO_ERR_CORRUPT;
    uint8_t stackbuf[4096], *f = fl <= sizeof stackbuf ? stackbuf : malloc(fl);
    if (!f) return KOLMO_ERR_ALLOC;
    memset(f, 0, HDR + IDX);
    memcpy(f, "CZN1", 4); f[4] = KC_VER; put32(f + 6, id); f[10] = 4;
    put64(f + 12, orig); put32(f + 20, 1); put32(f + 24, (uint32_t)(orig > 4096 ? orig : 4096));
    uint8_t *e = f + HDR; put32(e, (uint32_t)orig); put32(e + 4, (uint32_t)(4 + data)); put32(e + 8, sum); e[12] = (uint8_t)codec; e[13] = (uint8_t)tflag;
    uint32_t hc = kolmo_crc32c(0, f, 28); hc = kolmo_crc32c(hc, f + HDR, IDX); put32(f + 28, hc);
    memcpy(f + HDR + IDX, plan, 4); memcpy(f + HDR + IDX + 4, s + body, data);
    int64_t ret = run_decode_p(f, -1, fl, dst, -1, threads, 0, 0, 0, NULL, 0, 0, NULL, prof);
    if (f != stackbuf) free(f);
    return ret;
}

/* ---------------------------------------------------------------- profiles */
size_t kolmo_profile_bound(const kolmo_train_params *tp) {
    size_t d = tp && tp->dict_size ? tp->dict_size : (110u << 10);
    return 2 * d + (size_t)KP_MAX_PINS * (64 + 256 * 16) + 4096;
}
int kolmo_profile_load(const void *buf, size_t n, kolmo_profile **out) {
    if (!out) return KOLMO_ERR_PROFILE;
    return kp_load(buf, n, out);
}
void kolmo_profile_free(kolmo_profile *prof) { kp_free(prof); }
uint32_t kolmo_profile_id(const kolmo_profile *prof) { return prof ? prof->id : 0; }
void kolmo_profile_params(const kolmo_profile *prof, kolmo_params *p) {
    kolmo_params d = {KOLMO_FAST, 0, 0, 0, 1};
    *p = prof && prof->has_parm ? prof->parm : d;
}
size_t kolmo_profile_describe(const kolmo_profile *prof, char *out, size_t cap) { return prof && out ? kp_describe(prof, out, cap) : 0; }

int64_t kolmo_compress_profile(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                               const kolmo_profile *prof) {
    if (!prof) return p ? kolmo_compress(src, n, dst, cap, p) : KOLMO_ERR_PROFILE;
    kolmo_params q; if (p) q = *p; else kolmo_profile_params(prof, &q);
    if ((unsigned)q.transforms & ~(unsigned)(3 | KOLMO_T_NATIVE_LEAF | KOLMO_T_TYPED_HUF)) return KOLMO_ERR_CORRUPT;
    int64_t r = kc_small_compress(src, n, dst, cap, &q, prof);
    if (r) return r;
    r = compress_impl_p(src, -1, n, dst, -1, cap, &q, NULL, 0, 0, prof);
    if (r > 0) r = (int64_t)kc_pack(dst, (size_t)r, src, n);
    return r;
}
int64_t kolmo_decompress_profile(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int threads,
                                 const kolmo_profile *prof) {
    if (n >= 4 && !memcmp(src, "CZC1", 4)) return kc_decode(src, n, dst, cap, threads, prof);
    int64_t o = kolmo_original_size(src, n); if (o < 0) return o;
    if ((uint64_t)o > cap) return KOLMO_ERR_DSTSIZE;
    return run_decode_p(src, -1, n, dst, -1, threads, 0, 0, 0, NULL, 0, 0, NULL, prof);
}
int kolmo_frame_profile_id(const uint8_t *src, size_t n, uint32_t *id) {
    if (n >= 4 && !memcmp(src, "CZC1", 4)) {
        if (n < KC_MIN) return KOLMO_ERR_CORRUPT;
        if (id) *id = get32(src + 4);
        return 1;
    }
    if (n < HDR || memcmp(src, "CZN1", 4)) return KOLMO_ERR_CORRUPT;
    if (src[4] >= 11 && (src[10] & 4)) { if (id) *id = get32(src + 6); return 1; }
    return 0;
}

static double kf_now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }

/* Measure setting q with profile P on the held-out samples (every fifth when there are at least five, else all):
 * total compressed bytes, compression and decompression seconds. Every round trip is checked. */
static int64_t kf_eval(const uint8_t *samples, const size_t *sizes, size_t ns, const kolmo_params *q,
                       const struct kolmo_profile *P, double *size, double *enc, double *dec, size_t *bytes) {
    *size = *enc = *dec = 0; *bytes = 0;
    size_t pos = 0;
    for (size_t i = 0; i < ns; pos += sizes[i], i++) {
        if (ns >= 5 && i % 5 != 4) continue;
        const uint8_t *s = samples + pos; size_t n = sizes[i];
        size_t c = kolmo_bound(n, q); uint8_t *o = malloc(c), *b = malloc(n ? n : 1);
        if (!o || !b) { free(o); free(b); return KOLMO_ERR_ALLOC; }
        double t0 = kf_now(); int64_t z = kolmo_compress_profile(s, n, o, c, q, P); double t1 = kf_now();
        int64_t u = z >= 0 ? kolmo_decompress_profile(o, (size_t)z, b, n, 1, P) : z; double t2 = kf_now();
        int bad = z < 0 || u != (int64_t)n || memcmp(b, s, n);
        free(o); free(b);
        if (bad) return z < 0 ? z : KOLMO_ERR_CORRUPT;
        *size += (double)z; *enc += t1 - t0; *dec += t2 - t1; *bytes += n;
    }
    return 0;
}

/* Training. With at least five samples, a profile trained on four fifths is measured on the remaining fifth to
 * choose, in order: the setting (only with a target: every candidate setting that is not far below a floor in the
 * built-in table is measured, and the cheapest that meets the floors wins), then the largest leaf that takes the
 * dictionaries (64 KiB to unlimited, the smallest output wins; 256 KiB with fewer than five samples). The profile
 * written is then trained on every sample. */
static const uint64_t KF_DLIM[5] = {(uint64_t)64 << 10, (uint64_t)256 << 10, (uint64_t)1 << 20, (uint64_t)4 << 20, UINT64_MAX};
int64_t kolmo_profile_train(const uint8_t *samples, const size_t *sizes, size_t nsamples,
                            const kolmo_train_params *tp, uint8_t *dst, size_t cap) {
    if (!samples || !sizes || !nsamples || !dst) return KOLMO_ERR_PROFILE;
    kolmo_train_params d = {{KOLMO_FAST, 0, 0, 0, 1}, 0, 0, NULL};
    if (!tp) tp = &d;
    kolmo_params p = tp->params;
    if ((unsigned)p.mode > KOLMO_ULTRA || p.transforms < 0) return KOLMO_ERR_PROFILE;
    p.transforms &= 3; p.threads = 1;
    size_t dsz = tp->dict_size ? tp->dict_size : (110u << 10);
    if (dsz < 1024 || dsz > ((size_t)16 << 20)) return KOLMO_ERR_PROFILE;
    struct kolmo_target *t = NULL;
    if (tp->target) { int e = kt_parse(tp->target, &t); if (e) return e; }
    char note[1024] = ""; size_t nl = 0;
    uint64_t dlim = KP_DLIM_DEFAULT;
    int64_t r = 0;
    if (nsamples >= 5 && (t || !tp->no_dict)) {
        /* the interim profile: trained on the samples that are not held out */
        size_t tb = 0, ntr = 0, w = 0, pos = 0;
        for (size_t i = 0; i < nsamples; i++) if (i % 5 != 4) tb += sizes[i];
        size_t *tsz = malloc(nsamples * sizeof(size_t)); uint8_t *tbuf = malloc(tb ? tb : 1);
        if (!tsz || !tbuf) { free(tsz); free(tbuf); kt_free_target(t); return KOLMO_ERR_ALLOC; }
        for (size_t i = 0; i < nsamples; pos += sizes[i], i++)
            if (i % 5 != 4) { memcpy(tbuf + w, samples + pos, sizes[i]); w += sizes[i]; tsz[ntr++] = sizes[i]; }
        kp_buf ib = {0};
        r = kp_train(tbuf, tsz, ntr, &p, dsz, tp->no_dict, KP_DLIM_DEFAULT, NULL, &ib);
        free(tbuf); free(tsz);
        struct kolmo_profile *ip = NULL;
        if (r >= 0) r = kp_load(ib.p, ib.n, &ip);
        free(ib.p);
        if (r < 0) { kt_free_target(t); return r; }
        size_t hb = 0; double size, enc, dec;
        if (t) {
            int best = -1, cls = kt_class(samples, sizes[0]); double bestv = HUGE_VAL;
            nl += (size_t)snprintf(note + nl, sizeof note - nl, "target '%s', measured on the held-out fifth:", tp->target);
            for (int st = 0; st < KT_NSET && !r; st++) {
                if (t->min_dec > 0 && t->dec[st][cls] * 4 < t->min_dec) continue;    /* far below a floor: not measured */
                if (t->min_enc > 0 && t->enc[st][cls] * 4 < t->min_enc) continue;
                kolmo_params q = {KT_SET[st].mode, KT_SET[st].level, 1, 0, KT_SET[st].transforms};
                r = kf_eval(samples, sizes, nsamples, &q, ip, &size, &enc, &dec, &hb);
                if (r) break;
                if (nl < sizeof note) nl += (size_t)snprintf(note + nl, sizeof note - nl, " %s %.0f B %.1f/%.1f ms", KT_SET[st].name, size, enc * 1e3, dec * 1e3);
                if (t->min_dec > 0 && (dec > 0 ? hb / dec : 1e18) < t->min_dec) continue;
                if (t->min_enc > 0 && (enc > 0 ? hb / enc : 1e18) < t->min_enc) continue;
                double v = t->objective == 1 ? kt_eval(t, size, (double)hb, enc, dec) : size;
                if (v < bestv) { bestv = v; best = st; }
            }
            if (!r && best < 0) r = KOLMO_ERR_TARGET;       /* no setting meets the floors on the sample */
            if (!r) {
                p.mode = KT_SET[best].mode; p.level = KT_SET[best].level; p.transforms = KT_SET[best].transforms;
                if (nl < sizeof note) nl += (size_t)snprintf(note + nl, sizeof note - nl, "; chosen %s", KT_SET[best].name);
            }
        }
        if (!r && (ip->zdictlen || ip->rdictlen)) {
            double bests = HUGE_VAL;
            for (int k = 0; k < 5 && !r; k++) {
                ip->dlim = KF_DLIM[k];
                r = kf_eval(samples, sizes, nsamples, &p, ip, &size, &enc, &dec, &hb);
                if (!r && size < bests) { bests = size; dlim = KF_DLIM[k]; }
            }
            if (!r && nl < sizeof note)
                nl += (size_t)snprintf(note + nl, sizeof note - nl, "%sdictionary leaf limit chosen on %zu held-out bytes", nl ? "; " : "", hb);
        }
        kp_free(ip);
    } else if (t) {
        kt_free_target(t);
        return KOLMO_ERR_TARGET;                            /* a target needs samples to measure: at least five */
    }
    kt_free_target(t);
    if (r) return r;
    p.threads = 0;
    kp_buf b = {0};
    r = kp_train(samples, sizes, nsamples, &p, dsz, tp->no_dict, dlim, note[0] ? note : NULL, &b);
    if (r < 0) return r;
    if (b.n > cap) { free(b.p); return KOLMO_ERR_DSTSIZE; }
    memcpy(dst, b.p, b.n); free(b.p);
    return (int64_t)b.n;
}

/* ---------------------------------------------------------------- targets */
int kolmo_target_parse(const char *spec, kolmo_target **out) { if (!out) return KOLMO_ERR_TARGET; return kt_parse(spec, out); }
int kolmo_target_set_calibration(kolmo_target *t, const char *table, size_t n) { return t && table ? kt_set_calibration(t, table, n) : KOLMO_ERR_TARGET; }
void kolmo_target_free(kolmo_target *t) { kt_free_target(t); }
const char *kolmo_target_setting_name(int setting) { return setting >= 0 && setting < KT_NSET ? KT_SET[setting].name : NULL; }

/* Merge the frames of consecutive chunks into one frame: indexes and payloads concatenated, the highest version,
 * checksums over decoded bytes (a speed-mode frame checksums its stored bytes; those entries are recomputed). */
static int64_t kt_merge(uint8_t *const *fr, const size_t *fl, uint32_t k, const uint8_t *src, const uint64_t *starts,
                        uint8_t *dst, size_t cap, const struct kolmo_profile *prof, const int *chosen) {
    uint64_t nb = 0, pay = 0; int ver = 3, psum_all = 1; uint32_t B = 0; int modes[6] = {0};
    for (uint32_t c = 0; c < k; c++) {
        const uint8_t *f = fr[c]; uint32_t m = get32(f + 20);
        if (fl[c] < HDR || memcmp(f, "CZN1", 4) || (uint64_t)m * IDX + HDR > fl[c]) return KOLMO_ERR_CORRUPT;
        nb += m; pay += fl[c] - HDR - (size_t)m * IDX;
        if (f[4] > ver) ver = f[4];
        psum_all &= f[10] & 1;
        if (get32(f + 24) > B) B = get32(f + 24);
        modes[f[5] <= 5 ? f[5] : 1] += (int)chosen[c] >= 0;
    }
    if (nb > UINT32_MAX - 1) return KOLMO_ERR_CORRUPT;
    uint64_t total = HDR + nb * IDX + pay;
    if (total > cap) return KOLMO_ERR_DSTSIZE;
    uint8_t *ix = dst + HDR, *po = dst + HDR + nb * IDX;
    for (uint32_t c = 0; c < k; c++) {
        const uint8_t *f = fr[c]; uint32_t m = get32(f + 20); int ps = f[10] & 1;
        uint64_t raw = starts[c];
        for (uint32_t i = 0; i < m; i++) {
            memcpy(ix, f + HDR + (size_t)i * IDX, IDX);
            uint32_t len = get32(ix);
            if (raw + len > starts[c + 1]) return KOLMO_ERR_CORRUPT;
            if (ps && !psum_all) put32(ix + 8, blk_sum(3, src + raw, len));
            raw += len; ix += IDX;
        }
        size_t pl = fl[c] - HDR - (size_t)m * IDX;
        memcpy(po, f + HDR + (size_t)m * IDX, pl); po += pl;
    }
    int mode = 0; for (int i = 1; i < 6; i++) if (modes[i] > modes[mode]) mode = i;
    memcpy(dst, "CZN1", 4); dst[4] = (uint8_t)ver; dst[5] = (uint8_t)mode; memset(dst + 6, 0, 6);
    dst[10] = (uint8_t)psum_all;
    if (prof) { dst[10] |= 4; put32(dst + 6, kp_id(prof)); if (dst[4] < 11) dst[4] = 11; }
    put64(dst + 12, starts[k] - starts[0]); put32(dst + 20, (uint32_t)nb); put32(dst + 24, B);
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, (size_t)nb * IDX); put32(dst + 28, hc);
    return (int64_t)total;
}

#define KT_CHUNK ((size_t)16 << 20)
int64_t kolmo_compress_target(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, const kolmo_params *p,
                              const kolmo_target *t, const kolmo_profile *prof, kolmo_target_report *report) {
    if (!t || (!src && n) || !dst) return KOLMO_ERR_TARGET;
    int threads = p && p->threads > 0 ? p->threads : default_threads();
    size_t B = p && p->block ? p->block : KT_CHUNK; if (B < 4096) B = 4096;
    /* chunks of B bytes; text chunks end at a line end found in their second half */
    size_t nc = n / B + 2; uint64_t *starts = malloc((nc + 1) * sizeof(uint64_t)); uint32_t k = 0;
    if (!starts) return KOLMO_ERR_ALLOC;
    int text = n && !kp_is_binary(src, n);
    for (size_t pos = 0; pos < n || k == 0; ) {
        starts[k++] = pos;
        size_t end = n - pos > B ? pos + B : n;
        if (text && end < n) { size_t e = end; while (e > pos + B / 2 && src[e - 1] != '\n') e--; if (src[e - 1] == '\n') end = e; }
        pos = end;
        if (!n) break;
    }
    starts[k] = n;
    kt_job J; memset(&J, 0, sizeof J);
    J.src = src; J.starts = starts; J.nchunks = k; J.inner_threads = k == 1 ? threads : 1; J.t = t; J.prof = prof;
    J.frames = calloc(k, sizeof(uint8_t *)); J.flen = calloc(k, sizeof(size_t)); J.chosen = calloc(k, sizeof(int));
    J.pcost = calloc(k, sizeof(double)); J.penc = calloc(k, sizeof(double)); J.pdec = calloc(k, sizeof(double)); J.psize = calloc(k, sizeof(uint64_t));
    int64_t ret = KOLMO_ERR_ALLOC;
    if (J.frames && J.flen && J.chosen && J.pcost && J.penc && J.pdec && J.psize) {
        atomic_init(&J.next, 0); atomic_init(&J.err, 0);
        run_pool(kt_worker, &J, threads, k);
        ret = atomic_load(&J.err);
        if (!ret) {
            if (k == 1) {
                if (J.flen[0] > cap) ret = KOLMO_ERR_DSTSIZE;
                else { memcpy(dst, J.frames[0], J.flen[0]); ret = (int64_t)J.flen[0]; }
            } else ret = kt_merge(J.frames, J.flen, k, src, starts, dst, cap, prof, J.chosen);
            if (ret > 0 && prof) ret = (int64_t)kc_pack(dst, (size_t)ret, src, n);
        }
        if (!ret || ret > 0) {
            if (report) {
                memset(report, 0, sizeof *report); report->chunks = k;
                for (uint32_t c = 0; c < k; c++) {
                    report->per_setting[J.chosen[c]]++; report->predicted_cost += J.pcost[c];
                    report->predicted_enc_s += J.penc[c]; report->predicted_dec_s += J.pdec[c]; report->predicted_size += J.psize[c];
                }
            }
        }
    }
    if (J.frames) for (uint32_t c = 0; c < k; c++) free(J.frames[c]);
    free(J.frames); free(J.flen); free(J.chosen); free(J.pcost); free(J.penc); free(J.pdec); free(J.psize); free(starts);
    zs_trim();
    return ret;
}

/* ---------------------------------------------------------------- calibration
 * Built-in data, one corpus per class, from a fixed generator (the same bytes on every machine): prose from a
 * Zipf-distributed vocabulary, a CSV of sensor rows, application log lines, JSON event records, seven float32
 * random walks, and machine-code-like binary (a few hundred instruction patterns with random operands). */
typedef struct { uint64_t s; } kf_rng;
static uint32_t kf_next(kf_rng *r) { r->s ^= r->s << 13; r->s ^= r->s >> 7; r->s ^= r->s << 17; return (uint32_t)(r->s >> 11); }
static double kf_unif(kf_rng *r) { return (kf_next(r) & 0xFFFFFF) / 16777216.0; }
static void kf_word(kf_rng *r, char *w, int idx) {
    static const char *const SY[] = {"ka", "lo", "mi", "ne", "ru", "ta", "si", "po", "de", "ga", "en", "or", "ar", "is", "th", "an", "re", "on", "st", "al"};
    kf_rng q = {0x9E3779B97F4A7C15ull ^ (uint64_t)idx * 0x100000001B3ull}; (void)r;
    int len = 1 + (int)(kf_next(&q) % 3) + (idx > 50) + (idx > 400); w[0] = 0;
    for (int i = 0; i < len; i++) strcat(w, SY[kf_next(&q) % 20]);
}
static int kf_zipf(kf_rng *r, int n) { double u = kf_unif(r); return (int)(n * u * u * u); }
static size_t kf_corpus(int cls, uint8_t *o, size_t cap) {
    kf_rng r = {0x2545F4914F6CDD1Dull + (uint64_t)cls}; size_t w = 0; char line[512], wd[64], wd2[64];
    static const char *const LV[] = {"INFO", "INFO", "INFO", "DEBUG", "WARN", "ERROR"};
    static const char *const EV[] = {"login", "logout", "view", "click", "purchase", "search", "error"};
    double walk[7] = {20, 50, 1013, 3.5, 0.2, 100, 7};
    uint64_t t = 1763251200;
    for (int row = 0; w < cap; row++) {
        int l = 0;
        if (cls == 0) {                                  /* prose */
            int words = 6 + (int)(kf_next(&r) % 14);
            for (int i = 0; i < words && l < 400; i++) { kf_word(&r, wd, kf_zipf(&r, 3000)); if (!i) wd[0] = (char)(wd[0] - 32); l += snprintf(line + l, sizeof line - l, "%s%s", wd, i + 1 < words ? " " : ". "); }
            if (row % 4 == 3) line[l++] = '\n';
        } else if (cls == 1) {                           /* CSV */
            if (!row) l = snprintf(line, sizeof line, "id,time,station,temperature,humidity,pressure,status\n");
            else { t += 1 + kf_next(&r) % 30; walk[0] += (kf_unif(&r) - 0.5) * 0.3; walk[1] += (kf_unif(&r) - 0.5); walk[2] += (kf_unif(&r) - 0.5) * 0.2;
                   kf_word(&r, wd, kf_zipf(&r, 40));
                   l = snprintf(line, sizeof line, "%d,%llu,%s,%.2f,%d,%.1f,%s\n", row, (unsigned long long)t, wd, walk[0], (int)walk[1] & 127, walk[2], kf_next(&r) % 20 ? "ok" : "fault"); }
        } else if (cls == 2) {                           /* log */
            t += kf_next(&r) % 3; kf_word(&r, wd, kf_zipf(&r, 60)); kf_word(&r, wd2, kf_zipf(&r, 800));
            l = snprintf(line, sizeof line, "2026-11-16 %02llu:%02llu:%02llu,%03u %s [pool-%u-thread-%u] org.example.%s.Service: request %s finished in %u ms id=%u\n",
                         (unsigned long long)(t / 3600 % 24), (unsigned long long)(t / 60 % 60), (unsigned long long)(t % 60), kf_next(&r) % 1000,
                         LV[kf_next(&r) % 6], 1 + kf_next(&r) % 4, 1 + kf_next(&r) % 16, wd, wd2, kf_next(&r) % 900, 100000 + row * 7 + kf_next(&r) % 5);
        } else if (cls == 3) {                           /* JSON */
            t += kf_next(&r) % 5; kf_word(&r, wd, kf_zipf(&r, 500)); kf_word(&r, wd2, kf_zipf(&r, 30));
            l = snprintf(line, sizeof line, "{\"id\":%d,\"ts\":%llu,\"user\":\"%s\",\"event\":\"%s\",\"value\":%.3f,\"tags\":[\"%s\",\"beta\"],\"ok\":%s}\n",
                         row, (unsigned long long)t, wd, EV[kf_next(&r) % 7], kf_unif(&r) * 100, wd2, kf_next(&r) % 9 ? "true" : "false");
        } else if (cls == 4) {                           /* float32 columns */
            float v[7]; for (int c = 0; c < 7; c++) { walk[c] += (kf_unif(&r) - 0.5) * (c + 1) * 0.05; v[c] = (float)walk[c]; }
            memcpy(line, v, sizeof v); l = (int)sizeof v;
        } else {                                         /* machine-code-like */
            kf_rng q = {0xD1B54A32D192ED03ull + (uint64_t)kf_zipf(&r, 300)}; int len = 2 + (int)(kf_next(&q) % 7);
            for (int i = 0; i < len; i++) line[l++] = (char)kf_next(&q);
            if (kf_next(&r) % 2) { uint32_t imm = kf_next(&r) % 4096; memcpy(line + l, &imm, 4); l += 4; }
        }
        if (l <= 0) continue;
        size_t m = (size_t)l < cap - w ? (size_t)l : cap - w;
        memcpy(o + w, line, m); w += m;
    }
    return w;
}

static int64_t kf_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                            char *out, size_t cap, size_t CN);
int64_t kolmo_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                        char *out, size_t cap) {
    return kf_calibrate(files, sizes, nfiles, seconds_per_point, out, cap, 512u << 10);   /* 512 KiB per class */
}
static int64_t kf_calibrate(const uint8_t *const *files, const size_t *sizes, size_t nfiles, double seconds_per_point,
                            char *out, size_t cap, size_t CN) {
    if (!out || cap < 64) return KOLMO_ERR_DSTSIZE;
    if (!(seconds_per_point > 0)) seconds_per_point = 0.1;
    uint8_t *corp[KT_NCLS] = {0}; size_t clen[KT_NCLS] = {0}; int from_files[KT_NCLS] = {0};
    int64_t ret = 0;
    for (int c = 0; c < KT_NCLS; c++) if (!(corp[c] = malloc(CN))) ret = KOLMO_ERR_ALLOC;
    for (size_t i = 0; i < nfiles && !ret; i++) {
        if (!files[i] || !sizes[i]) continue;
        int c = kt_class(files[i], sizes[i]);
        size_t m = sizes[i] < CN - clen[c] ? sizes[i] : CN - clen[c];
        if (m) { memcpy(corp[c] + clen[c], files[i], m); clen[c] += m; from_files[c] = 1; }
    }
    for (int c = 0; c < KT_NCLS && !ret; c++) if (!from_files[c]) clen[c] = kf_corpus(c, corp[c], CN);
    size_t w = 0;
    w += (size_t)snprintf(out + w, cap - w, "kolmo-calibration 1\n# one thread; MB/s = 10^6 bytes per second; data: ");
    for (int c = 0; c < KT_NCLS && w < cap; c++) w += (size_t)snprintf(out + w, cap - w, "%s%s %s", c ? ", " : "", KT_CLS[c], from_files[c] ? "files" : "built-in");
    if (w < cap) w += (size_t)snprintf(out + w, cap - w, "\n# setting class encode_MBps decode_MBps\n");
    uint8_t *o = NULL, *b = NULL;
    if (!ret) { o = malloc(kolmo_bound(CN, NULL) + CN); b = malloc(CN); if (!o || !b) ret = KOLMO_ERR_ALLOC; }
    for (int st = 0; st < KT_NSET && !ret; st++)
        for (int c = 0; c < KT_NCLS && !ret; c++) {
            kolmo_params q = {KT_SET[st].mode, KT_SET[st].level, 1, 0, KT_SET[st].transforms};
            size_t oc = kolmo_bound(clen[c], &q) + CN; double be = HUGE_VAL, bd = HUGE_VAL, spent = 0; int64_t z = 0;
            for (int rep = 0; rep < 1000 && (rep < 1 || spent < seconds_per_point); rep++) {
                double t0 = kf_now(); z = kolmo_compress(corp[c], clen[c], o, oc, &q); double t1 = kf_now();
                if (z < 0) { ret = z; break; }
                if (t1 - t0 < be) be = t1 - t0;
                spent += t1 - t0;
            }
            spent = 0;
            for (int rep = 0; rep < 1000 && !ret && (rep < 1 || spent < seconds_per_point); rep++) {
                double t0 = kf_now(); int64_t u = kolmo_decompress(o, (size_t)z, b, CN, 1); double t1 = kf_now();
                if (u != (int64_t)clen[c] || memcmp(b, corp[c], clen[c])) { ret = KOLMO_ERR_CORRUPT; break; }
                if (t1 - t0 < bd) bd = t1 - t0;
                spent += t1 - t0;
            }
            if (!ret && w < cap)
                w += (size_t)snprintf(out + w, cap - w, "%s %s %.1f %.1f\n", KT_SET[st].name, KT_CLS[c],
                                      clen[c] / (be > 1e-9 ? be : 1e-9) / 1e6, clen[c] / (bd > 1e-9 ? bd : 1e-9) / 1e6);
        }
    free(o); free(b);
    for (int c = 0; c < KT_NCLS; c++) free(corp[c]);
    if (ret) return ret;
    if (w >= cap) return KOLMO_ERR_DSTSIZE;
    return (int64_t)w;
}
