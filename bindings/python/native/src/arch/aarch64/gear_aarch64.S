/* Scan a normalized FastCDC mask interval. Look up eight independent Gear entries.
 * The even-position hashes use H' = 4H + 2G0 + G1. Their four
 * increments are independent; test every odd and even hash in order.
 * Return the one-based matching position (zero if none), and the final hash. */
#include "../asm.h"

kolmo_function kolmo_gear_aarch64
    /* x0 = bytes, x1 = length, x2 = table, x3 = mask, x4 = running hash */
    ldr x5, [x4]
    mov x6, x0
    cmp x1, #8
    b.lo 2f
    .p2align 4
1:  ldrb w7, [x0, #0]
    ldrb w8, [x0, #1]
    ldrb w9, [x0, #2]
    ldrb w10, [x0, #3]
    ldrb w11, [x0, #4]
    ldrb w12, [x0, #5]
    ldrb w13, [x0, #6]
    ldrb w14, [x0, #7]
    ldr x7, [x2, x7, lsl #3]
    ldr x8, [x2, x8, lsl #3]
    ldr x9, [x2, x9, lsl #3]
    ldr x10, [x2, x10, lsl #3]
    ldr x11, [x2, x11, lsl #3]
    ldr x12, [x2, x12, lsl #3]
    ldr x13, [x2, x13, lsl #3]
    ldr x14, [x2, x14, lsl #3]
    add x8, x8, x7, lsl #1
    add x10, x10, x9, lsl #1
    add x12, x12, x11, lsl #1
    add x14, x14, x13, lsl #1
    add x15, x7, x5, lsl #1
    add x5, x8, x5, lsl #2
    tst x15, x3
    b.eq Lgear_hit1
    tst x5, x3
    b.eq Lgear_hit2
    add x15, x9, x5, lsl #1
    add x5, x10, x5, lsl #2
    tst x15, x3
    b.eq Lgear_hit3
    tst x5, x3
    b.eq Lgear_hit4
    add x15, x11, x5, lsl #1
    add x5, x12, x5, lsl #2
    tst x15, x3
    b.eq Lgear_hit5
    tst x5, x3
    b.eq Lgear_hit6
    add x15, x13, x5, lsl #1
    add x5, x14, x5, lsl #2
    tst x15, x3
    b.eq Lgear_hit7
    tst x5, x3
    b.eq Lgear_hit8
    add x0, x0, #8
    sub x1, x1, #8
    cmp x1, #8
    b.hs 1b
2:  cbz x1, 3f
    ldrb w7, [x0], #1
    ldr x7, [x2, x7, lsl #3]
    add x5, x7, x5, lsl #1
    tst x5, x3
    b.eq 8f
    sub x1, x1, #1
    b 2b
3:  str x5, [x4]
    mov x0, #0
    ret
Lgear_hit1:
    mov x5, x15
    add x0, x0, #1
    b 8f
Lgear_hit2:
    add x0, x0, #2
    b 8f
Lgear_hit3:
    mov x5, x15
    add x0, x0, #3
    b 8f
Lgear_hit4:
    add x0, x0, #4
    b 8f
Lgear_hit5:
    mov x5, x15
    add x0, x0, #5
    b 8f
Lgear_hit6:
    add x0, x0, #6
    b 8f
Lgear_hit7:
    mov x5, x15
    add x0, x0, #7
    b 8f
Lgear_hit8:
    add x0, x0, #8
    b 8f
8:  sub x0, x0, x6
    str x5, [x4]
    ret
kolmo_endfunc kolmo_gear_aarch64
kolmo_asm_end
