#include <stdint.h>
#include <stdio.h>

int main(void) {
    uint64_t checked = 0;
    for (unsigned k = 1; k <= 16; ++k) {
        const uint64_t reciprocal = ((UINT64_C(1) << 32) + k - 1) / k;
        for (uint32_t hash = 0; hash < (1u << 20); ++hash) {
            const unsigned r = hash - (uint32_t)((hash * reciprocal) >> 32) * k;
            if (r != hash % k) return 1;
            ++checked;
        }
    }
    printf("%llu matching remainder calculations\n", (unsigned long long)checked);
}
