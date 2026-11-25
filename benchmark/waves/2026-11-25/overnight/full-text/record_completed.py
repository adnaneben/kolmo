"""Publish the completed, verified comparison on the evidence branch and tracker."""
import argparse
import base64
import json
from pathlib import Path
import subprocess
import tempfile

repo = 'adnaneben/kolmo'
branch = 'codex/ppp-audit-and-results-20261125'
path = 'benchmark/waves/2026-11-25/overnight/full-text/measured.json'
url = f'https://github.com/{repo}/blob/{branch}/{path}'

def gh(*args, data=None):
    return subprocess.run(['gh', *args], input=data, stdout=subprocess.PIPE,
                          text=True, check=True).stdout

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('status', type=Path)
    opt = ap.parse_args()
    report = json.loads(opt.status.read_text())
    assert report['current'] == 'complete'
    rows = report['results']
    assert len(rows) == 4 and {r['tool'] for r in rows} == {'zstd-19','xz-9','kolmo-ultra','paq8px-8L'}
    assert report['input_bytes'] == 100_000_000
    assert all(r['recovered_sha256'] == report['source_sha256'] for r in rows)
    # Strip machine-local invocation paths. The full local receipt stays with the run.
    report.pop('pid', None)
    for r in rows:
        for field in ('encode_argv','decode_argv'):
            r[field] = [Path(arg).name if Path(arg).is_absolute() else arg for arg in r[field]]
    body = (json.dumps(report, indent=2)+'\n').encode()
    endpoint = f'repos/{repo}/contents/{path}'
    existing = subprocess.run(['gh','api',endpoint+'?ref='+branch],text=True,capture_output=True)
    if existing.returncode == 0:
        if base64.b64decode(json.loads(existing.stdout)['content']) != body:
            raise ValueError('published result differs; preserve both for human review')
    else:
        payload = {'message':'Record verified whole-enwik8 product measurements','branch':branch,
                   'content':base64.b64encode(body).decode()}
        gh('api','--method','PUT',endpoint,'--input','-',data=json.dumps(payload))
    comments = gh('api','--paginate',f'repos/{repo}/issues/57/comments','--jq','.[].body')
    if url in comments:
        return
    lines = ['Whole enwik8, 100,000,000 input bytes, Apple M5. One serial file-to-file run per tool, including full restore. All four restored SHA-256 `'+report['source_sha256']+'`. The shared-host timings are single observations.','',
             '| Tool | Stored bytes | Encode s | Decode s | Combined MB/s |','|---|---:|---:|---:|---:|']
    for r in rows:
        lines.append('| {} | {:,} | {:.2f} | {:.2f} | {:.3f} |'.format(r['tool'],r['stored_bytes'],r['encode_seconds'],r['decode_seconds'],r['combined_MB_s']))
    lines.extend(['','[Hashed inputs, frames, tools and measured settings]('+url+'). The hosted x86 benchmark is tracked separately.'])
    with tempfile.NamedTemporaryFile('w',suffix='.md') as f:
        f.write('\n'.join(lines)); f.flush()
        gh('issue','comment','57','--repo',repo,'--body-file',f.name)
if __name__ == '__main__':
    main()
