import json, os, sys
exec(open('/tmp/ozp/run.py').read().split("res=[]")[0])
R=json.load(open('/tmp/ozp/results.json'))
for rec in R:
    src=next(p[1] for p in panel if os.path.basename(p[1])==rec['file'] and p[0]==rec['group'])
    n=rec['n']; reps=3 if n<60e6 else 1
    extra=[('kolmo hc -l 9',['-m','hc','-l','9']),('kolmo max -l 1',['-m','max','-l','1']),('kolmo eco',['-m','eco'])]
    if n<=11e6: extra.append(('kolmo ultra -l 1',['-m','ultra','-l','1']))
    for kl,args in extra:
        rp = 1 if kl.startswith('kolmo ultra') or n>20e6 and kl!='kolmo hc -l 9' else reps
        m,e=measure([K]+args+['-T1',src,O+'k.czn'],[K,'-d','-T1',O+'k.czn',O+'k.back'],O+'k.czn',src,O+'k.back',rp)
        rec['rows'].append({'set':kl,'m':m,'err':e})
    json.dump(R,open('/tmp/ozp/results2.json','w'),indent=1)
    print(rec['file'], flush=True)
print('DONE')
