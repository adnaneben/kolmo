/* Print the same public Ultra preset and internal block/cap calculation used by compress_impl. */
#include "kolmo.c"
int main(void) {
    kolmo_params p = {0};
    if (kolmo_params_for_level(KOLMO_LEVEL_ULTRA, &p)) return 2;
    int codec, level;
    mode_codec(&p, &codec, &level);
    uint32_t nominal = kolmo_default_block(&p, 63u << 20);
    printf("unconstrained=%u cap_256_MiB=%u\n", nominal,
           kmem_block_for_cap(p.mode, codec, level, nominal, 256ull << 20));
    return 0;
}
