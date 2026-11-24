#!/usr/bin/env python3
"""Add a release's frames to the frozen version-12 corpus (tests/v12), which is kept forever.

COMPATIBILITY.md promises that every later release decodes every frame of tests/v12 to its original. This script
writes the frames of one release into tests/v12/r<release>/ with their originals in tests/v12/r<release>/in/, adds
"frame original" lines to tests/v12/MANIFEST (read by test-v12-corpus through the library) and records every frame in
tests/v12/r<release>/frames.json (read by tests/check_v12_frames.py through the CLI, which also covers the stream
format). Run it once per release, with that release's binary, for example the 0.0.2 tag build:

    python3 tests/make_v12_frames.py --kolmo path/to/kolmo-0.0.2 --release 0.0.2

It never rewrites a frame: a frame already listed is skipped, so adding inputs later only adds files. The inputs are synthetic and made here from a fixed pseudo-random generator (no third-party data, so the
corpus carries no licence beyond the project's), apart from two small executables compiled from a C file below when a
C compiler for that target is present.
"""
import argparse, hashlib, io, json, os, struct, subprocess, sys, tarfile, tempfile, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
CORPUS = os.path.join(HERE, "v12")


class Rng:  # xorshift64*: the same numbers on every Python version and platform
    def __init__(self, seed): self.s = seed or 1
    def next(self):
        s = self.s; s ^= (s >> 12); s ^= (s << 25) & 0xFFFFFFFFFFFFFFFF; s ^= (s >> 27); self.s = s
        return (s * 0x2545F4914F6CDD1D) & 0xFFFFFFFFFFFFFFFF
    def below(self, n): return self.next() % n
    def pick(self, seq): return seq[self.below(len(seq))]
    def bytes(self, n): return bytes(self.next() & 255 for _ in range(n))


WORDS = ("the of and to in is was that for it with as his on be at by had not are but from or have an they which one you "
         "were her all she there would their we him been has when who will more no if out so said what up its about into "
         "than them can only other new some could time these two may then do first any my now such like our over man me "
         "even most made after also did many before must through back years where much your way well down should because "
         "each just those people how too little state good very make world still own see men work long get here between "
         "both life being under never day same another know while last might us great old year off come since against go "
         "came right used take three river engine harbour letter window morning garden silver").split()


def text(rng, n):
    out, line = [], []
    while sum(len(x) for x in out) < n:
        w = rng.pick(WORDS)
        if not line: w = w.capitalize()
        line.append(w)
        if rng.below(12) == 0: line[-1] += rng.pick([",", ";", ""])
        if rng.below(14) == 0 or len(" ".join(line)) > 70:
            out.append(" ".join(line) + rng.pick([".", ".", "?", "!"]) + "\n"); line = []
    return "".join(out).encode()


def csv_table(rng, rows):
    cities = ["Lyon", "Oslo", "Porto", "Kyoto", "Quito", "Perth", "Tunis", "Lagos", "Hanoi", "Leeds"]
    b = ["id,date,city,temp_c,pressure_hpa,count,flag\n"]
    t, p = 12.0, 1013.0
    for i in range(rows):
        t += (rng.below(201) - 100) / 100; p += (rng.below(41) - 20) / 10
        b.append("%d,2026-%02d-%02d,%s,%.2f,%.1f,%d,%s\n" % (1000 + i, 1 + i // 28 % 12, 1 + i % 28, rng.pick(cities), t, p,
                                                           rng.below(5000), rng.pick(["Y", "N"])))
    return "".join(b).encode()


def json_lines(rng, n):
    out = []
    for i in range(n):
        out.append(json.dumps({"id": i, "user": "u%05d" % rng.below(3000), "ok": rng.below(9) > 0,
                               "score": rng.below(100000) / 100, "tags": [rng.pick(WORDS) for _ in range(rng.below(4))],
                               "ts": 1790000000 + i * 37 + rng.below(30)}, separators=(",", ":")))
    return ("\n".join(out) + "\n").encode()


def json_minified(rng, n):
    return json.dumps({"items": [{"k": i, "name": rng.pick(WORDS), "v": [rng.below(1000) for _ in range(3)]}
                                 for i in range(n)], "count": n}, separators=(",", ":")).encode()


def log_lines(rng, n):
    lv = ["INFO", "INFO", "INFO", "WARN", "DEBUG", "ERROR"]
    msgs = ["accepted connection from 10.%d.%d.%d port %d", "request /api/v2/items/%d took %d ms status %d",
            "cache miss for key user:%d shard %d", "worker %d finished job %d in %d ms"]
    out, ts = [], 1_790_000_000_000
    for i in range(n):
        ts += rng.below(900)
        m = rng.pick(msgs); k = m.count("%d")
        out.append("%d.%03d %s [svc-%d] %s\n" % (ts // 1000, ts % 1000, rng.pick(lv), rng.below(4),
                                               m % tuple(rng.below(60000) for _ in range(k))))
    return "".join(out).encode()


def ints(rng, n, fmt):
    v, out = 1000, []
    for i in range(n):
        v += rng.below(21) - 10
        out.append(v if fmt != "<q" else 1_790_000_000_000 + i * 1000 + rng.below(50))
    return struct.pack("<%d%s" % (n, fmt[1]), *out)


def floats(rng, n):
    x, out = 20.0, []
    for _ in range(n):
        x += (rng.below(2001) - 1000) / 10000; out.append(round(x, 3))
    return struct.pack("<%dd" % n, *out)


def npy(arr_bytes, descr, shape):
    hdr = "{'descr': '%s', 'fortran_order': False, 'shape': (%s), }" % (descr, ", ".join(map(str, shape)) + ("," if len(shape) == 1 else ""))
    hdr = hdr + " " * (63 - (len(hdr) + 10) % 64) + "\n"
    return b"\x93NUMPY\x01\x00" + struct.pack("<H", len(hdr)) + hdr.encode() + arr_bytes


def image8(rng, w, h):
    px = bytearray()
    for y in range(h):
        for x in range(w):
            px.append(((x * 3 + y * 2) // 4 + ((x // 32 + y // 32) % 2) * 40 + rng.below(7)) & 255)
    return bytes(px)


def pgm(px, w, h, maxv=255):
    return b"P5\n%d %d\n%d\n" % (w, h, maxv) + px


def image16(rng, w, h):
    return b"".join(struct.pack(">H", (x * 40 + y * 25 + rng.below(30)) & 0xFFFF) for y in range(h) for x in range(w))


def pcm16(rng, frames, ch):
    import math
    out = []
    for i in range(frames):
        for c in range(ch):
            out.append(int(9000 * math.sin(i * (0.031 + 0.007 * c)) + 3000 * math.sin(i * 0.0011)) + rng.below(64) - 32)
    return out


def wav(rng, frames=12000, ch=2, rate=16000):
    data = struct.pack("<%dh" % (frames * ch), *pcm16(rng, frames, ch))
    fmt = struct.pack("<HHIIHH", 1, ch, rate, rate * ch * 2, ch * 2, 16)
    return b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(data)) + data


def aiff(rng, frames=12000, ch=2, rate=16000):
    data = struct.pack(">%dh" % (frames * ch), *pcm16(rng, frames, ch))
    e = 16383 + 13; m = rate << (63 - 13)   # 80-bit extended for 16000 Hz
    comm = struct.pack(">hIh", ch, frames, 16) + struct.pack(">HQ", e, m)
    ssnd = struct.pack(">II", 0, 0) + data
    body = b"AIFF" + b"COMM" + struct.pack(">I", len(comm)) + comm + b"SSND" + struct.pack(">I", len(ssnd)) + ssnd
    return b"FORM" + struct.pack(">I", len(body)) + body


def y4m(rng, w=64, h=48, n=10):
    out = [b"YUV4MPEG2 W%d H%d F25:1 Ip A1:1 C420jpeg\n" % (w, h)]
    for f in range(n):
        y = bytes(((x + f * 2) * 3 + yy * 2 + rng.below(5)) & 255 for yy in range(h) for x in range(w))
        uv = bytes((128 + (x + yy + f) // 8) & 255 for yy in range(h // 2) for x in range(w // 2))
        out.append(b"FRAME\n" + y + uv + uv)
    return b"".join(out)


def fastq(rng, n):
    out = []
    for i in range(n):
        seq = "".join(rng.pick("ACGT") for _ in range(100))
        q = "".join(chr(33 + max(2, min(40, 36 - j // 12 + rng.below(5) - 2))) for j in range(100))
        out.append("@RUN7:1:%d:%d:%d/1\n%s\n+\n%s\n" % (1 + i // 400, rng.below(20000), rng.below(20000), seq, q))
    return "".join(out).encode()


def fasta(rng, n):
    s = []
    for _ in range(n):
        s.append(rng.pick("ACGT" if rng.below(50) else "N"))
    seq = "".join(s)
    return (">chrT synthetic\n" + "\n".join(seq[i:i + 60] for i in range(0, len(seq), 60)) + "\n").encode()


def records(rng, n):  # fixed 24-byte records: id u32, time u32, x f32, y f32, kind u16, pad u16, value u32
    return b"".join(struct.pack("<IIffHHI", i, 1790000000 + 3 * i, i * 0.5, 100 - i * 0.25, rng.below(4), 0, rng.below(100000))
                    for i in range(n))


def image12(rng, w, h):  # 12-bit samples in 16-bit big-endian words: the 16-bit image transform (tflag 6)
    return b"".join(struct.pack(">H", ((x * 4 + y * 3) // 2 + ((x // 40 + y // 40) % 2) * 300 + rng.below(9)) & 0x0FFF)
                    for y in range(h) for x in range(w))


def photo8(rng, w, h):  # a smooth field (bilinear over a coarse random grid) with a disc and mild noise: kimg (codec 31)
    gw, gh, cell = w // 32 + 2, h // 32 + 2, 32
    grid = [[40 + rng.below(176) for _ in range(gw)] for _ in range(gh)]
    px = bytearray()
    for y in range(h):
        gy, fy = y // cell, y % cell
        for x in range(w):
            gx, fx = x // cell, x % cell
            top = grid[gy][gx] * (cell - fx) + grid[gy][gx + 1] * fx
            bot = grid[gy + 1][gx] * (cell - fx) + grid[gy + 1][gx + 1] * fx
            v = (top * (cell - fy) + bot * fy) // (cell * cell)
            if (x - w // 2) ** 2 + (y - h // 2) ** 2 < (h // 4) ** 2: v = 230 - (x - w // 2) // 3
            px.append(max(0, min(255, v + rng.below(3) - 1)))
    return bytes(px)


def x86_code(rng, n):  # instruction-like bytes with many E8 rel32 calls to a few hundred functions: the x86 filter (tflag 8)
    funcs = [0x1000 + 64 * rng.below(20000) for _ in range(300)]
    ops = [b"\x55\x48\x89\xe5", b"\x48\x83\xec\x20", b"\x89\x7d\xfc", b"\x8b\x45\xfc", b"\x48\x8b\x45\xf0", b"\x01\xd0",
           b"\x83\xc0\x01", b"\x5d\xc3", b"\x48\x89\xc7", b"\x31\xc0", b"\x0f\xaf\xc2", b"\x39\xc8"]
    out = bytearray()
    while len(out) < n:
        k = rng.below(10)
        if k < 3: out += b"\xe8" + struct.pack("<I", (rng.pick(funcs) - (len(out) + 5)) & 0xFFFFFFFF)
        elif k == 3: out += b"\x0f\x84" + struct.pack("<i", rng.below(200) - 100)
        else: out += rng.pick(ops)
    return bytes(out[:n])


def names_csv(rng, rows):  # delimited text the table transform does not help: the typed transpose (plan 1, tflag 2)
    out = ["name,city,word,note,code\n"]
    for _ in range(rows):
        out.append("%s %s,%s,%s,%s %s %s,%s%s\n" % (rng.pick(WORDS).capitalize(), rng.pick(WORDS).capitalize(), rng.pick(WORDS),
                   rng.pick(WORDS), rng.pick(WORDS), rng.pick(WORDS), rng.pick(WORDS), rng.pick("ABCDEFGH"), rng.pick(WORDS)))
    return "".join(out).encode()


def pairs_csv(rng, rows):  # two columns (too few for GR6+): ultra codes the columns with one model each (codec 25)
    return "".join("%s %s,%d\n" % (rng.pick(WORDS), rng.pick(WORDS), 100000 + i * 7 + rng.below(5)) for i in range(rows)).encode()


def derived_csv(rng, cap):  # a time series with derived columns (per capita, running total, growth, price x volume): GR6+
    rows = ["country,year,population,co2,co2_per_capita,cumulative_co2,co2_growth_abs,price,volume,value,flag\n"]
    n, c = len(rows[0]), 0
    names = ["Aland", "Borduria", "Carpathia", "Doria", "Elbonia", "Freedonia", "Genovia", "Hyrkania"]
    while n + 4096 < cap:
        pop, co2, cum, prev = 100000 + rng.below(9000000), 1000 + rng.below(90000), 0, -1
        for y in range(1900, 2024):
            if n + 200 >= cap: break
            pop = max(1000, pop + rng.below(20001) - 5000); co2 = max(10, co2 + rng.below(2001) - 900); cum += co2
            pc = (co2 * 1000000000 // pop + 500) // 1000
            s = "%s,%d,%d,%d.%03d,%d.%03d,%d.%03d," % (names[c], y, pop, co2 // 1000, co2 % 1000, pc // 1000, pc % 1000, cum // 1000, cum % 1000)
            if prev >= 0:
                d = co2 - prev; s += "%s%d.%03d" % ("-" if d < 0 else "", abs(d) // 1000, abs(d) % 1000)
            pr, vol = 100 + rng.below(900), 1 + rng.below(500)
            s += ",%d.%02d,%d,%d.%02d,%s\n" % (pr // 100, pr % 100, vol, pr * vol // 100, pr * vol % 100, "" if rng.below(40) else "est")
            prev = co2; rows.append(s); n += len(s)
        c = (c + 1) % 8
    return "".join(rows).encode()


def ragged_csv(rng, rows):  # short rows, text in numeric columns, empty fields, long text: the table's exceptions
    out = ["id,price,qty,total,label,comment"]
    for i in range(rows):
        pr, q = rng.below(10000), 1 + rng.below(9)
        f = ["%d" % i, "%d.%02d" % (pr // 100, pr % 100), "%d" % q, "%d.%02d" % (pr * q // 100, pr * q % 100), rng.pick(WORDS),
             " ".join(rng.pick(WORDS) for _ in range(60)) if rng.below(50) == 0 else rng.pick(WORDS)]
        if rng.below(40) == 0: f = f[:3]
        if rng.below(30) == 0: f[2] = ""
        if rng.below(60) == 0: f[1] = "n/a"
        out.append(",".join(f))
    return ("\n".join(out) + "\n").encode()


def stations_csv(rng):  # grouped series (a group column): the table's grouped delta modes 2, 4 and 5
    out = ["station,day,tmin,tmax,rain,count"]
    for s in range(20):
        tmin = rng.below(100); tmax = tmin + 50; c = 1000; acc = 0
        for d in range(200):
            tmin += rng.below(5) - 2; tmax += rng.below(5) - 2; acc += rng.below(7); c += acc
            out.append("ST%03d,%d,%d.%d,%d.%d,%d.%d0,%d" % (s, 20260101 + d, tmin // 10, tmin % 10, tmax // 10, tmax % 10,
                                                          rng.below(30) // 10, rng.below(10), c))
    return ("\n".join(out) + "\n").encode()


def growth_csv(rng):  # raw-value columns (mode 0) and values whose trailing zeros the table drops
    out = ["region,month,sales,growth_pct,code,price"]
    for s in range(12):
        v, prev = 10000 + rng.below(90000), None
        for m in range(240):
            v = max(100, v + rng.below(2001) - 900); p = rng.below(100000)
            gp = "" if prev is None else "%.2f" % ((v - prev) / prev * 100)
            out.append("R%02d,%d,%d,%s,%d,%s" % (s, m, v, gp, rng.below(1 << 30) - (1 << 29),
                                                 ("%d.%d" % (p // 10, p % 10)) if rng.below(3) else "%d.50" % (p // 10)))
            prev = v
    return ("\n".join(out) + "\n").encode()


def long_field_csv(rng, rows):  # a table with one text field and one text-in-a-number field over 64 KiB: 0.0.2 wrote
    out = ["id,date,city,temp,note"]  # such blocks but its decoder refused them (fixed after 0.0.2; FORMAT.md 20.5)
    for i in range(rows):
        out.append("%d,2026-01-%02d,%s,%d.%02d,%s" % (i, 1 + i % 28, rng.pick(["Lyon", "Oslo", "Porto"]), rng.below(30),
                                                     rng.below(100), rng.pick(WORDS)))
    out[rows - 500] = "%d,2026-01-01,Lyon,1.25,%s" % (rows - 501, "x" * 70000)
    out[rows - 300] = "%d,2026-01-02,Oslo,%s,ok" % (rows - 301, "y" * 70000)
    return ("\n".join(out) + "\n").encode()


def quoted_csv(rng, rows):  # quoted fields and brackets holding the delimiter: the quote-aware transpose (plan 1, tflag 1)
    out = ["name,city,word,note,code\n"]
    for _ in range(rows):
        out.append('"%s %s",%s,"%s, %s",[%s %s],%s%s\n' % (rng.pick(WORDS).capitalize(), rng.pick(WORDS).capitalize(),
                   rng.pick(WORDS), rng.pick(WORDS), rng.pick(WORDS), rng.pick(WORDS), rng.pick(WORDS), rng.pick("ABCDEFGH"),
                   rng.pick(WORDS)))
    return "".join(out).encode()


def photo_rgb(rng, w, h, correlated):  # 3-channel smooth images: kimg (codec 31) with 3 channels, and its colour transform
    gw, gh, cell = w // 32 + 2, h // 32 + 2, 32
    grids = [[[40 + rng.below(150 if correlated else 176) for _ in range(gw)] for _ in range(gh)] for _ in range(1 if correlated else 3)]
    px = bytearray()
    for y in range(h):
        gy, fy = y // cell, y % cell
        for x in range(w):
            gx, fx = x // cell, x % cell
            vs = []
            for g in grids:
                top = g[gy][gx] * (cell - fx) + g[gy][gx + 1] * fx; bot = g[gy + 1][gx] * (cell - fx) + g[gy + 1][gx + 1] * fx
                vs.append((top * (cell - fy) + bot * fy) // (cell * cell))
            if correlated:
                v = vs[0] + rng.below(5) - 2; vs = [v + 20, v, v - 15 + (x >> 5)]
            else:
                vs = [v + rng.below(3) - 1 for v in vs]
            px += bytes(max(0, min(255, v)) for v in vs)
    return b"P6\n%d %d\n255\n" % (w, h) + bytes(px)


C_SOURCE = r"""
#include <stdio.h>
#include <string.h>
static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
static void rev(char *s) { size_t n = strlen(s); for (size_t i = 0; i < n / 2; i++) { char t = s[i]; s[i] = s[n-1-i]; s[n-1-i] = t; } }
int table[4096];
int main(int argc, char **argv) {
    for (int i = 0; i < 4096; i++) table[i] = fib(i % 20) * i;
    char buf[256]; snprintf(buf, sizeof buf, "%s %d", argc > 1 ? argv[1] : "frozen", table[argc]);
    rev(buf); puts(buf); return 0;
}
"""


def big_c_source(n=2500):  # enough code for a >= 1 MiB executable, the size at which Kolmo plans code sections apart
    src = ["#include <stdio.h>", "int f0(int a, int b) { return a - b; }"]
    for i in range(1, n):
        src.append("int f%d(int a, int b) { int s = 0; for (int i = 0; i < a; i++) { s += (i * %d) ^ b; if (s > %d) s -= a * %d; } "
                   "return s + f%d(b, a - 1); }" % (i, i % 97 + 1, 1000 + i, i % 13 + 1, i - 1))
    src.append('int main(int c, char **v) { printf("%%d\\n", f%d(c, 3)); return 0; }' % (n - 1))
    return "\n".join(src) + "\n"


def executable(arch, source=C_SOURCE):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "x.c"); exe = os.path.join(d, "x")
        open(src, "w").write(source)
        r = subprocess.run(["clang", "-O1", "-arch", arch, src, "-o", exe], capture_output=True)
        if r.returncode: return None
        return open(exe, "rb").read()


def tar_of(members):
    b = io.BytesIO()
    with tarfile.open(fileobj=b, mode="w", format=tarfile.USTAR_FORMAT) as t:
        for name, data in members:
            ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = 1790000000; ti.mode = 0o644
            ti.uid = ti.gid = 1000; ti.uname = ti.gname = "kolmo"
            t.addfile(ti, io.BytesIO(data))
    return b.getvalue()


def inputs():
    r = lambda s: Rng(0x9E3779B97F4A7C15 ^ s)
    d = {
        "empty": b"",
        "tiny.txt": b"hello, frozen frames\n",
        "text.txt": text(r(1), 100_000),
        "table.csv": csv_table(r(2), 3000),
        "events.jsonl": json_lines(r(3), 1500),
        "minified.json": json_minified(r(4), 2500),
        "service.log": log_lines(r(5), 2500),
        "series.i32": ints(r(6), 30000, "<i"),
        "stamps.i64": ints(r(7), 12000, "<q"),
        "values.f64": floats(r(8), 12000),
        "values.npy": npy(floats(r(9), 10000), "<f8", (10000,)),
        "gray.pgm": pgm(image8(r(10), 256, 192), 256, 192),
        "gray16.pgm": pgm(image16(r(11), 128, 96), 128, 96, 65535),
        "tone.wav": wav(r(12)),
        "tone.aiff": aiff(r(13)),
        "clip.y4m": y4m(r(14)),
        "reads.fastq": fastq(r(15), 600),
        "genome.fa": fasta(r(16), 60_000),
        "records.bin": records(r(17), 6000),
        "random.bin": r(18).bytes(16384),
        "zeros.bin": bytes(1 << 20),
    }
    for arch, name in (("arm64", "prog-arm64"), ("x86_64", "prog-x86_64")):
        exe = executable(arch)
        if exe: d[name] = exe
        exe = executable(arch, big_c_source(5000))
        if exe: d[name.replace("prog", "bigprog")] = exe
    d["gray-big.pgm"] = pgm(image8(r(20), 1024, 768), 1024, 768)
    d["gray16-big.pgm"] = pgm(image16(r(22), 512, 384), 512, 384, 65535)
    d["bundle.tar"] = tar_of([("a/text.txt", d["text.txt"][:70000]), ("a/table.csv", d["table.csv"]),
                              ("b/series.i32", d["series.i32"]), ("b/service.log", d["service.log"][:90000])])
    # inputs added after 0.0.2 shipped, written by the 0.0.2 tag binary: the block codecs and transforms the inputs above
    # do not reach (FORMAT.md 18.4, COMPATIBILITY.md)
    d["gray12.pgm"] = pgm(image12(r(23), 512, 384), 512, 384, 4095)
    d["photo.pgm"] = pgm(photo8(r(24), 640, 480), 640, 480)
    d["code.x86"] = x86_code(r(25), 1 << 20)
    d["names.csv"] = names_csv(r(26), 4000)
    d["pairs.csv"] = pairs_csv(r(27), 9000)
    d["derived.csv"] = derived_csv(r(28), 300000)
    d["ragged.csv"] = ragged_csv(r(29), 4000)
    d["stations.csv"] = stations_csv(r(30))
    d["growth.csv"] = growth_csv(r(31))
    d["longfield.csv"] = long_field_csv(r(32), 3000)
    d["quoted.csv"] = quoted_csv(r(33), 4000)
    d["rgb.ppm"] = photo_rgb(r(34), 320, 240, False)
    d["rgb-corr.ppm"] = photo_rgb(r(35), 512, 384, True)
    mixed = d["text.txt"] + d["table.csv"] + d["series.i32"] + d["gray.pgm"] + d["service.log"] + d["records.bin"]
    d["mixed.bin"] = (mixed * 3)[: 3 << 20]   # 2.3 MiB: several blocks at level 3, several segments with --stream=1
    return d


LEVELS = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "U"]
# larger inputs, at the levels where they take a route the small ones do not (executable code sections, the image
# model, 16-bit samples as fixed records, several blocks)
SOME = {"bigprog-arm64": ["3", "9"], "bigprog-x86_64": ["3", "9"], "gray-big.pgm": ["U"],
        "gray16-big.pgm": ["3", "U"], "mixed.bin": ["3"], "bundle.tar": ["3", "9"],
        "gray12.pgm": ["3", "9"], "photo.pgm": ["9", "U"], "code.x86": ["3", "9"], "names.csv": ["2", "3"],
        "pairs.csv": ["3", "U"], "derived.csv": ["3", "9", "U"], "ragged.csv": ["3"], "stations.csv": ["3", "9"],
        "growth.csv": ["3"], "longfield.csv": ["3", "9"], "quoted.csv": ["3"],
        "rgb.ppm": ["9"], "rgb-corr.ppm": ["9"]}


def frame_summary(f):
    """header fields and per-block (codec, tflag, plan type) of a CZN1 frame, or the segments of a CZNS stream"""
    if f[:4] == b"CZNS":
        segs, p = [], 12
        while True:
            (n,) = struct.unpack_from("<Q", f, p); p += 8
            if n == 0: break
            segs.append(frame_summary(f[p:p + n])); p += n
        return {"stream": True, "segments": segs}
    ver, mode, flags, memd = f[4], f[5], f[10], f[11]
    orig, nb, bs = struct.unpack_from("<QII", f, 12)
    blocks, po = [], 32 + 16 * nb
    for i in range(nb):
        raw, comp, _, codec, tflag = struct.unpack_from("<IIIBB", f, 32 + 16 * i)
        blocks.append([codec, tflag, f[po] if comp else None]); po += comp
    s = {"version": ver, "mode": mode, "flags": flags, "memd": memd, "orig": orig, "blocks": blocks}
    if po != len(f): s["trailer_bytes"] = len(f) - po
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kolmo", required=True); ap.add_argument("--release", required=True)
    ap.add_argument("--check-with", help="decoder for the round-trip check (default: --kolmo). For frames a release "
                    "writes but cannot read itself (FORMAT.md 20.5), name a build that has the fix")
    a = ap.parse_args()
    rel = "r" + a.release; out = os.path.join(CORPUS, rel); ind = os.path.join(out, "in")
    os.makedirs(ind, exist_ok=True)
    jpath = os.path.join(out, "frames.json")
    man = json.load(open(jpath)) if os.path.exists(jpath) else {"release": a.release, "frames": []}
    have = {e["file"] for e in man["frames"]}
    lines = []
    ver = subprocess.run([a.kolmo, "--version"], capture_output=True, text=True).stdout.strip()
    with tempfile.TemporaryDirectory() as tmp:
        jobs = []
        for name, data in inputs().items():
            for lv in SOME.get(name, LEVELS):
                jobs.append((name, data, "L" + lv, ["-" + lv]))
            if name == "mixed.bin":
                jobs.append((name, data, "L3-stream1M", ["-3", "--stream=1"]))
            if name == "series.i32":
                jobs.append((name, data, "L3-native-leaf", ["-3", "--native-leaf"]))
            if name in ("values.npy", "tone.wav"):
                jobs.append((name, data, "L3-typed-huf", ["-3", "-T1", "--typed-huf"]))
            if name in ("text.txt", "values.f64"):
                jobs.append((name, data, "U-codec-level1", ["-U", "--codec-level", "1"]))
            if name in ("table.csv", "stations.csv", "derived.csv"):   # queryable archives: column-split tables (tflag 13)
                for lv in ("3", "9"): jobs.append((name, data, "index" + lv, ["--index", "-" + lv]))
        for name, data, tag, opts in jobs:
            fn = "%s.%s.kolmo" % (name, tag)
            if fn in have: continue
            orig = os.path.join(ind, name)   # an input already frozen is reused as it is (the executables depend on
            if os.path.exists(orig): data = open(orig, "rb").read()   # the compiler that built them)
            else: open(orig, "wb").write(data)
            dst = os.path.join(out, fn)
            cmd = [a.kolmo] + opts + [orig, dst] if opts[0] == "--index" else [a.kolmo] + opts + ["-q", "-f", "-T1", orig, "-o", dst]
            r = subprocess.run(cmd, capture_output=True, text=True)
            if r.returncode:
                print("skip", fn, r.stderr.strip()[:200]); continue
            back = os.path.join(tmp, "back")
            r = subprocess.run([a.check_with or a.kolmo, "-d", "-q", "-f", dst, "-o", back], capture_output=True)
            if r.returncode or open(back, "rb").read() != data:
                sys.exit("round trip failed: " + fn)
            f = open(dst, "rb").read()
            man["frames"].append({"file": fn, "input": "in/" + name, "options": " ".join(opts), "written_by": ver,
                                  "frame_sha256": hashlib.sha256(f).hexdigest(), "frame_bytes": len(f),
                                  "decoded_sha256": hashlib.sha256(data).hexdigest(), "decoded_bytes": len(data),
                                  "summary": frame_summary(f)})
            if f[:4] == b"CZN1": lines.append("%s/%s %s/in/%s\n" % (rel, fn, rel, name))   # the library test reads frames, not streams
            print("%-40s %8d -> %8d" % (fn, len(data), len(f)), flush=True)
    man["frames"].sort(key=lambda e: e["file"])
    json.dump(man, open(jpath, "w"), indent=1); open(jpath, "a").write("\n")
    if lines:
        with open(os.path.join(CORPUS, "MANIFEST"), "a") as m: m.writelines(sorted(lines))


if __name__ == "__main__":
    main()
