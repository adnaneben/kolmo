import re, sys, subprocess
p = sys.argv[1]; s = open(p).read()
old = '''        #pragma omp critical(libsais_unbwt_take)
        {
            value = *counter; *counter = value + count;
        }'''
assert old in s
s = s.replace(old, '''        value = __atomic_fetch_add(counter, count, __ATOMIC_RELAXED);     /* was: omp critical */''')
old = re.search(r'#if defined\(LIBSAIS_OPENMP\)\n\s*#pragma omp parallel num_threads\(1\)\n#endif\n', s).group(0)
s = s.replace(old, '')
open(p, 'w').write(s)
# drop declarations the converted loops made unused
for _ in range(3):
    out = subprocess.run(['gcc', '-O1', '-Wall', '-c', p, '-o', '/dev/null'], capture_output=True, text=True).stderr
    lines = sorted({int(m.group(1)) for m in re.finditer(r"sais.c:(\d+):\d+: warning: unused variable .c.", out)}, reverse=True)
    if not lines: break
    L = s.split('\n')
    for ln in lines:
        assert re.match(r'^\s*fast_sint_t c;\s*$', L[ln - 1]), L[ln - 1]
        del L[ln - 1]
        if L[ln - 1].strip() == '' and L[ln - 2].strip() == '': del L[ln - 1]
    s = '\n'.join(L); open(p, 'w').write(s)
