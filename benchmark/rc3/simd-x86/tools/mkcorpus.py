import sys, os, struct, random, numpy as np
from PIL import Image
dl, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
sil = os.path.join(dl, "silesia")
def cp(src, dst, n=None):
    b = open(src, "rb").read()
    open(os.path.join(out, dst), "wb").write(b if n is None else b[:n])
cp(f"{sil}/dickens", "text_dickens.txt")
cp(f"{sil}/webster", "text_webster.txt", 16 << 20)
cp(f"{sil}/xml", "text_xml.xml")
# tables / logs: generated CSV and log lines
rnd = random.Random(7)
rows = ["ts,sym,px,qty,side,venue,flag"]
t = 1763290000000; px = 10000
for i in range(400000):
    t += rnd.randint(1, 50); px += rnd.randint(-5, 5)
    rows.append(f"{t},S{rnd.randint(0,40)},{px/100:.2f},{rnd.randint(1,900)},{'BS'[rnd.random()<.5]},V{rnd.randint(0,5)},{rnd.randint(0,3)}")
open(os.path.join(out, "table_trades.csv"), "w").write("\n".join(rows) + "\n")
paths = ["/api/item", "/api/user", "/static/img", "/login", "/search"]
lines = []
for i in range(300000):
    lines.append(f"2026-11-16T{(i//3600)%24:02d}:{(i//60)%60:02d}:{i%60:02d}.{rnd.randint(0,999):03d}Z host{rnd.randint(0,12)} {'GET' if rnd.random()<.8 else 'POST'} {rnd.choice(paths)}/{rnd.randint(1,99999)} {rnd.choice([200,200,200,304,404,500])} {rnd.randint(1,4000)}ms ua=Mozilla/5.0 rid={rnd.getrandbits(48):012x}\n")
open(os.path.join(out, "log_access.log"), "w").write("".join(lines))
# images 8-bit as npy (H,W,3) and grey (H,W)
def npy(arr, name):
    np.save(os.path.join(out, name), arr)
imgs = [np.asarray(Image.open(os.path.join(dl, f"kodim{i}.png")).convert("RGB")) for i in ("01","05","13","23")]
npy(np.concatenate(imgs, axis=0), "img8_rgb.npy")
npy(np.concatenate([np.asarray(Image.fromarray(a).convert("L")) for a in imgs], axis=0), "img8_grey.npy")
# 16-bit images (rc1: mr and x-ray as u16)
for nm, w in (("mr", 512), ("x-ray", None)):
    b = open(f"{sil}/{nm}", "rb").read()
    a = np.frombuffer(b[: len(b)//2*2], dtype="<u2")
    npy(a, f"img16_{nm.replace('-','')}.npy")
# audio: concatenated ESC-50 clips as one WAV (16-bit mono 44.1k)
pcm = b""
for f in sorted(os.listdir(dl)):
    if f.endswith(".wav"):
        d = open(os.path.join(dl, f), "rb").read(); i = d.find(b"data"); pcm += d[i+8:]
hdr = b"RIFF" + struct.pack("<I", 36+len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 44100, 88200, 2, 16) + b"data" + struct.pack("<I", len(pcm))
open(os.path.join(out, "audio_esc.wav"), "wb").write(hdr + pcm)
# video: first 60 frames of akiyo and foreman
for nm in ("akiyo", "foreman"):
    d = open(os.path.join(dl, f"{nm}_cif.y4m"), "rb").read()
    fs = 352*288*3//2 + 6; h = d.index(b"\n") + 1
    open(os.path.join(out, f"video_{nm}.y4m"), "wb").write(d[:h + 60*fs])
# numeric
r = np.random.default_rng(3)
npy(np.cumsum(r.integers(-300, 301, 4_000_000)).astype("<i4"), "num_walk_i32.npy")
npy(np.round(20 + np.cumsum(r.normal(0, .1, 2_000_000)), 3).astype("<f8"), "num_f64.npy")
cp(f"{sil}/sao", "num_sao.bin")
cp(f"{sil}/osdb", "table_osdb.bin")
cp(f"{sil}/mozilla", "bin_mozilla", 16 << 20)
