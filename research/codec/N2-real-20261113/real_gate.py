"""Check all real-codec arms, stock-reader reconstruction and cross-build identity."""
import argparse,pathlib,subprocess,json,hashlib,os,importlib.util
p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,required=True);a=p.parse_args();r=a.root.resolve();repo=r/'source/kolmo'
spec=importlib.util.spec_from_file_location('n2run',repo/'research/codec/N2-matched-native/run.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
out=r/'correctness';out.mkdir(exist_ok=False)
status={'status':'RUNNING','configurations':0,'stock_crossdecodes':0,'binary_identities':{},'commands':[]}
try:
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1','OMP_NUM_THREADS':'1','OPENBLAS_NUM_THREADS':'1'}
 for name,bin in [('release',r/'build-release2/cmake/n2_native'),('asan',r/'build-checks/n2_asan')]:
  status['binary_identities'][name]=m.sha(bin.read_bytes())
  for f in sorted((r/'prepared').glob('*.npy')):
   d=out/name/f.stem;d.mkdir(parents=True);cmd=[str(bin),str(f),str(d),'1','314159']
   status['commands'].append(cmd);res=subprocess.run(cmd,capture_output=True,env=env,timeout=180)
   (d/'stdout.jsonl').write_bytes(res.stdout);(d/'stderr.log').write_bytes(res.stderr)
   if res.returncode:raise RuntimeError(f'{name}: {f.name}: exit {res.returncode}')
   m.validate_rows([json.loads(x) for x in res.stdout.splitlines()],1)
   for arm in m.ARMS:
    frame=d/(arm+'.frame');status['configurations']+=1
    if name=='asan':
     previous=out/'release'/f.stem/(arm+'.frame');assert frame.read_bytes()==previous.read_bytes(),(f.name,arm,'cross-build frame')
    else:
     restored=d/(arm+'.decoded')
     reader=(r/'build-checks/zstd_reader') if arm.startswith('zstd3') else (r/'build-checks/stock_reader') if arm.startswith('oz_') else (repo/'build/kolmo')
     cmd=[str(reader),str(frame),str(restored)] if arm.startswith(('zstd3','oz_')) else [str(reader),'-d','-T1',str(frame),str(restored)]
     res=subprocess.run(cmd,capture_output=True,timeout=30);(d/(arm+'.reader.log')).write_bytes(res.stdout+res.stderr)
     if res.returncode or restored.read_bytes()!=f.read_bytes():raise RuntimeError(f'stock decode {f.name} {arm}')
     status['stock_crossdecodes']+=1
   print(name,f.name,'12 arms passed',flush=True)
 status['status']='PASSED_REAL_CODECS_AND_STOCK_CROSSDECODE'
finally:(out/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
