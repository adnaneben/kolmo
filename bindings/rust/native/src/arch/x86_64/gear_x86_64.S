/* SysV x86-64 baseline, no optional extensions. Evaluate pairs with
 * H2 = 4*H0 + 2*G0 + G1 and always return the earliest match. */
#include "../asm.h"

kolmo_function kolmo_gear_x86_64
    /* rdi = bytes, rsi = length, rdx = table, rcx = mask, r8 = hash */
    pushq %rbx
    pushq %r12
    pushq %r13
    movq %rdi, %r12
    movq (%r8), %r9
    cmpq $4, %rsi
    jb 2f
    .p2align 4
1:  movzbl (%rdi), %eax
    movzbl 1(%rdi), %r10d
    movzbl 2(%rdi), %r11d
    movzbl 3(%rdi), %ebx
    movq (%rdx,%rax,8), %rax
    movq (%rdx,%r10,8), %r10
    movq (%rdx,%r11,8), %r11
    movq (%rdx,%rbx,8), %rbx
    leaq (%r10,%rax,2), %r10
    leaq (%rbx,%r11,2), %rbx
    leaq (%rax,%r9,2), %r13
    leaq (%r10,%r9,4), %r9
    testq %r13, %rcx
    jz 4f
    testq %r9, %rcx
    jz 5f
    leaq (%r11,%r9,2), %r13
    leaq (%rbx,%r9,4), %r9
    testq %r13, %rcx
    jz 6f
    testq %r9, %rcx
    jz 7f
    addq $4, %rdi
    subq $4, %rsi
    cmpq $4, %rsi
    jae 1b
2:  testq %rsi, %rsi
    jz 3f
    movzbl (%rdi), %eax
    incq %rdi
    movq (%rdx,%rax,8), %rax
    leaq (%rax,%r9,2), %r9
    testq %r9, %rcx
    jz 8f
    decq %rsi
    jmp 2b
3:  xorl %eax, %eax
    jmp 9f
4:  movq %r13, %r9
    incq %rdi
    jmp 8f
5:  addq $2, %rdi
    jmp 8f
6:  movq %r13, %r9
    addq $3, %rdi
    jmp 8f
7:  addq $4, %rdi
8:  movq %rdi, %rax
    subq %r12, %rax
9:  movq %r9, (%r8)
    popq %r13
    popq %r12
    popq %rbx
    ret
kolmo_endfunc kolmo_gear_x86_64
kolmo_asm_end
