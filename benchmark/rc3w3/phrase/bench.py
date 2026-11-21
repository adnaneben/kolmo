#!/usr/bin/env python3
"""rc3w3 phrase lane: Kolmo's own modes on the study corpora, sizes and wall times, one thread count per run.

    python3 -I benchmark/rc3w3/phrase/bench.py OUT.jsonl THREADS MODE[,MODE...] FILE...

MODE is a kolmo option string ("-x", "-m hc", "-m ratio"). Each file is compressed and decompressed once (the runs are
long: -x codes 0.2-0.5 MB/s per thread), the output is compared with the input, and one JSON line is written per
file and mode. Every run of one table uses the same thread count (-T)."""
import json, os, subprocess, sys, time, tempfile, filecmp

def main():
    out, threads, modes, files = sys.argv[1], sys.argv[2], sys.argv[3].split(','), sys.argv[4:]
    kolmo = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build', 'kolmo')
    tmp = tempfile.mkdtemp(prefix='phrbench')
    with open(out, 'a') as fo:
        for f in files:
            for m in modes:
                c, d = os.path.join(tmp, 'c.kz'), os.path.join(tmp, 'd.out')
                t0 = time.perf_counter()
                subprocess.run([kolmo] + m.split() + ['-T', threads, f, c], check=True)
                t1 = time.perf_counter()
                subprocess.run([kolmo, '-d', '-T', threads, c, d], check=True)
                t2 = time.perf_counter()
                ok = filecmp.cmp(f, d, shallow=False)
                r = dict(file=os.path.basename(f), n=os.path.getsize(f), mode=m, threads=int(threads),
                         size=os.path.getsize(c), enc_s=round(t1 - t0, 3), dec_s=round(t2 - t1, 3), ok=ok)
                fo.write(json.dumps(r) + '\n'); fo.flush()
                print(json.dumps(r), flush=True)
                os.remove(c); os.remove(d)
    os.rmdir(tmp)

if __name__ == '__main__':
    main()
