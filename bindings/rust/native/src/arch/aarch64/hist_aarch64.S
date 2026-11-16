/* Byte histogram, AArch64 baseline (Armv8.0-A, Advanced SIMD for the final sum): kolmo_hist_aarch64(p, n, hc[256]).
 * Same result as the C reference kolmo_hist_c (src/algo/hist.c), checked by tests/test_asm.c. Eight counter tables
 * (hc and seven on the stack, 7 KiB): a run of one byte value waits on one load-add-store chain every eight bytes.
 * Bytes are taken from one 8-byte load with ubfx. Not chosen by the dispatch until measured on Arm hardware
 * (build with -DKOLMO_HIST_AARCH64 to choose it; benchmark/rc3/structure/hist_bench.c measures it). */
#include "../asm.h"

kolmo_function kolmo_hist_aarch64
    /* x0 = p, x1 = n, x2 = hc (table 0); tables 1..7 at sp + 1024 * (k - 1) */
    sub     sp, sp, #4096                   /* 7 KiB in two steps: add/sub immediates are 12 bits */
    sub     sp, sp, #3072
    movi    v0.16b, #0
    mov     x3, x2
    mov     x4, #32
1:  stp     q0, q0, [x3], #32               /* hc[] = 0 */
    subs    x4, x4, #1
    b.ne    1b
    mov     x3, sp
    mov     x4, #224
2:  stp     q0, q0, [x3], #32               /* tables 1..7 = 0 */
    subs    x4, x4, #1
    b.ne    2b
    mov     x3, sp                          /* table bases: x2, x3..x9 */
    add     x4, sp, #1024
    add     x5, sp, #2048
    add     x6, sp, #3072
    add     x7, sp, #4096
    add     x8, x7, #1024
    add     x9, x7, #2048
    lsr     x10, x1, #3                     /* 8-byte words */
    cbz     x10, 4f
3:  ldr     x11, [x0], #8
    ubfx    x12, x11, #0, #8
    ubfx    x13, x11, #8, #8
    ubfx    x14, x11, #16, #8
    ubfx    x15, x11, #24, #8
    ldr     w16, [x2, x12, lsl #2]
    ldr     w17, [x3, x13, lsl #2]
    add     w16, w16, #1
    add     w17, w17, #1
    str     w16, [x2, x12, lsl #2]
    str     w17, [x3, x13, lsl #2]
    ldr     w16, [x4, x14, lsl #2]
    ldr     w17, [x5, x15, lsl #2]
    add     w16, w16, #1
    add     w17, w17, #1
    str     w16, [x4, x14, lsl #2]
    str     w17, [x5, x15, lsl #2]
    ubfx    x12, x11, #32, #8
    ubfx    x13, x11, #40, #8
    ubfx    x14, x11, #48, #8
    lsr     x15, x11, #56
    ldr     w16, [x6, x12, lsl #2]
    ldr     w17, [x7, x13, lsl #2]
    add     w16, w16, #1
    add     w17, w17, #1
    str     w16, [x6, x12, lsl #2]
    str     w17, [x7, x13, lsl #2]
    ldr     w16, [x8, x14, lsl #2]
    ldr     w17, [x9, x15, lsl #2]
    add     w16, w16, #1
    add     w17, w17, #1
    str     w16, [x8, x14, lsl #2]
    str     w17, [x9, x15, lsl #2]
    subs    x10, x10, #1
    b.ne    3b
4:  ands    x1, x1, #7                      /* the last n % 8 bytes go to table 0 */
    b.eq    6f
5:  ldrb    w12, [x0], #1
    ldr     w16, [x2, x12, lsl #2]
    add     w16, w16, #1
    str     w16, [x2, x12, lsl #2]
    subs    x1, x1, #1
    b.ne    5b
6:  mov     x12, #0                         /* hc[c..c+3] += tables 1..7, 4 counters per step */
7:  ldr     q0, [x2, x12]
    ldr     q1, [x3, x12]
    ldr     q2, [x4, x12]
    ldr     q3, [x5, x12]
    ldr     q4, [x6, x12]
    ldr     q5, [x7, x12]
    ldr     q6, [x8, x12]
    ldr     q7, [x9, x12]
    add     v0.4s, v0.4s, v1.4s
    add     v2.4s, v2.4s, v3.4s
    add     v4.4s, v4.4s, v5.4s
    add     v6.4s, v6.4s, v7.4s
    add     v0.4s, v0.4s, v2.4s
    add     v4.4s, v4.4s, v6.4s
    add     v0.4s, v0.4s, v4.4s
    str     q0, [x2, x12]
    add     x12, x12, #16
    cmp     x12, #1024
    b.ne    7b
    add     sp, sp, #4096
    add     sp, sp, #3072
    ret
kolmo_endfunc kolmo_hist_aarch64

kolmo_asm_end
