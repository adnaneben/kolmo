#!/usr/bin/env python3
"""Independently validate P2-E17 complete CLI receipts and quantify uncharged-oracle gap.

Development evidence ONLY.  This code never treats a per-input cost-measured best
candidate as a deployable online selector.  It does not replace the rival with a
header-exact OpenZL adapter, validate fresh data or modify any production codec.
"""
import argparse
import collections
import hashlib
import json
import math
import pathlib
import statistics
import zipfile

EXPECTED_COMMIT = 'f4fcadcd762d7d7bc9e484a4730900eb3388d5e9'
RIVAL_PREFIX = 'openzl_'
CONTROL = 'zstd_3'
COST_FIELDS = ('archive_cost_usd', 'speed_cost_s')


def digest(payload):
    return hashlib.sha256(payload).hexdigest()


def model_cost(encode_wall, encode_cpu, decode_wall, decode_cpu, stored):
    e = encode_cpu if encode_cpu > encode_wall * 1.03 else encode_wall
    d = decode_cpu if decode_cpu > decode_wall * 1.03 else decode_wall
    return ((e + 3.3 * d) * (3.89 / 72) / 3600 + stored / 1e9 * 0.023,
            e + d + stored / 125_000_000)


def validate_receipt(z, expected_commit=EXPECTED_COMMIT):
    entries = z.namelist()
    if len(entries) != len(set(entries)):
        raise ValueError('duplicate ZIP members')
    if any(pathlib.PurePosixPath(n).is_absolute() or '..' in pathlib.PurePosixPath(n).parts for n in entries):
        raise ValueError('unsafe ZIP member path')
    if z.read('COMMIT.txt').decode().strip() != expected_commit:
        raise ValueError('wrong executed source commit')
    manifest = json.loads(z.read('MANIFEST.json'))
    if set(manifest) != set(entries) - {'MANIFEST.json'} - {n for n in entries if n.startswith('prepared/inputs/')}:
        raise ValueError('missing or unregistered ZIP member')
    for member, expected in manifest.items():
        if digest(z.read(member)) != expected:
            raise ValueError('manifest content mismatch: ' + member)
    confirm = json.loads(z.read('confirmation.json'))
    if confirm.get('status') != 'IDENTITIES_MATCH' or confirm.get('failed_profiles') != 0:
        raise ValueError('confirmation failed')
    rows = {}
    for trial in ('first', 'confirmation'):
        obj = json.loads(z.read(trial + '/records.json'))
        if obj.get('failed_profiles') != 0 or obj.get('completed') != 488 or len(obj['records']) != 488:
            raise ValueError('not 488 complete configurations: ' + trial)
        tab = collections.defaultdict(dict)
        for row in obj['records']:
            input_id, pol = row['input_id'], row['policy']['id']
            if pol in tab[input_id]:
                raise ValueError('duplicate policy/input record')
            if row['status'] != 'MEASURED_DEVELOPMENT_CONTROL' or row.get('byte_exact_verified') is not True:
                raise ValueError('unverified reconstruction: ' + input_id + '/' + pol)
            if row.get('input_bytes',0) <= 0 or row.get('stored_bytes',0) <= 0:
                raise ValueError('invalid input/stored bytes')
            if len(row.get('frame_sha256','')) != 64 or len(row.get('input_sha256','')) != 64:
                raise ValueError('missing source/frame hash')
            samples = row['runs']
            if [s['pair_id'] for s in samples] != ['0','1','2']:
                raise ValueError('incomplete or duplicated paired rounds')
            for s in samples:
                for k in ('encode_wall_s','encode_cpu_s','decode_wall_s','decode_cpu_s'):
                    if not math.isfinite(s[k]) or s[k]<0:
                        raise ValueError('negative/nonfinite sample')
                archive, speed = model_cost(s['encode_wall_s'],s['encode_cpu_s'],s['decode_wall_s'],s['decode_cpu_s'],row['stored_bytes'])
                if not math.isclose(archive,s['archive_cost_usd'],rel_tol=1e-9,abs_tol=1e-15):
                    raise ValueError('archive cost mismatch')
                if not math.isclose(speed,s['speed_cost_s'],rel_tol=1e-9,abs_tol=1e-12):
                    raise ValueError('speed cost mismatch')
            tab[input_id][pol] = row
        if len(tab) != 61 or any(len(x) != 8 for x in tab.values()):
            raise ValueError('missing inputs or policies')
        rows[trial] = dict(tab)
    if set(rows['first']) != set(rows['confirmation']):
        raise ValueError('input mismatch between repeats')
    for name, p in rows['first'].items():
        other = rows['confirmation'][name]
        if set(p) != set(other) or len([x for x in p if x.startswith(RIVAL_PREFIX)]) != 1:
            raise ValueError('rival/policy identity mismatch')
        for policy, record in p.items():
            ref = other[policy]
            for k in ('input_sha256','input_bytes','stored_bytes','frame_sha256','family','view'):
                if record[k] != ref[k]:
                    raise ValueError('source/frame/family repeat mismatch: '+name+'/'+policy)
            for r in (record,ref):
                if r['view']=='npy':
                    path='prepared/inputs/'+name+'.npy'
                elif r['view']=='csv':
                    path='prepared/inputs/'+name+'.csv'
                elif r['view']=='wav':
                    path='prepared/inputs/'+name+'.wav'
                else:
                    raise ValueError('unknown input view')
                # Every prepared source is independently retained and signed by manifest.
                if path not in z.namelist() or digest(z.read(path)) != r['input_sha256']:
                    raise ValueError('prepared input mismatch: '+name)
    if confirm.get('completed') != 488 or confirm.get('configurations') != 488:
        raise ValueError('confirmation count mismatch')
    return rows, len(manifest)


def fits(candidate, rival):
    return candidate['stored_bytes'] < rival['stored_bytes'] and all(
        a['archive_cost_usd'] <= b['archive_cost_usd']
        for a,b in zip(candidate['runs'],rival['runs']))


def audit(rows):
    policies = sorted(k for k in next(iter(rows['first'].values())) if not k.startswith(RIVAL_PREFIX) and k != CONTROL)
    scope = {'development': True, 'count_inputs': len(rows['first']), 'model': 'archive_v2_recorded_3_3_reads_30_days'}
    results = {'scope': scope, 'per_run': {}, 'robust_both_runs': {}, 'families': {}}
    for trial, tab in rows.items():
        ctr=collections.Counter()
        for i,p in tab.items():
            rival=next(v for k,v in p.items() if k.startswith(RIVAL_PREFIX))
            candidates=[(v['stored_bytes'],k) for k,v in p.items() if k not in (CONTROL,) and not k.startswith(RIVAL_PREFIX) and fits(v,rival)]
            if candidates:
                ctr[min(candidates)[1]]+=1
            else: ctr['openzl_9']+=1
        results['per_run'][trial]=dict(winners=dict(ctr),kolmo_winners=sum(v for k,v in ctr.items() if k!='openzl_9'))
    rb=collections.Counter();family=collections.defaultdict(lambda:collections.Counter())
    allpol=policies+[CONTROL]
    for id_,first in rows['first'].items():
        second=rows['confirmation'][id_]
        rfirst=next(v for k,v in first.items() if k.startswith(RIVAL_PREFIX))
        rsecond=next(v for k,v in second.items() if k.startswith(RIVAL_PREFIX))
        eligible=[(first[k]['stored_bytes'],k) for k in allpol if fits(first[k],rfirst) and fits(second[k],rsecond)]
        winner=min(eligible)[1] if eligible else 'openzl_9'
        if winner!='openzl_9':rb['oracle_feasible_and_smaller']+=1
        family[rfirst['family']]['files']+=1
        family[rfirst['family']]['oracle_wins']+=(winner!='openzl_9')
        if any(fits(first[k],rfirst) and fits(second[k],rsecond) for k in ('kolmo_fast','kolmo_hc','kolmo_max')):
            rb['oracle_production_only']+=1
        for k in allpol:
            if fits(first[k],rfirst) and fits(second[k],rsecond):
                rb[k]+=1;family[rfirst['family']][k]+=1
    results['robust_both_runs']=dict(sorted(rb.items()))
    results['families']={f:dict(sorted(v.items())) for f,v in sorted(family.items())}
    results['limitations']=[
        'All inputs are previously observed development. NPY/WAV headers are read as OpenZL numeric values (not separately losslessly framed); rival scope is not the strong header-exact frontier.',
        'Per-input oracle is hindsight only: neither the cost of rival trials nor selector/feature inference was charged; these are not admissible online winners.',
        'Production-only Kolmo modes do not type columns first; all speed/archive policy gates and source-disjoint validation remain unpassed.',
        'CLI timing charges startup and I/O; no native custom comparator, no new compression algorithm or new independent holdout was executed.',
    ]
    return results


def main():
    a=argparse.ArgumentParser();a.add_argument('artifact',type=pathlib.Path);a.add_argument('--output',type=pathlib.Path)
    args=a.parse_args()
    with zipfile.ZipFile(args.artifact) as z:
        rows, n=validate_receipt(z)
    res=audit(rows)
    res['verified_member_count']=n
    res['artifact_sha256']=digest(args.artifact.read_bytes())
    if args.output:args.output.write_text(json.dumps(res,indent=2,sort_keys=True)+'\n')
    print(json.dumps(res,indent=2,sort_keys=True))

if __name__=='__main__':main()
