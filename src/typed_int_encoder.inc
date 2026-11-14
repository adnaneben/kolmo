/* RC opt-in type-first encoder. Forward/leaf semantics ported from P2-E9/P2-E11,
 * with bounded header parsing, checked parameter writes and generic fallback.
 * One <=16 MiB buffer frame, one worker, no dictionary, existing CZN1 v7 syntax.
 * Neither this option nor --native-leaf is selected by default. */
typedef struct { kbuf par, res, rest; size_t cnt, m; int w, bo, bs, ow, pln; uint64_t g; } ktf_prog;
static void ktf_free(ktf_prog *P) { free(P->par.p); free(P->res.p); free(P->rest.p); memset(P, 0, sizeof *P); }

#define KTF_LD(p, W, SG) ((W) == 8 ? ktf_ld8(p) : (W) == 4 ? ((SG) ? (uint64_t)(int64_t)(int32_t)ktf_ld4(p) : (uint64_t)ktf_ld4(p)) \
                                              : ((SG) ? (uint64_t)(int64_t)(int16_t)ktf_ld2(p) : (uint64_t)ktf_ld2(p)))
static inline uint64_t ktf_ld8(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline uint32_t ktf_ld4(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t ktf_ld2(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
/* P2-E9: predictor choice with all three orders evaluated in the same sweep (research only). Same samples, same
 * per-order arithmetic (bias, gcd with its own early exit, zigzag, bit-length sums) and the same comparison order as
 * production kg_num_choose, so the decision AND the returned cost are identical; checked against production on every
 * input and by a randomized differential test (p2e9.c). Requires fastint.inc (KTF_LD). */
#define KTF_C_SG(W, SG)                                                                                                  \
static void ktf_c_##W##_##SG(const uint8_t *a, size_t cnt, size_t runs, size_t rl, double *bcost, int *bo, int *bs) {    \
    uint64_t g0 = 0, g1 = 0, g2 = 0, c0 = 0, c1 = 0, c2 = 0, t0 = 0, t1 = 0, t2 = 0; int64_t mn = INT64_MAX;          \
    const size_t step = (cnt - rl) / (runs > 1 ? runs - 1 : 1);                                                       \
    for (size_t r = 0; r < runs; r++) {                         /* sweep 1: min (order 0), gcds (orders 1, 2) */       \
        const uint8_t *b = a + step * r * (size_t)W; uint64_t v1 = 0, v2 = 0;                                         \
        for (size_t q = 0; q < rl; q++) {                                                                             \
            uint64_t v = KTF_LD(b + q * W, W, SG); if ((int64_t)v < mn) mn = (int64_t)v;                               \
            if (q >= 1) { int64_t d = (int64_t)(v - v1); uint64_t r0 = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; if (g1 != 1) g1 = kg_gcd(r0, g1); } \
            if (q >= 2) { int64_t d = (int64_t)(v - (2 * v1 - v2)); uint64_t r0 = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d; if (g2 != 1) g2 = kg_gcd(r0, g2); } \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    for (size_t r2 = 0; r2 < runs && g0 != 1; r2++) {            /* order-0 gcd of v - min, production's order */      \
        const uint8_t *b2 = a + step * r2 * (size_t)W;                                                                \
        for (size_t q = 0; q < rl && g0 != 1; q++) g0 = kg_gcd(KTF_LD(b2 + q * W, W, SG) - (uint64_t)mn, g0);           \
    }                                                                                                                 \
    uint64_t G0 = g0 ? g0 : 1, G1 = g1 ? g1 : 1, G2 = g2 ? g2 : 1;                                                    \
    for (size_t r = 0; r < runs; r++) {                         /* sweep 2: bit-length costs for all three orders */   \
        const uint8_t *b = a + step * r * (size_t)W; uint64_t v1 = 0, v2 = 0;                                         \
        for (size_t q = 0; q < rl; q++) {                                                                             \
            uint64_t v = KTF_LD(b + q * W, W, SG), z;                                                                  \
            z = G0 == 1 ? v - (uint64_t)mn : (v - (uint64_t)mn) / G0; c0 += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; t0++; \
            if (q >= 1) { int64_t d = (int64_t)(v - v1); z = kg_zz(G1 == 1 ? d : d / (int64_t)G1); c1 += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; t1++; } \
            if (q >= 2) { int64_t d = (int64_t)(v - (2 * v1 - v2)); z = kg_zz(G2 == 1 ? d : d / (int64_t)G2); c2 += z ? (uint64_t)(65 - __builtin_clzll(z)) : 1; t2++; } \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    if (t0 && (double)c0 / (double)t0 < *bcost) { *bcost = (double)c0 / (double)t0; *bo = 0; *bs = SG; }              \
    if (t1 && (double)c1 / (double)t1 < *bcost) { *bcost = (double)c1 / (double)t1; *bo = 1; *bs = SG; }              \
    if (t2 && (double)c2 / (double)t2 < *bcost) { *bcost = (double)c2 / (double)t2; *bo = 2; *bs = SG; }              \
}
KTF_C_SG(2, 0) KTF_C_SG(2, 1) KTF_C_SG(4, 0) KTF_C_SG(4, 1) KTF_C_SG(8, 1)
static double ktf_c_choose(const uint8_t *a, size_t cnt, int w, int *bo, int *bs) {
    size_t runs = cnt > 16384 ? 16 : 1, rl = cnt > 16384 ? 1024 : cnt; double bcost = 1e300;
    if (w == 2) { ktf_c_2_0(a, cnt, runs, rl, &bcost, bo, bs); ktf_c_2_1(a, cnt, runs, rl, &bcost, bo, bs); }
    else if (w == 4) { ktf_c_4_0(a, cnt, runs, rl, &bcost, bo, bs); ktf_c_4_1(a, cnt, runs, rl, &bcost, bo, bs); }
    else ktf_c_8_1(a, cnt, runs, rl, &bcost, bo, bs);
    return bcost;
}



/* passes for one (W, SG): bias/gcd/max, then residual write. Mirrors kg_encode_num exactly. */
#define KTF_PASSES(W, SG)                                                                                              \
static int ktf_pass_##W##_##SG(const uint8_t *a, size_t cnt, int bo, uint64_t *pbias, uint64_t *pg, int *pow, kbuf *res) { \
    uint64_t bg = 0, bbias = 0, bmax = 0, zmax = 0;                                                                   \
    if (bo == 0) {                                                                                                    \
        int64_t mn = INT64_MAX;                                                                                       \
        for (size_t q = 0; q < cnt; q++) { int64_t v = (int64_t)KTF_LD(a + q * W, W, SG); if (v < mn) mn = v; }        \
        bbias = (uint64_t)mn;                                                                                         \
        for (size_t q = 0; q < cnt; q++) {                                                                            \
            uint64_t r = KTF_LD(a + q * W, W, SG) - bbias; if (r > zmax) zmax = r;                                     \
            if (bg != 1) bg = kg_gcd(r, bg);                                                                          \
        }                                                                                                             \
    } else {                                                                                                          \
        uint64_t v1 = 0, v2 = 0;                                                                                      \
        for (size_t q = 0; q < cnt; q++) {                                                                            \
            uint64_t v = KTF_LD(a + q * W, W, SG);                                                                     \
            if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                           \
            int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2));                                                  \
            uint64_t r = d < 0 ? (uint64_t)0 - (uint64_t)d : (uint64_t)d, z = kg_zz(d); if (z > zmax) zmax = z;       \
            if (bg != 1) bg = kg_gcd(r, bg);                                                                          \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    if (!bg) bg = 1;                                                                                                  \
    if (bg == 1) bmax = zmax;                                                                                         \
    else {                                                                                                            \
        uint64_t v1 = 0, v2 = 0;                                                                                      \
        for (size_t q = 0; q < cnt; q++) {                                                                            \
            uint64_t v = KTF_LD(a + q * W, W, SG), z;                                                                  \
            if (bo == 0) z = (v - bbias) / bg;                                                                        \
            else if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                      \
            else z = kg_zz((int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)) / (int64_t)bg);                                \
            if (z > bmax) bmax = z;                                                                                   \
            v2 = v1; v1 = v;                                                                                          \
        }                                                                                                             \
    }                                                                                                                 \
    int ow = kg_bw(bmax), pln = ow > 1; size_t m = cnt - (size_t)bo;                                                  \
    if (kb_res(res, m * (size_t)ow + 1)) return -1;                                                                   \
    uint8_t *r = res->p; uint64_t v1 = 0, v2 = 0;                                                                     \
    for (size_t q = 0; q < cnt; q++) {                                                                                \
        uint64_t v = KTF_LD(a + q * W, W, SG), z;                                                                      \
        if ((int)q < bo) { v2 = v1; v1 = v; continue; }                                                               \
        if (bo == 0) z = bg == 1 ? v - bbias : (v - bbias) / bg;                                                      \
        else { int64_t d = (int64_t)(v - (bo == 1 ? v1 : 2 * v1 - v2)); z = kg_zz(bg == 1 ? d : d / (int64_t)bg); }   \
        size_t i = q - (size_t)bo;                                                                                    \
        if (!pln) r[i] = (uint8_t)z;                                                                                  \
        else if (ow == 2) { r[i] = (uint8_t)z; r[m + i] = (uint8_t)(z >> 8); }                                        \
        else if (ow == 4) { r[i] = (uint8_t)z; r[m + i] = (uint8_t)(z >> 8); r[2 * m + i] = (uint8_t)(z >> 16); r[3 * m + i] = (uint8_t)(z >> 24); } \
        else for (int b = 0; b < ow; b++) r[(size_t)b * m + i] = (uint8_t)(z >> (8 * b));                             \
        v2 = v1; v1 = v;                                                                                              \
    }                                                                                                                 \
    res->n = m * (size_t)ow; *pbias = bbias; *pg = bg; *pow = ow; return 0;                                           \
}
KTF_PASSES(2, 0) KTF_PASSES(2, 1) KTF_PASSES(4, 0) KTF_PASSES(4, 1) KTF_PASSES(8, 0) KTF_PASSES(8, 1)

/* pre-guard hook: when set, called after the predictor choice with the estimated residual bits; nonzero aborts */
typedef int (*ktf_guard_fn)(void *ctx, double est_bits);
static int ktf_forward_g(const uint8_t *src, size_t n, int w, size_t h, ktf_prog *P, ktf_guard_fn gf, void *gctx);
static int ktf_forward(const uint8_t *src, size_t n, int w, size_t h, ktf_prog *P) { return ktf_forward_g(src, n, w, h, P, NULL, NULL); }
static int ktf_forward_g(const uint8_t *src, size_t n, int w, size_t h, ktf_prog *P, ktf_guard_fn gf, void *gctx) {
    memset(P, 0, sizeof *P);
    if ((w != 2 && w != 4 && w != 8) || h > n || (n - h) / (size_t)w < 16) return -1;
    size_t cnt = (n - h) / (size_t)w; const uint8_t *a = src + h; int bo = 0, bs = 1, ow; uint64_t bias, g;
    double bits = ktf_c_choose(a, cnt, w, &bo, &bs);            /* P2-E9: fused-order choice, identical decision */
    if (gf && gf(gctx, bits * (double)cnt)) return 1;                       /* guard: not worth a typed graph */
    int rc = w == 2 ? (bs ? ktf_pass_2_1 : ktf_pass_2_0)(a, cnt, bo, &bias, &g, &ow, &P->res)
           : w == 4 ? (bs ? ktf_pass_4_1 : ktf_pass_4_0)(a, cnt, bo, &bias, &g, &ow, &P->res)
                    : (bs ? ktf_pass_8_1 : ktf_pass_8_0)(a, cnt, bo, &bias, &g, &ow, &P->res);
    if (rc) return -1;
    int pln = ow > 1;
    kbuf *par = &P->par;
    if (kb_byte(par, (uint8_t)w) || kb_byte(par, (uint8_t)(bs | bo << 1 | pln << 3)) || kb_byte(par, (uint8_t)ow)
        || kb_uv(par,h) || kb_uv(par,cnt) || kb_uv(par,g) || kb_uv(par,kg_zz((int64_t)bias))) return -1;
    for (int q=0;q<bo;q++) if(kb_uv(par,kg_zz((int64_t)kg_ld(a+(size_t)q*w,w,bs)))) return -1;
    if (!par->p || kb_res(&P->rest, n - cnt * (size_t)w + 1)) return -1;
    if(kb_put(&P->rest,src,h) || kb_put(&P->rest,a+cnt*(size_t)w,n-h-cnt*(size_t)w)) return -1;
    P->cnt = cnt; P->m = cnt - (size_t)bo; P->w = w; P->bo = bo; P->bs = bs; P->ow = ow; P->pln = pln; P->g = g;
    return 0;
}

static size_t ktf_wrap(const uint8_t *src, size_t n, int w, size_t h, const uint8_t *pay, size_t pn, uint8_t *dst) {
    uint8_t *e = dst + HDR, *p = dst + HDR + IDX;
    memcpy(dst, "CZN1", 4); dst[4] = 7; dst[5] = (uint8_t)KOLMO_FAST; dst[6] = dst[7] = dst[8] = dst[9] = 0; dst[10] = 0; dst[11] = 0;
    put64(dst + 12, n); put32(dst + 20, 1); put32(dst + 24, (uint32_t)n);
    p[0] = T_KG; p[1] = 0; p[2] = (uint8_t)w; p[3] = (uint8_t)(h < 255 ? h : 255); memmove(p + 4, pay, pn);
    put32(e, (uint32_t)n); put32(e + 4, (uint32_t)(pn + 4)); put32(e + 8, blk_sum(3, src, n)); e[12] = C_GRAPH; e[13] = 0; e[14] = e[15] = 0;
    uint32_t hc = kolmo_crc32c(0, dst, 28); hc = kolmo_crc32c(hc, dst + HDR, IDX); put32(dst + 28, hc);
    return HDR + IDX + 4 + pn;
}
static size_t ktf_payload_huf(ktf_prog *P, uint8_t *dst, size_t cap) {
    kbuf o = {0}; size_t out = 0, zc = P->res.n + P->res.n / 8 + 1024 * (size_t)(P->ow + 1);
    uint8_t *hb = malloc(zc); if (!hb) return 0;
    size_t z = 0; int cc = C_HUF, planes = P->pln ? P->ow : 1; size_t plen = P->pln ? P->m : P->res.n;
    for (int b = 0; b < planes && cc == C_HUF; b++) {
        size_t r = plen ? huf_compress_all(P->res.p + (size_t)b * plen, plen, hb + z, zc - z) : 0;
        if (plen && !r) cc = C_STORE; else z += r;
    }
    if (cc != C_HUF || z >= P->res.n) { cc = C_STORE; z = P->res.n; memcpy(hb, P->res.p, z); }
    if (kb_byte(&o, 'G') || kb_byte(&o, 1) || kb_uv(&o, 1)                                   /* header, one node */
        || kb_byte(&o, KG_INT) || kb_uv(&o, 1) || kb_uv(&o, 0) || kb_uv(&o, 2) || kb_uv(&o, P->par.n) || kb_put(&o, P->par.p, P->par.n)
        || kb_uv(&o, 2)                                                                         /* two leaves */
        || kb_uv(&o, 1) || kb_byte(&o, (uint8_t)cc) || kb_uv(&o, P->res.n) || kb_uv(&o, z) || kb_put(&o, hb, z)
        || kb_uv(&o, 2) || kb_byte(&o, C_STORE) || kb_uv(&o, P->rest.n) || kb_uv(&o, P->rest.n) || kb_put(&o, P->rest.p, P->rest.n)) goto done;
    if (o.n <= cap) { memcpy(dst, o.p, o.n); out = o.n; }
done:
    free(o.p); free(hb); return out;
}

static const uint8_t *ktf_unique(const uint8_t *s,size_t n,const char *key) {
    size_t k=strlen(key); const uint8_t *p=memmem(s,n,key,k);
    if(!p || memmem(p+k,n-(size_t)(p+k-s),key,k)) return NULL;
    return p+k;
}
/* Deliberately conservative: unsupported declarations are opaque fallback, never
 * semantic normalization. All payload bytes and the exact original header survive. */
static int ktf_type(const uint8_t *s,size_t n,int *w,size_t *h) {
    const uint16_t one=1; if(*(const uint8_t*)&one!=1)return 0;
    if(n>=10 && !memcmp(s,"\x93NUMPY",6)) {
        size_t off= s[6]==1?10:12;
        if((s[6]!=1 && s[6]!=2 && s[6]!=3) || s[7]!=0 || n<off) return 0;
        size_t hl=off==10?(size_t)s[8]|(size_t)s[9]<<8:(size_t)get32(s+8);
        if(hl>65536 || hl>n-off)return 0;
        const uint8_t *a=s+off,*end=a+hl;
        const uint8_t *d=ktf_unique(a,hl,"'descr': '");
        const uint8_t *f=ktf_unique(a,hl,"'fortran_order': ");
        const uint8_t *sh=ktf_unique(a,hl,"'shape': (");
        if(!d || !f || !sh || end-d<4 || end-f<5 || memcmp(f,"False",5)
           || d[0]!='<' || (d[1]!='i'&&d[1]!='u') || d[3]!='\''
           || (d[2]!='2'&&d[2]!='4'&&d[2]!='8'))return 0;
        size_t count=1;int dims=0,closed=0;
        while(sh<end){
            while(sh<end && *sh==' ')sh++;
            if(sh<end && *sh==')'){closed=1;break;}
            if(sh==end || *sh<'0'||*sh>'9' || ++dims>32)return 0;
            size_t v=0;
            while(sh<end && *sh>='0'&&*sh<='9'){
                unsigned digit=*sh++-'0';if(v>(SIZE_MAX-digit)/10)return 0;v=v*10+digit;
            }
            if(v && count>SIZE_MAX/v)return 0;count*=v;
            while(sh<end&&*sh==' ')sh++;
            if(sh<end&&*sh==','){sh++;continue;}
            if(sh<end&&*sh==')'){closed=1;break;}return 0;
        }
        size_t ww=(size_t)(d[2]-'0');
        if(!closed || !dims || count>SIZE_MAX/ww || count*ww!=n-off-hl)return 0;
        *w=(int)ww;*h=off+hl;return 1;
    }
    if(n>=12 && !memcmp(s,"RIFF",4) && !memcmp(s+8,"WAVE",4)) {
        if((uint64_t)get32(s+4)+8!=n)return 0;
        size_t i=12;int fmt=0;unsigned align=0;
        while(i<=n && n-i>=8){
            size_t len=get32(s+i+4);if(len>n-i-8)return 0;
            const uint8_t *p=s+i+8;
            if(!memcmp(s+i,"fmt ",4)){
                if(fmt || len<16)return 0;
                unsigned channels=p[2]|(unsigned)p[3]<<8;
                align=p[12]|(unsigned)p[13]<<8;
                if(p[0]!=1||p[1]!=0||p[14]!=16||p[15]!=0||!channels||align!=channels*2)return 0;
                fmt=1;
            }
            if(!memcmp(s+i,"data",4)){
                if(!fmt || !align || len%align || i+8+len!=n)return 0;
                *w=2;*h=i+8;return 1;
            }
            if((len&1) && len==n-i-8)return 0;i+=8+len+(len&1);
        }
    }
    return 0;
}
typedef struct {const uint8_t *s;size_t n;int oom;} ktf_guard_ctx;
static int ktf_guard(void *v,double bits) {
    ktf_guard_ctx *g=v;size_t sn=g->n<16384?g->n:16384,off=((g->n-sn)/2)&~(size_t)7;
    size_t cap=ZSTD_compressBound(sn);uint8_t *z=malloc(cap);
    if(!z){g->oom=1;return 1;}
    size_t zn=ZSTD_compress(z,cap,g->s+off,sn,3);free(z);
    return ZSTD_isError(zn) || bits/8.0>(double)zn*(double)g->n/(double)sn;
}
/* Returns zero to use normal planner; positive accepted frame; negative error.
 * A valid typed declaration rejected by the fixed guard requests a plain fallback. */
static int64_t ktf_encode(const uint8_t *s,size_t n,uint8_t *dst,size_t cap,int *plain) {
    int w;size_t h;*plain=0;
    if(!ktf_type(s,n,&w,&h) || (n-h)/(size_t)w<16)return 0;
    ktf_prog P;ktf_guard_ctx gc={s,n,0};
    int fr=ktf_forward_g(s,n,w,h,&P,ktf_guard,&gc);
    if(fr){ktf_free(&P);if(fr<0||gc.oom)return KOLMO_ERR_ALLOC;*plain=1;return 0;}
    if(cap<HDR+IDX+4){ktf_free(&P);return KOLMO_ERR_DSTSIZE;}
    size_t pn=ktf_payload_huf(&P,dst+HDR+IDX+4,cap-HDR-IDX-4);ktf_free(&P);
    if(!pn){*plain=1;return 0;}
    return (int64_t)ktf_wrap(s,n,w,h,dst+HDR+IDX+4,pn,dst);
}
