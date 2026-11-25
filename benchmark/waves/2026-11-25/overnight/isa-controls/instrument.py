"""Add test-only per-path counters and a targeted C reference control in a private #106 tree."""
from pathlib import Path
import sys

s=Path(sys.argv[1])/"src"
def sub(file,before,after,expected=1):
    p=s/file; code=p.read_text()
    if code.count(before)!=expected: raise ValueError((file,before))
    p.write_text(code.replace(before,after))

sub("kolmo.c",'#include "arch/cpu.h"','''#include "arch/cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {
    unsigned long long crc_native, crc_ref, decimal_vector, decimal_swar, int_vector, int_total,
                       dec64_vector, dec64_ref, img_vector, word_vector, word_ref, scan_groups;
} isa_trace;
static int isa_reference;
extern int kolmo_force_c;
static void isa_log(void) {
    fprintf(stderr,"ISATRACE %d %d %d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\\n",
            kolmo_cpu_level(),kolmo_force_c,isa_reference,isa_trace.crc_native,isa_trace.crc_ref,
            isa_trace.decimal_vector,isa_trace.decimal_swar,isa_trace.int_vector,isa_trace.int_total,
            isa_trace.dec64_vector,isa_trace.dec64_ref,isa_trace.img_vector,isa_trace.word_vector,
            isa_trace.word_ref,isa_trace.scan_groups);
}
__attribute__((constructor)) static void isa_setup(void) {
    const char *ctrl=getenv("ISA_RESEARCH_CONTROL");
    if (ctrl && (!strcmp(ctrl,"force-c") || !strcmp(ctrl,"reference"))) kolmo_force_c=1;
    isa_reference=ctrl && !strcmp(ctrl,"reference");
    atexit(isa_log);
}''')
sub("kolmo.c",'#include "arch/crc32c.c"','''#define kolmo_crc32c isa_native_crc
#include "arch/crc32c.c"
#undef kolmo_crc32c
#if !(defined(__aarch64__) && defined(__ARM_FEATURE_CRC32))
#error "This measurement requires the Apple ARM64 CRC implementation."
#endif
#undef __ARM_FEATURE_CRC32
#define kolmo_crc32c isa_reference_crc
#include "arch/crc32c.c"
#undef kolmo_crc32c
#define __ARM_FEATURE_CRC32 1
uint32_t kolmo_crc32c(uint32_t crc, const void *p, size_t n) {
    if (isa_reference) { isa_trace.crc_ref+=n; return isa_reference_crc(crc,p,n); }
    isa_trace.crc_native+=n; return isa_native_crc(crc,p,n);
}''')
sub("arch/decimal.c",'    i = fmt_slots_neon','    if (!isa_reference) i = fmt_slots_neon')
sub("arch/aarch64/decimal_neon.c",'            uint32x2_t a32 =','            isa_trace.decimal_vector+=2;\n            uint32x2_t a32 =')
sub("algo/decimal.c",'static inline uint8_t *fmt_dec_swar(uint8_t *o, int64_t m, int s) {','static inline uint8_t *fmt_dec_swar(uint8_t *o, int64_t m, int s) {\n    isa_trace.decimal_swar++;')
sub("transforms/word.c",'    const uint8x16_t lim = vdupq_n_u8(31);','    if (isa_reference) return p;\n    const uint8x16_t lim = vdupq_n_u8(31);')
sub("transforms/word.c",'        uint8x16_t c = vcleq_u8','        isa_trace.scan_groups++;\n        uint8x16_t c = vcleq_u8')
sub("transforms/word.c",'    while (p < end) {\n        const uint8_t *s = p;','    isa_trace.word_vector+=(size_t)(o-out); isa_trace.word_ref+=(size_t)(oe-o);\n    while (p < end) {\n        const uint8_t *s = p;')
sub("transforms/graph.c",'    fi_inv_fn f = FI_USE','    isa_trace.int_total+=(size_t)(cnt-(uint64_t)ord);\n    fi_inv_fn f = !isa_reference && FI_USE')
sub("transforms/graph.c",'g, bias, v1, v2, wp)) {} else','g, bias, v1, v2, wp)) {isa_trace.int_vector+=m;} else')
sub("transforms/graph.c",'            (R) += k_; (W) += k_ * (STEP);','            isa_trace.dec64_vector+=k_; (R) += k_; (W) += k_ * (STEP);')
sub("transforms/graph.c",'    const uint64_t cnt = tg->n;','    const uint64_t cnt = tg->n; isa_trace.dec64_ref+=cnt;')
sub("transforms/graph.c",'!kolmo_force_c && kolmo_img8_back_neon(r, m, W, C, p)) return;','!kolmo_force_c && kolmo_img8_back_neon(r, m, W, C, p)) { isa_trace.img_vector+=m; return; }')
