"""Build an isolated candidate that finds the first/last set NEON nibble directly."""
from pathlib import Path
import shutil,subprocess,sys,difflib
repo=Path(sys.argv[1]).resolve();work=Path(sys.argv[2]).resolve()
work.mkdir(parents=True,exist_ok=True);s=work/'src';shutil.copytree(repo/'src',s,dirs_exist_ok=True)
p=s/'transforms/x86.c';original=p.read_text()
code=original.replace('static inline uint32_t e8_mask16(','''#if !defined(__SSE2__) && defined(__ARM_NEON) && defined(__aarch64__)
typedef uint64_t e8_mask_t;
static inline int e8_first(e8_mask_t m) { return __builtin_ctzll(m) / 4; }
static inline int e8_last(e8_mask_t m) { return (63 - __builtin_clzll(m)) / 4; }
#else
typedef uint32_t e8_mask_t;
static inline int e8_first(e8_mask_t m) { return __builtin_ctz(m); }
static inline int e8_last(e8_mask_t m) { return 31 - __builtin_clz(m); }
#endif
static inline e8_mask_t e8_mask16(''')
old='''    uint64_t m4 = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(c), 4)), 0);
    uint32_t m = 0; while (m4) { int t = __builtin_ctzll(m4); m |= 1u << (t >> 2); m4 &= ~(0xFull << (t & ~3)); }
    return m;'''
assert code.count(old)==1
code=code.replace(old,'''    /* Four adjacent bits represent each matching byte lane. */
    return vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(c), 4)), 0);''')
code=code.replace('uint32_t m = e8_mask16','e8_mask_t m = e8_mask16').replace('int t = 31 - __builtin_clz(m); size_t k','int t = e8_last(m); size_t k').replace('i += (size_t)__builtin_ctz(m);','i += (size_t)e8_first(m);')
p.write_text(code)
(work/'candidate.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),code.splitlines(True),fromfile='a/src/transforms/x86.c',tofile='b/src/transforms/x86.c')))
flags=['cc','-O3','-mcpu=native','-ffp-contract=off','-Wno-unknown-attributes','-Wno-constant-logical-operand']
args=['-I'+str(s),s/'cli/cli.c',s/'cli/features.c',s/'kolmo.c',repo/'build/libkolmoarch.a','-lpthread','-lm','-Wl,-dead_strip','-o',work/'kolmo']
micro=Path(__file__).resolve().parent.parent/'micro.c'
if micro.is_file():
    subprocess.run(flags+['-DBASELINE="'+str(repo/'src/transforms/x86.c')+'"','-DCANDIDATE="'+str(p)+'"',micro,'-o',work/'micro'],check=True)
subprocess.run(flags+args,check=True)
