#!/usr/bin/env python3
"""Reproduce locally tested RC sources; refuse drift, bad payloads or extra paths."""
import base64, gzip, hashlib, json, pathlib, subprocess
ROOT = pathlib.Path(__file__).resolve().parent
BASE = '64488d89e81ed097b3efe8d48c57328b3e9e7e7c'
PR9_BASE = 'a64f57fdeb742f4bf57d44c8cfc994ebf48ae121'
PR9 = 'e31b3fe3a278c657e79682eaea440904252d28aa'
GZ = '626c6a59334a5d1566915d879131e1e38df7597e62b750fb2423c8f3ad0cb57a'
PATCH = '35eb79a22d490df42a80a977ab0e35bccc3e3e579eb10119fab06ce6192d2424'
def run(*args, data=None):
    return subprocess.run(args, input=data, check=True, stdout=subprocess.PIPE).stdout

def main():
    if run('git','status','--porcelain').strip():
        raise SystemExit('Refuse non-clean checkout')
    run('git','diff','--exit-code',BASE,'HEAD','--','src','vendor','Makefile','tests','bindings','README.md','SPEC.md')
    data = b''.join(p.read_bytes() for p in sorted((ROOT/'payload').glob('*.b64')))
    packed = base64.b64decode(data, validate=True)
    if hashlib.sha256(packed).hexdigest() != GZ:
        raise SystemExit('Transport digest mismatch')
    patch = gzip.decompress(packed)
    if hashlib.sha256(patch).hexdigest() != PATCH:
        raise SystemExit('Reviewed patch digest mismatch')
    pre = run('git','rev-parse','HEAD').decode().strip()
    pr9 = run('git','diff','--binary',PR9_BASE,PR9,'--','.',':(exclude).github/workflows/ci.yml')
    run('git','apply','--check','-',data=pr9)
    run('git','apply','--index','-',data=pr9)
    run('git','commit','-m','integration: port PR9 native plane-context leaf as opt-in; defaults unchanged [skip ci]')
    run('git','apply','--check','-',data=patch)
    run('git','apply','--index','-',data=patch)
    run('git','diff','--cached','--check')
    run('git','commit','-m','integration: combine optional CM leaves, guarded typed routes, checked fast inverse and CLI cleanup [skip ci]')
    run('bash','bindings/python/sync-native.sh')
    run('bash','bindings/rust/sync-native.sh')
    run('git','add','bindings/python/native','bindings/rust/native')
    run('git','commit','-m','integration: synchronize Python and Rust bundled native sources [skip ci]')
    expected = json.loads((ROOT/'EXPECTED_FILES.json').read_text())
    for name, digest in expected.items():
        if hashlib.sha256(pathlib.Path(name).read_bytes()).hexdigest() != digest:
            raise SystemExit('Final source hash mismatch: '+name)
    changes = run('git','diff','--name-only',pre,'HEAD').decode().splitlines()
    if set(changes) != set(expected):
        raise SystemExit('Unexpected changed paths: '+repr(set(changes)^set(expected)))
    run('git','diff','--check',pre,'HEAD')
    receipt = {'transport_commit':pre,'source_commit':run('git','rev-parse','HEAD').decode().strip(),
               'source_files':len(expected),'sha256':expected,'reviewed_patch_sha256':PATCH,
               'status':'SOURCE_IDENTITY_VERIFIED; BUILD_GATE_SEPARATE'}
    out = pathlib.Path('/tmp/kolmo-rc1-import')
    out.mkdir(exist_ok=True)
    (out/'SOURCE_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt))
if __name__ == '__main__':
    main()
