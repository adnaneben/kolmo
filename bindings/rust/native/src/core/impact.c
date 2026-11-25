/* Research harness for the impact matrix (benchmark/impact/). Compiled only into build/kolmo-impact (make
 * kolmo-impact, -DKOLMO_IMPACT): the library, the kolmo CLI and every other target never include this file, so default
 * output is unchanged. The frames it writes are ordinary version 12 frames (only codecs and transforms the stock
 * decoder reads), so `kolmo -d` from the release build checks every round trip.
 *
 * Environment (read per block; unset = no change):
 *   KOLMO_IMPACT_CODEC=N      after the level has chosen a block's coding, also code the block with codec/role N
 *                             (7 CM, 18 BWT, 24 KLZMA, 26 GR6+, 28 KLZ, 32 KLZH2, 6 R_LZMA, 3 R_LZH ...)
 *   KOLMO_IMPACT_CLEVEL=L     the codec's own level for that trial (default 9; GR6+ ignores it; CM 0 = model 3)
 *   KOLMO_IMPACT_INPUT=raw|stream   raw (default): the plain block bytes, plan cleared. stream: the level's transformed
 *                             stream (plan, tflag and meta kept), for stacking a transform with another coder. A block
 *                             without a transform, or with the column-split flag 13, falls back to raw. GR6+ is raw only.
 *   KOLMO_IMPACT_POLICY=best|force  best (default): keep the trial when smaller than the level's choice; force: keep it
 *                             whenever it codes (stored only when it does not), to measure the codec on every block
 *   KOLMO_IMPACT_GATE=0|1     GR6+ only: 1 (default) runs it only where gr6_applies() holds, as ultra does; 0 always
 *   KOLMO_IMPACT_NOTF=1       ablation: code the plain block with the level's own codec instead of its transform
 *                             (kept per POLICY); measures what each transform contributes at each level
 *   KOLMO_IMPACT_TRANSFORMS=n override the level table's analysis setting (0 none, 1 full, 2 quick, 3 minimal)
 *   KOLMO_IMPACT_LOG=path     append one JSON line per block: index, length, the level's choice and the trial's size
 */
#ifndef KOLMO_IMPACT_C
#define KOLMO_IMPACT_C 1
#include <stdio.h>

#include <time.h>
static int impact_env_int(const char *k, int d) { const char *v = getenv(k); return v && *v ? atoi(v) : d; }
static int impact_env_is(const char *k, const char *s) { const char *v = getenv(k); return v && !strcmp(v, s); }

static void impact_params(kolmo_params *q) {
    const char *v = getenv("KOLMO_IMPACT_TRANSFORMS");
    if (v && *v) { int t = atoi(v); if (t >= 0 && t <= 3) q->transforms = (q->transforms & ~3) | t; }
}

static pthread_mutex_t impact_log_mu = PTHREAD_MUTEX_INITIALIZER;
static void impact_log(uint32_t i, size_t len, int bcodec, int btflag, size_t bsize, int tcodec, int tin, size_t tsize, int kept, double bsec, double tsec) {
    const char *path = getenv("KOLMO_IMPACT_LOG"); if (!path || !*path) return;
    pthread_mutex_lock(&impact_log_mu);
    FILE *f = fopen(path, "a");
    if (f) {
        fprintf(f, "{\"block\":%u,\"len\":%zu,\"base_codec\":%d,\"base_tflag\":%d,\"base_size\":%zu,\"trial_codec\":%d,\"trial_input\":\"%s\","
                   "\"trial_size\":%zu,\"kept\":%d,\"trial_sec\":%.6f}\n",
                i, len, bcodec, btflag, bsize, tcodec, tin ? "stream" : "raw", tsize, kept, tsec);
        fclose(f);
    }
    (void)bsec;
    pthread_mutex_unlock(&impact_log_mu);
}
static double impact_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* Called once per block after the level's choice (blocks.c, just before the block is emitted). The slot is
 * plan(4) | meta | payload; on a kept trial it is rewritten as the stock decoder expects. */
static void impact_block(uint32_t i, codec_ctx *xp, tls_t *S, size_t slotcap, int level, const uint8_t *raw, size_t len,
                         uint8_t *slot, int *codec, int *tflag, size_t *meta, size_t *cl, const uint8_t *in, size_t inlen) {
    const int notf = impact_env_int("KOLMO_IMPACT_NOTF", 0);
    int tc = impact_env_int("KOLMO_IMPACT_CODEC", -1);
    if (tc < 0 && !notf) return;
    if (!len) return;
    uint8_t *sl = slot + 4;
    const size_t base = *cl == 0 || *cl + *meta >= len ? len : *cl + *meta;   /* bytes after the plan prefix */
    const int base_codec = *cl == 0 || *cl + *meta >= len ? C_STORE : codec_wire(xp, *codec), base_tflag = *tflag;
    int force = impact_env_is("KOLMO_IMPACT_POLICY", "force");
    int stream = impact_env_is("KOLMO_IMPACT_INPUT", "stream") && *tflag && *tflag != 13 && in != raw;
    int lvl = impact_env_int("KOLMO_IMPACT_CLEVEL", 9);
    if (notf) {                                     /* ablation: the level's codec on the plain bytes */
        if (!*tflag) { impact_log(i, len, base_codec, base_tflag, base, -1, 0, base, 0, 0, 0); return; }
        tc = *codec; stream = 0; lvl = level;
        force = 1;
    }
    if (tc == C_GR6) {
        stream = 0;
        if (impact_env_int("KOLMO_IMPACT_GATE", 1) && !gr6_applies(raw, len)) { impact_log(i, len, base_codec, base_tflag, base, tc, 0, 0, 0, 0, 0); return; }
    }
    if (tc == C_CM && lvl == 9 && !notf) lvl = 0;           /* CM: 0 = the default model set (ultra's), 1 = model 1 */
    uint8_t *alt = S->rbuf = grow(S->rbuf, &S->rbcap, slotcap + 4);
    if (!alt) return;
    const uint8_t *src = stream ? in : raw; size_t sn = stream ? inlen : len, m = stream ? *meta : 0;
    double t0 = impact_now();
    size_t z = 0;
    if (m + 1 < slotcap) z = codec_compress(xp, tc, lvl, src, sn, alt, slotcap - m);
    double t1 = impact_now();
    size_t tsize = z ? z + m : 0;
    int keep = z && tsize < len && (force || tsize < base);
    impact_log(i, len, base_codec, base_tflag, base, tc, stream, tsize, keep, 0, t1 - t0);
    if (!keep) return;
    if (stream) memcpy(sl + m, alt, z);             /* plan and meta stay as written */
    else { memcpy(sl, alt, z); slot[0] = T_NONE; slot[1] = slot[2] = slot[3] = 0; *tflag = 0; *meta = 0; }
    *cl = z; *codec = tc;
}
#endif
