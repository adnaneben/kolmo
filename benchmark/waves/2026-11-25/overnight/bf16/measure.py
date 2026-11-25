"""One-thread, whole-file comparison against an already-loaded ZipNN worker. No network."""
import argparse, csv, hashlib, json, random, subprocess, sys, tempfile, time
from pathlib import Path

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def zip_operation(factory, mode, source, target):
    z = factory(input_format="byte", bytearray_dtype="bfloat16", threads=1)
    data = Path(source).read_bytes()
    frame = z.compress(data) if mode == "c" else z.decompress(data)
    Path(target).write_bytes(bytes(frame))

def zip_worker():
    from zipnn import ZipNN
    print("READY", flush=True)
    for line in sys.stdin:
        mode, source, target = json.loads(line)
        t0 = time.perf_counter_ns()
        zip_operation(ZipNN, mode, source, target)
        print(time.perf_counter_ns() - t0, flush=True)

if __name__ == "__main__" and sys.argv[1:] == ["--zip-worker"]:
    zip_worker()
    sys.exit(0)

if len(sys.argv) == 5 and sys.argv[1] == "--zip-once":
    from zipnn import ZipNN
    zip_operation(ZipNN, *sys.argv[2:])
    sys.exit(0)

p = argparse.ArgumentParser()
p.add_argument("--kolmo", required=True)
p.add_argument("--main", required=True)
p.add_argument("--zip-python", required=True)
p.add_argument("--inputs", nargs="+", required=True)
p.add_argument("--results", required=True, type=Path)
p.add_argument("--rounds", type=int, default=3)
p.add_argument("--cold-only", action="store_true", help="Run matched fresh-process ZipNN arms only")
args = p.parse_args()
args.results.mkdir(exist_ok=True, parents=True)
worker = None if args.cold_only else subprocess.Popen([args.zip_python, __file__, "--zip-worker"], text=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
if worker: assert worker.stdout.readline().strip() == "READY"
def zip_request(mode, src, dst):
    worker.stdin.write(json.dumps([mode, str(src), str(dst)]) + "\n")
    worker.stdin.flush()
    return int(worker.stdout.readline())

def timed(cmd):
    start = time.perf_counter_ns()
    subprocess.run(cmd, stdout=subprocess.DEVNULL, check=True)
    return time.perf_counter_ns() - start

fields = "source input_bytes input_sha256 round order method encode_ns decode_ns stored_bytes stored_sha256 restored_sha256".split()
with (args.results / ("cold.csv" if args.cold_only else "raw.csv")).open("w", newline="") as result, tempfile.TemporaryDirectory() as tmp:
    writer = csv.DictWriter(result, fields)
    writer.writeheader()
    tmp = Path(tmp)
    for source in map(Path, args.inputs):
        original_hash = digest(source)
        size = source.stat().st_size
        assert size <= 100_000_000
        header = source.read_bytes()[:8]
        header_size = int.from_bytes(header, "little")
        with source.open("rb") as f:
            f.seek(8); tensor_map = json.loads(f.read(header_size))
        tensors = [v for k,v in tensor_map.items() if k != "__metadata__"]
        assert all(v["dtype"] == "BF16" for v in tensors)
        assert 8 + header_size + max(v["data_offsets"][1] for v in tensors) == size
        methods = ["ZipNN fresh CLI"] if args.cold_only else ["#104-L1", "#104-L3", "#104-L7", "main-L3", "ZipNN"]
        for round_no in range(1,args.rounds+1):
            order = methods.copy()
            random.Random(104 * round_no + size).shuffle(order)
            for pos, method in enumerate(order):
                packed, restored = tmp/"frame", tmp/"restored"
                packed.unlink(missing_ok=True)
                restored.unlink(missing_ok=True)
                if method == "ZipNN fresh CLI":
                    e_ns = timed([args.zip_python, __file__, "--zip-once", "c", source, packed])
                    d_ns = timed([args.zip_python, __file__, "--zip-once", "d", packed, restored])
                elif method == "ZipNN":
                    e_ns = zip_request("c", source, packed)
                    d_ns = zip_request("d", packed, restored)
                else:
                    binary = args.main if method.startswith("main") else args.kolmo
                    e_ns = timed([binary, "-q", "-T1", "-" + method[-1], "-o", str(packed), str(source)])
                    d_ns = timed([binary, "-q", "-T1", "-d", "-o", str(restored), str(packed)])
                restored_hash = digest(restored)
                assert restored_hash == original_hash
                row = dict(zip(fields,[source.name,size,original_hash,round_no,pos,method,e_ns,d_ns,packed.stat().st_size,digest(packed),restored_hash]))
                writer.writerow(row); result.flush()
                print(source.name, round_no, method, row["stored_bytes"],e_ns,d_ns,flush=True)
if worker:
    worker.stdin.close()
    assert worker.wait() == 0
