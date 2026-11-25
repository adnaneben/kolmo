/* Private report extension for the verbose CLI. Keep the public report ABI unchanged. */
#ifndef KOLMO_TARGET_REPORT_H
#define KOLMO_TARGET_REPORT_H
#include "../kolmo.h"
#if defined(__GNUC__)
__attribute__((visibility("hidden")))
#endif
int64_t kolmo_compress_target_detail(const uint8_t *src, size_t n, uint8_t *dst, size_t cap,
                                     const kolmo_params *p, const kolmo_target *t,
                                     kolmo_target_report *report, uint32_t *fallback_chunks);
#endif
