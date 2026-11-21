/* GR6 (baseline): repeat model + graph map, then fixed 6-bit units. C port of research/sixbit/gr6.py, same units.
 *
 * Tokens: runs of [A-Za-z0-9.-], or any other single byte. Per token, one value in 6-bit units (5 data bits +
 * continuation): 0 DITTO (same token at the same position in the line above), 2+2r graph rank r in the
 * move-to-front list of the context (position in line, previous token), 3+2id global id, 1 NEW + characters
 * (62-entry frequency table, END = 62, ESC = 63 + two units).
 * File: "GR6B" | n u64 | table[62] | units u64 | packed units
 * usage: gr6 c in out | gr6 d in out | gr6 b in   (b = round trip + timing)
 */
#include "common.h"

#define CAP 1024
static int isw(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'; }

typedef struct {
    intern tok; map64 gmap; mtf *lists; size_t nlists, caplists;
    uint32_t *above, *line; size_t na, nl, capl, capa; uint32_t nl_id;
} state;

static void st_init(state *s) {
    memset(s, 0, sizeof *s); intern_init(&s->tok); map_init(&s->gmap, 16);
    s->nl_id = UINT32_MAX;
}
static mtf *ctx_list(state *s, int insert) {
    uint64_t key = (uint64_t)s->nl << 32 | (s->nl ? s->line[s->nl - 1] + 1 : 0);
    uint32_t *v = map_slot(&s->gmap, key, insert);
    if (!v) return NULL;
    if (*v == UINT32_MAX) {
        if (s->nlists == s->caplists) { s->caplists = s->caplists * 2 + 1024; s->lists = xrealloc(s->lists, s->caplists * sizeof(mtf)); }
        memset(&s->lists[s->nlists], 0, sizeof(mtf)); *v = s->nlists++;
    }
    return &s->lists[*v];
}
static void st_update(state *s, uint32_t id, int pos_in_list) {
    mtf *l = ctx_list(s, 1);
    mtf_front(l, id, pos_in_list, CAP);
    if (s->nl == s->capl) { s->capl = s->capl * 2 + 64; s->line = xrealloc(s->line, s->capl * 4); }
    s->line[s->nl++] = id;
    if (id == s->nl_id) {
        uint32_t *t = s->above; s->above = s->line; s->line = t; size_t c = s->capa; s->capa = s->capl; s->capl = c;
        s->na = s->nl; s->nl = 0;
    }
}
static inline void put_v(buf *u, uint64_t v) { while (v >= 32) { buf_put(u, 32 | (v & 31)); v >>= 5; } buf_put(u, v); }

static void build_table(const uint8_t *d, size_t n, uint8_t *table) {
    size_t cnt[256] = {0}, first[256]; for (int i = 0; i < 256; i++) first[i] = SIZE_MAX;
    for (size_t i = 0; i < n; i++) { if (!cnt[d[i]]) first[d[i]] = i; cnt[d[i]]++; }
    int ord[256]; for (int i = 0; i < 256; i++) ord[i] = i;
    for (int i = 1; i < 256; i++) { int x = ord[i], j = i; while (j > 0 && (cnt[ord[j - 1]] < cnt[x] || (cnt[ord[j - 1]] == cnt[x] && first[ord[j - 1]] > first[x]))) { ord[j] = ord[j - 1]; j--; } ord[j] = x; }
    for (int i = 0; i < 62; i++) table[i] = ord[i];
}

static buf encode(const uint8_t *d, size_t n, size_t *nunits_out, size_t counts[4]) {
    uint8_t table[62]; build_table(d, n, table);
    int code[256]; for (int i = 0; i < 256; i++) code[i] = -1; for (int i = 61; i >= 0; i--) code[table[i]] = i;
    state s; st_init(&s); buf u = {0};
    for (size_t i = 0; i < n;) {
        size_t j = i + 1; if (isw(d[i])) while (j < n && isw(d[j])) j++;
        int isnew; uint32_t id = intern_get(&s.tok, d + i, j - i, 0, &isnew);
        int pos = -1;
        if (id != UINT32_MAX && s.nl < s.na && s.above[s.nl] == id) { put_v(&u, 0); counts[0]++; mtf *l = ctx_list(&s, 0); if (l) pos = mtf_find(l, id); }
        else {
            mtf *l = id != UINT32_MAX ? ctx_list(&s, 0) : NULL;
            if (l && (pos = mtf_find(l, id)) >= 0) { put_v(&u, 2 + 2 * (uint64_t)pos); counts[1]++; }
            else if (id != UINT32_MAX) { put_v(&u, 3 + 2 * (uint64_t)id); counts[2]++; }
            else {
                put_v(&u, 1); counts[3]++;
                for (size_t k = i; k < j; k++) { if (code[d[k]] >= 0) buf_put(&u, code[d[k]]); else { buf_put(&u, 63); buf_put(&u, d[k] >> 6); buf_put(&u, d[k] & 63); } }
                buf_put(&u, 62);
            }
        }
        if (id == UINT32_MAX) { id = intern_get(&s.tok, d + i, j - i, 1, NULL); if (j - i == 1 && d[i] == '\n') s.nl_id = id; }
        st_update(&s, id, pos);
        i = j;
    }
    buf out = {0}; buf_add(&out, "GR6B", 4); uint64_t nn = n; buf_add(&out, &nn, 8); buf_add(&out, table, 62);
    uint64_t k = u.n; buf_add(&out, &k, 8); buf_reserve(&out, u.n); out.n += pack6(u.p, u.n, out.p + out.n);
    *nunits_out = u.n; free(u.p);
    return out;
}

static buf decode(const uint8_t *p, size_t pn) {
    (void)pn;
    uint64_t n, k; memcpy(&n, p + 4, 8); const uint8_t *table = p + 12; memcpy(&k, p + 74, 8);
    uint8_t *u = xmalloc(k + 8); unpack6(p + 82, k, u);
    state s; st_init(&s); buf out = {0}; buf_reserve(&out, n); size_t i = 0; uint8_t tmp[1 << 16]; buf lit = {0};
    while (i < k) {
        uint64_t v = 0; int sh = 0; for (;;) { uint8_t x = u[i++]; v |= (uint64_t)(x & 31) << sh; sh += 5; if (!(x & 32)) break; }
        uint32_t id; int pos = -1;
        if (v == 0) { id = s.above[s.nl]; mtf *l = ctx_list(&s, 0); if (l) pos = mtf_find(l, id); }
        else if (v == 1) {
            lit.n = 0;
            while (u[i] != 62) { if (u[i] == 63) { buf_put(&lit, u[i + 1] << 6 | u[i + 2]); i += 3; } else buf_put(&lit, table[u[i++]]); }
            i++; id = intern_get(&s.tok, lit.p, lit.n, 1, NULL);
            if (lit.n == 1 && lit.p[0] == '\n') s.nl_id = id;
        } else if (v % 2 == 0) { pos = (v - 2) / 2; id = ctx_list(&s, 0)->a[pos]; }
        else id = (v - 3) / 2;
        buf_add(&out, intern_str(&s.tok, id), s.tok.len[id]);
        st_update(&s, id, pos);
    }
    (void)tmp; free(u);
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: gr6 c|d|b in [out]\n"); return 2; }
    size_t n; uint8_t *in = read_file(argv[2], &n);
    if (argv[1][0] == 'c') { size_t k, c[4] = {0}; buf o = encode(in, n, &k, c); write_file(argv[3], o.p, o.n); return 0; }
    if (argv[1][0] == 'd') { buf o = decode(in, n); write_file(argv[3], o.p, o.n); return 0; }
    double te = 1e9, td = 1e9; buf o = {0}, back = {0}; size_t k = 0, c[4];
    for (int r = 0; r < 3; r++) {
        memset(c, 0, sizeof c); free(o.p); free(back.p);
        double a = now(); o = encode(in, n, &k, c); double b = now(); back = decode(o.p, o.n); double e = now();
        if (b - a < te) te = b - a; if (e - b < td) td = e - b;
    }
    int ok = back.n == n && memcmp(back.p, in, n) == 0;
    printf("gr6 %s: %zu -> %zu (%.1f%%, ratio %.3f) units %zu  enc %.1f MB/s  dec %.1f MB/s  %s\n", argv[2], n, o.n,
           100.0 * o.n / n, (double)n / o.n, k, n / te / 1e6, n / td / 1e6, ok ? "OK" : "FAIL");
    return !ok;
}
