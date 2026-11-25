"""Build an aggregate from the raw per-run measurements without conflating phase medians."""
import csv, json, pathlib, statistics, sys
root=pathlib.Path(sys.argv[1])
manifest=json.loads((root/'manifest.json').read_text())
rows=[json.loads(s) for s in (root/'trials.jsonl').read_text().splitlines()]
summary=list(csv.DictReader((root/'summary.csv').open()))
compat=json.loads((root/'stock-compatibility.json').read_text())
control=json.loads((root/'unchanged-settings.json').read_text())
assert len(rows)==96 and len(summary)==12 and len(control)==9
doc=['# Repair and transfer test for explicit Ultra table limits','',
     'The Model 4 encoder on main accepted `--x-tables=16..22` through the public API and then ignored the numeric preference. The historical review A11 had called out this gap. Commit `f5bd6620ef61505e87a2943114046e27dee28a6e` starts explicit settings from the existing full-table policy, then raises the header’s size shift until the largest context fits the requested maximum. There are four 16-byte buckets per 64-byte line. The existing explicit working-memory allowance still takes precedence when it calls for smaller tables. All changes affect only encoding with an explicit numeric setting. Production defaults are unchanged. Source, tests, native binding mirrors and docs are included in `model4-cap.patch`.','',
     '## Matched measurements','',
     'The baseline is exact main `a1d39559686a9ec17c8614b9154a64d7b5b98672` and all Kolmo cases use the regular Ultra file CLI, T1. '+manifest['brotli_version'].capitalize()+' level 11 is a same-host competitor. Each cell has seven shuffled repetitions. MB/s means decimal raw bytes / elapsed seconds. Combined throughput uses the median of paired encode + decode seconds. Memory is the median peak RSS of the separate encoder/decoder processes (MiB). Shared-host elapsed timings are directional.','',
     '| Input | Arm | Stored B | Enc MB/s | Dec MB/s | Combined MB/s | Enc / dec MiB | Model 4 shift |','|---|---|---:|---:|---:|---:|---:|---:|']
for s in summary:
    doc.append(f"| {s['input']} | {s['arm']} | {int(s['stored_bytes']):,} | {float(s['encode_MB_s']):.3f} | {float(s['decode_MB_s']):.3f} | {float(s['combined_MB_s']):.3f} | {float(s['enc_rss_bytes'])/2**20:.1f} / {float(s['dec_rss_bytes'])/2**20:.1f} | {s['shift'] or '—'} |")
doc+=['','All 96 runs restored their source SHA-256. All '+str(len(compat))+' distinct native frame hashes were further decoded and SHA-verified with the independent v0.0.2 binary. Nine whole-frame digest equalities verified that old numeric17 matched old scaled, and fixed default/scaled and full each matched their original counterparts on all three workloads. Separate fixed-numeric16 controls, with headers, are retained. The stock decoder digest is recorded in `manifest.json`.','',
      '## Paired comparisons and decisions','']
for inp in manifest['inputs']:
    a,b,full,br=([v for v in rows if v['rep'] is not None and v['input']==inp and v['arm']==arm] for arm in ('main numeric17','fixed numeric17','main full','Brotli 11'))
    suma=lambda r:r['encode_s']+r['decode_s']
    delta=[100*(suma(x)/suma(y)-1) for x,y in zip(a,b)]
    by=lambda v:v[0]['stored_bytes']
    win=sum(v>0 for v in delta)
    relative=lambda after,before:100*(after/before-1)
    doc.append(f"- {inp}: fixed numeric17 changed bytes by {relative(by(b),by(a)):+.3f}% against the prior numeric setting, {relative(by(b),by(full)):+.3f}% against full, and {relative(by(b),by(br)):+.2f}% against Brotli 11. Median paired combined speed change versus the previous numeric setting was {statistics.median(delta):+.2f}% ({win}/7 faster pairs).")
doc+=['','Treat this as an explicit API contract repair and a controllable size/memory tradeoff. Small byte changes alone do not justify modifying the default. The two whole manuals share a format despite independent authorship. The Webster prefix tests a different text distribution. No cross-ISA or all-text claim follows, and CPU contention can affect the speed ordering. The reader has to allocate according to the shift encoded in each accepted frame; this API is an encoder policy, not a process-wide memory cap.','',
      '## Related ownership and open research','',
      'Claude R16 explicitly recorded historical claim #68 for an LSTM mixer path and #66 for a new neural codec. The proposal uses a new model nibble or codec; this repair does not implement either, and archived estimates are not Kolmo benchmark results. R11’s #56 prior-pack research, shipping-reader rejection of Lexi/prior extensions, and the GR6 gated existing-column route remain separately recorded. Historic authorship establishes who proposed them, not whether a fresh worker is active. Existing GR6/L3, alternate mask, confidence expert, reset and Lexi LUT counterexamples remain preserved in PRs #48/#91. A future neural experiment needs a distinct opt-in wire contract, deterministic reference decoder, count of model side data and initialization cost, and held-out end-to-end evidence.','',
      '## Reproduction','',
      'Build commit a1d3955 and apply the patch in a separate checkout. Run `make -j2 build/kolmo check-cm4` for the candidate, then the regular frozen v12 tests. Run `python3 compare.py BASE_MAIN_CLI PATCHED_CLI RELEASED_V002_CLI BROTLI EVIDENCE_DIR` and `python3 report.py EVIDENCE_DIR`. Input paths and slicing are in the runner, source file and slice SHA-256 in the manifest. Inputs are installed public files and are omitted from the evidence. Build/test logs, raw phase/CPU/RSS, method, compatibility receipts and exact source patch accompany this report.']
(root/'README.md').write_text('\n'.join(doc)+'\n')
