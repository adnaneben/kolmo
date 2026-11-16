/* Byte-plane split and merge, SSE2 (x86-64 baseline). Apache-2.0, Kolmo contributors. */
#ifndef KOLMO_NO_ASM
#include "arch/x86_64/kernels_x86.h"
#define K_TGT
#define K_SFX sse2
#include "arch/x86_64/planes_tmpl.h"
#endif
