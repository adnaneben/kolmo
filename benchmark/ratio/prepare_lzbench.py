#!/usr/bin/env python3
"""Install standalone codecs in the pinned local audit checkout; no upstream writes."""
import pathlib
import shutil
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
checkout = pathlib.Path(sys.argv[1]).resolve()
revision = subprocess.check_output(['git', '-C', str(checkout), 'rev-parse', 'HEAD'], text=True).strip()
assert revision == 'd68cdbc2309408e82da881c61d23cf70cca8db09'
if '{ "kolmo",' not in (checkout/'bench/lzbench.h').read_text():
    subprocess.run([sys.executable, str(root/'benchmark/audit/prepare_lzbench.py'), str(checkout)], check=True)
sources = checkout/'bench/independent'
sources.mkdir(exist_ok=True)
names = ['krc.c', 'krc.h']
if (root/'src/experimental/kbc.c').exists():
    names += ['kbc.c', 'kbc.h']
for name in names:
    shutil.copy2(root/'src/experimental'/name, sources/name)
has_kbc = 'kbc.c' in names
(checkout/'mk/independent.mk').write_text('CODECS += INDEPENDENT\nINDEPENDENT_FLAGS := -Ibench/independent\nINDEPENDENT_OBJS := bench/independent_codecs.o bench/independent/krc.o' + (' bench/independent/kbc.o' if has_kbc else '') + '\n')
adapter = '''#include "codecs.h"
#include "independent/krc.h"
int64_t lzbench_krc_compress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
 return krc_compress((const uint8_t*)in,n,(uint8_t*)out,cap,o->threads>0?o->threads:1);
}
int64_t lzbench_krc_decompress(char *in, size_t n, char *out, size_t cap, codec_options_t *) {
 return krc_decompress((const uint8_t*)in,n,(uint8_t*)out,cap);
}
'''
if has_kbc:
    adapter += '''#include "independent/kbc.h"
int64_t lzbench_kbc_compress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
 return kbc_compress((const uint8_t*)in,n,(uint8_t*)out,cap,o->threads>0?o->threads:1);
}
int64_t lzbench_kbc_decompress(char *in, size_t n, char *out, size_t cap, codec_options_t *o) {
 return kbc_decompress((const uint8_t*)in,n,(uint8_t*)out,cap,o->threads>0?o->threads:1);
}
'''
(checkout/'bench/independent_codecs.cpp').write_text(adapter)
header = checkout/'bench/codecs.h'
text = header.read_text()
for codec in (['krc','kbc'] if has_kbc else ['krc']):
    if f'int64_t lzbench_{codec}_compress(' not in text:
        text += f'\nint64_t lzbench_{codec}_compress(char*, size_t, char*, size_t, codec_options_t*);\nint64_t lzbench_{codec}_decompress(char*, size_t, char*, size_t, codec_options_t*);\n'
header.write_text(text)
header = checkout/'bench/lzbench.h'
text = header.read_text()
for codec, desc in ([('krc','independent KRC1'),('kbc','independent KBC1')] if has_kbc else [('krc','independent KRC1')]):
    if f'{{ "{codec}",' not in text:
        entry = f'\n#ifndef BENCH_REMOVE_INDEPENDENT\n    {{ "{codec}", "{desc}", "independent codec", 0, 0, 0, FULL_THREADING, lzbench_{codec}_compress, lzbench_{codec}_decompress, NULL, NULL }},\n#endif\n'
        needle = 'static const compressor_desc_t comp_desc[] =\n{'
        assert needle in text
        text = text.replace(needle, needle+entry)
header.write_text(text)
print('Installed independent source adapters:', ', '.join(names), 'at', revision)
