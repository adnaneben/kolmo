"""The 0.0.2 command line (docs/LEVELS.md): every option, the gzip/xz/zstd conventions, every removed 0.0.1 form, exit
codes, standard input and output, several files, overwrite protection, and round trips at every level.

usage: python3 tests/test_cli.py [build/kolmo]
Environment: KOLMO_REF=path/to/kolmo-0.0.1 also checks that the 0.0.1 binary decodes every level of this one, and that
this one decodes 0.0.1's modes (frame format unchanged). Without it those checks are skipped."""
import os
import pty
import select
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
K = str(Path(sys.argv[1]).resolve()) if len(sys.argv) > 1 else str(ROOT / "build/kolmo")
REF = os.environ.get("KOLMO_REF")
ENV = {k: v for k, v in os.environ.items() if k != "KOLMO_MEMLIMIT"}
fails = []


def run(*args, input=None, env=None, cwd=None):
    return subprocess.run([K, *map(str, args)], input=input, capture_output=True, env=env or ENV, cwd=cwd)


def check(cond, what):
    if not cond:
        fails.append(what)
        print("FAIL", what)


def expect(r, code, what, err=None):
    ok = r.returncode == code and (err is None or err.encode() in r.stderr)
    check(ok, f"{what}: exit {r.returncode} (want {code}){'' if err is None else ', want ' + repr(err)}; stderr {r.stderr[:200]!r}")
    return ok


def data(n=200_000, seed=7):
    import random
    rnd = random.Random(seed)
    words = ["level", "kolmo", "scale", "the", "fast", "small", "block", "of", "table", "line"]
    text = " ".join(rnd.choice(words) for _ in range(n // 5)).encode()[: n // 2]
    csv = "".join(f"{i},{rnd.randint(0, 999)},{rnd.random():.3f}\n" for i in range(n // 40)).encode()
    return (text + b"\n" + csv)[:n]


LEVEL_FORMS = {   # every spelling of each level gives the same bytes
    1: [["-1"], ["-F"], ["--fastest"], ["--level", "1"], ["--level=1"], ["--level", "fastest"]],
    2: [["-2"], ["--level=2"]],
    3: [[], ["-3"], ["-L"], ["--light"], ["--level", "3"], ["--level=light"]],
    4: [["-4"], ["--level", "4"]],
    5: [["-5"], ["-B"], ["--balanced"], ["--level=5"]],
    6: [["-6"], ["--level", "6"]],
    7: [["-7"], ["-C"], ["--compact"], ["--level=compact"]],
    8: [["-8"], ["--level=8"]],
    9: [["-9"], ["-X"], ["--max"], ["--level", "max"]],
    "U": [["-U"], ["--ultra"], ["--level", "ultra"], ["--level=ultra"]],
}

with tempfile.TemporaryDirectory() as tmp:
    T = Path(tmp)
    src = T / "in.txt"
    payload = data()
    src.write_bytes(payload)
    small = T / "small.txt"
    small.write_bytes(payload[:40_000])

    # ---- version and help
    r = run("-V"); expect(r, 0, "-V"); check(r.stdout.startswith(b"kolmo 0.0.2 "), f"-V prints {r.stdout!r}")
    expect(run("--version"), 0, "--version")
    r = run("-h"); expect(r, 0, "-h")
    h = r.stdout.decode()
    for frag in ["-1  -F  --fastest", "-3  -L  --light", "-5  -B  --balanced", "-7  -C  --compact", "-9  -X  --max",
                 "-U      --ultra", "--level N", "--decompress", "--stdout", "--output", "--keep", "--rm", "--force",
                 "--test", "--list", "--threads", "--memlimit", "--quiet", "--verbose", "--block-size", "--analysis",
                 "--stream", "--range", "--codec-level", "--target", "--calib", "--native-leaf", "--typed-huf",
                 "--gpu", "--x-tables", "calibrate"]:
        check(frag in h, f"-h lacks {frag!r}")
    expect(run("--help"), 0, "--help")

    # ---- every level, every spelling: the same bytes, a round trip, and the default is 3
    frames = {}
    for lv, forms in LEVEL_FORMS.items():
        inp = small if lv == "U" else src
        outs = set()
        for k, form in enumerate(forms):
            z = T / f"lv{lv}.{k}.kolmo"
            if expect(run(*form, "-T", "2", inp, "-o", z), 0, f"level {lv} {form}"):
                outs.add(z.read_bytes())
        check(len(outs) == 1, f"level {lv}: spellings give different bytes")
        frames[lv] = outs.pop() if outs else b""
        z = T / f"lv{lv}.0.kolmo"
        back = T / f"lv{lv}.back"
        expect(run("-d", z, "-o", back), 0, f"level {lv} decode")
        check(back.read_bytes() == inp.read_bytes(), f"level {lv} round trip")
        check(frames[lv][:5] == b"CZN1\x0c", f"level {lv}: container version 12")
    # the named anchors differ on this input (whether every in-between level is distinct is the measuring lane's
    # concern: on this small input 6 and 7 can coincide)
    for lv, nx in ((1, 3), (3, 5), (5, 7), (7, 9), (9, "U")):
        check(frames[lv] != frames[nx], f"levels {lv} and {nx} give the same bytes")
    # threads never change the output
    for lv in (1, 3, 9):
        z1, z4 = T / "t1.kolmo", T / "t4.kolmo"
        run(f"-{lv}", "-T1", src, "-o", z1, "-f"); run(f"-{lv}", "-T", "4", src, "-o", z4, "-f")
        check(z1.read_bytes() == z4.read_bytes(), f"level {lv}: -T1 and -T4 differ")
    # the last level option wins
    z = T / "last.kolmo"
    expect(run("-9", "-1", src, "-o", z), 0, "-9 -1"); check(z.read_bytes() == frames[1], "-9 -1 is not level 1")
    expect(run("-U", "--level", "2", src, "-o", z, "-f"), 0, "-U --level 2"); check(z.read_bytes() == frames[2], "-U --level 2 is not level 2")
    expect(run("-1", "-C", src, "-o", z, "-f"), 0, "-1 -C"); check(z.read_bytes() == frames[7], "-1 -C is not level 7")
    # combined short options and attached values
    expect(run("-9T2", src, "-o", z, "-f"), 0, "-9T2"); check(z.read_bytes() == frames[9], "-9T2")
    expect(run("-kfT4", "-5", src, "-o", z), 0, "-kfT4"); check(z.read_bytes() == frames[5], "-kfT4")
    expect(run("-fo" + str(z), "-7", src), 0, "-oFILE"); check(z.read_bytes() == frames[7], "-oFILE")
    expect(run("--threads=3", "--output=" + str(z), "-f", src), 0, "--opt=value"); check(z.read_bytes() == frames[3], "--opt=value")
    r = run("-dc", T / "lv3.0.kolmo"); expect(r, 0, "-dc"); check(r.stdout == payload, "-dc output")
    r = run("-dcT2", T / "lv1.0.kolmo"); expect(r, 0, "-dcT2"); check(r.stdout == payload, "-dcT2 output")

    # ---- file conventions: suffix, keep, --rm, -f, -o, multiple files
    d = T / "files"; d.mkdir()
    a, b, c = d / "a.txt", d / "b.txt", d / "c.txt"
    for i, f in enumerate((a, b, c)):
        f.write_bytes(payload[i * 1000:] + bytes([i]))
    os.chmod(a, 0o640)
    os.utime(a, (1_600_000_000, 1_600_000_000))
    expect(run(a), 0, "kolmo FILE")
    check(a.exists() and (d / "a.txt.kolmo").exists(), "kolmo FILE keeps FILE and writes FILE.kolmo")
    st = (d / "a.txt.kolmo").stat()
    check(stat.S_IMODE(st.st_mode) == 0o640, f"permissions not copied: {oct(st.st_mode)}")
    check(int(st.st_mtime) == 1_600_000_000, f"mtime not copied: {st.st_mtime}")
    check(not list(d.glob(".kolmo-*")), "staging files left behind")
    # overwrite protection
    before = (d / "a.txt.kolmo").read_bytes()
    a.write_bytes(b"changed" * 100)
    expect(run(a), 1, "existing output without -f", "a.txt.kolmo: already exists")
    check((d / "a.txt.kolmo").read_bytes() == before, "existing output changed without -f")
    expect(run("-f", a), 0, "-f overwrites")
    check((d / "a.txt.kolmo").read_bytes() != before, "-f did not overwrite")
    a.unlink()
    expect(run("-d", d / "a.txt.kolmo"), 0, "-d FILE.kolmo")
    check(a.read_bytes() == b"changed" * 100 and (d / "a.txt.kolmo").exists(), "-d writes FILE and keeps FILE.kolmo")
    expect(run("-d", d / "a.txt.kolmo"), 1, "-d onto an existing file", "a.txt: already exists")
    expect(run("-d", a), 1, "-d of a name without .kolmo", "unknown suffix")
    expect(run(d / "a.txt.kolmo"), 1, "compressing a .kolmo file", "already has the .kolmo suffix")
    expect(run("-d", d / "a.txt.kolmo", "-o", d / "named.out"), 0, "-d -o")
    check((d / "named.out").read_bytes() == b"changed" * 100, "-d -o content")
    # --rm and -k (the last wins)
    expect(run("--rm", b), 0, "--rm"); check(not b.exists() and (d / "b.txt.kolmo").exists(), "--rm removes the input")
    expect(run("--rm", "-d", d / "b.txt.kolmo"), 0, "-d --rm"); check(b.exists() and not (d / "b.txt.kolmo").exists(), "-d --rm")
    expect(run("--rm", "-k", b), 0, "--rm -k"); check(b.exists(), "--rm -k kept the input")
    r = run("--rm", "-c", c); expect(r, 0, "--rm -c"); check(c.exists() and b"--rm is ignored" in r.stderr, "--rm with -c")
    r = run("-q", "--rm", "-c", c); check(r.returncode == 0 and not r.stderr, "-q silences the --rm warning")
    # several files, and processing continues past an error with exit status 1
    m = T / "multi"; m.mkdir()
    names = [m / f"f{i}.txt" for i in range(3)]
    for i, f in enumerate(names):
        f.write_bytes(payload[: 10_000 * (i + 1)])
    r = run(names[0], m / "missing.txt", names[1], names[2])
    expect(r, 1, "several files with one missing", "missing.txt")
    check(all((m / f"f{i}.txt.kolmo").exists() for i in range(3)), "files after the error were not processed")
    for f in names:
        f.unlink()
    expect(run("-d", *[m / f"f{i}.txt.kolmo" for i in range(3)]), 0, "-d of several files")
    check(all(names[i].read_bytes() == payload[: 10_000 * (i + 1)] for i in range(3)), "several files round trip")
    expect(run("-o", T / "x.kolmo", names[0], names[1]), 2, "-o with two inputs")
    r = run("-dc", *[m / f"f{i}.txt.kolmo" for i in range(3)])
    check(r.returncode == 0 and r.stdout == b"".join(payload[: 10_000 * (i + 1)] for i in range(3)), "-dc of several files concatenates")
    # -c with several inputs concatenates their frames, and -d decodes concatenated frames in order (as zstd)
    r = run("-c", names[0], names[1]); expect(r, 0, "-c with several inputs")
    cat = T / "cat.kolmo"; cat.write_bytes(r.stdout)
    r = run("-dc", cat); check(r.returncode == 0 and r.stdout == names[0].read_bytes() + names[1].read_bytes(), "concatenated frames decode")
    r = run("-d", input=frames[1] + frames[9]); check(r.returncode == 0 and r.stdout == payload + payload, "concatenated frames from a pipe")
    expect(run("-t", cat), 0, "-t of concatenated frames")
    r = run("-l", cat); check(r.returncode == 0 and b"2 frames" in r.stdout, f"-l of concatenated frames: {r.stdout!r}")
    (T / "trail.kolmo").write_bytes(frames[3] + b"garbage!")
    expect(run("-dc", T / "trail.kolmo"), 1, "trailing garbage after a frame")
    expect(run("-t", T / "trail.kolmo"), 1, "-t with trailing garbage")
    expect(run("--range", "0:10", cat), 1, "--range of concatenated frames")
    expect(run("--range", "0:99999999999", T / "lv3.0.kolmo"), 0, "--range longer than the file")
    r = run("--range", "99999999999:10", T / "lv3.0.kolmo")
    expect(r, 1, "--range past the end", "outside the decoded data")
    # the output is never the input file, in any path, even with -f
    same = T / "same.txt"; same.write_bytes(payload[:5000])
    for args in (["-f", "-o", same, same], ["-f", "--stream", "-o", same, same], ["-9", "-f", "-o", same, same]):
        expect(run(*args), 1, f"output is the input {args[:2]}", "the output is the input")
        check(same.read_bytes() == payload[:5000], f"output = input destroyed the input {args[:2]}")
    sh = subprocess.run(f"'{K}' -f -c '{same}' >> '{same}'", shell=True, capture_output=True)
    check(sh.returncode == 1 and same.read_bytes() == payload[:5000], "-c appending to the input")
    expect(run(m), 1, "a directory", "is a directory")
    # a file whose name starts with '-', after --
    dash = T / "-x"
    dash.write_bytes(b"dash file " * 50)
    expect(run("--", "-x", cwd=T), 0, "-- -x")
    check((T / "-x.kolmo").exists(), "-- -x wrote -x.kolmo")

    # ---- standard input and output
    r = run(input=payload); expect(r, 0, "stdin to stdout"); check(r.stdout == frames[3], "stdin compress = level 3 bytes")
    r = run("-d", input=frames[3]); expect(r, 0, "-d stdin"); check(r.stdout == payload, "-d stdin output")
    r = run("-", input=payload); check(r.returncode == 0 and r.stdout == frames[3], "'-' is standard input")
    r = run("-9", "-c", src); check(r.returncode == 0 and r.stdout == frames[9], "-c")
    r = run("-o", "-", src); check(r.returncode == 0 and r.stdout == frames[3], "-o -")
    out = T / "from_stdin.kolmo"
    expect(run("-1", "-o", out, input=payload), 0, "stdin to -o"); check(out.read_bytes() == frames[1], "stdin -o bytes")
    r = run("-t", input=frames[5]); expect(r, 0, "-t stdin")
    # pipes through a real shell
    sh = subprocess.run(f"'{K}' -9 < '{src}' | '{K}' -d | cmp - '{src}'", shell=True)
    check(sh.returncode == 0, "shell pipe -9 | -d")

    # ---- -t and -l
    expect(run("-t", T / "lv7.0.kolmo"), 0, "-t good")
    bad = T / "bad.kolmo"
    fb = bytearray(frames[3]); fb[len(fb) // 2] ^= 0x55; bad.write_bytes(bytes(fb))
    expect(run("-t", bad), 1, "-t corrupt")
    expect(run("-t", T / "lv3.0.kolmo", bad, T / "lv1.0.kolmo"), 1, "-t several with one corrupt")
    check(not (T / "bad").exists(), "-t wrote something")
    r = run("-v", "-t", T / "lv9.0.kolmo"); check(r.returncode == 0 and b"OK" in r.stderr, "-t -v reports")
    r = run("-l", T / "lv1.0.kolmo", T / "lv9.0.kolmo"); expect(r, 0, "-l")
    lst = r.stdout.decode()
    check("compressed" in lst and "original" in lst and str(len(payload)) in lst and "lv9.0.kolmo" in lst, f"-l output {lst!r}")
    check("rt:" in lst and "max:" in lst, f"-l shows the engine: {lst!r}")
    expect(run("-l", input=frames[3]), 2, "-l from standard input")
    expect(run("-l", bad, T / "lv1.0.kolmo"), 0 if bad.stat().st_size else 1, "-l of a damaged payload (header intact)")
    expect(run("-l", src), 1, "-l of a file that is not kolmo")

    # ---- -q and -v
    r = run("-v", src, "-o", T / "v.kolmo"); check(r.returncode == 0 and b"->" in r.stderr and b"%" in r.stderr, "-v reports")
    r = run("-q", "--gpu", "-c", small); check(r.returncode == 0 and b"gpu" not in r.stderr.lower(), "-q silences --gpu warning")

    # ---- terminals: compressed data is not written to (or read from) one without -f
    def on_pty(args, stdin_tty=False, inp=b""):
        m_fd, s_fd = pty.openpty()
        p = subprocess.Popen([K, *map(str, args)], stdout=s_fd, stdin=s_fd if stdin_tty else subprocess.PIPE,
                             stderr=subprocess.PIPE, env=ENV)
        os.close(s_fd)
        if not stdin_tty:
            try:
                p.stdin.write(inp); p.stdin.close()
            except BrokenPipeError:
                pass
        buf = b""
        while True:
            rr, _, _ = select.select([m_fd], [], [], 0.2)
            if rr:
                try:
                    chunk = os.read(m_fd, 65536)
                except OSError:
                    chunk = b""
                if not chunk and p.poll() is not None:
                    break
                buf += chunk
            elif p.poll() is not None:
                break
        err = p.stderr.read(); p.wait(); os.close(m_fd)
        return p.returncode, err, buf
    code, err, _ = on_pty(["-c", src])
    check(code == 1 and b"terminal" in err, f"-c to a terminal: {code} {err!r}")
    code, err, _ = on_pty([], inp=payload)
    check(code == 1 and b"terminal" in err, f"stdin to a terminal: {code} {err!r}")
    code, err, out_ = on_pty(["-f", "-c", small])
    check(code == 0 and len(out_) > 10, f"-f -c to a terminal: {code} {err!r}")
    code, err, out_ = on_pty(["-dc", T / "lv3.0.kolmo"])
    check(code == 0, f"decompressed data to a terminal is allowed: {code} {err!r}")
    code, err, _ = on_pty(["-d"], stdin_tty=True)
    check(code == 1 and b"terminal" in err, f"-d reading a terminal: {code} {err!r}")

    # ---- usage errors: exit 2
    for args, what in [(["--bogus"], "unknown long option"), (["-j"], "unknown short option"), (["-o"], "-o without value"),
                       (["-T"], "-T without value"), (["-T", "abc", src], "-T abc"), (["-T", "-5", src], "-T -5"),
                       (["--threads=x", src], "--threads=x"), (["--level", "99", src], "--level 99"), (["-0", src], "-0"),
                       (["-10", src], "-10"), (["--level", "0", src], "--level 0"), (["--level=fast", src], "--level=fast"),
                       (["--codec-level", "0", src], "--codec-level 0"), (["--codec-level", "x", src], "--codec-level x"),
                       (["--analysis", "lots", src], "--analysis lots"), (["--block-size", "-1", src], "--block-size -1"),
                       (["--range", "abc", T / "lv3.0.kolmo"], "--range abc"), (["--range=1", T / "lv3.0.kolmo"], "--range=1"),
                       (["-c", "-o", T / "q", src], "-c with -o"), (["-t", "-o", T / "q", T / "lv3.0.kolmo"], "-t with -o"),
                       (["--native-leaf", "--typed-huf", src], "--native-leaf with --typed-huf"),
                       (["--memlimit", "lots", src], "--memlimit lots"), (["--stream=abc", src], "--stream=abc"),
                       (["--x-tables=99", src], "--x-tables=99"), (["--keep=1", src], "flag with a value"),
                       (["--level"], "--level without value"), (["--target", "nonsense!!", src], "--target nonsense"),
                       (["-9", "-M", "1M", src], "memlimit below the floor")]:
        expect(run(*args), 2, what)
    expect(run(src, env={**ENV, "KOLMO_MEMLIMIT": "lots"}), 2, "KOLMO_MEMLIMIT=lots", "KOLMO_MEMLIMIT")

    # ---- removed in 0.0.2: exit 2 and a one-line hint
    exists2 = T / "out_existing.kolmo"
    exists2.write_bytes(b"x")
    removed = [(["-m", "speed", src], "-m was removed", "-1 (fastest)"), (["-mspeed", src], "-m was removed", None),
               (["-m", "ratio", src], "-m was removed", None), (["-x", src], "-x was removed", "-U"),
               (["-l", "5", src], "-l LEVEL was removed", "--codec-level"), (["-l5", src], "-l LEVEL was removed", None),
               (["-B", "4", src], "-B MiB was removed", "--block-size"), (["-B0.5", src], "-B MiB was removed", None),
               (["-Q", src], "-Q was removed", "--analysis minimal"), (["-n", src], "-n was removed", "--analysis none"),
               (["-s", src], "-s was removed", "--stream"), (["-S", "2", src], "-S was removed", "--stream=SEGMiB"),
               (["-r", "0:5", T / "lv3.0.kolmo"], "-r was removed", "--range"), (["-D", "d.dict", src], "-D was removed", None),
               (["-t", "x.dict", src], "-t DICT was removed", None), (["-P", "p.prof", src], "-P was removed", None),
               (["train", src], "'kolmo train' was removed", None), (["profile", src], "'kolmo profile' was removed", None),
               ([src, T / "newout"], "'kolmo IN OUT' was removed", "kolmo IN -o OUT"),
               ([src, exists2], "'kolmo IN OUT' was removed", None),
               (["-d", T / "lv3.0.kolmo", T / "newout"], "'kolmo -d IN OUT' was removed", "-o OUT"),
               (["-9", "-x", src], "-x was removed", None)]
    for args, msg, hint in removed:
        r = run(*args)
        if expect(r, 2, f"removed {args[:2]}", msg):
            line = r.stderr.decode().strip()
            check("\n" not in line and "0.0.2" in line, f"removed {args[:2]}: not one line: {line!r}")
            if hint:
                check(hint in line, f"removed {args[:2]}: hint lacks {hint!r}: {line!r}")
    check(not (T / "newout").exists(), "a removed form wrote output")

    # ---- expert options
    rt = T / "rt"; rt.mkdir()

    def roundtrip(args, inp, what):
        z, o = rt / "e.kolmo", rt / "e.out"
        for f in (z, o):
            if f.exists():
                f.unlink()
        if expect(run(*args, inp, "-o", z), 0, what) and expect(run("-d", z, "-o", o), 0, what + " decode"):
            check(o.read_bytes() == inp.read_bytes(), what + " round trip")
        return z.read_bytes() if z.exists() else b""
    roundtrip(["--block-size", "0.0625"], src, "--block-size")
    roundtrip(["--block-size=1", "-9"], src, "--block-size=1 -9")
    for an in ("none", "minimal", "quick", "full"):
        roundtrip(["--analysis", an], src, f"--analysis {an}")
    roundtrip(["-1", "--analysis=full"], src, "-1 --analysis full")
    roundtrip(["--codec-level", "19"], src, "--codec-level 19")
    roundtrip(["-U", "--codec-level", "1"], small, "-U --codec-level 1")
    roundtrip(["--native-leaf"], src, "--native-leaf")
    roundtrip(["--typed-huf", "-T1"], src, "--typed-huf")
    roundtrip(["--target", "size"], small, "--target size")
    roundtrip(["--target=decode>=1GB/s", "-v"], small, "--target=decode>=1GB/s")
    roundtrip(["-U", "--x-tables=full"], small, "-U --x-tables=full")
    roundtrip(["--x-tables", "scaled", "-U"], small, "--x-tables scaled")
    roundtrip(["--gpu", "-q", "-9"], small, "--gpu")
    roundtrip(["-M", "512M"], src, "-M 512M")
    roundtrip(["--memlimit=max", "-9"], src, "--memlimit=max")
    roundtrip(["--memlimit", "auto"], src, "--memlimit auto")
    expect(run(src, "-o", rt / "env.kolmo", env={**ENV, "KOLMO_MEMLIMIT": "1G"}), 0, "KOLMO_MEMLIMIT=1G")
    s1 = roundtrip(["--stream"], src, "--stream")
    check(s1[:4] == b"CZNS", "--stream writes a CZNS stream")
    s2 = roundtrip(["--stream=1", "-5"], src, "--stream=1")
    check(s2[:4] == b"CZNS", "--stream=1 writes a CZNS stream")
    r = run("--stream", "-c", src); check(r.returncode == 0 and r.stdout[:4] == b"CZNS", "--stream -c")
    stream = r.stdout
    r = run("-d", input=stream); check(r.returncode == 0 and r.stdout == payload, "streamed decode from a pipe keeps the magic bytes")
    r = run("--stream", input=payload); check(r.returncode == 0 and r.stdout[:4] == b"CZNS", "--stream from a pipe")
    expect(run("-t", input=stream), 0, "-t of a stream from a pipe")
    (rt / "s.kolmo").write_bytes(stream)
    r = run("-l", rt / "s.kolmo"); check(r.returncode == 0 and b"stream of" in r.stdout, f"-l of a stream: {r.stdout!r}")
    r = run("--range", "1000:500", T / "lv3.0.kolmo"); check(r.returncode == 0 and r.stdout == payload[1000:1500], "--range to stdout")
    r = run("--range=0:10", T / "lv9.0.kolmo", "-o", rt / "range.out")
    check(r.returncode == 0 and (rt / "range.out").read_bytes() == payload[:10], "--range -o")
    expect(run("--range", "0:10", rt / "s.kolmo"), 1, "--range of a stream")

    # ---- inputs of 8 MiB and more take the seekable file path (header and index read, not the whole frame), both ways
    big = rt / "big.csv"
    big.write_bytes(b"".join(b"%d,%d,%d.%02d\n" % (i, i * 7 % 1000, i % 97, i % 100) for i in range(700_000)))
    check(big.stat().st_size >= 8 << 20, "big input is under 8 MiB")
    r = run("-vv", "-T", "2", big)
    check(r.returncode == 0 and b"[file path]" in r.stderr, f"compressing 8+ MiB at level 3 did not use the file path: {r.stderr!r}")
    big.rename(rt / "big.orig")
    r = run("-vv", "-d", rt / "big.csv.kolmo")
    check(r.returncode == 0 and b"[file path]" in r.stderr, f"decoding 8+ MiB did not use the file path: {r.stderr!r}")
    check(big.read_bytes() == (rt / "big.orig").read_bytes(), "file-path round trip")
    (rt / "big2.kolmo").write_bytes((rt / "big.csv.kolmo").read_bytes() + frames[1])
    r = run("-vv", "-dc", rt / "big2.kolmo")
    check(r.returncode == 0 and r.stdout == (rt / "big.orig").read_bytes() + payload, "8+ MiB frame followed by a second frame")

    # ---- failures never clobber an existing destination, even with -f, and leave no staging file
    dest = rt / "keep.out"
    dest.write_bytes(b"precious")
    sb = bytearray(stream); sb[len(sb) // 2] ^= 0xFF; (rt / "bad_stream.kolmo").write_bytes(bytes(sb))
    expect(run("-d", "-f", rt / "bad_stream.kolmo", "-o", dest), 1, "corrupt stream with -f")
    check(dest.read_bytes() == b"precious", "a failed streamed decode clobbered the destination")
    r = run("-d", "-f", "-o", dest, input=bytes(sb)); check(r.returncode == 1 and dest.read_bytes() == b"precious", "failed piped decode clobbered")
    expect(run("-d", "-f", bad, "-o", dest), 1, "corrupt frame with -f")
    check(dest.read_bytes() == b"precious", "a failed decode clobbered the destination")
    expect(run("-d", "-f", src, "-o", dest), 1, "not a kolmo file")
    check(dest.read_bytes() == b"precious" and not list(rt.glob(".kolmo-*")), "failed decodes left staging files or changed output")

    # ---- subcommands still dispatch (calibrate's own usage error; the archive commands in test_features_b_cli.sh)
    expect(run("calibrate", "--bogus"), 2, "calibrate usage")
    expect(run("contents", T / "nonexistent.kol"), 1, "kolmo contents dispatches")
    for old, new in (("a", "archive"), ("x", "extract"), ("l", "contents")):
        r = run(old, src); expect(r, 2, f"kolmo {old}", f"use 'kolmo {new}'")

    # ---- the frame format: 0.0.1 decodes every level, and 0.0.2 decodes 0.0.1 output
    if REF and os.path.exists(REF):
        for lv in LEVEL_FORMS:
            inp = small if lv == "U" else src
            z, o = T / f"lv{lv}.0.kolmo", T / f"ref{lv}.out"
            rr = subprocess.run([REF, "-d", z, o], capture_output=True)
            check(rr.returncode == 0 and o.read_bytes() == inp.read_bytes(), f"kolmo-0.0.1 decodes level {lv}: {rr.stderr[:200]!r}")
        for mode in ("speed", "balanced", "eco", "hc", "ratio", "x"):
            inp = small if mode == "x" else src
            z, o = T / f"ref.{mode}.kolmo", T / f"ref.{mode}.out"
            rr = subprocess.run([REF, *(["-x"] if mode == "x" else ["-m", mode]), inp, z], capture_output=True)
            check(rr.returncode == 0, f"kolmo-0.0.1 -m {mode}")
            expect(run("-d", z, "-o", o), 0, f"0.0.2 decodes 0.0.1 {mode}")
            check(o.exists() and o.read_bytes() == inp.read_bytes(), f"0.0.2 decodes 0.0.1 {mode}: content")
            same = {"speed": 1, "balanced": 3, "eco": 5, "hc": 7, "ratio": 9, "x": "U"}[mode]
            check(z.read_bytes() == frames[same], f"0.0.1 -m {mode} and 0.0.2 level {same} give different bytes")
        print("frame format: checked against", REF)
    else:
        print("frame format: KOLMO_REF not set, 0.0.1 cross-check skipped")

print("test_cli:", "OK" if not fails else f"{len(fails)} failure(s)")
sys.exit(1 if fails else 0)
