/* libFuzzer target: kolmo_decompress_dict with a fixed small dictionary (campaign 1's fuzz/campaign/dict.bin on claude/rc2-fuzz-20261115, embedded via
 * fuzz/fuzz_dict_data.h). Decodes with the right dictionary, with a wrong one and with none; never trusts a claimed
 * size beyond 16 MiB. */
#include "kolmo.h"
#include "fuzz_dict_data.h"
#include <stdlib.h>
#include <string.h>
#define CAP (16u << 20)
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4 || size > CAP) return 0;
    uint8_t small[1 << 16];
    kolmo_decompress_dict(data, size, small, sizeof small, 1, fuzz_dict, sizeof fuzz_dict);
    int64_t orig = kolmo_original_size(data, size);
    if (orig < 0 || orig > (int64_t)CAP) return 0;
    uint8_t *o = malloc((size_t)orig + 1); if (!o) return 0;
    int64_t r = kolmo_decompress_dict(data, size, o, (size_t)orig, 1 + (int)(size & 1), fuzz_dict, sizeof fuzz_dict);
    if (r >= 0 && r != orig) __builtin_trap();
    kolmo_decompress_dict(data, size, o, (size_t)orig, 1, fuzz_dict, sizeof fuzz_dict - 1);  /* wrong dictionary */
    free(o);
    return 0;
}
