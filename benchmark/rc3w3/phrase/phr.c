/* rc3w3 phrase study: multi-item symbols (words, word n-grams, repeated lines) in front of Kolmo's coders.
 *
 *   phr [options] FILE...
 *
 * For each file: a deterministic phrase dictionary is built on a sample (Re-Pair style pair merging over word
 * tokens), the whole input is parsed greedily (longest phrase at each token start, a token trie), every phrase is
 * replaced by a one- or two-byte code (rare byte values; literal occurrences escaped), the dictionary travels in
 * front of the stream, and the result is coded by Kolmo's own context-mixing coder (codec 7, model 3, the -x text
 * model) so sizes compare directly with -x. Every transform is inverted and compared with the input.
 *
 * Built against the library sources (it includes src/kolmo.c, as the tests do): see Makefile.study.
 * Output: one JSON line per file and variant on stdout. */
#include "../../../src/kolmo.c"
#include <time.h>
#include "cmx.c"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* ---------------------------------------------------------------- options */
static size_t o_sample = 8u << 20;   /* bytes of the sample the dictionary is built on */
static int o_minc = 4;               /* a pair is merged when it occurs at least this often in the sample */
static int o_maxl = 64;              /* longest phrase in bytes */
static int o_rounds = 64;            /* pair-merging rounds */
static int o_per = 512;              /* pairs merged per round at most */
static int o_minuse = 3; static double o_upmb = 0;             /* a phrase is kept when the whole input uses it at least this often */
static int o_minlen = 2;             /* ... and it is at least this long */
static int o_maxtok = 0;             /* phrases of at most this many tokens (0: no limit) */
static int o_singles = -1;           /* one-byte codes (-1: half of the code bytes) */
static int o_order = 0;              /* code order: 0 by use, 1 lexicographic */
static int o_cm = 1;                 /* run CM (else sizes of the transform only) */
static int o_words_only = 0;         /* no pair merging: words only (a word transform) */
static int o_wordleaf = 1;           /* words are tokens (else every byte is a token) */
static int o_capfold = 0;            /* reserved */
static int o_maxcodes = 0;           /* cap on the dictionary size (0: code capacity) */

/* ---------------------------------------------------------------- tokens */
static inline int wch(uint8_t c) { return ((c | 32) >= 'a' && (c | 32) <= 'z') || c >= 128; }
#define TK_MAXW 32
/* token end from position i */
static inline size_t tok_end(const uint8_t *s, size_t n, size_t i) {
    if (!o_wordleaf || !wch(s[i])) return i + 1;
    size_t j = i + 1; while (j < n && j - i < TK_MAXW && wch(s[j])) j++;
    return j;
}

/* string interning keyed by bytes (strings live in caller memory) */
typedef struct { uint32_t *slot; uint32_t mask, n, cap; const uint8_t **p; uint32_t *len; uint64_t *h; } strtab;
static uint64_t sh64(const uint8_t *s, size_t n) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ n;
    while (n >= 8) { uint64_t v; memcpy(&v, s, 8); h = (h ^ v) * 0xff51afd7ed558ccdull; h ^= h >> 32; s += 8; n -= 8; }
    uint64_t v = 0; memcpy(&v, s, n); h = (h ^ v) * 0xc4ceb9fe1a85ec53ull; h ^= h >> 29; return h;
}
static void st_init(strtab *t) { memset(t, 0, sizeof *t); t->mask = (1u << 16) - 1; t->slot = malloc(4u << 16); memset(t->slot, 0xff, 4u << 16); }
static void st_free(strtab *t) { free(t->slot); free(t->p); free(t->len); free(t->h); }
static uint32_t st_get(strtab *t, const uint8_t *s, uint32_t n, int ins) {
    uint64_t h = sh64(s, n); uint32_t i = (uint32_t)h & t->mask;
    for (;;) {
        uint32_t id = t->slot[i]; if (id == UINT32_MAX) break;
        if (t->h[id] == h && t->len[id] == n && !memcmp(t->p[id], s, n)) return id;
        i = (i + 1) & t->mask;
    }
    if (!ins) return UINT32_MAX;
    if (t->n == t->cap) { t->cap = t->cap * 2 + 1024; t->p = realloc(t->p, t->cap * sizeof *t->p); t->len = realloc(t->len, t->cap * 4); t->h = realloc(t->h, t->cap * 8); }
    uint32_t id = t->n++; t->p[id] = s; t->len[id] = n; t->h[id] = h; t->slot[i] = id;
    if (t->n * 2 > t->mask) {
        uint32_t nm = t->mask * 2 + 1; free(t->slot); t->slot = malloc(4 * ((size_t)nm + 1)); memset(t->slot, 0xff, 4 * ((size_t)nm + 1)); t->mask = nm;
        for (uint32_t k = 0; k < t->n; k++) { uint32_t q = (uint32_t)t->h[k] & nm; while (t->slot[q] != UINT32_MAX) q = (q + 1) & nm; t->slot[q] = k; }
    }
    return id;
}

/* symbols of the sample: 0..255 bytes, then words, then merged pairs */
typedef struct { uint32_t l, r, len, ntok; const uint8_t *s; } sym_t;

static int ph_cmp_u64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }
typedef struct { uint64_t key; uint32_t cnt; } paircnt;
static int cmp_pc(const void *a, const void *b) {
    const paircnt *x = a, *y = b; if (x->cnt != y->cnt) return x->cnt > y->cnt ? -1 : 1; return x->key < y->key ? -1 : x->key > y->key;
}
/* radix sort of 64-bit keys (LSD, 16 bits per pass) */
static void rsort64(uint64_t *a, size_t n) {
    uint64_t *t = malloc(n * 8); size_t *c = malloc(65536 * sizeof(size_t));
    for (int sh = 0; sh < 64; sh += 16) {
        memset(c, 0, 65536 * sizeof(size_t));
        for (size_t i = 0; i < n; i++) c[(a[i] >> sh) & 65535]++;
        size_t s = 0; for (int k = 0; k < 65536; k++) { size_t v = c[k]; c[k] = s; s += v; }
        for (size_t i = 0; i < n; i++) t[c[(a[i] >> sh) & 65535]++] = a[i];
        memcpy(a, t, n * 8);
    }
    free(t); free(c);
}

/* ---------------------------------------------------------------- dictionary: a set of strings */
typedef struct { const uint8_t **s; uint32_t *len, *ntok; size_t n, cap; } dict_t;
static void dict_add(dict_t *d, const uint8_t *s, uint32_t len, uint32_t ntok) {
    if (d->n == d->cap) { d->cap = d->cap * 2 + 1024; d->s = realloc(d->s, d->cap * sizeof *d->s); d->len = realloc(d->len, d->cap * 4); d->ntok = realloc(d->ntok, d->cap * 4); }
    d->s[d->n] = s; d->len[d->n] = len; d->ntok[d->n] = ntok; d->n++;
}

/* sample: chunks of 1 MiB spread evenly, cut at token boundaries */
static void build_candidates(const uint8_t *in, size_t n, dict_t *D) {
    size_t S = n < o_sample ? n : o_sample, nch = (S + (1u << 20) - 1) >> 20; if (!nch) nch = 1;
    size_t csz = S / nch;
    uint32_t *seq = malloc((S + 16) * 4); const uint8_t **sp = malloc((S + 16) * sizeof *sp); size_t m = 0;
    strtab words; st_init(&words);
    sym_t *sy = NULL; size_t ns = 256, scap = 0;
    scap = 1 << 16; sy = malloc(scap * sizeof *sy);
    static uint8_t bytes[256]; for (int b = 0; b < 256; b++) { bytes[b] = (uint8_t)b; sy[b] = (sym_t){UINT32_MAX, UINT32_MAX, 1, 1, &bytes[b]}; }
    uint32_t SEP = UINT32_MAX - 1;
    for (size_t c = 0; c < nch; c++) {
        size_t a = nch == 1 ? 0 : (n - csz) / (nch - 1) * c, e = a + csz;
        if (e > n) e = n;
        while (a > 0 && a < e && wch(in[a]) && wch(in[a - 1])) a++;
        for (size_t i = a; i < e;) {
            size_t j = tok_end(in, n, i); uint32_t id;
            if (j - i == 1) id = in[i];
            else {
                uint32_t w = st_get(&words, in + i, (uint32_t)(j - i), 1);
                id = 256 + w;
                if (id >= ns) {
                    if (ns == scap) { scap *= 2; sy = realloc(sy, scap * sizeof *sy); }
                    sy[ns++] = (sym_t){UINT32_MAX, UINT32_MAX, (uint32_t)(j - i), 1, in + i};
                }
            }
            sp[m] = in + i; seq[m++] = id; i = j;
        }
        sp[m] = NULL; seq[m++] = SEP;                    /* chunks never merge across their boundary */
    }
    size_t nwords = ns;
    if (!o_words_only) {
        uint64_t *keys = malloc(m * 8);
        for (int r = 0; r < o_rounds; r++) {
            size_t nk = 0;
            for (size_t i = 0; i + 1 < m; i++) {
                uint32_t a = seq[i], b = seq[i + 1];
                if (a == SEP || b == SEP) continue;
                if (sy[a].len + sy[b].len > (uint32_t)o_maxl) continue;
                if (o_maxtok && sy[a].ntok + sy[b].ntok > (uint32_t)o_maxtok) continue;
                if (a == b && i > 0 && seq[i - 1] == a) { /* runs: count every other pair */
                    size_t k = i; while (k > 0 && seq[k - 1] == a) k--; if ((i - k) & 1) continue;
                }
                keys[nk++] = (uint64_t)a << 32 | b;
            }
            rsort64(keys, nk);
            paircnt *pc = NULL; size_t npc = 0, pcap = 0;
            for (size_t i = 0; i < nk;) {
                size_t j = i; while (j < nk && keys[j] == keys[i]) j++;
                if (j - i >= (size_t)o_minc) { if (npc == pcap) { pcap = pcap * 2 + 1024; pc = realloc(pc, pcap * sizeof *pc); } pc[npc++] = (paircnt){keys[i], (uint32_t)(j - i)}; }
                i = j;
            }
            if (!npc) { free(pc); break; }
            qsort(pc, npc, sizeof *pc, cmp_pc);
            size_t take = npc < (size_t)o_per ? npc : (size_t)o_per;
            /* only pairs at least a quarter as frequent as the best of the round (keeps the order close to Re-Pair) */
            while (take > 1 && pc[take - 1].cnt * 4 < pc[0].cnt) take--;
            /* map pair -> new symbol: a small open-addressed table */
            size_t hm = 1; while (hm < take * 4) hm <<= 1;
            uint64_t *hk = malloc(hm * 8); uint32_t *hv = malloc(hm * 4); memset(hk, 0xff, hm * 8);
            for (size_t k = 0; k < take; k++) {
                uint32_t a = (uint32_t)(pc[k].key >> 32), b = (uint32_t)pc[k].key;
                if (ns == scap) { scap *= 2; sy = realloc(sy, scap * sizeof *sy); }
                sy[ns] = (sym_t){a, b, sy[a].len + sy[b].len, sy[a].ntok + sy[b].ntok, NULL};
                size_t q = g6_mix64(pc[k].key) & (hm - 1); while (hk[q] != UINT64_MAX) q = (q + 1) & (hm - 1);
                hk[q] = pc[k].key; hv[q] = (uint32_t)ns++;
            }
            size_t w = 0;
            for (size_t i = 0; i < m;) {
                if (i + 1 < m && seq[i] != SEP && seq[i + 1] != SEP) {
                    uint64_t key = (uint64_t)seq[i] << 32 | seq[i + 1];
                    size_t q = g6_mix64(key) & (hm - 1); uint32_t hit = UINT32_MAX;
                    while (hk[q] != UINT64_MAX) { if (hk[q] == key) { hit = hv[q]; break; } q = (q + 1) & (hm - 1); }
                    if (hit != UINT32_MAX) {
                        if (!sy[hit].s) sy[hit].s = sp[i];
                        sp[w] = sp[i]; seq[w++] = hit; i += 2; continue;
                    }
                }
                sp[w] = sp[i]; seq[w++] = seq[i]; i++;
            }
            m = w;
            free(hk); free(hv); free(pc);
        }
        free(keys);
    }
    /* candidates: words (length >= 2) and every merged symbol that occurred */
    for (size_t k = 256; k < ns; k++) if (sy[k].s && sy[k].len >= (uint32_t)o_minlen) dict_add(D, sy[k].s, sy[k].len, sy[k].ntok);
    (void)nwords;
    free(seq); free(sp); free(sy); st_free(&words);
}

/* ---------------------------------------------------------------- trie over tokens of the dictionary strings */
typedef struct {
    strtab words;                 /* token strings -> token id (bytes are ids 0..255, words 256+) */
    uint64_t *ek; uint32_t *ev; size_t emask, en;   /* edges: (node << 32 | token) -> child */
    int32_t *term; size_t nn, ncap;                 /* node -> phrase index or -1 */
} trie_t;
static uint32_t tok_id(trie_t *T, const uint8_t *s, size_t len, int ins) {
    if (len == 1) return s[0];
    uint32_t w = st_get(&T->words, s, (uint32_t)len, ins); return w == UINT32_MAX ? UINT32_MAX : 256 + w;
}
static uint32_t *edge_slot(trie_t *T, uint64_t key, int ins) {
    size_t q = g6_mix64(key) & T->emask;
    while (T->ek[q] != UINT64_MAX) { if (T->ek[q] == key) return &T->ev[q]; q = (q + 1) & T->emask; }
    if (!ins) return NULL;
    T->ek[q] = key; T->en++; return &T->ev[q];
}
static void trie_build(trie_t *T, const dict_t *D, const uint8_t *keep) {
    memset(T, 0, sizeof *T); st_init(&T->words);
    size_t tot = 16; for (size_t k = 0; k < D->n; k++) if (!keep || keep[k]) tot += D->ntok[k];
    T->emask = 1; while (T->emask < tot * 2) T->emask <<= 1; T->ek = malloc(T->emask * 8); T->ev = malloc(T->emask * 4);
    memset(T->ek, 0xff, T->emask * 8); T->emask--;
    T->ncap = tot + 1; T->term = malloc(T->ncap * 4); T->nn = 1; T->term[0] = -1;
    for (size_t k = 0; k < D->n; k++) {
        if (keep && !keep[k]) continue;
        const uint8_t *s = D->s[k]; size_t len = D->len[k]; uint32_t node = 0;
        for (size_t i = 0; i < len;) {
            size_t j = tok_end(s, len, i);
            uint32_t t = tok_id(T, s + i, j - i, 1);
            uint64_t key = (uint64_t)node << 32 | t;
            size_t q = g6_mix64(key) & T->emask; uint32_t child = UINT32_MAX;
            while (T->ek[q] != UINT64_MAX) { if (T->ek[q] == key) { child = T->ev[q]; break; } q = (q + 1) & T->emask; }
            if (child == UINT32_MAX) { child = (uint32_t)T->nn++; T->term[child] = -1; T->ek[q] = key; T->ev[q] = child; T->en++; }
            node = child; i = j;
        }
        if (T->term[node] < 0) T->term[node] = (int32_t)k;
    }
}
static void trie_free(trie_t *T) { st_free(&T->words); free(T->ek); free(T->ev); free(T->term); }

/* greedy parse: callback per item. Returns the phrase index or -1 (one literal token) and its length */
static inline int32_t parse_at(trie_t *T, const uint8_t *in, size_t n, size_t i, size_t *plen) {
    uint32_t node = 0; int32_t best = -1; size_t bl = 0, k = i; int steps = 0;
    while (k < n) {
        size_t j = tok_end(in, n, k);
        uint32_t t = tok_id(T, in + k, j - k, 0); if (t == UINT32_MAX) break;
        uint64_t key = (uint64_t)node << 32 | t;
        size_t q = g6_mix64(key) & T->emask; uint32_t child = UINT32_MAX;
        while (T->ek[q] != UINT64_MAX) { if (T->ek[q] == key) { child = T->ev[q]; break; } q = (q + 1) & T->emask; }
        if (child == UINT32_MAX) break;
        node = child; k = j;
        if (T->term[node] >= 0) { best = T->term[node]; bl = k - i; }
        if (++steps > 4096) break;
    }
    *plen = bl; return best;
}

/* ---------------------------------------------------------------- the transform */
typedef struct { uint8_t *p; size_t n, cap; } obuf;
static void ob_put(obuf *b, const void *s, size_t n) { if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 4096; b->p = realloc(b->p, b->cap); } memcpy(b->p + b->n, s, n); b->n += n; }
static void ob_byte(obuf *b, uint8_t c) { ob_put(b, &c, 1); }

typedef struct { size_t dict_bytes, stream_bytes, nphr, nsingle, nprefix, coded, literal_bytes, escapes; double t_build, t_enc, t_dec; } pstat;

static int cmp_lex_idx_D; static const dict_t *g_D;
static int cmp_lex(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    size_t lx = g_D->len[x], ly = g_D->len[y]; int c = memcmp(g_D->s[x], g_D->s[y], lx < ly ? lx : ly);
    if (c) return c; return lx < ly ? -1 : lx > ly;
}
static const uint64_t *g_use;
static int cmp_use(const void *a, const void *b) {
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    if (g_use[x] != g_use[y]) return g_use[x] > g_use[y] ? -1 : 1; return cmp_lex(a, b);
}

/* encoded: hdr | dict | stream.  hdr: ncodebytes u8, escape u8, nsingle u8, codebytes..., nphr u32 */
static uint8_t *phrase_encode(const uint8_t *in, size_t n, size_t *outn, pstat *st) {
    double t0 = now();
    int saved_minuse = o_minuse; if (o_upmb > 0 && o_upmb * (double)n / 1048576.0 > o_minuse) o_minuse = (int)(o_upmb * (double)n / 1048576.0);
    dict_t D = {0}; build_candidates(in, n, &D);
    /* byte use, code bytes: unused first, then the rarest (escaped) */
    uint64_t bc[256] = {0}; for (size_t i = 0; i < n; i++) bc[in[i]]++;
    int ord[256]; for (int b = 0; b < 256; b++) ord[b] = b;
    for (int a = 0; a < 256; a++) for (int b = a + 1; b < 256; b++) if (bc[ord[b]] < bc[ord[a]] || (bc[ord[b]] == bc[ord[a]] && ord[b] < ord[a])) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
    int nun = 0; while (nun < 256 && !bc[ord[nun]]) nun++;
    /* use unused bytes (at most 64), else the rarest 16 with an escape byte */
    int ncb, esc = -1;
    uint8_t cb[256];
    if (nun >= 2) { ncb = nun < 64 ? nun : 64; for (int k = 0; k < ncb; k++) cb[k] = (uint8_t)ord[k]; }
    else { esc = ord[0]; ncb = 16; for (int k = 0; k < ncb; k++) cb[k] = (uint8_t)ord[k + 1]; }
    int nsingle = o_singles >= 0 ? o_singles : ncb / 2; if (nsingle > ncb - 1) nsingle = ncb - 1;
    int npre = ncb - nsingle;
    size_t capcodes = (size_t)nsingle + (size_t)npre * 256;
    if (o_maxcodes && capcodes > (size_t)o_maxcodes) capcodes = o_maxcodes;
    /* parse twice: count uses with every candidate, keep the useful ones, count again */
    uint8_t *keep = malloc(D.n + 1); memset(keep, 1, D.n + 1);
    uint64_t *use = calloc(D.n + 1, 8);
    for (int pass = 0; pass < 3; pass++) {
        trie_t T; trie_build(&T, &D, keep);
        memset(use, 0, (D.n + 1) * 8);
        for (size_t i = 0; i < n;) {
            size_t pl; int32_t ph = parse_at(&T, in, n, i, &pl);
            if (ph >= 0) { use[ph]++; i += pl; } else i = tok_end(in, n, i);
        }
        trie_free(&T);
        size_t kept = 0;
        for (size_t k = 0; k < D.n; k++) { keep[k] = keep[k] && use[k] >= (uint64_t)o_minuse; kept += keep[k]; }
        if (kept > capcodes) {     /* keep the capacity's worth with the largest byte saving */
            uint64_t *sv = malloc(kept * 8); size_t q = 0;
            for (size_t k = 0; k < D.n; k++) if (keep[k]) sv[q++] = (use[k] * (D.len[k] - 1)) << 24 | (k & 0xffffff);
            qsort(sv, q, 8, ph_cmp_u64);
            for (size_t r = 0; r + capcodes < q; r++) keep[sv[r] & 0xffffff] = 0;
            free(sv);
        }
    }
    /* final parse, code assignment */
    trie_t T; trie_build(&T, &D, keep);
    memset(use, 0, (D.n + 1) * 8);
    for (size_t i = 0; i < n;) { size_t pl; int32_t ph = parse_at(&T, in, n, i, &pl); if (ph >= 0) { use[ph]++; i += pl; } else i = tok_end(in, n, i); }
    uint32_t *idx = malloc((D.n + 1) * 4); size_t np = 0;
    for (size_t k = 0; k < D.n; k++) if (keep[k] && use[k]) idx[np++] = (uint32_t)k;
    g_D = &D; g_use = use;
    qsort(idx, np, 4, cmp_use);                              /* by use: the first nsingle get one-byte codes */
    if (o_order == 1 && np > (size_t)nsingle) qsort(idx + nsingle, np - nsingle, 4, cmp_lex);
    int32_t *code = malloc((D.n + 1) * 4); for (size_t k = 0; k < D.n; k++) code[k] = -1;
    for (size_t r = 0; r < np; r++) code[idx[r]] = (int32_t)r;
    st->t_build = now() - t0; t0 = now();
    obuf o = {0};
    uint8_t h[4] = {(uint8_t)ncb, (uint8_t)(esc < 0 ? 0 : 1), (uint8_t)nsingle, (uint8_t)(esc < 0 ? 0 : esc)};
    ob_put(&o, h, 4); ob_put(&o, cb, ncb);
    uint32_t np32 = (uint32_t)np; ob_put(&o, &np32, 4);
    for (size_t r = 0; r < np; r++) { uint8_t l = (uint8_t)D.len[idx[r]]; ob_byte(&o, l); ob_put(&o, D.s[idx[r]], l); }
    st->dict_bytes = o.n;
    uint8_t special[256] = {0}; for (int k = 0; k < ncb; k++) special[cb[k]] = 1; if (esc >= 0) special[esc] = 1;
    size_t coded = 0, lit = 0, escs = 0;
    for (size_t i = 0; i < n;) {
        size_t pl; int32_t ph = parse_at(&T, in, n, i, &pl);
        if (ph >= 0 && code[ph] >= 0) {
            int32_t c = code[ph];
            if (c < nsingle) ob_byte(&o, cb[c]);
            else { c -= nsingle; ob_byte(&o, cb[nsingle + (c >> 8)]); ob_byte(&o, (uint8_t)(c & 255)); }
            coded++; i += pl;
        } else {
            size_t j = tok_end(in, n, i);
            for (size_t k = i; k < j; k++) { if (special[in[k]]) { ob_byte(&o, (uint8_t)esc); escs++; } ob_byte(&o, in[k]); }
            lit += j - i; i = j;
        }
    }
    st->t_enc = now() - t0;
    st->stream_bytes = o.n - st->dict_bytes; st->nphr = np; st->nsingle = nsingle; st->nprefix = npre; st->coded = coded; st->literal_bytes = lit; st->escapes = escs;
    trie_free(&T); free(idx); free(code); free(use); free(keep); free(D.s); free(D.len); free(D.ntok);
    o_minuse = saved_minuse;
    *outn = o.n; return o.p;
}

static int phrase_decode(const uint8_t *in, size_t tn, uint8_t *out, size_t n) {
    if (tn < 8) return -1;
    int ncb = in[0], hasesc = in[1], nsingle = in[2], esc = in[3];
    if (ncb < 1 || tn < 8 + (size_t)ncb) return -1;
    const uint8_t *cb = in + 4, *p = in + 4 + ncb, *e = in + tn;
    uint32_t np; memcpy(&np, p, 4); p += 4;
    const uint8_t **ps = malloc((np + 1) * sizeof *ps); uint8_t *pl = malloc(np + 1);
    for (uint32_t r = 0; r < np; r++) { if (p >= e) return -1; pl[r] = *p++; ps[r] = p; p += pl[r]; if (p > e) return -1; }
    int16_t kind[256]; for (int b = 0; b < 256; b++) kind[b] = -1;
    for (int k = 0; k < ncb; k++) kind[cb[k]] = (int16_t)k;
    if (hasesc) kind[esc] = 1000;
    uint8_t *o = out, *oe = out + n;
    while (p < e) {
        uint8_t c = *p++; int k = kind[c];
        if (k < 0) { if (o >= oe) return -1; *o++ = c; continue; }
        if (k == 1000) { if (p >= e || o >= oe) return -1; *o++ = *p++; continue; }
        uint32_t r;
        if (k < nsingle) r = (uint32_t)k;
        else { if (p >= e) return -1; r = (uint32_t)nsingle + ((uint32_t)(k - nsingle) << 8 | *p++); }
        if (r >= np || (size_t)(oe - o) < pl[r]) return -1;
        memcpy(o, ps[r], pl[r]); o += pl[r];
    }
    free(ps); free(pl);
    return o == oe ? 0 : -1;
}


/* ---------------------------------------------------------------- phrase contexts (cmx.c): the dictionary as token-hash sets */
static void px_insert(uint32_t h, int whole) {
    uint64_t k = (uint64_t)h | 1ull << 63; size_t q = (size_t)((h * 0x9E3779B97F4A7C15ull) >> 20) & px_mask;
    while (px_set[q]) { if ((px_set[q] & ~(1ull << 62)) == k) { if (whole) px_set[q] |= 1ull << 62; return; } q = (q + 1) & px_mask; }
    px_set[q] = k | (whole ? 1ull << 62 : 0);
}
/* dictionary strings from a transformed stream's header */
static size_t px_build_from(const uint8_t *t, size_t tn) {
    int ncb = t[0]; const uint8_t *p = t + 4 + ncb; uint32_t np; memcpy(&np, p, 4); p += 4;
    size_t cap = 1; while (cap < (size_t)np * 8 * 4 + 1024) cap <<= 1;
    free(px_set); px_set = calloc(cap, 8); px_mask = cap - 1;
    for (uint32_t r = 0; r < np; r++) {
        size_t l = *p++; const uint8_t *s = p; p += l;
        uint32_t toks[256]; int nt = 0;
        for (size_t i = 0; i < l && nt < 256;) {
            if (px_wch(s[i])) { uint32_t h = 0; while (i < l && px_wch(s[i])) { h = (h + s[i] + 1) * 0x3D4D51CBu; i++; } toks[nt++] = h; }
            else toks[nt++] = (uint32_t)s[i++] + 1;
        }
        if (nt > 8) nt = 8;          /* the history holds 8 tokens */
        for (int k = 1; k <= nt; k++) { uint32_t h = 0; for (int j = k - 1; j >= 0; j--) h = px_th(h, toks[j]); px_insert(h, k == nt); }
    }
    (void)tn; return (size_t)(p - t);
}
static size_t cmx3(const uint8_t *s, size_t n, double *t) {
    uint8_t *c = malloc(n + n / 8 + 1024); double t0 = now();
    size_t r = cmx_compress_m(s, n, c, n + n / 8 + 1024, 3);
    *t = now() - t0; free(c); return r;
}

/* ---------------------------------------------------------------- coders */
static size_t cm3(const uint8_t *s, size_t n, double *t) {
    uint8_t *c = malloc(n + n / 8 + 1024); double t0 = now();
    size_t r = cm_compress_m(s, n, c, n + n / 8 + 1024, looks_text(s, n) ? 3 : 2);
    *t = now() - t0;
    free(c); return r;
}
static size_t cm3_rt(const uint8_t *s, size_t n, double *te, double *td) {
    size_t cap = n + n / 8 + 1024; uint8_t *c = malloc(cap), *d = malloc(n + 64); double t0 = now();
    size_t r = cm_compress_m(s, n, c, cap, looks_text(s, n) ? 3 : 2);
    *te = now() - t0; t0 = now();
    int bad = cm_decompress(c, r, d, n) || memcmp(d, s, n);
    *td = now() - t0;
    if (bad) { fprintf(stderr, "CM round trip FAILED\n"); exit(2); }
    free(c); free(d); return r;
}

int main(int argc, char **argv) {
    int ai = 1, base = 1, word = 1, rt = 0, ctx = 0, sub = 1, subx = 0, adapt = 0;
    for (; ai < argc && argv[ai][0] == '-'; ai++) {
        const char *a = argv[ai];
        if (!strncmp(a, "--sample=", 9)) o_sample = strtoull(a + 9, 0, 0) << 20;
        else if (!strncmp(a, "--minc=", 7)) o_minc = atoi(a + 7);
        else if (!strncmp(a, "--maxl=", 7)) o_maxl = atoi(a + 7);
        else if (!strncmp(a, "--rounds=", 9)) o_rounds = atoi(a + 9);
        else if (!strncmp(a, "--per=", 6)) o_per = atoi(a + 6);
        else if (!strncmp(a, "--minuse=", 9)) o_minuse = atoi(a + 9);
        else if (!strncmp(a, "--upmb=", 7)) o_upmb = atof(a + 7);
        else if (!strncmp(a, "--minlen=", 9)) o_minlen = atoi(a + 9);
        else if (!strncmp(a, "--maxtok=", 9)) o_maxtok = atoi(a + 9);
        else if (!strncmp(a, "--singles=", 10)) o_singles = atoi(a + 10);
        else if (!strncmp(a, "--order=", 8)) o_order = atoi(a + 8);
        else if (!strncmp(a, "--maxcodes=", 11)) o_maxcodes = atoi(a + 11);
        else if (!strcmp(a, "--nocm")) o_cm = 0;
        else if (!strcmp(a, "--nobase")) base = 0;
        else if (!strcmp(a, "--noword")) word = 0;
        else if (!strcmp(a, "--words-only")) o_words_only = 1;
        else if (!strcmp(a, "--bytes")) o_wordleaf = 0;
        else if (!strcmp(a, "--rt")) rt = 1;
        else if (!strcmp(a, "--ctx")) ctx = 1;
        else if (!strcmp(a, "--nosub")) sub = 0;
        else if (!strcmp(a, "--subx")) subx = 1;
        else if (!strcmp(a, "--adapt")) adapt = 1;
        else if (!strncmp(a, "--pxT=", 6)) px_T = atoi(a + 6);
        else if (!strncmp(a, "--pxvar=", 8)) px_var = atoi(a + 8);
        else { fprintf(stderr, "unknown option %s\n", a); return 1; }
    }
    (void)o_capfold; (void)cmp_lex_idx_D;
    for (; ai < argc; ai++) {
        FILE *f = fopen(argv[ai], "rb"); if (!f) { perror(argv[ai]); return 1; }
        fseek(f, 0, SEEK_END); size_t n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *d = malloc(n + 64); if (fread(d, 1, n, f) != n) return 1; fclose(f);
        const char *name = strrchr(argv[ai], '/') ? strrchr(argv[ai], '/') + 1 : argv[ai];
        double te = 0, td = 0;
        if (base && o_cm) {
            size_t c = rt ? cm3_rt(d, n, &te, &td) : cm3(d, n, &te);
            printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"cm\",\"size\":%zu,\"enc_s\":%.3f,\"dec_s\":%.3f}\n", name, n, c, te, td); fflush(stdout);
        }
        if (word && o_cm) {
            uint8_t *w = malloc(n + TSLACK + 64); int64_t wn = word_encode(d, n, w, n + TSLACK);
            if (wn > 0) {
                size_t c = rt ? cm3_rt(w, (size_t)wn, &te, &td) : cm3(w, (size_t)wn, &te);
                printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"word+cm\",\"tn\":%lld,\"size\":%zu,\"enc_s\":%.3f,\"dec_s\":%.3f}\n", name, n, (long long)wn, c, te, td); fflush(stdout);
            }
            free(w);
        }
        pstat st = {0}; size_t tn;
        uint8_t *t = phrase_encode(d, n, &tn, &st);
        uint8_t *back = malloc(n + 64); double t0 = now();
        int ok = !phrase_decode(t, tn, back, n) && !memcmp(back, d, n);
        st.t_dec = now() - t0;
        if (!ok) { fprintf(stderr, "%s: phrase round trip FAILED\n", name); return 2; }
        size_t c = 0, cd = 0;
        if (ctx) {
            double tx; free(px_set); px_set = NULL;
            size_t c0 = cmx3(d, n, &tx);
            printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"cmx-null\",\"size\":%zu,\"enc_s\":%.3f}\n", name, n, c0, tx); fflush(stdout);
            px_build_from(t, tn); double x; size_t cdict = cm3(t, st.dict_bytes, &x);
            size_t c1 = cmx3(d, n, &tx);
            printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"cmx-dict\",\"size\":%zu,\"body\":%zu,\"dict_cm\":%zu,\"nphr\":%zu,\"enc_s\":%.3f}\n", name, n, c1 + cdict, c1, cdict, st.nphr, tx); fflush(stdout);
            free(px_set); px_set = NULL;
        }
        if (adapt) {
            double tx; free(px_set); px_set = NULL; px_mode = 1; free(px_cnt); px_cnt = NULL;
            size_t c1 = cmx3(d, n, &tx);
            printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"cmx-adapt\",\"T\":%d,\"var\":%d,\"size\":%zu,\"enc_s\":%.3f}\n", name, n, px_T, px_var, c1, tx); fflush(stdout);
            px_mode = 0; free(px_cnt); px_cnt = NULL;
        }
        if (subx) {
            double tx; free(px_set); px_set = NULL; memset(px_cls, 0, 256);
            int ncb = t[0], ns1 = t[2]; for (int k = 0; k < ncb; k++) px_cls[t[4 + k]] = k < ns1 ? 1 : 2;
            px_codes = 1; size_t c1 = cmx3(t, tn, &tx); px_codes = 0;
            printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"sub+cmx\",\"size\":%zu,\"nphr\":%zu,\"enc_s\":%.3f}\n", name, n, c1, st.nphr, tx); fflush(stdout);
        }
        if (o_cm && sub) { c = rt ? cm3_rt(t, tn, &te, &td) : cm3(t, tn, &te); double x; cd = cm3(t, st.dict_bytes, &x); }
        if (!sub) { free(t); free(back); free(d); continue; }
        printf("{\"file\":\"%s\",\"n\":%zu,\"variant\":\"phrase+cm\",\"tn\":%zu,\"dict\":%zu,\"nphr\":%zu,\"singles\":%zu,\"prefixes\":%zu,\"coded\":%zu,\"lit\":%zu,\"esc\":%zu,"
               "\"size\":%zu,\"dict_cm\":%zu,\"build_s\":%.3f,\"tenc_s\":%.3f,\"tdec_s\":%.3f,\"enc_s\":%.3f,\"dec_s\":%.3f,"
               "\"opts\":\"sample=%zuM minc=%d maxl=%d rounds=%d per=%d minuse=%d minlen=%d maxtok=%d singles=%d order=%d words_only=%d bytes=%d maxcodes=%d\"}\n",
               name, n, tn, st.dict_bytes, st.nphr, st.nsingle, st.nprefix, st.coded, st.literal_bytes, st.escapes, c, cd, st.t_build, st.t_enc, st.t_dec, te, td,
               o_sample >> 20, o_minc, o_maxl, o_rounds, o_per, o_minuse, o_minlen, o_maxtok, o_singles, o_order, o_words_only, !o_wordleaf, o_maxcodes);
        fflush(stdout);
        free(t); free(back); free(d);
    }
    return 0;
}
