/* Byte histogram over four tables: one table stalls on runs of the same byte (each count waits for the last).
 * kolmo_hist_c is the portable reference of the assembly kernels src/arch/<isa>/hist_<isa>.S. */
static void kolmo_hist_c(const uint8_t *p, size_t n, uint32_t hc[256]) {
    uint32_t h1[256] = {0}, h2[256] = {0}, h3[256] = {0}; size_t i = 0;
    memset(hc, 0, 256 * sizeof(uint32_t));
    for (; i + 4 <= n; i += 4) { hc[p[i]]++; h1[p[i + 1]]++; h2[p[i + 2]]++; h3[p[i + 3]]++; }
    for (; i < n; i++) hc[p[i]]++;
    for (int c = 0; c < 256; c++) hc[c] += h1[c] + h2[c] + h3[c];
}

/* Dispatch: the example of the assembly pattern (docs/LAYOUT.md). Both kernels use the baseline ISA only, so they
 * need no CPUID or HWCAP test; a kernel that needs an extension tests it here once (kolmo_have_avx2(), arch/cpu.h).
 * x86-64: chosen for n >= 16 KiB (benchmark/rc3/structure/RESULTS.md has the measurement). AArch64: compiled and tested, chosen only
 * with -DKOLMO_HIST_AARCH64 until measured on Arm hardware. -DKOLMO_NO_ASM (make NO_ASM=1): the reference only. */
#if !defined(KOLMO_NO_ASM) && defined(__x86_64__)
void kolmo_hist_x86_64(const uint8_t *p, size_t n, uint32_t hc[256]);       /* src/arch/x86_64/hist_x86_64.S */
#define KOLMO_HIST_KERNEL kolmo_hist_x86_64
#elif !defined(KOLMO_NO_ASM) && defined(__aarch64__)
void kolmo_hist_aarch64(const uint8_t *p, size_t n, uint32_t hc[256]);      /* src/arch/aarch64/hist_aarch64.S */
#if defined(KOLMO_HIST_AARCH64)
#define KOLMO_HIST_KERNEL kolmo_hist_aarch64
#endif
#endif
/* The kernel clears and sums eight tables (8 KiB), the reference four: below 16 KiB the reference is as fast or
 * faster (hist_bench), so short inputs stay with it. kg_hist is kept out of line: inlined, the larger body changed
 * GCC's inlining choices in its callers and cost 10% on CSV at -m fast, where kg_hist is never called. */
#ifndef KOLMO_HIST_KERNEL_MIN
#define KOLMO_HIST_KERNEL_MIN 16384
#endif
__attribute__((noinline)) static void kg_hist(const uint8_t *p, size_t n, uint32_t hc[256]) {
#if defined(KOLMO_HIST_KERNEL)
    if (n >= KOLMO_HIST_KERNEL_MIN) { KOLMO_HIST_KERNEL(p, n, hc); return; }
#endif
    kolmo_hist_c(p, n, hc);
}
