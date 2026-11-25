/* Scan a normalized FastCDC mask interval. Load four independent Gear entries
 * ahead of the recurrence and test the four updated hashes in byte order.
 * Return the one-based matching position (zero if none), and the final hash. */
#include "../asm.h"

kolmo_function kolmo_gear_aarch64
    /* x0 = bytes, x1 = length, x2 = table, x3 = mask, x4 = running hash */
    ldr x5, [x4]
    mov x6, x0
    cmp x1, #4
    b.lo 2f
    .p2align 4
1:  ldrb w7, [x0]
    ldrb w8, [x0, #1]
    ldrb w9, [x0, #2]
    ldrb w10, [x0, #3]
    ldr x7, [x2, x7, lsl #3]
    ldr x8, [x2, x8, lsl #3]
    ldr x9, [x2, x9, lsl #3]
    ldr x10, [x2, x10, lsl #3]
    add x5, x7, x5, lsl #1
    tst x5, x3
    b.eq 4f
    add x5, x8, x5, lsl #1
    tst x5, x3
    b.eq 5f
    add x5, x9, x5, lsl #1
    tst x5, x3
    b.eq 6f
    add x5, x10, x5, lsl #1
    tst x5, x3
    b.eq 7f
    add x0, x0, #4
    sub x1, x1, #4
    cmp x1, #4
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
7:  add x0, x0, #1
6:  add x0, x0, #1
5:  add x0, x0, #1
4:  add x0, x0, #1
8:  sub x0, x0, x6
    str x5, [x4]
    ret
kolmo_endfunc kolmo_gear_aarch64
kolmo_asm_end
