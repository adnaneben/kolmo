"""Campaign 3 raw inputs (fixed random seeds, nothing downloaded). usage: python3 -I gen_inputs.py OUTDIR
Small inputs that drive build/kolmo into every rc3 route: WAV (codec 19), Y4M (codec 20), long-form numeric tables
(table slack, version 11), wide/narrow CSV (codec 25 parts at -x, column-split blocks), line-structured text (GR6+,
codec 26 at -x), integer series with sparse planes (leaves 21-23), text for block sorting (codec 18), logs, JSON
lines (IDX1 kinds), and sets of similar files for DDP1 archives and profile training."""
import os, random, struct, sys

O = sys.argv[1]; os.makedirs(O, exist_ok=True)
r = random.Random(20261117)

def w(name, data):
    with open(os.path.join(O, name), "wb") as f: f.write(data)

def wav(n, ch, rate=44100, tail=b""):
    import math
    s = []
    for i in range(n):
        for c in range(ch):
            s.append(max(-32768, min(32767, int(8000 * math.sin(i * 0.031 * (c + 1)) + 3000 * math.sin(i * 0.0071 + c) + r.gauss(0, 60)))))
    pcm = struct.pack("<%dh" % len(s), *s)
    return (b"RIFF" + struct.pack("<I", 36 + len(pcm) + len(tail)) + b"WAVEfmt " +
            struct.pack("<IHHIIHH", 16, 1, ch, rate, rate * 2 * ch, 2 * ch, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm + tail)

w("mono.wav", wav(30000, 1)); w("stereo.wav", wav(20000, 2, tail=b"LIST\x04\x00\x00\x00abcd")); w("tiny.wav", wav(800, 1)); w("small.wav", wav(3000, 1)); w("small2.wav", wav(1800, 2))

def y4m(W, H, F, c="420jpeg"):
    b = bytearray(b"YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C%s\n" % (W, H, c.encode()))
    cw, ch = (W // 2, H // 2) if c.startswith("420") else (W, H)
    for f in range(F):
        b += b"FRAME\n" + bytes(((x + 2 * f) * 3 + y * 5 + r.randint(0, 3)) & 255 for y in range(H) for x in range(W))
        b += bytes((128 + k * 9 + ((x + y + f) & 15)) & 255 for k in range(2) for y in range(ch) for x in range(cw))
    return bytes(b)
w("video.y4m", y4m(64, 48, 6)); w("video444.y4m", y4m(32, 32, 4, "444")); w("small.y4m", y4m(48, 32, 5))

# long-form numeric table: many short numbers (table slack: transformed block larger than block + 16 KiB)
rows = ["country,year,value"] + ["%s,%d,%d" % ("C%d" % (i // 60), 1960 + i % 60, r.randint(0, 999)) for i in range(60000)]
w("longform.csv", ("\n".join(rows) + "\n").encode())
rows = ["ts,station,co2,temp,flag"] + ["%d,s%d,%.1f,%.2f,%s" % (1700000000 + i, i % 7, 400 + (i % 97) / 3, (i % 31) - 5, "ok" if i % 13 else "bad") for i in range(12000)]
w("table.csv", ("\n".join(rows) + "\n").encode())
w("table.tsv", ("\n".join(x.replace(",", "\t") for x in rows[:5000]) + "\n").encode())
w("wide.csv", ("\n".join(",".join(str(r.randint(0, 50) * (j + 1)) for j in range(40)) for _ in range(1500)) + "\n").encode())
# line-structured text (GR6+): fields separated by a fixed separator, mixed numbers and words
w("records.txt", "".join("%d %s %d.%02d %s\n" % (i, r.choice(["GET", "PUT", "POST"]), r.randint(0, 99), r.randint(0, 99), r.choice(["alpha", "beta", "gamma"])) for i in range(9000)).encode())
w("semi.txt", "".join("%s;%d;%d\n" % (r.choice(["x", "yy", "zzz"]), i * 3, r.randint(-500, 500)) for i in range(8000)).encode())
# logs and JSON lines
w("app.log", "".join("2026-11-17T10:%02d:%02d.%03dZ host%d INFO req=%d took=%dms path=/api/v1/item/%d\n" % ((i // 60) % 60, i % 60, r.randint(0, 999), i % 5, i, r.randint(1, 900), r.randint(1, 4000)) for i in range(8000)).encode())
w("events.jsonl", "".join('{"id":%d,"user":"u%d","score":%.2f,"tags":["a","b%d"],"ok":%s}\n' % (i, r.randint(1, 300), r.random() * 100, i % 9, "true" if i % 3 else "false") for i in range(6000)).encode())
# text for block sorting
words = ["the", "of", "and", "compress", "kolmo", "block", "sorting", "transform", "stream", "model", "context"]
w("text.txt", " ".join(r.choice(words) + ("\n" if r.random() < 0.1 else "") for _ in range(60000)).encode())
# integer series: sparse high planes (zero-run leaf), spikes (wide planes), timestamps
v = 0; a = []
for i in range(40000):
    v += r.randint(-40, 40); a.append(v if i % 997 else v + (1 << 20))
w("walk.i32", struct.pack("<%di" % len(a), *a))
w("stamps.i64", struct.pack("<20000q", *[1763290000000 + i * 1000 + r.randint(0, 9) for i in range(20000)]))
w("sparse.u16", struct.pack("<50000H", *[(r.randint(1, 255) if r.random() < 0.05 else 0) for _ in range(50000)]))
w("floats.f64", struct.pack("<20000d", *[round(r.gauss(20, 5), 2) for _ in range(20000)]))
import math
w("photo.pgm", b"P5\n320 240\n255\n" + bytes(int(128 + 60 * math.sin(x / 17) + 50 * math.cos(y / 23) + r.randint(0, 5)) & 255 for y in range(240) for x in range(320)))
w("image.pgm", b"P5\n256 128\n255\n" + bytes((x * 3 + y * 5 + r.randint(0, 7)) & 255 for y in range(128) for x in range(256)))
w("fastq.fq", "".join("@r%d/1\n%s\n+\n%s\n" % (i, "".join(r.choice("ACGT") for _ in range(100)), "".join(chr(33 + r.randint(20, 40)) for _ in range(100))) for i in range(1500)).encode())
w("small.txt", b"hello kolmo\n" * 20)
w("empty.bin", b"")
w("random.bin", bytes(r.getrandbits(8) for _ in range(5000)))
# similar files for archives and profiles
os.makedirs(os.path.join(O, "set"), exist_ok=True)
base = rows[:800]
for k in range(6):
    part = base[:1] + [x for i, x in enumerate(base[1:]) if (i + k) % 5] + ["%d,s9,%d.0,0.00,new" % (k, k)]
    with open(os.path.join(O, "set", "snap%d.csv" % k), "wb") as f: f.write(("\n".join(part) + "\n").encode())
