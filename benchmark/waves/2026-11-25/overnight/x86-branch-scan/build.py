"""Build an isolated candidate that finds the first/last set NEON nibble directly."""
from pathlib import Path
import shutil,subprocess,sys,difflib
repo=Path(sys.argv[1]).resolve();work=Path(sys.argv[2]).resolve()
work.mkdir(parents=True,exist_ok=True);s=work/'src';shutil.copytree(repo/'src',s,dirs_exist_ok=True)
p=s/'transforms/x86.c';original=p.read_text()
start=original.index('    uint8x16_t v = vld1q_u8(b)')
end=original.index('    uint32_t m = 0; while (m4)',start)
fragment=original[start:end]
helper=fragment.replace('    uint64_t m4 = ', '    return ')
mark='static inline uint32_t e8_mask16('
code=original.replace(mark,'''#if !defined(__SSE2__) && defined(__ARM_NEON) && defined(__aarch64__)
/* Each candidate byte occupies four consecutive bits. */
static inline uint64_t e8_neon_nibbles(const uint8_t *b) {
'''+helper+'''}
#endif
'''+mark)
code=code.replace(fragment,'    uint64_t m4 = e8_neon_nibbles(b);\n')
old='''            uint32_t m = e8_mask16(b + i);
            if (!m) { i += 16; continue; }
            i += (size_t)__builtin_ctz(m);'''
assert code.count(old)==1
code=code.replace(old,'''#if !defined(__SSE2__) && defined(__ARM_NEON) && defined(__aarch64__)
            uint64_t m = e8_neon_nibbles(b + i);
            if (!m) { i += 16; continue; }
            i += (size_t)(__builtin_ctzll(m) / 4);
#else
'''+old+'\n#endif')
p.write_text(code)
(work/'candidate.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),code.splitlines(True),fromfile='a/src/transforms/x86.c',tofile='b/src/transforms/x86.c')))
flags=['cc','-O3','-mcpu=native','-ffp-contract=off','-Wno-unknown-attributes','-Wno-constant-logical-operand']
args=['-I'+str(s),s/'cli/cli.c',s/'cli/features.c',s/'kolmo.c',repo/'build/libkolmoarch.a','-lpthread','-lm','-Wl,-dead_strip','-o',work/'kolmo']
micro=Path(__file__).with_name('micro.c')
if micro.is_file():
    subprocess.run(flags+['-DBASELINE="'+str(repo/'src/transforms/x86.c')+'"','-DCANDIDATE="'+str(p)+'"',micro,'-o',work/'micro'],check=True)
subprocess.run(flags+args,check=True)
