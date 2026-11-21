/* Portable GNU assembler for Kolmo's kernels (docs/LAYOUT.md, "Assembly conventions").
 *
 * Every src/arch/<isa>/<kernel>_<isa>.S starts with   #include "../asm.h"
 * and wraps each function in                          kolmo_function NAME ... kolmo_endfunc NAME
 * and ends with                                       kolmo_asm_end
 *
 * The .S files are run through the C preprocessor by the C compiler (cc -c x.S), so the same file builds with gcc
 * and clang on Linux (ELF) and with Apple clang on macOS (Mach-O). Differences handled here:
 *   - symbol names: Mach-O prefixes C symbols with an underscore; ELF does not;
 *   - visibility: kernels are internal to libkolmo (.hidden on ELF, .private_extern on Mach-O);
 *   - ELF only: .type/.size, the non-executable-stack note, and the GNU property note that marks the object as
 *     compatible with Intel CET (IBT, shadow stack) or Arm BTI/PAC when the compiler targets them
 *     (-fcf-protection, -mbranch-protection): one object without it would turn the protection off for the whole
 *     binary;
 *   - landing pads: endbr64 (x86-64, CET IBT) or "bti c" (AArch64 BTI) at every function entry when enabled.
 * Statements are put in .macro bodies, not in multi-statement #defines: ';' separates statements in GNU as but
 * starts a comment in Apple's arm64 assembler.
 *
 * Calling convention: System V AMD64 (args rdi, rsi, rdx, rcx, r8, r9; scratch rax, rcx, rdx, rsi, rdi, r8-r11;
 * callee-saved rbx, rbp, r12-r15) and AAPCS64 (args x0-x7; scratch x0-x17; callee-saved x19-x28, d8-d15). */
#ifndef KOLMO_ARCH_ASM_H
#define KOLMO_ARCH_ASM_H

#if !defined(__ASSEMBLER__)
#error "src/arch/asm.h is for .S files"
#endif

/* landing pad at function entry */
#if defined(__x86_64__) && defined(__CET__) && (__CET__ & 1)
#define KOLMO_LANDING endbr64
#elif defined(__aarch64__) && defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT
#define KOLMO_LANDING hint 34                    /* bti c */
#else
#define KOLMO_LANDING
#endif

#if defined(__APPLE__)

.macro kolmo_function name
    .text
    .p2align 4
    .globl _\name
    .private_extern _\name
_\name:
    KOLMO_LANDING
.endm
.macro kolmo_endfunc name
.endm
.macro kolmo_asm_end
.endm

#else /* ELF */

.macro kolmo_function name
    .text
    .p2align 4
    .globl \name
    .hidden \name
    .type \name, %function
\name:
    KOLMO_LANDING
.endm
.macro kolmo_endfunc name
    .size \name, . - \name
.endm

/* GNU property note: x86 feature bits from __CET__ (1 IBT, 2 shadow stack); AArch64 BTI (1) and PAC (2) */
#if defined(__x86_64__) && defined(__CET__)
#define KOLMO_PROP_TYPE 0xc0000002
#define KOLMO_PROP_BITS (__CET__ & 3)
#elif defined(__aarch64__) && ((defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT) || \
                               (defined(__ARM_FEATURE_PAC_DEFAULT) && __ARM_FEATURE_PAC_DEFAULT))
#define KOLMO_PROP_TYPE 0xc0000000
#if defined(__ARM_FEATURE_BTI_DEFAULT) && __ARM_FEATURE_BTI_DEFAULT
#define KOLMO_PROP_BTI 1
#else
#define KOLMO_PROP_BTI 0
#endif
#if defined(__ARM_FEATURE_PAC_DEFAULT) && __ARM_FEATURE_PAC_DEFAULT
#define KOLMO_PROP_PAC 2
#else
#define KOLMO_PROP_PAC 0
#endif
#define KOLMO_PROP_BITS (KOLMO_PROP_BTI | KOLMO_PROP_PAC)
#endif

.macro kolmo_asm_end
    .section .note.GNU-stack, "", %progbits
#if defined(KOLMO_PROP_TYPE)
    .pushsection .note.gnu.property, "a"
    .p2align 3
    .long 4                                      /* name size: "GNU\0" */
    .long 16                                     /* descriptor size */
    .long 5                                      /* NT_GNU_PROPERTY_TYPE_0 */
    .asciz "GNU"
    .long KOLMO_PROP_TYPE                        /* GNU_PROPERTY_{X86,AARCH64}_FEATURE_1_AND */
    .long 4
    .long KOLMO_PROP_BITS
    .long 0                                      /* padding to 8 bytes */
    .popsection
#endif
.endm

#endif /* ELF */

#endif /* KOLMO_ARCH_ASM_H */
