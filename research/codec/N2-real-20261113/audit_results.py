"""Independent audit of N2's real matrix; no codec rerun or hindsight selector."""
import argparse,csv,hashlib,io,json,lzma,math,pathlib,statistics
ARMS=('zstd3','kolmo_fast','tp_prod3','tp_phuf_pg16','tp_pent','tp_arch19','oz_exact6','oz_exact9','oz_whole9','oz_serial9','zstd3_duplicate','oz_exact9_duplicate')
CANDIDATES=ARMS[2:6]
def sha(data):return hashlib.sha256(data).hexdigest()
def charged(w,c):return c if c>1.03*w else w
def costs(e,d,size):
 ew,ec=e['wall_ns']/e['batch']/1e9,e['cpu_ns']/e['batch']/1e9
 dw,dc=d['wall_ns']/d['batch']/1e9,d['cpu_ns']/d['batch']/1e9
 E,D=charged(ew,ec),charged(dw,dc)
 return {'encode_wall_s':ew,'encode_cpu_s':ec,'decode_wall_s':dw,'decode_cpu_s':dc,'speed_cost_s':E+D+size/125000000.,'archive_cost_usd':(E+3.3*D)*(3.89/72)/3600+size/1e9*.023}
def audit(root):
 man=json.loads((root/'MANIFEST.json').read_text());actual={str(p.relative_to(root)) for p in root.rglob('*') if p.is_file() and p.name!='MANIFEST.json'}
 if actual!=set(man):raise ValueError('manifest inventory')
 for name,h in man.items():
  if sha((root/name).read_bytes())!=h:raise ValueError('manifest hash '+name)
 inputs=json.loads((root/'INPUTS.json').read_text());summary=json.loads((root/'SUMMARY.json').read_text());ids=json.loads((root/'FRAME_HASHES.json').read_text());rows_all=[];count=med=frames=0;recalc={}
 for inp in inputs:
  if sha((root/'inputs'/inp['name']).read_bytes())!=inp['sha256']:raise ValueError('input')
 for pi,(pn,nrounds) in enumerate((('first',7),('confirmation',5))):
  recalc[pn]={}
  for ii,inp in enumerate(inputs):
   p=root/pn/inp['name'];rows=[json.loads(line) for line in (p/'stdout.jsonl').read_text().splitlines()];by={}
   for x in rows:
    key=(x['arm'],x['operation'],x['round'])
    if key in by or x['arm'] not in ARMS or x['operation'] not in ('encode','decode') or not 0<=x['round']<nrounds:raise ValueError('sample identity')
    for field in ('wall_ns','cpu_ns','batch','input_bytes','stored_bytes'):
     if type(x[field]) is not int or x[field] < (0 if field=='cpu_ns' else 1):raise ValueError('sample numeric')
    if x['input_bytes']!=inp['bytes'] or x['batch']!=max(1,min(16,4194304//inp['bytes'])):raise ValueError('batch/input')
    by[key]=x
    rows_all.append([pi,ii,ARMS.index(x['arm']),0 if x['operation']=='encode' else 1,x['round'],x['batch'],x['stored_bytes'],x['wall_ns'],x['cpu_ns']])
   if len(by)!=len(ARMS)*2*nrounds:raise ValueError('missing samples')
   count+=len(rows);recalc[pn][inp['name']]={}
   for arm in ARMS:
    f=(p/(arm+'.frame')).read_bytes();size=len(f)
    if sha(f)!=ids[inp['name']][arm]:raise ValueError('frame repeat hash')
    frames+=1
    samples=[costs(by[arm,'encode',rd],by[arm,'decode',rd],size) for rd in range(nrounds)]
    orig=summary[pn][inp['name']][arm]
    if size!=orig['stored_bytes']:raise ValueError('stored bytes')
    for rd,s in enumerate(samples):
     if by[arm,'encode',rd]['stored_bytes']!=size or by[arm,'decode',rd]['stored_bytes']!=size:raise ValueError('sample/frame size')
     for k,v in s.items():
      if not math.isclose(v,orig['runs'][rd][k],rel_tol=1e-12,abs_tol=1e-24):raise ValueError('cost arithmetic')
    md={k:statistics.median(x[k] for x in samples) for k in samples[0]}
    for k,v in md.items():
     if not math.isclose(v,orig['medians'][k],rel_tol=1e-12,abs_tol=1e-24):raise ValueError('median')
     med+=1
    recalc[pn][inp['name']][arm]={'bytes':size,'runs':samples,'medians':md}
   for a,b in [('zstd3','zstd3_duplicate'),('oz_exact9','oz_exact9_duplicate')]:
    if ids[inp['name']][a]!=ids[inp['name']][b]:raise ValueError('duplicate-control frame')
 results={};noise=[];totals={}
 for pn,byinput in recalc.items():
  results[pn]={}
  totals[pn]={a:{'bytes':sum(d[a]['bytes'] for d in byinput.values()),**{k:sum(d[a]['medians'][k] for d in byinput.values()) for k in ('encode_wall_s','decode_wall_s','speed_cost_s','archive_cost_usd')}} for a in ARMS}
  for a in CANDIDATES:
   results[pn][a]={}
   for rival,cost in [('zstd3','speed_cost_s'),('oz_exact9','archive_cost_usd'),('oz_whole9','archive_cost_usd')]:
    results[pn][a][rival]=[name for name,d in byinput.items() if d[a]['bytes']<d[rival]['bytes'] and all(x[cost]<=y[cost] for x,y in zip(d[a]['runs'],d[rival]['runs']))]
  for name,d in byinput.items():
   for b,dup in [('zstd3','zstd3_duplicate'),('oz_exact9','oz_exact9_duplicate')]:
    for k in ('encode_wall_s','decode_wall_s'):
     noise.append({'pass':pn,'input':name,'control':b,'operation':k,'ratio':d[dup]['medians'][k]/d[b]['medians'][k]})
 both={a:{r:sorted(set(results['first'][a][r])&set(results['confirmation'][a][r])) for r in ('zstd3','oz_exact9','oz_whole9')} for a in CANDIDATES}
 for a in both:both[a]['both_archive_rivals']=sorted(set(both[a]['oz_exact9'])&set(both[a]['oz_whole9']))
 receipt={'status':'AUDITED_OBSERVED_DEVELOPMENT_NOT_ACCEPTANCE','manifest_files':len(man),'inputs':len(inputs),'input_bytes':sum(x['bytes'] for x in inputs),'samples':count,'frame_checks':frames,'medians_checked':med,'fixed_opportunities':results,'robust_both_passes':both,'totals':totals,'duplicate_noise':noise,'cost_model':'1 Gbit/s speed; 30d,3.3 reads,0.023 USD/GB-month,(3.89/72) USD/vCPU-hour,0 transfers; historical assumptions','online_selector':False,'fresh_validation':False}
 return receipt,rows_all,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args();a.output.mkdir(exist_ok=False)
 report,rows,inputs=audit(a.root)
 (a.output/'AUDIT.json').write_text(json.dumps(report,indent=2)+'\n')
 s=io.StringIO();w=csv.writer(s,lineterminator='\n');w.writerow(['pass','input','arm','operation','round','batch','stored_bytes','wall_ns','cpu_ns']);w.writerows(rows)
 raw=s.getvalue().encode();compressed=lzma.compress(raw,preset=9);(a.output/'timings.csv.xz').write_bytes(compressed)
 schema={'inputs':inputs,'arms':ARMS,'passes':['first','confirmation'],'operations':['encode','decode'],'raw_sha256':sha(raw),'compressed_sha256':sha(compressed),'rows':len(rows)}
 (a.output/'TIMING_SCHEMA.json').write_text(json.dumps(schema,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k not in ('duplicate_noise','totals','fixed_opportunities')},indent=2))
