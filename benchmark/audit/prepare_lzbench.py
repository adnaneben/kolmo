#!/usr/bin/env python3
"""Add a local Kolmo adapter to a clean lzbench checkout, without changing Kolmo.

Usage: python3 benchmark/audit/prepare_lzbench.py /tmp/kolmo-lzbench-audit
Run from the Kolmo repository after `make lib`.
"""
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parents[2]
checkout = pathlib.Path(sys.argv[1]).resolve()
marker = '// Local Kolmo audit adapter'
header = checkout / 'bench/lzbench.h'
if marker in header.read_text():
    raise SystemExit('Adapter already installed; use a clean checkout.')
shared = root / 'build/libkolmo.dylib'
if not shared.exists():
    raise SystemExit('Build the macOS shared library with make lib first.')
(checkout / 'mk/kolmo.mk').write_text(f'''CODECS += KOLMO
KOLMO_OBJS := bench/kolmo_codecs.o
KOLMO_FLAGS := -I{root}/src
ifneq ($(DONT_BUILD_KOLMO),1)
LDFLAGS += {shared}
endif
''')
(checkout / 'bench/kolmo_codecs.cpp').write_text('''#include "codecs.h"
#include "czn.h"
int64_t lzbench_kolmo_compress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
    czn_params p = {(czn_mode)o->level, 0, o->threads > 0 ? o->threads : 1, (uint32_t)o->additional_param, 1};
    return czn_compress((const uint8_t *)in, n, (uint8_t *)out, cap, &p);
}
int64_t lzbench_kolmo_decompress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
    return czn_decompress((const uint8_t *)in, n, (uint8_t *)out, cap, o->threads > 0 ? o->threads : 1);
}
''')
f = checkout / 'bench/codecs.h'
s = f.read_text()
s += '''\n#ifndef BENCH_REMOVE_KOLMO
int64_t lzbench_kolmo_compress(char*, size_t, char*, size_t, codec_options_t*);
int64_t lzbench_kolmo_decompress(char*, size_t, char*, size_t, codec_options_t*);
#endif
'''
f.write_text(s)
s = header.read_text()
needle = 'static const compressor_desc_t comp_desc[] =\n{'
assert needle in s
s = s.replace(needle, needle + '''
// Local Kolmo audit adapter
#ifndef BENCH_REMOVE_KOLMO
    { "kolmo", "kolmo audit", "structure transforms + hybrid codecs", 0, 5, 0, INTERNAL_MT, lzbench_kolmo_compress, lzbench_kolmo_decompress, NULL, NULL },
    { "kolmo1m", "kolmo 1MiB", "structure transforms + hybrid codecs", 0, 5, 1048576, INTERNAL_MT, lzbench_kolmo_compress, lzbench_kolmo_decompress, NULL, NULL },
#endif''')
header.write_text(s)
print(f'Installed adapter in {checkout}; modes 0..5 = rt, fast, eco, hc, max, ultra.')
print('Use -I for internal Kolmo threads. External concurrent calls are deliberately disabled.')
