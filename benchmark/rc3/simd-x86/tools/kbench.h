#include <time.h>
#include <stdio.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
/* fastest of R runs, plus the median, in ns per unit */
#define BENCH(R, units, best, med, stmt) do { double _t[64]; int _r = (R) > 64 ? 64 : (R); \
    for (int _i = 0; _i < _r; _i++) { double _a = now(); stmt; _t[_i] = (now() - _a) * 1e9 / (units); } \
    for (int _i = 1; _i < _r; _i++) for (int _j = _i; _j > 0 && _t[_j] < _t[_j - 1]; _j--) { double _x = _t[_j]; _t[_j] = _t[_j - 1]; _t[_j - 1] = _x; } \
    best = _t[0]; med = _t[_r / 2]; } while (0)
