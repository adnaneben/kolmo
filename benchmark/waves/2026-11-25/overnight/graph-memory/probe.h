/* Included immediately after the private kbuf definition in the isolated measuring build. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { META, LEAF, INV, FIELDS_TEMP, DEC64_TEMP, NCAT };
#define TR_LIMIT (128UL << 20)
#define TR_SLOTS 16384

typedef struct { void *p; size_t n; int cat; } tr_slot;
typedef struct { uintptr_t lo, hi; } tr_range;
static tr_slot tr_slots[TR_SLOTS];
static tr_range tr_ranges[TR_SLOTS];
static unsigned char tr_seen[TR_SLOTS];
static struct {
    int active, phase;
    size_t used, live, peak, cur[NCAT], max[NCAT], at_peak[NCAT], calls, reallocs;
    size_t view_peak, backing_peak, fused_live, fused_peak, fused_count, dict_peak, ops[8];
} tr;

static void tr_check(int yes) {
    if (!yes) { fputs("graph audit invariant failed\n", stderr); abort(); }
}
static size_t tr_index(const void *p) {
    for (size_t i=0; i<tr.used; i++) if (tr_slots[i].p==p) return i;
    tr_check(0); return 0;
}
static int tr_cat(const char *fn) {
    if (!strcmp(fn,"kb_res")) return tr.phase;
    if (!strcmp(fn,"kg_dec64_inv")) return DEC64_TEMP;
    if (!strcmp(fn,"kg_fields_inv")) return FIELDS_TEMP;
    return META;
}
static void tr_add(void *p, size_t n, int cat) {
    if (!p || !tr.active) return;
    tr_check(tr.used<TR_SLOTS && n<=TR_LIMIT-tr.live);
    tr_slots[tr.used++]=(tr_slot){p,n,cat}; tr.cur[cat]+=n; tr.live+=n; tr.calls++;
    if (tr.cur[cat]>tr.max[cat]) tr.max[cat]=tr.cur[cat];
    if (tr.live>tr.peak) {
        tr.peak=tr.live;
        memcpy(tr.at_peak,tr.cur,sizeof tr.cur);
    }
}
static void tr_drop(size_t i) {
    tr_slot s=tr_slots[i]; tr.cur[s.cat]-=s.n; tr.live-=s.n;
    tr_slots[i]=tr_slots[--tr.used];
}
static void *tr_malloc(size_t n, const char *fn) {
    void *p=malloc(n); tr_add(p,n,tr_cat(fn)); return p;
}
static void *tr_calloc(size_t count,size_t n,const char *fn) {
    void *p=calloc(count,n); tr_add(p,count*n,tr_cat(fn)); return p;
}
static void *tr_realloc(void *old,size_t n,const char *fn) {
    int tracked=tr.active && old!=NULL;
    size_t i=tracked?tr_index(old):0;
    void *p=realloc(old,n);
    if (p && tracked) { tr_drop(i); tr.reallocs++; }
    tr_add(p,n,tr_cat(fn)); return p;
}
static void tr_free(void *p) {
    if (p && tr.active) tr_drop(tr_index(p));
    free(p);
}
static int tr_range_cmp(const void *a, const void *b) {
    uintptr_t x=((const tr_range *)a)->lo, y=((const tr_range *)b)->lo;
    return (x>y)-(x<y);
}
static void tr_views(const kbuf *s, size_t ns) {
    size_t count=0, backing=0;
    memset(tr_seen,0,tr.used);
    for (size_t q=0; q<ns; q++) {
        if (!s[q].p || !s[q].n || s[q].cap) continue;
        uintptr_t lo=(uintptr_t)s[q].p, hi=lo+s[q].n;
        size_t j=0;
        for (;j<tr.used;j++) {
            uintptr_t base=(uintptr_t)tr_slots[j].p;
            if (lo>=base && hi<=base+tr_slots[j].n) {
                if (!tr_seen[j]) { backing+=tr_slots[j].n; tr_seen[j]=1; }
                break;
            }
        }
        tr_check(j<tr.used && count<TR_SLOTS);
        tr_ranges[count++]=(tr_range){lo,hi};
    }
    qsort(tr_ranges,count,sizeof *tr_ranges,tr_range_cmp);
    uintptr_t end=0; size_t bytes=0;
    for (size_t j=0;j<count;j++) {
        if (tr_ranges[j].hi>end) {
            uintptr_t begin=tr_ranges[j].lo>end?tr_ranges[j].lo:end;
            bytes+=tr_ranges[j].hi-begin; end=tr_ranges[j].hi;
        }
    }
    if (bytes>tr.view_peak) tr.view_peak=bytes;
    if (backing>tr.backing_peak) tr.backing_peak=backing;
}
static void tr_fuse(const kbuf *o) {
    for (int j=0;j<4;j++) tr.fused_live+=o[j].n;
    if (tr.fused_live>tr.fused_peak) tr.fused_peak=tr.fused_live;
    tr.fused_count++;
}
static void tr_done(size_t n,size_t raw,int rc) {
    tr_check(rc==0 && tr.live==0 && tr.used==0 && tr.fused_live==0);
    fprintf(stderr,"KGTRACE %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu %zu",
            n,raw,tr.peak,tr.calls,tr.reallocs,tr.view_peak,tr.backing_peak,tr.fused_peak,tr.fused_count,tr.dict_peak,tr.live);
    for (int k=0;k<NCAT;k++) fprintf(stderr," %zu",tr.max[k]);
    for (int k=0;k<NCAT;k++) fprintf(stderr," %zu",tr.at_peak[k]);
    for (int k=1;k<=7;k++) fprintf(stderr," %zu",tr.ops[k]);
    fputc('\n',stderr); tr.active=0;
}
#define malloc(n) tr_malloc((n),__func__)
#define calloc(c,n) tr_calloc((c),(n),__func__)
#define realloc(p,n) tr_realloc((p),(n),__func__)
#define free(p) tr_free(p)
