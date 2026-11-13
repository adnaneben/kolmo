#!/usr/bin/env python3
"""Stage a research-only singleton decoder; never edit production files."""
from __future__ import annotations
import argparse, hashlib, json, os, platform, subprocess, time
from pathlib import Path
CORE = 'e06467c8872d494b97270811bedf6fd0bac2aa31'
GRAPH = '91ad067dc583b9d9e6f060f214a0f7c402bb4a31'
def blob(b: bytes) -> str:
    return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def sha(p: Path) -> str: return hashlib.sha256(p.read_bytes()).hexdigest()
def stage(source: Path, output: Path) -> dict:
    original=(source/'src/kolmo.c').read_bytes()
    if blob(original)!=CORE or blob((source/'src/kgraph.inc').read_bytes())!=GRAPH:
        raise ValueError('Source identity differs from the reviewed pin')
    text=original.decode(); begin=text.index('static int rans_decompress_all(')
    end=text.index('static size_t codec_compress(', begin)
    old=text[begin:end]
    # Count the full-scale entry during the existing parse. A valid summed table
    # permits exactly one such entry. Keep the original allocation/error paths.
    new=old.replace('uint32_t f[256], c[257]; c[0] = 0;',
                    'uint32_t f[256], c[257]; c[0] = 0; int singleton = -1;', 1)
    oldline='if (v) { f[k] = (uint32_t)v; c[k + 1] = c[k] + f[k];'
    if new.count(oldline)!=1: raise ValueError('Frequency anchor changed')
    new=new.replace(oldline, 'if (v) { if (v == RANS_M) singleton = k; f[k] = (uint32_t)v; c[k + 1] = c[k] + f[k];',1)
    fill='        for (int k = 0; k < 256; k++) if (f[k]) memset(sym + c[k], k, f[k]);\n'
    if new.count(fill)!=1: raise ValueError('Table fill anchor changed')
    new=new.replace(fill,'',1)
    at='        uint8_t *w = out + o; uint64_t x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];'
    if new.count(at)!=1: raise ValueError('State anchor changed')
    shortcut='''        /* R4 research variant: full-scale probability leaves all states unchanged.
         * Validate the SAME terminal states and unused-byte condition before output. */
        if (singleton >= 0) {
            if (z != 32) goto bad;
            for (int k = 0; k < 4; k++) if (x[k] != R64_L) goto bad;
            memset(out + o, singleton, (size_t)m);
            i += (size_t)z; o += (size_t)m;
            continue;
        }
'''+fill+at
    new=new.replace(at,shortcut,1)
    output.mkdir(parents=True,exist_ok=True)
    (output/'pinned_core.c').write_bytes(original)
    # Same direct allocation hook in both staged variants; production untouched.
    hook_old = old.replace('malloc(RANS_M)', 'r4_malloc(RANS_M)', 1)
    hook_new = new.replace('malloc(RANS_M)', 'r4_malloc(RANS_M)', 1)
    (output/'original_core.c').write_text(text[:begin]+hook_old+text[end:])
    (output/'candidate_core.c').write_text(text[:begin]+hook_new+text[end:])
    (output/'reference.inc').write_text(old.replace('rans_decompress_all(', 'r4_reference_rans(',1))
    import difflib
    (output/'candidate.diff').write_text(''.join(difflib.unified_diff(text.splitlines(True),(text[:begin]+new+text[end:]).splitlines(True),fromfile='pinned/src/kolmo.c',tofile='research-only/candidate_core.c')))
    return {'core_blob':CORE,'graph_blob':GRAPH,'original_rans_sha256':hashlib.sha256(old.encode()).hexdigest(),'generated':{name:sha(output/name) for name in ('pinned_core.c','original_core.c','candidate_core.c','reference.inc','candidate.diff')}}
def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--variant',choices=['base','fast','asan'],required=True)
    args=parser.parse_args(); root=args.source.resolve(); out=args.output.resolve()
    receipt=stage(root,out); here=Path(__file__).resolve().parent
    include=[f'-I{root/part}' for part in ('src','vendor/zstd','vendor/lz4','vendor/libbsc','vendor/brotli/include')]
    cc='clang' if args.variant=='asan' else 'gcc'
    opt=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.variant=='asan' else ['-O2','-fno-plt']
    core=out/('original_core.c' if args.variant=='base' else 'candidate_core.c')
    exe=out/f'probe-{args.variant}'
    cmd=[cc,*opt,'-std=gnu11','-ffp-contract=off','-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0',*include,
         f'-DR4_CORE="{core}"',f'-DR4_REFERENCE="{out}/reference.inc"',here/'probe.c',root/'build/libvendor.a','-llzma','-lpthread','-lm','-lstdc++','-o',exe]
    started=time.monotonic()
    result=subprocess.run(list(map(str,cmd)),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
    log=out/f'{args.variant}-build.log';log.write_bytes(result.stdout)
    receipt.update(variant=args.variant,command=list(map(str,cmd)),returncode=result.returncode,seconds=time.monotonic()-started,
                   platform=platform.platform(),compiler=subprocess.check_output([cc,'--version'],text=True),research_sha256={p.name:sha(p) for p in here.glob('*') if p.is_file()},
                   vendor_archive_sha256=sha(root/'build/libvendor.a'),sanitizer_scope='full core and harness; vendor archive NOT instrumented')
    if result.returncode==0: receipt['binary_sha256']=sha(exe)
    (out/f'{args.variant}-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    if blob((root/'src/kolmo.c').read_bytes())!=CORE: raise RuntimeError('Production source changed')
    if result.returncode: raise SystemExit(result.returncode)
    print(exe)
if __name__=='__main__': main()
