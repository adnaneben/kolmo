#!/usr/bin/env python3
"""Build E03 in isolation; frozen production and E02C/E02D files are never edited."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
REFERENCE = '91c4cdaaca57d0bb667a059341dbf25fd5a8df71'
FROZEN = ['src', 'vendor', 'Makefile', 'benchmark/accept/TARGETS.md',
          'research/codec/E02C', 'research/codec/E02D']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cc', default='gcc')
    p.add_argument('--sanitize', action='store_true')
    a = p.parse_args()
    if sys.platform != 'linux':
        p.error('Linux-specific measurements; M5 evaluation is a separate task')
    subprocess.run(['git', 'diff', '--exit-code', REFERENCE, '--', *FROZEN], cwd=ROOT, check=True)
    spec = importlib.util.spec_from_file_location('e02c_build', HERE.parent / 'E02C/build.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    out = a.output.resolve()
    core, meta = module.stage(ROOT, out)
    subprocess.run(['make', '-j4', 'ARCH=', 'build/libvendor.a'], cwd=ROOT, check=True)
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if a.sanitize else ['-O3']
    command = [a.cc, *flags, '-std=gnu11', '-ffp-contract=off', '-DXXH_NAMESPACE=ZSTD_',
               '-DZSTD_MULTITHREAD', '-DZSTD_LEGACY_SUPPORT=0', f'-DE02C_CORE="{core}"',
               '-Isrc', '-Ivendor/zstd', '-Ivendor/lz4', '-Ivendor/libbsc', '-Ivendor/brotli/include',
               str(HERE / 'probe.c'), 'build/libvendor.a', '-llzma', '-lpthread', '-lm', '-lstdc++',
               '-o', str(out / 'probe')]
    meta.update(command=command, reference=REFERENCE, sanitized=a.sanitize,
                compiler=subprocess.check_output([a.cc, '--version'], text=True),
                research_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip())
    (out / 'build.json').write_text(json.dumps(meta, indent=2) + '\n')
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(out / 'probe'), 'test'], check=True, cwd=ROOT,
                   env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                            UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    subprocess.run(['git', 'diff', '--exit-code', REFERENCE, '--', *FROZEN], cwd=ROOT, check=True)
    print('E03_BUILD_AND_SELFTEST_PASSED_NOT_RELEASE_ACCEPTANCE')


if __name__ == '__main__':
    main()
