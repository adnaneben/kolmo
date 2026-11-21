/* kfam's byte model and binary arithmetic coder (src/experimental/kfam.c, unchanged) as a stream coder for the
 * stacking tool. Context modes: 0 none; 1 plane index (i / m); 2 cross-plane byte (byte i - m, plane k - 1). */
#define main kfam_cli_main
#include "kfam.c"
#undef main
typedef struct { int mode; size_t m; } kctx;
static uint8_t kctx_at(const kctx *c, size_t i, const uint8_t *d) {
    if (c->mode == 1) return (uint8_t)(c->m ? i / c->m : 0);
    if (c->mode == 2) return c->m && i >= c->m ? d[i - c->m] : 0;
    return 0;
}
static int kctx_fn(void *arg, size_t i, const uint8_t *o) { return kctx_at(arg, i, o); }
size_t kfapi_enc(const uint8_t *d, size_t n, int mode, size_t m, uint8_t **out) {
    pthread_once(&ktab_once, ktab_init);
    stream s; memset(&s, 0, sizeof s); s.kind = K_BYTES; s.data.p = (uint8_t *)d; s.data.n = n;
    uint8_t *cx = NULL; kctx c = {mode, m};
    if (mode) { cx = malloc(n ? n : 1); if (!cx) return 0; for (size_t i = 0; i < n; i++) cx[i] = kctx_at(&c, i, d); s.ctx.p = cx; s.ctx.n = n; }
    enc_stream(&s); free(cx);
    if (s.err) { free(s.out.p); return 0; }
    *out = s.out.p; return s.out.n;
}
int kfapi_dec(const uint8_t *in, size_t inn, size_t rawn, int mode, size_t m, uint8_t *dst) {
    pthread_once(&ktab_once, ktab_init);
    stream s; memset(&s, 0, sizeof s); s.kind = K_BYTES; kctx c = {mode, m};
    djob j = {&s, in, inn, rawn, mode ? kctx_fn : NULL, &c};
    dec_stream(&j);
    if (s.err) { free(s.data.p); return -1; }
    memcpy(dst, s.data.p, rawn); free(s.data.p); return 0;
}
