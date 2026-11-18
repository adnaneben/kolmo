/* khuf decoder main loop, x86-64 with BMI2 (shlx, shrx): kolmo_khuf_dec4_x86_64(st, R, dm, W).
 * Same result as the C reference kolmo_khuf_dec4_c (src/algo/khuf.c), checked by tests/test_asm.c; dispatch in
 * algo/khuf.c (BMI2 at run time; KOLMO_CPU below avx2 or KOLMO_NO_ASM: the reference).
 *
 * Runs exactly R >= 1 rounds over the four streams of st (kolmo_khuf_st4: b[4], c[4], p[4], o[4]); a round refills
 * each stream (c &= 63; b |= bswap(load64(p)) >> c; p += (63 - c) >> 3; c |= 56) and then makes K lookups per stream
 * (K = 4 for W = 12, 5 for W = 11), each: e = dm[b >> (64 - W)]; store64(o, e >> 8); o += e >> 56; b <<= e & 63;
 * c -= e. The caller has checked that R rounds fit the output and input buffers.
 *
 * Why assembly: the reference keeps 12 values live across the loop (bit buffer, bit count and output pointer of four
 * streams) beside the table; GCC and clang spill some of them, which puts a store-to-load round trip into every
 * lookup's dependency chain. Here they all live in registers and the input pointers (used once per round) on the
 * stack. Registers: b r8-r11, c r12-r15, o rbx rbp rsi rdi, dm rdx, scratch rax rcx. */
#include "../asm.h"

#define ST_B 0
#define ST_C 32
#define ST_P 64
#define ST_O 96
/* stack frame (after the six pushes): p0..p3 at 0..24, R at 32, st at 40 */
#define F_P 0
#define F_R 32
#define F_ST 40

/* refill stream q: b, c are its registers, k its index */
.macro KH_REF b, c, k
    and     $63, \c
    mov     F_P+8*\k(%rsp), %rax
    mov     (%rax), %rcx
    bswap   %rcx
    shrx    \c, %rcx, %rcx
    or      %rcx, \b
    mov     \c, %rcx
    shr     $3, %ecx
    sub     %rcx, %rax
    add     $7, %rax                        /* p += (63 - c) >> 3 = 7 - (c >> 3) */
    mov     %rax, F_P+8*\k(%rsp)
    or      $56, \c
.endm
/* one lookup in stream (b, c, o); sh = 64 - W */
.macro KH_LK b, c, o, sh
    mov     \b, %rax
    shr     $\sh, %rax
    mov     (%rdx,%rax,8), %rax
    shlx    %rax, \b, \b
    sub     %rax, \c
    mov     %rax, %rcx
    shr     $8, %rcx
    mov     %rcx, (\o)
    shr     $56, %rax
    add     %rax, \o
.endm
.macro KH_ROUND4 sh
    KH_LK   %r8,  %r12, %rbx, \sh
    KH_LK   %r9,  %r13, %rbp, \sh
    KH_LK   %r10, %r14, %rsi, \sh
    KH_LK   %r11, %r15, %rdi, \sh
.endm
.macro KH_REFILL4
    KH_REF  %r8,  %r12, 0
    KH_REF  %r9,  %r13, 1
    KH_REF  %r10, %r14, 2
    KH_REF  %r11, %r15, 3
.endm

kolmo_function kolmo_khuf_dec4_x86_64
    /* rdi = st, rsi = R, rdx = dm, ecx = W */
    push    %rbx
    push    %rbp
    push    %r12
    push    %r13
    push    %r14
    push    %r15
    sub     $48, %rsp
    mov     %rdi, F_ST(%rsp)
    mov     %rsi, F_R(%rsp)
    mov     ST_P(%rdi), %rax
    mov     %rax, F_P(%rsp)
    mov     ST_P+8(%rdi), %rax
    mov     %rax, F_P+8(%rsp)
    mov     ST_P+16(%rdi), %rax
    mov     %rax, F_P+16(%rsp)
    mov     ST_P+24(%rdi), %rax
    mov     %rax, F_P+24(%rsp)
    mov     ST_B(%rdi), %r8
    mov     ST_B+8(%rdi), %r9
    mov     ST_B+16(%rdi), %r10
    mov     ST_B+24(%rdi), %r11
    mov     ST_C(%rdi), %r12
    mov     ST_C+8(%rdi), %r13
    mov     ST_C+16(%rdi), %r14
    mov     ST_C+24(%rdi), %r15
    mov     ST_O(%rdi), %rbx
    mov     ST_O+8(%rdi), %rbp
    mov     ST_O+24(%rdi), %rax
    mov     ST_O+16(%rdi), %rsi
    mov     %rax, %rdi
    cmp     $12, %ecx
    jne     2f
    .p2align 4
1:                                          /* W = 12: four lookups per stream and round */
    KH_REFILL4
    KH_ROUND4 52
    KH_ROUND4 52
    KH_ROUND4 52
    KH_ROUND4 52
    decq    F_R(%rsp)
    jnz     1b
    jmp     3f
    .p2align 4
2:                                          /* W = 11: five lookups */
    KH_REFILL4
    KH_ROUND4 53
    KH_ROUND4 53
    KH_ROUND4 53
    KH_ROUND4 53
    KH_ROUND4 53
    decq    F_R(%rsp)
    jnz     2b
3:
    mov     F_ST(%rsp), %rax
    mov     %r8, ST_B(%rax)
    mov     %r9, ST_B+8(%rax)
    mov     %r10, ST_B+16(%rax)
    mov     %r11, ST_B+24(%rax)
    mov     %r12, ST_C(%rax)
    mov     %r13, ST_C+8(%rax)
    mov     %r14, ST_C+16(%rax)
    mov     %r15, ST_C+24(%rax)
    mov     %rbx, ST_O(%rax)
    mov     %rbp, ST_O+8(%rax)
    mov     %rsi, ST_O+16(%rax)
    mov     %rdi, ST_O+24(%rax)
    mov     F_P(%rsp), %rcx
    mov     %rcx, ST_P(%rax)
    mov     F_P+8(%rsp), %rcx
    mov     %rcx, ST_P+8(%rax)
    mov     F_P+16(%rsp), %rcx
    mov     %rcx, ST_P+16(%rax)
    mov     F_P+24(%rsp), %rcx
    mov     %rcx, ST_P+24(%rax)
    add     $48, %rsp
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %rbp
    pop     %rbx
    ret
kolmo_endfunc kolmo_khuf_dec4_x86_64

/* kolmo_khuf_dec1_x86_64(st, R, d1, W): the one-symbol loop, same result as kh_dec1_c (src/algo/khuf.c). Per round:
 * refill each stream as above, then K = 4 (W = 12) or 5 (W < 12) lookups per stream, each: e = d1[b >> (64 - W)]
 * (length | symbol << 8); o[k] = e >> 8; b <<= e & 63; c -= e; then o += K. Registers: b r8-r11, c r12-r15, o rbx rsi
 * rdi rbp (registers without a REX prefix, so that the symbol is stored straight from %ah), d1 rdx, the shift in rcx,
 * scratch rax. */
.macro K1_REF b, c, k
    and     $63, \c
    mov     F_P+8*\k(%rsp), %rax
    mov     (%rax), %rcx
    bswap   %rcx
    shrx    \c, %rcx, %rcx
    or      %rcx, \b
    mov     \c, %rcx
    shr     $3, %ecx
    sub     %rcx, %rax
    add     $7, %rax
    mov     %rax, F_P+8*\k(%rsp)
    or      $56, \c
.endm
.macro K1_LK b, c, o, k
    shrx    %rcx, \b, %rax
    movzwl  (%rdx,%rax,2), %eax
    shlx    %rax, \b, \b
    sub     %rax, \c
    movb    %ah, \k(\o)
.endm
.macro K1_COL k
    K1_LK   %r8,  %r12, %rbx, \k
    K1_LK   %r9,  %r13, %rsi, \k
    K1_LK   %r10, %r14, %rdi, \k
    K1_LK   %r11, %r15, %rbp, \k
.endm
.macro K1_REFILL4
    K1_REF  %r8,  %r12, 0
    K1_REF  %r9,  %r13, 1
    K1_REF  %r10, %r14, 2
    K1_REF  %r11, %r15, 3
.endm

kolmo_function kolmo_khuf_dec1_x86_64
    /* rdi = st, rsi = R, rdx = d1, ecx = W */
    push    %rbx
    push    %rbp
    push    %r12
    push    %r13
    push    %r14
    push    %r15
    sub     $56, %rsp
    mov     %rdi, F_ST(%rsp)
    mov     %rsi, F_R(%rsp)
    mov     $64, %eax
    sub     %ecx, %eax
    mov     %eax, 48(%rsp)                  /* the shift 64 - W, restored after each refill */
    mov     ST_P(%rdi), %rax
    mov     %rax, F_P(%rsp)
    mov     ST_P+8(%rdi), %rax
    mov     %rax, F_P+8(%rsp)
    mov     ST_P+16(%rdi), %rax
    mov     %rax, F_P+16(%rsp)
    mov     ST_P+24(%rdi), %rax
    mov     %rax, F_P+24(%rsp)
    mov     ST_B(%rdi), %r8
    mov     ST_B+8(%rdi), %r9
    mov     ST_B+16(%rdi), %r10
    mov     ST_B+24(%rdi), %r11
    mov     ST_C(%rdi), %r12
    mov     ST_C+8(%rdi), %r13
    mov     ST_C+16(%rdi), %r14
    mov     ST_C+24(%rdi), %r15
    mov     ST_O(%rdi), %rbx
    mov     ST_O+8(%rdi), %rsi
    mov     ST_O+24(%rdi), %rbp
    mov     ST_O+16(%rdi), %rdi
    cmp     $12, %ecx
    jne     2f
    .p2align 4
1:                                          /* W = 12: four symbols per stream and round */
    K1_REFILL4
    mov     48(%rsp), %ecx
    K1_COL  0
    K1_COL  1
    K1_COL  2
    K1_COL  3
    add     $4, %rbx
    add     $4, %rsi
    add     $4, %rdi
    add     $4, %rbp
    decq    F_R(%rsp)
    jnz     1b
    jmp     3f
    .p2align 4
2:                                          /* W < 12: five */
    K1_REFILL4
    mov     48(%rsp), %ecx
    K1_COL  0
    K1_COL  1
    K1_COL  2
    K1_COL  3
    K1_COL  4
    add     $5, %rbx
    add     $5, %rsi
    add     $5, %rdi
    add     $5, %rbp
    decq    F_R(%rsp)
    jnz     2b
3:
    mov     F_ST(%rsp), %rax
    mov     %r8, ST_B(%rax)
    mov     %r9, ST_B+8(%rax)
    mov     %r10, ST_B+16(%rax)
    mov     %r11, ST_B+24(%rax)
    mov     %r12, ST_C(%rax)
    mov     %r13, ST_C+8(%rax)
    mov     %r14, ST_C+16(%rax)
    mov     %r15, ST_C+24(%rax)
    mov     %rbx, ST_O(%rax)
    mov     %rsi, ST_O+8(%rax)
    mov     %rdi, ST_O+16(%rax)
    mov     %rbp, ST_O+24(%rax)
    mov     F_P(%rsp), %rcx
    mov     %rcx, ST_P(%rax)
    mov     F_P+8(%rsp), %rcx
    mov     %rcx, ST_P+8(%rax)
    mov     F_P+16(%rsp), %rcx
    mov     %rcx, ST_P+16(%rax)
    mov     F_P+24(%rsp), %rcx
    mov     %rcx, ST_P+24(%rax)
    add     $56, %rsp
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %rbp
    pop     %rbx
    ret
kolmo_endfunc kolmo_khuf_dec1_x86_64

kolmo_asm_end
