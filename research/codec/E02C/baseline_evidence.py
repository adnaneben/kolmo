#!/usr/bin/env python3
"""Retrieve already-completed Actions evidence and verify recorded log/frame hashes.
Uses the runner's read-only GH_TOKEN via gh, never prints or persists credentials.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path
PIN='2f3308a3380a140ad0f6873ae228de7715e5eef5'
RUN=43187874283
EXPECTED={'focused':'passed_targeted_ownership_and_byte_identity_NOT_RELEASE_ACCEPTANCE','main-baseline':'reproduced_expected_original_main_suite_leak','main-candidate':'passed_candidate_full_main_sanitizer_NOT_RELEASE_ACCEPTANCE'}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=a.output.resolve();root.mkdir(parents=True,exist_ok=False)
    meta=json.loads(subprocess.check_output(['gh','api',f'repos/adnaneben/kolmo/actions/runs/{RUN}'],text=True))
    if meta['head_sha']!=PIN or meta['status']!='completed' or meta['conclusion']!='success':raise RuntimeError('Baseline run identity/status mismatch')
    subprocess.run(['gh','run','download',str(RUN),'-R','adnaneben/kolmo','-D',str(root/'artifacts')],check=True)
    receipt={'validated_reference':PIN,'workflow_run':RUN,'phases':{},'identity_pairs':0,'cross_decodes':0}
    for phase,wanted in EXPECTED.items():
        paths=list((root/'artifacts').glob(f'planner-isolated-{phase}-{RUN}-*/status.json'))
        if len(paths)!=1:raise RuntimeError('Missing/ambiguous phase artifact')
        path=paths[0];s=json.loads(path.read_text())
        if s['status']!=wanted or s['candidate_head']!=PIN or s['candidate_core_blob']!='e06467c8872d494b97270811bedf6fd0bac2aa31':raise RuntimeError('Phase status/source mismatch')
        for cmd in s['commands']:
            if cmd['status']!='completed' or digest(path.parent/cmd['log'])!=cmd['sha256']:raise RuntimeError('Command log hash mismatch')
        receipt['phases'][phase]={'status':wanted,'status_sha256':digest(path),'verified_command_logs':len(s['commands'])}
        if phase=='focused':
            ip=path.parent/'identity/identity.json';identity=json.loads(ip.read_text())
            if identity['status']!='passed_45_frame_pairs_90_cross_decodes_NOT_RELEASE_ACCEPTANCE' or len(identity['pairs'])!=45:raise RuntimeError('Incomplete identity matrix')
            for pair in identity['pairs']:
                source=ip.parent/'corpus'/pair['file'];raw=source.read_bytes()
                if digest(source)!=pair['input_sha256'] or pair['cross_decodes']!=2:raise RuntimeError('Input hash/count')
                for label in ('baseline','candidate'):
                    prefix=ip.parent/'frames'/f"{pair['file']}.{pair['profile']}.{label}"
                    if digest(Path(str(prefix)+'.kmo'))!=pair['frame_sha256'] or Path(str(prefix)+'.out').read_bytes()!=raw:raise RuntimeError('Frame hash/cross-decode mismatch')
                receipt['identity_pairs']+=1;receipt['cross_decodes']+=2
    (root/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
if __name__=='__main__':main()
