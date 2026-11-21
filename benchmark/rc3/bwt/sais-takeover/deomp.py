#!/usr/bin/env python3
"""Turn libsais's OpenMP parallel regions into functions run by Kolmo's crew (src/algo/par.c).

Each `#pragma omp parallel` region becomes a static function taking a pointer to a struct that carries the enclosing
function's parameters by value and, for names given in REFS, pointers to its locals (OpenMP shares them). The
region keeps libsais's own code: omp_get_thread_num()/omp_get_num_threads() read thread-local values set by the
crew, `#pragma omp barrier` becomes libsais_omp_barrier(), `#pragma omp master` an if on thread 0."""
import re, sys

path = sys.argv[1]
REFS = {}   # function name -> list of local names captured by reference; filled from the command line as f=a,b
for a in sys.argv[2:]:
    f, names = a.split('=', 1)
    REFS[f] = [tuple(x.split(':', 1)) for x in names.split(';') if x]   # name:type

L = open(path).read().split('\n')

def find_func_start(i):
    j = i
    while j >= 0:
        if re.match(r'^(static\s+)?[A-Za-z_][\w \*]*\b(\w+)\s*\(', L[j]) and not L[j].rstrip().endswith(';'):
            return j
        j -= 1
    raise Exception('no function for line %d' % i)

def parse_params(sig):
    inner = sig[sig.index('(') + 1: sig.rindex(')')]
    out = []
    for p in inner.split(','):
        p = p.strip()
        if p == 'void' or not p: continue
        m = re.match(r'^(.*?)(\w+)$', p)
        out.append((m.group(1).strip(), m.group(2)))
    return out

def match_brace(start_line):
    depth = 0
    for j in range(start_line, len(L)):
        for ch in L[j]:
            if ch == '{': depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0: return j
    raise Exception('unbalanced from %d' % start_line)

out_regions = []   # (insert_before_line, text)
counter = {}
i = 0
while i < len(L):
    if re.match(r'^\s*#pragma omp parallel for', L[i]) and L[i - 1].strip() == '#if defined(LIBSAIS_OPENMP)':
        # `parallel for schedule(static, C)`: a region in which thread t runs the chunks t, t + T, t + 2T, ...
        prag = L[i].strip()
        chunk = re.search(r'schedule\(static,\s*(\d+)\)', prag).group(1)
        e = i + 1
        endif = None
        while not L[e].strip().startswith('for'):
            if L[e].strip() == '#endif': endif = e
            e += 1
        var = re.match(r'\s*for\s*\(\s*(\w+)\s*=', L[e]).group(1)
        assert L[e + 1].strip() == '{', L[e + 1]
        b1 = match_brace(e + 1)
        loop = L[e:b1 + 1]
        loop = [loop[0], loop[1], '            if ((kolmo_iter++ / %s) %% omp_get_num_threads() != omp_get_thread_num()) { continue; }' % chunk] + loop[2:]
        pre = L[i + 1:e]           # the #else / UNUSED / #endif lines stay where they were
        L[i:b1 + 1] = [prag.replace(' for schedule(static, %s)' % chunk, ''), '#endif'] + \
            ['    {', '        fast_sint_t %s, kolmo_iter = 0;' % var] + ['    ' + x for x in loop] + ['    }']
        continue
    if re.match(r'^\s*#pragma omp parallel(?! for)', L[i]):
        prag = L[i].strip()
        if L[i - 2].strip() == '#if defined(LIBSAIS_OPENMP)' and re.match(r'^\s*(sa_sint_t|fast_sint_t)\s+\w+\s*=.*;\s*$', L[i - 1]):
            L[i - 2], L[i - 1] = L[i - 1], L[i - 2]          # the thread-count declaration moves out of the #if
        bare = L[i + 1].strip() == '{'
        if not bare and not (L[i - 1].strip() == '#if defined(LIBSAIS_OPENMP)' and L[i + 1].strip() == '#endif' and L[i + 2].strip() == '{'):
            print('MANUAL', i + 1, L[i].strip()); i += 1; continue
        fs = find_func_start(i)
        fname = re.search(r'(\w+)\s*\(', L[fs]).group(1)
        params = parse_params(L[fs])
        k = counter.get(fname, 0); counter[fname] = k + 1
        rname = '%s_region%d' % (fname, k)
        b0 = i + 1 if bare else i + 2
        b1 = match_brace(b0)
        body = L[b0:b1 + 1]
        nt = re.search(r'num_threads\(([^)]*\)?)\)', prag).group(1)
        cond = re.search(r'\bif\((.*)\)\s*$', prag)
        cond = cond.group(1) if cond else '1'
        refs = REFS.get('%s#%d' % (fname, k), REFS.get(fname, []))
        btxt = '\n'.join(body)
        for r, _ in refs:
            btxt = re.sub(r'(?<![.\w])(?<!->)\b%s\b' % re.escape(r), '(*kolmo_ref_%s)' % r, btxt)
        fields = ['    %s %s;' % (t, n) for t, n in params] + ['    void * kolmo_ref_%s;' % r for r, _ in refs]
        unpack = ['    %s %s = kolmo_a->%s; (void)%s;' % (t.replace('const ', 'const ') , n, n, n) for t, n in params]
        for r, t in refs:
            if t.endswith(']'):
                el, dims = t[:t.index('[')], t[t.index('['):]
                unpack.append('    %s (* kolmo_ref_%s)%s = kolmo_a->kolmo_ref_%s;' % (el, r, dims, r))
            else:
                unpack.append('    %s * kolmo_ref_%s = kolmo_a->kolmo_ref_%s;' % (t, r, r))
        reg = ['typedef struct %s_args' % rname, '{'] + (fields or ['    int unused;']) + ['} %s_args;' % rname, '',
               'static void %s(void * kolmo_args)' % rname, '{', '    %s_args * kolmo_a = (%s_args *)kolmo_args;' % (rname, rname)] + unpack
        reg += btxt.split('\n') + ['}', '']
        out_regions.append((fs, reg))
        init = ', '.join([n for t, n in params] + ['(void *)&%s' % r for r, _ in refs]) or '0'
        call = ['    {', '        %s_args kolmo_args = { %s };' % (rname, init),
                '        libsais_omp_parallel(%s, &kolmo_args, (%s) ? (%s) : 1);' % (rname, cond, nt), '    }']
        a0 = i if bare else i - 1
        L[a0:b1 + 1] = call
        i = a0 + len(call)
        continue
    i += 1

# insert region functions before their enclosing functions (from the bottom up)
for fs, reg in sorted(out_regions, key=lambda x: -x[0]):
    L[fs:fs] = reg

s = '\n'.join(L)
s = re.sub(r'^\s*#pragma omp barrier\s*$', '        libsais_omp_barrier();', s, flags=re.M)
s = re.sub(r'^(\s*)#pragma omp master\s*$', r'\1if (omp_get_thread_num() == 0)', s, flags=re.M)
s = s.replace('omp_get_dynamic() == 0', '1')
open(path, 'w').write(s)
print('regions', sum(counter.values()), 'functions', len(counter))
