/* libcubwt's C API on Metal, so libbsc's own GPU hook (bwt.cpp, built with LIBBSC_CUDA_SUPPORT) sends Kolmo's BWT
 * sorts to sa_metal.m without a change to vendor/libbsc. Apache-2.0, Kolmo contributors.
 *
 * libbsc calls allocate, then bwt_aux, then free, for every BWT block when the caller passes LIBBSC_FEATURE_CUDA (Kolmo
 * does so on the GPU worker thread only). Any error here makes libbsc sort on the CPU with libsais, which gives the same
 * bytes. The inverse transform and the plain BWT (without auxiliary indexes) are not offered: Kolmo does not ask for
 * them on the GPU. */
#include "libbsc/bwt/libcubwt/libcubwt.cuh"
#include "arch/metal/metal.h"
#include "core/hetero.h"

static int token;

int64_t libcubwt_allocate_device_storage(void **device_storage, int64_t max_length) {
    if (max_length < 0 || !kolmo_gpu_sort_allowed((size_t)max_length)) return LIBCUBWT_GPU_NOT_SUPPORTED;
    *device_storage = &token;
    return LIBCUBWT_NO_ERROR;
}

int64_t libcubwt_free_device_storage(void *device_storage) { (void)device_storage; return LIBCUBWT_NO_ERROR; }

int64_t libcubwt_bwt_aux(void *device_storage, const uint8_t *T, uint8_t *L, int64_t n, int64_t r, uint32_t *I) {
    if (device_storage != &token || n < 2 || n > ((int64_t)1 << 30) || r < 2 || r > n) return LIBCUBWT_BAD_PARAMETER;
    int rc = kolmo_metal_bwt_aux(T, L, (int32_t)n, (int32_t)r, (int32_t *)I);
    kolmo_gpu_sort_done(rc ? -1.0 : kolmo_metal_last_gpu_seconds(), (size_t)n);
    return rc ? LIBCUBWT_GPU_ERROR : LIBCUBWT_NO_ERROR;
}

int64_t libcubwt_bwt(void *device_storage, const uint8_t *T, uint8_t *L, int64_t n) {
    (void)device_storage; (void)T; (void)L; (void)n;
    return LIBCUBWT_GPU_NOT_SUPPORTED;
}

int64_t libcubwt_unbwt(void *device_storage, const uint8_t *T, uint8_t *U, int64_t n, const int32_t *freq, int32_t i) {
    (void)device_storage; (void)T; (void)U; (void)n; (void)freq; (void)i;
    return LIBCUBWT_GPU_NOT_SUPPORTED;
}
