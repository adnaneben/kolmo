/* Byte histogram, x86-64 baseline (no extension needed): kolmo_hist_x86_64(p, n, hc[256]).
 * Same result as the C reference kolmo_hist_c (src/algo/hist.c), checked by tests/test_asm.c; dispatch in algo/hist.c.
 * Eight counter tables (hc and seven on the stack, 7 KiB) so that a run of one byte value waits on the latency of
 * one memory increment every eight bytes, not every four; the 8 bytes come from two 4-byte loads, each split by
 * shifts in its own register (two independent chains). The tables are summed into hc with SSE2 (baseline x86-64). */
#include "../asm.h"

kolmo_function kolmo_hist_x86_64
    /* rdi = p, rsi = n, rdx = hc */
    mov     %rdi, %r8                       /* p */
    mov     %rdx, %r9                       /* hc: table 0 */
    mov     %rsi, %r10                      /* n */
    sub     $7176, %rsp                     /* tables 1..7 at rsp + 1024 * (k - 1); rsp 16-byte aligned (paddd m128) */
    xor     %eax, %eax
    mov     %r9, %rdi
    mov     $128, %ecx
    rep stosq                               /* hc[] = 0 */
    mov     %rsp, %rdi
    mov     $896, %ecx
    rep stosq                               /* tables 1..7 = 0 */
    mov     %r10, %rcx
    shr     $3, %rcx                        /* 8-byte words */
    jz      2f
1:
    mov     (%r8), %eax                     /* bytes 0-3 -> tables 0-3 */
    mov     4(%r8), %r11d                   /* bytes 4-7 -> tables 4-7 */
    movzbl  %al, %edx
    movzbl  %r11b, %esi
    shr     $8, %eax
    shr     $8, %r11d
    incl    (%r9,%rdx,4)
    incl    3072(%rsp,%rsi,4)
    movzbl  %al, %edx
    movzbl  %r11b, %esi
    shr     $8, %eax
    shr     $8, %r11d
    incl    (%rsp,%rdx,4)
    incl    4096(%rsp,%rsi,4)
    movzbl  %al, %edx
    movzbl  %r11b, %esi
    shr     $8, %eax
    shr     $8, %r11d
    incl    1024(%rsp,%rdx,4)
    incl    5120(%rsp,%rsi,4)
    incl    2048(%rsp,%rax,4)
    incl    6144(%rsp,%r11,4)
    add     $8, %r8
    dec     %rcx
    jnz     1b
2:
    and     $7, %r10                        /* the last n % 8 bytes go to table 0 */
    jz      4f
3:
    movzbl  (%r8), %edx
    incl    (%r9,%rdx,4)
    inc     %r8
    dec     %r10
    jnz     3b
4:
    xor     %ecx, %ecx                      /* hc[c..c+3] += tables 1..7, 4 counters per step */
5:
    movdqu  (%r9,%rcx), %xmm0
    movdqu  (%rsp,%rcx), %xmm1
    paddd   1024(%rsp,%rcx), %xmm0
    paddd   2048(%rsp,%rcx), %xmm1
    paddd   3072(%rsp,%rcx), %xmm0
    paddd   4096(%rsp,%rcx), %xmm1
    paddd   5120(%rsp,%rcx), %xmm0
    paddd   6144(%rsp,%rcx), %xmm1
    paddd   %xmm1, %xmm0
    movdqu  %xmm0, (%r9,%rcx)
    add     $16, %rcx
    cmp     $1024, %rcx
    jne     5b
    add     $7176, %rsp
    ret
kolmo_endfunc kolmo_hist_x86_64

kolmo_asm_end
