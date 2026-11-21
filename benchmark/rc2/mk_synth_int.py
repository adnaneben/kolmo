"""Synthetic integer arrays that stress the integer node's choices: widths 2/4/8, signed and unsigned, common
factors, constant data, int64 extremes, short arrays and the 16,384-value sampling boundary.

usage: mk_synth_int.py OUT_DIR
Writes .npy files (declared type, used by --typed-huf and the typed planner) and the same data raw (.raw),
which the default planner sees without a declaration. Deterministic (fixed seed).
"""
import os
import sys

import numpy as np

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
rng = np.random.default_rng(20261115)


def walk(n, step, dt):
    return np.cumsum(rng.integers(-step, step + 1, n)).astype(dt)


cases = {}
for n in (17, 300, 16384, 16385, 70000, 1_200_000):
    cases[f"walk_i16_{n}"] = walk(n, 40, np.int16)
    cases[f"walk_i32_{n}"] = walk(n, 4000, np.int32)
    cases[f"walk_i64_{n}"] = walk(n, 10**9, np.int64)
    cases[f"rand_u16_{n}"] = rng.integers(0, 2**16, n, dtype=np.uint16)
    cases[f"rand_u32_{n}"] = rng.integers(0, 2**32, n, dtype=np.uint32)
    cases[f"gcd12_i32_{n}"] = (walk(n, 50, np.int32) * 12).astype(np.int32)
    cases[f"gcd1000_i64_{n}"] = (np.arange(n, dtype=np.int64) * 1000 + 1_700_000_000_000)
    cases[f"line_i64_{n}"] = np.arange(n, dtype=np.int64) * 7 - 3
    cases[f"quad_i64_{n}"] = (np.arange(n, dtype=np.int64) ** 2)
    cases[f"const_i32_{n}"] = np.full(n, -5, dtype=np.int32)
    cases[f"few_u16_{n}"] = rng.choice(np.array([3, 300, 3000, 30000], dtype=np.uint16), n)
    cases[f"ext_i64_{n}"] = rng.choice(np.array([np.iinfo(np.int64).min, np.iinfo(np.int64).max, 0, -1], dtype=np.int64), n)
    cases[f"ext_i16_{n}"] = rng.choice(np.array([-32768, 32767, 0, -1], dtype=np.int16), n)
    cases[f"runs_u32_{n}"] = np.repeat(rng.integers(0, 1000, n // 64 + 1, dtype=np.uint32), 64)[:n]
    cases[f"stereo_i16_{n}"] = np.stack([walk(n, 300, np.int16), walk(n, 300, np.int16)], axis=1)

for name, a in cases.items():
    np.save(os.path.join(out, name + ".npy"), a)
    a.tofile(os.path.join(out, name + ".raw"))
print(len(cases), "arrays,", 2 * len(cases), "files in", out)
