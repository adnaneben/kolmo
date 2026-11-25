/* CPU feature detection and kernel dispatch. Apache-2.0, Kolmo contributors. See cpu.h. */
#include "arch/kernels.h"
#include "arch/arch.h"
#include "algo/predict.h"
#include "algo/planes.h"
#include "algo/mixer.h"
#include "algo/video.h"
#include <stdlib.h>
#include <string.h>
#if defined(__x86_64__) && !defined(KOLMO_NO_ASM)
#define KOLMO_X86_KERNELS 1
#include "arch/x86_64/kernels_x86.h"
#endif
#if KOLMO_NEON
#include "arch/aarch64/mixer_neon.h"
#endif

const char *kolmo_isa_name(int level) {
    static const char *n[] = {"c", "sse2", "sse41", "avx2", "neon"};
    return level >= 0 && level <= KOLMO_ISA_NEON ? n[level] : "?";
}

int kolmo_cpu_max_level(void) {
#if KOLMO_X86_KERNELS
    static int v = -1; int r = __atomic_load_n(&v, __ATOMIC_RELAXED);
    if (r < 0) {
        __builtin_cpu_init();
        r = __builtin_cpu_supports("avx2") ? KOLMO_ISA_AVX2 : __builtin_cpu_supports("sse4.1") ? KOLMO_ISA_SSE41 : KOLMO_ISA_SSE2;
        __atomic_store_n(&v, r, __ATOMIC_RELAXED);
    }
    return r;
#elif KOLMO_NEON
    return KOLMO_ISA_NEON;
#else
    return KOLMO_ISA_C;
#endif
}

void kolmo_kernels_for_level(int level, kolmo_kernels *k) {
    int mx = kolmo_cpu_max_level(); if (level > mx) level = mx; if (level < 0) level = 0;
    k->med8_inv = kolmo_med8_inv_c; k->med8_fwd = kolmo_med8_fwd_c;
    k->planes_split = kolmo_planes_split_c; k->planes_merge = kolmo_planes_merge_c;
    k->cm_dot = kolmo_cm_dot_c; k->cm_train = kolmo_cm_train_c; k->cm_hash = kolmo_cm_hash_c;
    k->cm_dot16 = kolmo_cm_dot16_c; k->cm_train16 = kolmo_cm_train16_c;
    k->vid_strip_enc = kolmo_vid_strip_enc_c; k->vid_strip_dec = kolmo_vid_strip_dec_c;
    k->img16_fwd = kolmo_img16_fwd_c; k->sad16 = kolmo_sad16_c;
#if KOLMO_X86_KERNELS
    if (level >= KOLMO_ISA_SSE2) { k->med8_inv = kolmo_med8_inv_sse2; k->med8_fwd = kolmo_med8_fwd_sse2; k->planes_split = kolmo_planes_split_sse2; k->planes_merge = kolmo_planes_merge_sse2;
                                   k->vid_strip_enc = kolmo_vid_strip_enc_sse2; k->vid_strip_dec = kolmo_vid_strip_dec_sse2;
                                   k->img16_fwd = kolmo_img16_fwd_sse2; k->sad16 = kolmo_sad16_sse2; }
    if (level >= KOLMO_ISA_SSE41) { k->med8_inv = kolmo_med8_inv_sse41; k->cm_dot = kolmo_cm_dot_sse41; k->cm_train = kolmo_cm_train_sse41; k->cm_hash = kolmo_cm_hash_sse41;
                                    k->cm_dot16 = kolmo_cm_dot16_sse41; k->cm_train16 = kolmo_cm_train16_sse41;
                                    k->vid_strip_dec = kolmo_vid_strip_dec_sse41; }
    if (level >= KOLMO_ISA_AVX2) { k->med8_fwd = kolmo_med8_fwd_avx2; k->cm_dot = kolmo_cm_dot_avx2; k->cm_train = kolmo_cm_train_avx2; k->cm_hash = kolmo_cm_hash_avx2; k->cm_dot16 = kolmo_cm_dot16_avx2; k->cm_train16 = kolmo_cm_train16_avx2; k->img16_fwd = kolmo_img16_fwd_avx2; k->sad16 = kolmo_sad16_avx2; }
    /* AVX2: the SSE4.1 kernel (the VEX-encoded build of it measured 25-35% slower; benchmark/rc3/simd-x86) */
#elif KOLMO_NEON
    if (level == KOLMO_ISA_NEON) {
        k->cm_dot16 = kolmo_cm_dot16_neon;
        k->cm_train16 = kolmo_cm_train16_neon;
    }
#endif
}

static kolmo_kernels g_k; static int g_level = -1;
static void kolmo_kernels_init(void) {
    int level = kolmo_cpu_max_level();
    const char *e = getenv("KOLMO_CPU");
#if KOLMO_NEON
    /* Promote the ARM64 candidate after measuring the full codec against the reference. */
    if (!e || strcmp(e, "neon")) level = KOLMO_ISA_C;
#else
    if (e) for (int l = 0; l <= KOLMO_ISA_AVX2; l++) if (!strcmp(e, kolmo_isa_name(l)) && l < level) level = l;
#endif
    kolmo_kernels_for_level(level, &g_k);
    g_level = level < kolmo_cpu_max_level() ? level : kolmo_cpu_max_level();
}
#include <pthread.h>
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
const kolmo_kernels *kolmo_kernels_get(void) { pthread_once(&g_once, kolmo_kernels_init); return &g_k; }
int kolmo_cpu_level(void) { kolmo_kernels_get(); return g_level; }
