"""Offline recipient-envelope experiment; accepts only the script's own newly generated envelopes."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import random
import shutil
import statistics
import subprocess
import tempfile
from time import perf_counter_ns

ap = argparse.ArgumentParser()
ap.add_argument("--openssl", required=True, type=Path)
ap.add_argument("--kolmo", required=True, type=Path)
ap.add_argument("--repo", required=True, type=Path)
ap.add_argument("--silesia", required=True, type=Path)
ap.add_argument("--out", type=Path, required=True)
args = ap.parse_args()
ossl, kolmo = args.openssl.resolve(), args.kolmo.resolve()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
umask = os.umask(0o077)
def run(argv, cwd, check=True):
    t = perf_counter_ns()
    proc = subprocess.run(argv, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    elapsed = (perf_counter_ns()-t)/1e6
    if check and proc.returncode:
        raise RuntimeError(proc.stderr.decode(errors='replace'))
    return elapsed, proc.returncode
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def recipient(w, name):
    run([ossl, "req","-x509","-newkey","rsa:3072","-noenc","-subj",f"/CN=Ephemeral {name}","-days","1","-out",name+".cert","-keyout",name+".key"], w)
def wrap(w, arc, env, cipher="-aes-256-gcm"):
    return run([ossl,"cms","-encrypt","-binary",cipher,"-outform","DER","-in",arc,"-out",env,"-recip",w/"recipient.cert","-keyopt","rsa_padding_mode:oaep","-keyopt","rsa_oaep_md:sha256","-keyopt","rsa_mgf1_md:sha256"], w)[0]
def unwrap(w, env, priv, name="recipient"):
    return run([ossl,"cms","-decrypt","-binary","-inform","DER","-in",env,"-recip",w/(name+".cert"),"-inkey",w/(name+".key"),"-out",priv], w, False)
rows, inputs, negatives = [], [], []
spec = {"code+prose":[args.repo/"SPEC.md", args.repo/"src/core/extension.c"], "Silesia-prefix":[args.silesia]}
with tempfile.TemporaryDirectory(prefix="kolmo-cms-") as t:
    w = Path(t)
    recipient(w, "recipient")
    recipient(w, "other")
    for group, sources in spec.items():
        src = w/group
        src.mkdir(mode=0o700)
        baseline = {}
        for p in sources:
            dest = src/p.name
            shutil.copyfile(p, dest)
            dest.chmod(0o640)
            os.utime(dest,(1700000000,1700000000))
            baseline[p.name] = digest(dest)
            inputs.append([group, p.name, dest.stat().st_size, baseline[p.name]])
        for rep in range(-1,7):
            arms = ["plain","CMS"]
            random.Random(613+rep).shuffle(arms)
            identity = None
            for arm in arms:
                with tempfile.TemporaryDirectory(dir=w) as trial:
                    td=Path(trial)
                    arc,env,priv=td/"a.kol",td/"a.cms",td/"verified.kol"
                    enc,_=run([kolmo,"archive","-1","-T","1",arc,*baseline],src)
                    arcsha=digest(arc)
                    if identity and arcsha!=identity: raise ValueError("source archive changed")
                    identity=arcsha
                    stored=arc.stat().st_size
                    encryption=decryption=0.
                    if arm=="CMS":
                        encryption=wrap(w,arc,env)
                        decryption,rc=unwrap(w,env,priv)
                        if rc or digest(priv)!=arcsha: raise ValueError("authentication or readback failed")
                        input_file=priv
                        delivered=env.stat().st_size
                    else:
                        input_file=arc
                        delivered=stored
                    restore=td/"restore"
                    restore.mkdir(mode=0o700)
                    dec,_=run([kolmo,"extract","-T","1","-C",restore,input_file],td)
                    members=list(restore.iterdir())
                    if {p.name:digest(p) for p in members}!=baseline: raise ValueError("restored members differ")
                    if any((p.stat().st_mode & 0o777)!=0o640 or int(p.stat().st_mtime)!=1700000000 for p in members):
                        raise ValueError("restored metadata differ")
                    if rep>=0:
                        n=sum(f.stat().st_size for f in sources)
                        rows.append([group,rep,arm,n,stored,delivered,arcsha,enc,encryption,decryption,dec])
                    if group=="code+prose" and arm=="CMS" and rep==0:
                        # Exercise failure handling without passing any negative input to Kolmo.
                        original=env.read_bytes()
                        for label,data,key in [("modified final tag byte",original[:-1]+bytes([original[-1]^1]),"recipient"),("truncated envelope",original[:-16],"recipient"),("wrong recipient",original,"other"),("trailing bytes",original+b'trailing bytes',"recipient")]:
                            sample=td/"negative.cms"; sample.write_bytes(data)
                            sink=td/"unpublished"
                            _,rc=unwrap(w,sample,sink,key)
                            negatives.append([label,rc,sink.stat().st_size if sink.exists() else 0])
                            sink.unlink(missing_ok=True)
                        cbc=td/"cbc.der"
                        wrap(w,arc,cbc,"-aes-256-cbc")
                        _,rc=unwrap(w,cbc,td/"unpublished")
                        negatives.append(["generic decrypt accepts external CBC envelope",rc,(td/"unpublished").stat().st_size])
os.umask(umask)
def write(name,header,data):
    with (out/name).open("w",newline="") as f:
        w=csv.writer(f,lineterminator="\n");w.writerow(header);w.writerows(data)
write("measurements.csv",["source","rep","arm","raw_bytes","kolmo_bytes","delivered_bytes","kolmo_sha256","create_ms","encrypt_ms","decrypt_ms","restore_ms"],rows)
write("inputs.csv",["source","name","raw_bytes","sha256"],inputs)
write("envelope-checks.csv",["case","openssl_exit","private_output_bytes"],negatives)
for group in spec:
    for arm in ("plain","CMS"):
        r=[x for x in rows if x[0]==group and x[2]==arm]
        med=statistics.median
        print(group,arm,r[0][3:6],"ms create+wrap",round(med(x[7]+x[8] for x in r),3),"unwrap+restore",round(med(x[9]+x[10] for x in r),3),"combined MB/s",round(med(x[3]/(sum(x[7:11])*1000) for x in r),3),"wrap-only/unwrap-only ms",round(med(x[8] for x in r),3),round(med(x[9] for x in r),3))
