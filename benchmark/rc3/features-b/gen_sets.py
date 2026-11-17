"""Builds the deduplication sets of the features-b report under DATA (argument 1).

  finetune/   8 checkpoints of a 48 MiB float32 model (safetensors layout) in a partial fine-tune: each step updates 20% of
              the tensors (every value changes a little), the other tensors stay frozen. Seeded, so the bytes are the same
              on every run.
  growlog/    6 hourly backups of a growing log: the first 24, 48, ... 144 MiB of the HDFS log (argument 2).
  snapshots/  10 daily CSV exports of a table that gains 5000 rows a day and has 200 rows edited a day.
  srctree/    5 source trees: `git archive` of this repository at 5 commits 20 apart (argument 3: the repository).
Run with python3 -I. """
import json, os, random, struct, subprocess, sys, tarfile, io
import numpy as np

data, log, repo = sys.argv[1], sys.argv[2], sys.argv[3]

def finetune():
    d = os.path.join(data, "finetune"); os.makedirs(d, exist_ok=True)
    rng = np.random.default_rng(11)
    shapes = [(1024, 1024)] * 8 + [(4096, 512)] * 2 + [(512,)] * 16 + [(256, 1024)] * 4
    tensors = [rng.standard_normal(s).astype(np.float32) * 0.02 for s in shapes]
    for step in range(8):
        if step:
            for i in rng.choice(len(tensors), size=max(1, len(tensors) // 5), replace=False):
                tensors[i] = (tensors[i] + rng.standard_normal(tensors[i].shape).astype(np.float32) * 1e-4).astype(np.float32)
        header, off = {"__metadata__": {"step": str(step * 500)}}, 0
        for i, t in enumerate(tensors):
            header[f"layer{i}.weight"] = {"dtype": "F32", "shape": list(t.shape), "data_offsets": [off, off + t.nbytes]}; off += t.nbytes
        h = json.dumps(header).encode(); h += b" " * (-len(h) % 8)
        with open(os.path.join(d, f"ckpt-{step:02d}.safetensors"), "wb") as f:
            f.write(struct.pack("<Q", len(h))); f.write(h)
            for t in tensors: f.write(t.tobytes())

def growlog():
    d = os.path.join(data, "growlog"); os.makedirs(d, exist_ok=True)
    with open(log, "rb") as f: big = f.read(144 << 20)
    for i in range(6):
        n = (24 << 20) * (i + 1); cut = big.rfind(b"\n", 0, n) + 1
        with open(os.path.join(d, f"app.log.{i:02d}"), "wb") as f: f.write(big[:cut])

def snapshots():
    d = os.path.join(data, "snapshots"); os.makedirs(d, exist_ok=True)
    random.seed(7)
    rows = [f"{i},{random.choice(['ber','par','lis','osl'])},{random.randint(0,99999)},{random.random()*1000:.3f}\n" for i in range(150000)]
    for day in range(10):
        for _ in range(200): rows[random.randrange(len(rows))] = f"{random.randrange(10**6)},edit,{random.randint(0,9)},{random.random():.2f}\n"
        rows += [f"{len(rows)+i},{random.choice(['ber','par'])},{random.randint(0,99999)},{random.random()*1000:.3f}\n" for i in range(5000)]
        with open(os.path.join(d, f"day{day:02d}.csv"), "w") as f: f.write("id,site,count,value\n" + "".join(rows))

def srctree():
    d = os.path.join(data, "srctree"); os.makedirs(d, exist_ok=True)
    commits = subprocess.run(["git", "-C", repo, "log", "--format=%h", "-n", "81"], capture_output=True, text=True, check=True).stdout.split()
    for k, c in enumerate(commits[::20][:5]):
        blob = subprocess.run(["git", "-C", repo, "archive", "--format=tar", c],
                              capture_output=True, check=True).stdout
        with tarfile.open(fileobj=io.BytesIO(blob)) as t:
            for m in t.getmembers():
                if not m.isfile() or ".." in m.name.split("/") or m.name.startswith("/"): continue
                out = os.path.join(d, f"v{k}", m.name); os.makedirs(os.path.dirname(out), exist_ok=True)
                with open(out, "wb") as f: f.write(t.extractfile(m).read())

sets = sys.argv[4:] or ["finetune", "growlog", "snapshots", "srctree"]
for fn in [globals()[s] for s in sets]: fn(); print(fn.__name__, "done", flush=True)
