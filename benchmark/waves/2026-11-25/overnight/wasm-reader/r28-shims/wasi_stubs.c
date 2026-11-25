/* R28 prototype: POSIX calls WASI preview 1 lacks, as no-ops (single-threaded build; no fork; advice ignored). */
#include <stddef.h>
int pthread_atfork(void (*a)(void), void (*b)(void), void (*c)(void)) { (void)a; (void)b; (void)c; return 0; }
int madvise(void *p, size_t n, int adv) { (void)p; (void)n; (void)adv; return 0; }
