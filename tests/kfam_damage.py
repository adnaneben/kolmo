"""Damaged-frame test for build/kfam-asan (rc2 research codec): python3 -I kfam_damage.py KFAM_ASAN WORK [N]
Compresses a FASTQ and a CSV, flips 1-4 random bits N times each, decodes every copy under ASan/UBSan with a 20 s
limit. Passes when nothing hangs and no sanitizer report appears (rejecting or decoding garbage both count as clean)."""
import random, subprocess, sys
exe, w = sys.argv[1], sys.argv[2]; N = int(sys.argv[3]) if len(sys.argv) > 3 else 400
inputs = {"fastq": b"".join(b"@r%d x/1\nACGTNACGTAACGT\n+\nIIIIIHHHGGIIII\n" % i for i in range(300)),
          "csv": b"country,year,co2,share\n" + b"".join(b"C%d,%d,%d.%d,%d\n" % (i % 7, 1990 + i % 30, i * 37 % 1000, i % 10, i % 97) for i in range(2000))}
bad = 0
for fam, data in inputs.items():
    open(f"{w}/kd.in", "wb").write(data)
    subprocess.run([exe, "-f", fam, f"{w}/kd.in", f"{w}/kd.k"], check=True)
    k = open(f"{w}/kd.k", "rb").read(); rnd = random.Random(1); rej = ok = slow = rep = 0
    for t in range(N):
        b = bytearray(k)
        for _ in range(rnd.randint(1, 4)): b[rnd.randrange(len(b))] ^= 1 << rnd.randrange(8)
        open(f"{w}/kd.bad", "wb").write(b)
        try: r = subprocess.run([exe, "-d", f"{w}/kd.bad", f"{w}/kd.out"], capture_output=True, timeout=20)
        except subprocess.TimeoutExpired: slow += 1; continue
        if b"Sanitizer" in r.stderr or b"runtime error" in r.stderr: rep += 1; print(r.stderr.decode()[:300])
        elif r.returncode: rej += 1
        else: ok += 1
    print(f"{fam}: {N} damaged frames: {rej} rejected, {ok} decoded, {slow} over 20 s, {rep} sanitizer reports")
    bad += slow + rep
sys.exit(1 if bad else 0)
