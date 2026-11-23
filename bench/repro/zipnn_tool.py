#!/usr/bin/env python3
"""ZipNN as a file-to-file tool for panel.py: whole file as bytes, ZipNN's defaults (method AUTO, byte grouping
by dtype), one thread. Writes the in-process seconds of the (de)compression call to OUT.time, so Python and torch
start-up are not charged to ZipNN.

usage: zipnn_tool.py c|d DTYPE IN OUT      (DTYPE: bfloat16, float32, float16)"""
import sys, time

from zipnn import ZipNN

mode, dtype, src, dst = sys.argv[1:5]
z = ZipNN(input_format="byte", bytearray_dtype=dtype, threads=1)
data = open(src, "rb").read()
t = time.perf_counter()
out = z.compress(data) if mode == "c" else z.decompress(data)
el = time.perf_counter() - t
open(dst, "wb").write(bytes(out))
open(dst + ".time", "w").write(repr(el))
