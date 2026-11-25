"""Round-trip direct ZipNN on the payload while preserving the SafeTensors header."""
import argparse
from hashlib import sha256
from importlib.metadata import version
import json
from pathlib import Path
import time

from zipnn import ZipNN

def run(path, repeats):
    source = path.read_bytes()
    if len(source) < 8:
        raise ValueError('missing SafeTensors signature')
    offset = 8 + int.from_bytes(source[:8], 'little')
    if not 8 < offset < len(source):
        raise ValueError('invalid SafeTensors header extent')
    header = source[:offset]
    metadata = json.loads(header[8:])
    if {e['dtype'] for k, e in metadata.items() if k != '__metadata__'} != {'BF16'}:
        raise ValueError('reference requires homogeneous BF16 tensors')
    payload = source[offset:]
    if len(payload) % 2:
        raise ValueError('invalid BF16 length')
    for repeat in range(-1,repeats):
        codec = ZipNN(input_format='byte', bytearray_dtype='bfloat16', threads=1)
        start = time.perf_counter()
        stored = bytes(codec.compress(bytearray(payload)))
        encoded = time.perf_counter()
        decoder = ZipNN(input_format='byte', bytearray_dtype='bfloat16', threads=1)
        restored = bytes(decoder.decompress(stored))
        decoded = time.perf_counter()
        if header + restored != source:
            raise RuntimeError(f'round trip failed on {path.name}')
        if repeat >= 0:
            print(json.dumps(dict(input=path.name, input_sha256=sha256(source).hexdigest(),
                                  frame_sha256=sha256(header + stored).hexdigest(),
                                  raw_bytes=len(source), stored_bytes=len(header)+len(stored),
                                  header_bytes=len(header), threads=1, repeat=repeat,
                                  version=version('zipnn'), encode_s=encoded-start, decode_s=decoded-encoded)),
                  flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('paths',nargs='+',type=Path)
    parser.add_argument('--repeats',type=int,default=7)
    args = parser.parse_args()
    for path in args.paths:
        run(path,args.repeats)
