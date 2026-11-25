/* R28 prototype only: WASI without the exception-handling proposal has no setjmp; GR6 (codec 6) error unwinding traps */
#pragma once
typedef int jmp_buf[1];
#define setjmp(b) 0
static inline _Noreturn void longjmp(jmp_buf b, int v) { (void)b; (void)v; __builtin_trap(); }
