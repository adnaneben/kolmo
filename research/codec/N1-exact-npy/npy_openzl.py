#!/usr/bin/env python3
"""Experimental exact-NPY-header adapter for pinned OpenZL numeric profiles.

Byte-for-byte NPY reconstruction. OpenZL is invoked on the typed payload only.
All Python, temp I/O, parser, framing, hashing, and zli subprocess cost counts.
"""
from __future__ import annotations
import argparse
import ast
import hashlib
import json
import os
import re
import resource
import struct
import subprocess
import tempfile
from pathlib import Path

MAGIC=b'KNPZL1\0\0'
FRAME=struct.Struct('<8sQIQQ32s')
MAX_HEADER=65536
MAX_ORIGINAL=32 << 20
MAX_FRAME=64 << 20
TYPES=re.compile(r'^([<>])([iu])([248])$')

class FormatError(ValueError): pass

def digest(data: bytes)->bytes:
    return hashlib.sha256(data).digest()

def npy_header(raw:bytes, require_payload:bool=True)->tuple[int,int,str]:
    if len(raw)<10 or raw[:6]!=b'\x93NUMPY': raise FormatError('not NPY')
    major,minor=raw[6:8]
    if minor!=0 or major not in (1,2,3): raise FormatError('unsupported NPY version')
    prefix=10 if major==1 else 12
    if len(raw)<prefix: raise FormatError('truncated NPY length')
    count=int.from_bytes(raw[8:prefix],'little')
    if not (0<count<=MAX_HEADER) or len(raw)<prefix+count:
        raise FormatError('invalid NPY header size')
    try:
        meta=ast.literal_eval(raw[prefix:prefix+count].decode('utf-8' if major==3 else 'latin-1').strip())
    except (ValueError,SyntaxError,UnicodeError,RecursionError,MemoryError) as exc:
        raise FormatError('unparseable NPY metadata') from exc
    if not isinstance(meta,dict) or set(meta)!={'descr','fortran_order','shape'}:
        raise FormatError('unsupported NPY metadata')
    if meta['fortran_order'] is not False: raise FormatError('Fortran order not supported')
    m=TYPES.fullmatch(meta['descr']) if isinstance(meta['descr'],str) else None
    if m is None: raise FormatError('explicitly endian-tagged int16/int32/int64 only')
    endian,kind,width_text=m.groups()
    width=int(width_text)
    shape=meta['shape']
    if not isinstance(shape,tuple) or len(shape)>8: raise FormatError('bad NPY shape')
    cells=1
    for dim in shape:
        if type(dim) is not int or dim<0 or dim>MAX_ORIGINAL:
            raise FormatError('invalid NPY dimension')
        cells*=dim
        if cells*width>MAX_ORIGINAL: raise FormatError('array too large')
    size=cells*width
    header_end=prefix+count
    if header_end+size>MAX_ORIGINAL: raise FormatError('original file too large')
    if require_payload and len(raw)!=header_end+size:
        raise FormatError('NPY payload size does not match shape')
    profile=('le-' if endian=='<' else 'be-')+kind+str(width*8)
    return header_end,size,profile

def read_bounded(path:Path,max_size:int)->bytes:
    if path.stat().st_size>max_size: raise FormatError('file exceeds size limit')
    data=path.read_bytes()
    if len(data)>max_size: raise FormatError('file grew past size limit')
    return data

def zli_action(zli:Path,action:str,src:Path,dst:Path,*,profile:str='',level:int=9)->None:
    if action=='compress':
        args=[str(zli),'compress','-p',profile,'-l',str(level),'-f',str(src),'-o',str(dst)]
    else:
        args=[str(zli),'decompress','-f',str(src),'-o',str(dst)]
    try:
        run=subprocess.run(args,timeout=75,capture_output=True,check=False,
             preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_FSIZE,(MAX_FRAME,MAX_FRAME)))
    except subprocess.TimeoutExpired as exc:
        raise FormatError('OpenZL child exceeded 75 seconds') from exc
    if run.returncode:
        err=run.stderr.decode('utf-8',errors='replace')[-500:]
        raise FormatError(f'OpenZL failed ({run.returncode}): {err}')
    if not dst.is_file(): raise FormatError('OpenZL output missing')

def encode(data:bytes,zli:Path,level:int)->bytes:
    if level not in (6,7,9): raise FormatError('only registered 6/7/9 levels allowed')
    head,payload_size,profile=npy_header(data)
    with tempfile.TemporaryDirectory(prefix='knpy-') as tmp:
        inp,out=Path(tmp)/'input.bin',Path(tmp)/'encoded.zl'
        inp.write_bytes(data[head:])
        zli_action(zli,'compress',inp,out,profile=profile,level=level)
        payload=read_bounded(out,MAX_FRAME)
    frame=FRAME.pack(MAGIC,len(data),head,payload_size,len(payload),digest(data))
    result=frame+data[:head]+payload
    if len(result)>MAX_FRAME: raise FormatError('encoded frame exceeds limit')
    return result

def decode(data:bytes,zli:Path)->bytes:
    if len(data)<FRAME.size or len(data)>MAX_FRAME: raise FormatError('invalid framed size')
    magic,n,h,raw_n,packed_n,expected=FRAME.unpack_from(data)
    if magic!=MAGIC or n>MAX_ORIGINAL or h>MAX_HEADER+12 or packed_n>MAX_FRAME:
        raise FormatError('invalid frame header')
    if FRAME.size+h+packed_n!=len(data) or h+raw_n!=n:
        raise FormatError('truncated or trailing frame bytes')
    header=data[FRAME.size:FRAME.size+h]
    parsed_h,parsed_raw,_=npy_header(header,require_payload=False)
    if (parsed_h,parsed_raw)!=(h,raw_n): raise FormatError('inconsistent metadata')
    with tempfile.TemporaryDirectory(prefix='knpy-') as tmp:
        inp,out=Path(tmp)/'encoded.zl',Path(tmp)/'decoded.bin'
        inp.write_bytes(data[FRAME.size+h:])
        zli_action(zli,'decompress',inp,out)
        decoded=read_bounded(out,MAX_ORIGINAL)
    if len(decoded)!=raw_n: raise FormatError('decoded length mismatch')
    original=header+decoded
    if digest(original)!=expected: raise FormatError('original-byte SHA256 mismatch')
    return original

def main()->None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('inspect','compress','decompress'))
    p.add_argument('source',type=Path)
    p.add_argument('destination',nargs='?',type=Path)
    p.add_argument('--zli',type=Path)
    p.add_argument('--level',type=int,default=9)
    x=p.parse_args()
    if x.action=='inspect':
        data=read_bounded(x.source,MAX_ORIGINAL)
        h,n,profile=npy_header(data)
        print(json.dumps(dict(input_sha256=digest(data).hex(),original_bytes=len(data),
                              header_bytes=h,payload_bytes=n,openzl_profile=profile,
                              status='NOT_MEASURED_ADAPTER')))
        return
    if x.destination is None or x.zli is None:
        p.error('action requires destination and --zli')
    if not x.zli.is_file(): p.error('zli executable missing')
    data=read_bounded(x.source,MAX_ORIGINAL if x.action=='compress' else MAX_FRAME)
    result=encode(data,x.zli,x.level) if x.action=='compress' else decode(data,x.zli)
    x.destination.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=x.destination.parent,prefix='.knpy-',delete=False) as tmp:
        staged=Path(tmp.name)
        try: tmp.write(result)
        except BaseException:
            staged.unlink(missing_ok=True)
            raise
    os.replace(staged,x.destination)

if __name__=='__main__': main()
