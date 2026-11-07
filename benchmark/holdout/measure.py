"""Run the preregistered single-worker holdout experiment."""
import random
import tempfile
from common import *

def commands(name,source,encoded,decoded,graph):
    if name in ('release','kolmo'):
        return kolmo_enc(name,'fast',source,encoded),kolmo_dec(name,encoded,decoded)
    if name=='zstd':
        return ([BIN/name,'-q','--single-thread','-3','-o',encoded,source],
                [BIN/name,'-q','--single-thread','-d','-o',decoded,encoded])
    return ([BIN/'zli','compress','--compressor',graph,source,'--output',encoded],
            [BIN/'zli','decompress',encoded,'--output',decoded])

def main():
    verify_freeze()
    assert digest(OUT/'protocol.md')==json.loads((OUT/'freeze.json').read_text())['protocol_sha256']
    inputs=json.loads((OUT/'inputs.json').read_text())
    graphs=OUT/'profiles'
    graphs.mkdir(exist_ok=True)
    records_path=OUT/'holdout-runs.json'
    records=json.loads(records_path.read_text()) if records_path.exists() else {}
    for name,meta in inputs.items():
        graph=graphs/(name+'.zlc')
        source=WORK/(name+'.train')
        assert digest(source)==meta['train_sha256']
        if name not in records:
            # Write into scratch first. Persist a complete encoder graph only after training succeeds.
            temp=WORK/(name+'.training.zlc')
            if temp.exists(): temp.unlink()
            args=[BIN/'zli','train','--profile','csv','--threads','4','--max-time-secs','10']
            if meta['fields_delimiter']!=',':
                args+=['--profile-arg',meta['fields_delimiter']]
            args += [source,'--output',temp]
            training=measure(args)
            assert temp.stat().st_size>0
            temp.replace(graph)
            records[name]=dict(graph_sha256=digest(graph),graph_bytes=graph.stat().st_size,training=training,trials=[])
            save(records_path,records)
            print('trained',name,training,flush=True)
        assert digest(graph)==records[name]['graph_sha256']
    rng=random.Random(42571)
    with tempfile.TemporaryDirectory(dir=WORK) as tmp:
        tmp=Path(tmp)
        for repeat in range(8):
            names=list(inputs);rng.shuffle(names)
            for name in names:
                source=WORK/(name+'.test');meta=inputs[name];expected=meta['test_sha256']
                assert digest(source)==expected
                codecs=['release','kolmo','zstd','openzl'];rng.shuffle(codecs)
                for codec in codecs:
                    trials=records[name]['trials']
                    if any(t['repeat']==repeat and t['codec']==codec for t in trials): continue
                    encoded=tmp/'encoded';decoded=tmp/'decoded'
                    enc,dec=commands(codec,source,encoded,decoded,graphs/(name+'.zlc'))
                    et=measure(enc);dt=measure(dec)
                    assert digest(decoded)==expected
                    size=encoded.stat().st_size
                    frame_sha=digest(encoded)
                    assert all(t['compressed']==size and t['encoded_sha256']==frame_sha for t in trials if t['codec']==codec)
                    trials.append(dict(codec=codec,repeat=repeat,compressed=size,encoded_sha256=frame_sha,decoded_sha256=expected,encode=et,decode=dt))
                    save(records_path,records)
                    encoded.unlink();decoded.unlink()
                print('verified',name,'warmup' if repeat==0 else f'round {repeat}/7',flush=True)
if __name__=='__main__': main()
