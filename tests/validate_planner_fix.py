#!/usr/bin/env python3
"""Validate the planner record-ownership repair in disposable checkouts.

The candidate checkout must already contain the committed repaired src/kolmo.c:
its blob must equal the pinned original with exactly the three ownership edits.
An unrepaired candidate is rejected rather than patched, so a pass always refers
to the committed production source. No push, merge, suppression, or performance claim.
"""
from __future__ import annotations
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

PIN = '3c6890a91314b4d71db1911043ec19b87c16a86d'
CORE = 'db8b7ad665ab0ff61f6634a2d6cb76ed1b17b70a'
EDITS = [
    ('plan_t keep = pl; int chosen = 0;\n',
     'plan_t keep = pl; int chosen = 0;\n        /* Ownership survives replacement of the tentative winning plan. */\n        rec_plan_t *owned_rec = NULL;\n'),
    ('keep.rec = R; R = NULL; chosen = 3;',
     'keep.rec = R; owned_rec = R; R = NULL; chosen = 3;'),
    ('structured_done:\n        if (chosen >= 2) pl = keep;',
     'structured_done:\n        if (owned_rec && chosen != 3) free(owned_rec);\n        if (chosen >= 2) pl = keep;'),
]


def blob(data: bytes) -> str:
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def repaired(original: bytes) -> bytes:
    if blob(original) != CORE:
        raise RuntimeError('Pinned original core hash mismatch')
    text = original.decode('utf-8')
    for old, new in EDITS:
        if text.count(old) != 1: raise RuntimeError('Nonunique repair anchor')
        text = text.replace(old, new, 1)
    return text.encode('utf-8')


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--evidence', type=Path, required=True)
    p.add_argument('--phase', choices=['focused', 'main-baseline', 'main-candidate'], required=True)
    a = p.parse_args()
    baseline, candidate, evidence = a.baseline.resolve(), a.candidate.resolve(), a.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    state = {'status': 'running', 'phase': a.phase, 'pin': PIN, 'commands': [],
             'scope': 'repair correctness; NOT release or performance acceptance'}
    def save():
        (evidence / 'status.json').write_text(json.dumps(state, indent=2) + '\n')
    def run(cmd, cwd, allowed=(0,), timeout=14400):
        name = f"{len(state['commands']):03d}.log"
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        entry = {'argv': [str(x) for x in cmd], 'cwd': str(cwd), 'log': name, 'status': 'running'}
        state['commands'].append(entry); save(); start = time.monotonic()
        try:
            with (evidence / name).open('w') as f:
                f.write('$ ' + json.dumps(entry['argv']) + '\n'); f.flush()
                proc = subprocess.run(entry['argv'], cwd=cwd, env=env, stdout=f,
                                      stderr=subprocess.STDOUT, timeout=timeout)
            text = (evidence / name).read_text(errors='replace')
            entry.update(returncode=proc.returncode, seconds=time.monotonic() - start,
                         status='completed', sha256=hashlib.sha256((evidence / name).read_bytes()).hexdigest())
            save()
            if proc.returncode not in allowed:
                sys.stderr.write(f'--- tail of {name} ---\n' + ''.join(text.splitlines(True)[-80:]) + '--- end ---\n')
                raise RuntimeError(f'Command failed; see {name}')
            return proc.returncode, text
        except BaseException:
            if entry['status'] == 'running': entry['status'] = 'interrupted_or_launch_failed'; save()
            raise
    def compile_core(tree, source, output, sanitized):
        flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitized else ['-O3']
        cmd = ['gcc', *flags, '-ffp-contract=off', '-DXXH_NAMESPACE=ZSTD_', '-DZSTD_MULTITHREAD',
               '-DZSTD_LEGACY_SUPPORT=0', '-Isrc', '-Ivendor/zstd', '-Ivendor/lz4',
               '-Ivendor/libbsc', '-Ivendor/brotli/include', *source,
               str(baseline / 'build/libvendor.a'), '-llzma', '-lpthread', '-lm', '-lstdc++', '-o', str(output)]
        run(cmd, tree)
    save()
    try:
        if baseline == candidate: raise RuntimeError('Two separate disposable checkouts are required')
        original = (baseline / 'src/kolmo.c').read_bytes()
        expected = repaired(original)
        candidate_file = candidate / 'src/kolmo.c'
        current = candidate_file.read_bytes()
        if current == original: raise RuntimeError('Candidate does not contain the committed repair')
        if current != expected: raise RuntimeError('Unexpected candidate core; do not substitute a new baseline')
        state['original_core_blob'] = blob(original); state['candidate_core_blob'] = blob(current)
        state['candidate_head'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=candidate, text=True).strip()
        if subprocess.run(['git', 'diff', '--quiet', 'HEAD', '--', 'src/kolmo.c'], cwd=candidate).returncode:
            raise RuntimeError('Candidate src/kolmo.c differs from its committed HEAD')
        for relative in ('src/kgraph.inc', 'benchmark/accept/TARGETS.md'):
            if (baseline / relative).read_bytes() != (candidate / relative).read_bytes():
                raise RuntimeError(f'Out-of-scope change: {relative}')
        diff = ''.join(difflib.unified_diff(original.decode().splitlines(True), expected.decode().splitlines(True),
                                         fromfile='a/src/kolmo.c', tofile='b/src/kolmo.c'))
        (evidence / 'candidate.diff').write_text(diff)
        run(['git', 'rev-parse', 'HEAD'], baseline)
        if subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=baseline, text=True).strip() != PIN:
            raise RuntimeError('Baseline is not the pinned commit')
        run(['gcc', '--version'], baseline)
        run(['make', '-j4', 'ARCH=', 'build/libvendor.a'], baseline)
        if a.phase == 'focused':
            tests = Path(__file__).resolve().parent
            logs = {}
            for label, tree in (('baseline', baseline), ('candidate', candidate)):
                target = tree / 'tests/test_planner_ownership.c'
                if target.resolve() != (tests / target.name).resolve(): shutil.copyfile(tests / target.name, target)
                exe = tree / 'planner-ownership-asan'
                compile_core(tree, ['tests/test_planner_ownership.c'], exe, True)
                code, text = run([exe], tree, allowed=(0, 1, 2), timeout=3600)
                forbidden = ('runtime error:', 'ERROR: AddressSanitizer:', 'DEADLYSIGNAL',
                             'FAIL insufficient planner-path coverage')
                if any(x in text for x in forbidden): raise RuntimeError(f'{label}: unrelated failure or insufficient coverage')
                marker = re.search(r'PLANNER_OWNERSHIP cases=(\d+) record_allocations=(\d+) graph_cases=(\d+) leaked_cases=(\d+) outstanding=(\d+)', text)
                if not marker or int(marker[1]) != 36 or not int(marker[2]) or not int(marker[3]):
                    raise RuntimeError(f'{label}: missing complete real-planner coverage')
                if label == 'baseline':
                    if code != 1 or not int(marker[4]) or not int(marker[5]) or 'LeakSanitizer: detected memory leaks' not in text:
                        raise RuntimeError('Original targeted leak was not reproduced')
                    traces = re.findall(r'(?:Direct|Indirect) leak of .*?(?=\n(?:Direct|Indirect) leak of |\nSUMMARY:|\Z)', text, re.S)
                    if not traces or any(not re.search(r'\bin make_plan\b', x) or not re.search(r'kolmo\.c:2015(?:\D|$)', x) for x in traces):
                        raise RuntimeError('Original targeted leak is not the pinned make_plan record allocation')
                    state['baseline_focused_leak_summary'] = re.findall(r'SUMMARY: AddressSanitizer: .*', text)
                elif code or int(marker[4]) or int(marker[5]) or 'ERROR:' in text or 'PLANNER OWNERSHIP PASSED' not in text:
                    raise RuntimeError('Candidate targeted sanitizer did not pass')
                logs[label] = re.findall(r'^seed=.*?plan=\d+', text, flags=re.M)
            if len(logs['baseline']) != 36 or logs['baseline'] != logs['candidate']:
                raise RuntimeError('Selected-plan identities changed')
            for label, tree in (('baseline', baseline), ('candidate', candidate)):
                compile_core(tree, ['src/cli.c', 'src/kolmo.c'], tree / 'planner-identity-cli', False)
            run([sys.executable, tests / 'test_planner_identity.py',
                 '--baseline', baseline / 'planner-identity-cli', '--candidate', candidate / 'planner-identity-cli',
                 '--output', evidence / 'identity'], candidate, timeout=7200)
            state['status'] = 'passed_targeted_ownership_and_byte_identity_NOT_RELEASE_ACCEPTANCE'
        else:
            tree = baseline if a.phase == 'main-baseline' else candidate
            exe = tree / 'kolmotest-main-asan'
            compile_core(tree, ['tests/kolmotest.c', 'src/kolmo.c'], exe, True)
            code, text = run([exe], tree, allowed=(0, 1), timeout=14400)
            if any(x in text for x in ('runtime error:', 'ERROR: AddressSanitizer:', 'DEADLYSIGNAL', 'FAIL ')):
                raise RuntimeError('Unrelated main-suite failure')
            if a.phase == 'main-baseline':
                traces = re.findall(r'(?:Direct|Indirect) leak of .*?(?=\n(?:Direct|Indirect) leak of |\nSUMMARY:|\Z)', text, re.S)
                if code != 1 or 'ERROR: LeakSanitizer: detected memory leaks' not in text or not traces:
                    raise RuntimeError('Original main-suite leak not reproduced')
                if any(not re.search(r'\bin make_plan\b', t) or not re.search(r'kolmo\.c:2015(?:\D|$)', t) for t in traces):
                    raise RuntimeError('Unexpected main-suite allocation stack')
                if not re.search(r'SUMMARY: AddressSanitizer: [1-9][0-9]* byte\(s\) leaked in [1-9][0-9]* allocation\(s\)', text):
                    raise RuntimeError('Missing leak summary')
                state['status'] = 'reproduced_expected_original_main_suite_leak'
            else:
                if code or 'ERROR:' in text or 'LeakSanitizer:' in text:
                    raise RuntimeError('Candidate full main sanitizer failed')
                state['status'] = 'passed_candidate_full_main_sanitizer_NOT_RELEASE_ACCEPTANCE'
        save(); print(state['status'])
    except BaseException as exc:
        state['status'] = 'failed_or_interrupted_NOT_VALIDATED'; state['error'] = str(exc); save(); raise


if __name__ == '__main__':
    main()
