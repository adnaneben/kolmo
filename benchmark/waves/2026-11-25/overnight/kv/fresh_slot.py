import argparse, json, pathlib, subprocess, time, urllib.request, urllib.error, socket
parser = argparse.ArgumentParser(description="Generate a fresh public-prompt llama.cpp slot on CPU")
parser.add_argument("llama_server")
parser.add_argument("gguf")
parser.add_argument("new_output", type=pathlib.Path)
args = parser.parse_args()
cache = args.new_output
cache.mkdir(exist_ok=False)
model = args.gguf
exe = args.llama_server
prompt='It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness.'
with socket.socket() as s:
 s.bind(('127.0.0.1',0));port=s.getsockname()[1]
opts=[exe,'-m',model,'--device','none','--fit','off','--no-repack','--no-op-offload','-ngl','0','--no-kv-offload','--no-warmup','--ctx-size','128','--batch-size','64','--ubatch-size','64','-t','2','-np','1','--cache-type-k','f16','--cache-type-v','f16','--slots','--slot-save-path',str(cache),'--host','127.0.0.1','--port',str(port)]
def req(method,path,body=None):
 u=urllib.request.Request(f'http://127.0.0.1:{port}'+path,method=method,data=json.dumps(body).encode() if body is not None else None,headers={'Content-Type':'application/json'})
 with urllib.request.urlopen(u,timeout=90) as r:return json.load(r)
with (cache/'run.log').open('wb') as logfile:
 proc=subprocess.Popen(opts,stdout=logfile,stderr=logfile)
 try:
  start=time.monotonic()
  while time.monotonic()-start<90 and proc.poll() is None:
   try:
    if req('GET','/health').get('status')=='ok':break
   except (OSError,urllib.error.URLError):pass
   time.sleep(.5)
  if proc.poll() is not None:raise RuntimeError(f'process exit {proc.returncode}')
  result=req('POST','/completion',dict(prompt=prompt,n_predict=1,temperature=0,seed=23,cache_prompt=True,id_slot=0,return_tokens=True))
  saved=req('POST','/slots/0?action=save',dict(filename='public-kv.bin'))
  (cache/'inference.json').write_text(json.dumps(dict(prompt=prompt,request_options=opts,prompt_tokens=result.get('tokens_evaluated'),sampled_tokens=result.get('tokens'),time=result.get('timings'),saved=saved),indent=2))
  print(saved)
 finally:
  proc.terminate()
  try:proc.wait(timeout=7)
  except subprocess.TimeoutExpired:proc.kill();proc.wait()
