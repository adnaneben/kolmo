/* Deduplicating archives: content-defined chunking and a shared chunk store (SPEC.md, "Chunk store").
 *
 * Every file is cut into chunks where a rolling Gear hash meets a mask (FastCDC, Xia et al., USENIX ATC 2016: a smaller
 * chance of a cut before the target size and a larger one after it, "normalized chunking", so sizes cluster around the
 * target; cut points depend only on the bytes, so an insertion moves the cuts near it and nowhere else). In text files a
 * cut moves on to the next line end (at most 4 KiB), so the chunk store holds whole lines and the table and log
 * transforms still see rows. A chunk equal to one seen before (same XXH3-64 and the same bytes) is stored once. The store
 * (the distinct chunks in first-seen order) is compressed as one ordinary frame; the DDP1 section holds the chunk
 * lengths and checks, and per file its name, size, mode, time and list of chunks.
 *
 * Own implementation of the published algorithm; the Gear table is generated (splitmix64), not copied.
 * Licence: Apache-2.0. */
#include "core/features.h"
#include <stdlib.h>
#include <string.h>

#define DD_SEED 0x64647031u            /* chunk checks: low 32 bits of XXH3-64 with this seed */
#define DD_LINE_REACH 4096

static uint64_t dd_gear[256];
static uint64_t dd_split(uint64_t *s) { uint64_t z = (*s += 0x9E3779B97F4A7C15ull); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
static void dd_init(void) {
    static int done = 0; if (done) return;          /* idempotent and the same on every thread: no lock needed */
    uint64_t s = 0x6b6f6c6d6f646470ull; for (int i = 0; i < 256; i++) dd_gear[i] = dd_split(&s);
    done = 1;
}
/* a mask of `bits` one bits spread over the top 48 bits (the high bits of the Gear hash mix the most bytes) */
static uint64_t dd_mask(int bits) {
    uint64_t m = 0; if (bits < 1) bits = 1; if (bits > 47) bits = 47;
    for (int i = 0; i < bits; i++) m |= 1ull << (63 - (i * 47) / bits);
    return m;
}
typedef struct { uint32_t avg, min, max; uint64_t ms, ml; int lines; } dd_cfg;
static void dd_config(dd_cfg *c, uint32_t avg, int lines) {
    int b = 0; while ((1u << b) < avg) b++;
    c->avg = 1u << b; c->min = c->avg / 4; c->max = c->avg * 8; c->ms = dd_mask(b + 2); c->ml = dd_mask(b - 2); c->lines = lines;
}
/* the length of the next chunk of s[0..n) */
static size_t dd_cut(const dd_cfg *c, const uint8_t *s, size_t n, int text) {
    if (n <= c->min) return n;
    size_t end = n < c->max ? n : c->max, norm = n < c->avg ? n : c->avg, i = c->min, cut = end;
    uint64_t h = 0;
    for (; i < norm; i++) { h = (h << 1) + dd_gear[s[i]]; if (!(h & c->ms)) { cut = i + 1; goto found; } }
    for (; i < end; i++) { h = (h << 1) + dd_gear[s[i]]; if (!(h & c->ml)) { cut = i + 1; goto found; } }
found:
    if (text && c->lines && cut < n && s[cut - 1] != '\n') {
        size_t lim = n - cut < DD_LINE_REACH ? n - cut : DD_LINE_REACH;
        const uint8_t *q = memchr(s + cut, '\n', lim);
        if (q) cut = (size_t)(q - s) + 1;
    }
    return cut;
}
static int dd_text(const uint8_t *s, size_t n) {
    size_t h = n < 65536 ? n : 65536, pr = 0; if (!h) return 0;
    for (size_t i = 0; i < h; i++) { uint8_t c = s[i]; pr += (c >= 32 && c < 127) || c == 9 || c == 10 || c == 13 || c >= 128; }
    return pr * 100 >= h * 95 && memchr(s, '\n', h) != NULL;
}

/* open-addressing table of distinct chunks: hash -> chunk id */
typedef struct { const uint8_t *p; uint32_t len; uint64_t h; } dd_chunk;
typedef struct { dd_chunk *c; uint64_t nc, cap; int64_t *slot; uint64_t mask; } dd_set;
static int dd_set_init(dd_set *S, uint64_t want) {
    uint64_t cap = 1024; while (cap < want * 2) cap *= 2;
    S->slot = malloc(sizeof(int64_t) * cap); S->mask = cap - 1; S->nc = 0; S->cap = want > 16 ? want : 16;
    S->c = malloc(sizeof(dd_chunk) * S->cap);
    if (!S->slot || !S->c) return -1;
    for (uint64_t i = 0; i < cap; i++) S->slot[i] = -1;
    return 0;
}
static int64_t dd_set_add(dd_set *S, const uint8_t *p, uint32_t len, int *isnew) {
    uint64_t h = kolmo_xxh3(p, len, 0x63646331);
    for (uint64_t i = h & S->mask;; i = (i + 1) & S->mask) {
        int64_t id = S->slot[i];
        if (id < 0) {
            if (S->nc == S->cap) { dd_chunk *g = realloc(S->c, sizeof(dd_chunk) * S->cap * 2); if (!g) return -1; S->c = g; S->cap *= 2; }
            if ((S->nc + 1) * 2 > S->mask + 1) {                      /* grow the table */
                uint64_t ncap = (S->mask + 1) * 2; int64_t *ns = malloc(sizeof(int64_t) * ncap); if (!ns) return -1;
                for (uint64_t k = 0; k < ncap; k++) ns[k] = -1;
                for (uint64_t k = 0; k < S->nc; k++) { uint64_t j = S->c[k].h & (ncap - 1); while (ns[j] >= 0) j = (j + 1) & (ncap - 1); ns[j] = (int64_t)k; }
                free(S->slot); S->slot = ns; S->mask = ncap - 1;
                for (i = h & S->mask; S->slot[i] >= 0; i = (i + 1) & S->mask) {}
            }
            S->c[S->nc] = (dd_chunk){p, len, h}; S->slot[i] = (int64_t)S->nc; *isnew = 1;
            return (int64_t)S->nc++;
        }
        const dd_chunk *c = &S->c[id];
        if (c->h == h && c->len == len && !memcmp(c->p, p, len)) { *isnew = 0; return id; }   /* the same bytes */
    }
}

size_t kolmo_archive_bound(const kolmo_archive_file *files, size_t nfiles, const kolmo_params *p) {
    size_t tot = 0, meta = 64;
    for (size_t i = 0; i < nfiles; i++) { tot += files[i].size; meta += strlen(files[i].name) + 64 + files[i].size / 64 * 10; }
    return kolmo_bound(tot, p) + meta + tot / 16 + 4096;
}

static uint64_t dd_zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
static int64_t dd_unzz(uint64_t u) { return (int64_t)(u >> 1) ^ -(int64_t)(u & 1); }

int64_t kolmo_archive_create(const kolmo_archive_file *files, size_t nfiles, uint8_t *dst, size_t cap,
                             const kolmo_params *p, const kolmo_archive_params *ap, kolmo_archive_stats *st) {
    kolmo_archive_stats S0; if (!st) st = &S0; memset(st, 0, sizeof *st);
    kolmo_archive_params A = ap ? *ap : (kolmo_archive_params){0, 1};
    if (!A.avg_chunk) A.avg_chunk = 16384;
    if (A.avg_chunk < 256 || A.avg_chunk > (4u << 20) || (A.avg_chunk & (A.avg_chunk - 1))) return KOLMO_ERR_LIMIT;
    dd_init(); dd_cfg C; dd_config(&C, A.avg_chunk, A.line_cuts);
    uint64_t total = 0; for (size_t f = 0; f < nfiles; f++) { if (!files[f].name || (!files[f].data && files[f].size)) return KOLMO_ERR_LIMIT; total += files[f].size; }
    dd_set D; int64_t ret = KOLMO_ERR_ALLOC; uint8_t *store = NULL; kx_buf tab = {0}, recipes = {0};
    if (dd_set_init(&D, total / C.avg + 16)) goto done;
    /* the file table: name, size, mode, mtime, then the chunk list (gaps between successive ids, zigzag) */
    kx_uv(&recipes, nfiles);
    for (size_t f = 0; f < nfiles; f++) {
        const kolmo_archive_file *F = &files[f]; size_t nl = strlen(F->name);
        kx_uv(&recipes, nl); kx_bytes(&recipes, F->name, nl);
        kx_uv(&recipes, F->size); kx_uv(&recipes, F->mode); kx_uv(&recipes, dd_zz(F->mtime));
        int text = dd_text(F->data, F->size);
        kx_buf refs = {0}; uint64_t nref = 0; int64_t prev = -1;
        for (size_t at = 0; at < F->size;) {
            size_t L = dd_cut(&C, F->data + at, F->size - at, text); int isnew;
            int64_t id = dd_set_add(&D, F->data + at, (uint32_t)L, &isnew); if (id < 0) { free(refs.p); goto done; }
            kx_uv(&refs, dd_zz(id - prev - 1)); prev = id; nref++; at += L; st->chunks++;
        }
        kx_uv(&recipes, nref); kx_bytes(&recipes, refs.p, refs.n); free(refs.p);
        st->input_bytes += F->size;
    }
    st->files = nfiles; st->unique_chunks = D.nc;
    /* the store */
    for (uint64_t i = 0; i < D.nc; i++) st->unique_bytes += D.c[i].len;
    store = malloc(st->unique_bytes ? (size_t)st->unique_bytes : 1); if (!store) goto done;
    {   size_t o = 0; for (uint64_t i = 0; i < D.nc; i++) { memcpy(store + o, D.c[i].p, D.c[i].len); o += D.c[i].len; } }
    /* DDP1 table: chunk lengths and checks, then the file table; stored as a frame of its own */
    kx_uv(&tab, D.nc);
    for (uint64_t i = 0; i < D.nc; i++) {
        uint8_t ck[4]; kx_put32(ck, (uint32_t)kolmo_xxh3(D.c[i].p, D.c[i].len, DD_SEED));
        kx_uv(&tab, D.c[i].len); kx_bytes(&tab, ck, 4);
    }
    kx_bytes(&tab, recipes.p, recipes.n);
    if (tab.oom || recipes.oom) goto done;
    {   int64_t fe = kolmo_compress(store, (size_t)st->unique_bytes, dst, cap, p);
        if (fe < 0) { ret = fe; goto done; }
        st->store_frame = (uint64_t)fe;
        kolmo_params tp = {KOLMO_HC, 0, p ? p->threads : 0, 0, 1};
        size_t tcap = 16 + kolmo_bound(tab.n, &tp); uint8_t *body = malloc(tcap); if (!body) goto done;
        body[0] = 'D'; body[1] = 'D'; body[2] = 1; body[3] = (uint8_t)(C.lines ? 1 : 0); kx_put32(body + 4, C.avg); kx_put64(body + 8, tab.n);
        int64_t tz = kolmo_compress(tab.p, tab.n, body + 16, tcap - 16, &tp);
        if (tz < 0) { free(body); ret = tz; goto done; }
        st->table_bytes = 16 + (uint64_t)tz;
        static const char tag[1][4] = {{'D', 'D', 'P', '1'}};
        const uint8_t *bodies[1] = {body}; uint64_t lens[1] = {16 + (uint64_t)tz};
        ret = kx_append(dst, cap, (uint64_t)fe, 1, tag, bodies, lens); free(body); }
done:
    free(D.c); free(D.slot); free(store); free(tab.p); free(recipes.p);
    return ret;
}

/* ------------------------------------------------------------------ reading */
typedef struct {
    kx_frame f; uint64_t nc, nfiles; uint64_t *coff; const uint8_t *checks;   /* nc + 1 store offsets; 4 bytes per chunk */
    uint8_t *tab; size_t tn; size_t files_at;                               /* the file table starts at tab + files_at */
} dd_arc;
static void dd_close(dd_arc *A) { kx_frame_close(&A->f); free(A->coff); free((void *)A->checks); free(A->tab); A->coff = NULL; A->checks = NULL; A->tab = NULL; }
static int dd_open(const uint8_t *s, size_t n, dd_arc *A) {
    memset(A, 0, sizeof *A);
    int r = kx_frame_open(s, n, &A->f); if (r) return r;
    uint64_t L; int err; const uint8_t *b = kx_find(s, n, A->f.end, "DDP1", &L, &err);
    if (!b) { dd_close(A); return err ? err : KOLMO_ERR_CORRUPT; }
    if (L < 16 || b[0] != 'D' || b[1] != 'D' || b[2] != 1 || b[3] > 1) { dd_close(A); return KOLMO_ERR_CORRUPT; }
    L = kx_get64(b + 8);                                             /* the table, decoded from its own frame */
    if (L > (uint64_t)(n - 16) * 64 + (1u << 20) || kolmo_original_size(b + 16, (size_t)(n - (size_t)(b + 16 - s))) != (int64_t)L) { dd_close(A); return KOLMO_ERR_CORRUPT; }
    A->tab = malloc((size_t)L + 1); if (!A->tab) { dd_close(A); return KOLMO_ERR_ALLOC; }
    {   int64_t g = kolmo_decompress(b + 16, (size_t)(n - (size_t)(b + 16 - s)), A->tab, (size_t)L, 0);
        if (g != (int64_t)L) { dd_close(A); return g < 0 ? (int)g : KOLMO_ERR_CORRUPT; } }
    const uint8_t *t = A->tab; size_t i = 0; uint64_t v;
    if (kx_getuv(t, (size_t)L, &i, &A->nc) || A->nc > L / 5) { dd_close(A); return KOLMO_ERR_CORRUPT; }
    A->coff = malloc(8 * ((size_t)A->nc + 1)); uint8_t *ck = malloc(4 * (size_t)A->nc + 1);
    A->checks = ck;
    if (!A->coff || !ck) { dd_close(A); return KOLMO_ERR_ALLOC; }
    uint64_t o = 0;
    for (uint64_t c = 0; c < A->nc; c++) {
        if (kx_getuv(t, (size_t)L, &i, &v) || !v || v > A->f.orig - o || L - i < 4) { dd_close(A); return KOLMO_ERR_CORRUPT; }
        A->coff[c] = o; o += v; memcpy(ck + 4 * c, t + i, 4); i += 4;
    }
    A->coff[A->nc] = o;
    if (o != A->f.orig) { dd_close(A); return KOLMO_ERR_CORRUPT; }
    if (kx_getuv(t, (size_t)L, &i, &A->nfiles) || A->nfiles > L) { dd_close(A); return KOLMO_ERR_CORRUPT; }
    A->tn = (size_t)L; A->files_at = i;
    return 0;
}
/* the entry of file `want` (fields out), walking the table from the start; *refs_at: where its chunk list starts */
typedef struct { const uint8_t *name; size_t nl; uint64_t size, mode, nref; int64_t mtime; size_t refs; } dd_entry;
static int dd_next(const dd_arc *A, size_t *i, dd_entry *e) {
    const uint8_t *t = A->tab; size_t n = A->tn; uint64_t v;
    if (kx_getuv(t, n, i, &v) || v > n - *i || v > 4096) return -1;
    e->name = t + *i; e->nl = (size_t)v; *i += (size_t)v;
    if (memchr(e->name, 0, e->nl)) return -1;
    if (kx_getuv(t, n, i, &e->size) || kx_getuv(t, n, i, &e->mode) || e->mode > 0xFFFFFFFFu || kx_getuv(t, n, i, &v)) return -1;
    e->mtime = dd_unzz(v);
    if (kx_getuv(t, n, i, &e->nref) || e->nref > n - *i) return -1;
    e->refs = *i;
    for (uint64_t r = 0; r < e->nref; r++) if (kx_getuv(t, n, i, &v)) return -1;
    return 0;
}
static int dd_seek(const dd_arc *A, uint64_t want, dd_entry *e) {
    if (want >= A->nfiles) return KOLMO_ERR_CORRUPT;
    size_t i = A->files_at;
    for (uint64_t f = 0; f <= want; f++) if (dd_next(A, &i, e)) return KOLMO_ERR_CORRUPT;
    return 0;
}
/* the chunk ids of an entry into ids[0..nref); checks the ids and that the lengths add up to the size */
static int dd_ids(const dd_arc *A, const dd_entry *e, uint64_t *ids) {
    size_t i = e->refs; int64_t prev = -1; uint64_t v, tot = 0;
    for (uint64_t r = 0; r < e->nref; r++) {
        kx_getuv(A->tab, A->tn, &i, &v);
        int64_t id = prev + 1 + dd_unzz(v);
        if (id < 0 || (uint64_t)id >= A->nc) return KOLMO_ERR_CORRUPT;
        ids[r] = (uint64_t)id; prev = id; tot += A->coff[id + 1] - A->coff[id];
        if (tot > e->size) return KOLMO_ERR_CORRUPT;
    }
    return tot == e->size ? 0 : KOLMO_ERR_CORRUPT;
}
static int dd_check(const dd_arc *A, uint64_t id, const uint8_t *p) {
    return (uint32_t)kolmo_xxh3(p, (size_t)(A->coff[id + 1] - A->coff[id]), DD_SEED) == kx_get32(A->checks + 4 * id) ? 0 : KOLMO_ERR_CHECKSUM;
}

int64_t kolmo_archive_count(const uint8_t *src, size_t n) {
    dd_arc A; int r = dd_open(src, n, &A); if (r) return r;
    int64_t c = (int64_t)A.nfiles; dd_close(&A); return c;
}
int kolmo_archive_stat(const uint8_t *src, size_t n, uint64_t i, char *name, size_t namecap, uint64_t *size,
                       uint32_t *mode, int64_t *mtime) {
    dd_arc A; int r = dd_open(src, n, &A); if (r) return r;
    dd_entry e; r = dd_seek(&A, i, &e);
    if (!r) {
        if (name && namecap) { size_t l = e.nl < namecap - 1 ? e.nl : namecap - 1; memcpy(name, e.name, l); name[l] = 0; }
        if (size) *size = e.size;
        if (mode) *mode = (uint32_t)e.mode;
        if (mtime) *mtime = e.mtime;
    }
    dd_close(&A); return r;
}

int64_t kolmo_archive_extract(const uint8_t *src, size_t n, uint64_t fi, uint8_t *dst, size_t cap, int threads) {
    dd_arc A; int r = dd_open(src, n, &A); if (r) return r;
    dd_entry e; uint64_t *ids = NULL; uint8_t *buf = NULL; int64_t ret;
    if ((r = dd_seek(&A, fi, &e))) { ret = r; goto done; }
    if (e.size > cap) { ret = KOLMO_ERR_DSTSIZE; goto done; }
    ids = malloc(8 * ((size_t)e.nref + 1)); if (!ids) { ret = KOLMO_ERR_ALLOC; goto done; }
    if ((r = dd_ids(&A, &e, ids))) { ret = r; goto done; }
    /* the store blocks this file needs, in runs of consecutive blocks; each run is decoded once */
    {   uint32_t nb = A.f.nb; uint8_t *need = calloc((size_t)nb + 1, 1); if (!need) { ret = KOLMO_ERR_ALLOC; goto done; }
        uint32_t b = 0;
        for (uint64_t k = 0; k < e.nref; k++) {
            uint64_t lo = A.coff[ids[k]], hi = A.coff[ids[k] + 1];
            /* first block with roff[b + 1] > lo (binary search) */
            uint32_t a = 0, z = nb; while (a < z) { uint32_t m = a + (z - a) / 2; if (A.f.roff[m + 1] <= lo) a = m + 1; else z = m; }
            for (b = a; b < nb && A.f.roff[b] < hi; b++) need[b] = 1;
        }
        uint64_t *at = malloc(8 * ((size_t)nb + 1)); size_t bytes = 0;
        if (!at) { free(need); ret = KOLMO_ERR_ALLOC; goto done; }
        for (b = 0; b < nb; b++) { at[b] = bytes; if (need[b]) bytes += (size_t)(A.f.roff[b + 1] - A.f.roff[b]); }
        buf = malloc(bytes + 1); if (!buf) { free(need); free(at); ret = KOLMO_ERR_ALLOC; goto done; }
        for (b = 0; b < nb;) {
            if (!need[b]) { b++; continue; }
            uint32_t c = b; while (c < nb && need[c]) c++;
            uint64_t lo = A.f.roff[b], len = A.f.roff[c] - lo;
            if (len) { int64_t q = kolmo_read_range(src, n, lo, (size_t)len, buf + at[b], threads); if (q < 0) { free(need); free(at); ret = q; goto done; } }
            b = c;
        }
        size_t o = 0;
        for (uint64_t k = 0; k < e.nref; k++) {
            uint64_t lo = A.coff[ids[k]], len = A.coff[ids[k] + 1] - lo;
            uint32_t a = 0, z = nb; while (a < z) { uint32_t m = a + (z - a) / 2; if (A.f.roff[m + 1] <= lo) a = m + 1; else z = m; }
            memcpy(dst + o, buf + at[a] + (lo - A.f.roff[a]), (size_t)len);   /* needed blocks from a on are adjacent in buf */
            if ((r = dd_check(&A, ids[k], dst + o))) { free(need); free(at); ret = r; goto done; }
            o += (size_t)len;
        }
        free(need); free(at);
    }
    ret = (int64_t)e.size;
done:
    free(ids); free(buf); dd_close(&A);
    return ret;
}

int64_t kolmo_archive_extract_all(const uint8_t *src, size_t n, kolmo_archive_fn fn, void *ctx, int threads) {
    dd_arc A; int r = dd_open(src, n, &A); if (r) return r;
    uint8_t *store = malloc(A.f.orig ? (size_t)A.f.orig : 1), *file = NULL; uint64_t *ids = NULL, idcap = 0; int64_t ret;
    if (!store) { ret = KOLMO_ERR_ALLOC; goto done; }
    if (A.f.orig && (ret = kolmo_decompress(src, n, store, (size_t)A.f.orig, threads)) < 0) goto done;
    /* the check of every chunk, once */
    for (uint64_t c = 0; c < A.nc; c++) if ((r = dd_check(&A, c, store + A.coff[c]))) { ret = r; goto done; }
    size_t i = A.files_at;
    for (uint64_t f = 0; f < A.nfiles; f++) {
        dd_entry e; if (dd_next(&A, &i, &e)) { ret = KOLMO_ERR_CORRUPT; goto done; }
        if (e.nref + 1 > idcap) { free(ids); idcap = e.nref + 1; ids = malloc(8 * (size_t)idcap); if (!ids) { ret = KOLMO_ERR_ALLOC; goto done; } }
        if ((r = dd_ids(&A, &e, ids))) { ret = r; goto done; }
        const uint8_t *data; uint8_t *own = NULL;
        if (e.nref == 1) data = store + A.coff[ids[0]];
        else if (!e.nref) data = store;
        else {                                                         /* consecutive chunks are one run of the store */
            int run = 1; for (uint64_t k = 1; k < e.nref && run; k++) run = ids[k] == ids[k - 1] + 1;
            if (run) data = store + A.coff[ids[0]];
            else {
                own = realloc(file, (size_t)e.size + 1); if (!own) { ret = KOLMO_ERR_ALLOC; goto done; }
                file = own; size_t o = 0;
                for (uint64_t k = 0; k < e.nref; k++) { uint64_t L = A.coff[ids[k] + 1] - A.coff[ids[k]]; memcpy(file + o, store + A.coff[ids[k]], (size_t)L); o += (size_t)L; }
                data = file;
            }
        }
        char name[4097]; memcpy(name, e.name, e.nl); name[e.nl] = 0;
        if (fn && fn(ctx, name, data, e.size, (uint32_t)e.mode, e.mtime)) { ret = KOLMO_ERR_IO; goto done; }
    }
    ret = (int64_t)A.nfiles;
done:
    free(store); free(file); free(ids); dd_close(&A);
    return ret;
}
