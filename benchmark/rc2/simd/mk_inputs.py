"""Deterministic inputs for the SIMD/assembly study (benchmark/rc2/SIMD-ASM.md).

usage: python3 -I mk_inputs.py OUT_DIR
Writes trades.csv (numeric CSV with decimal columns: the path of the AVX2 decimal formatter), events.ndjson
(log-like JSON records) and walk_i32.npy (a 16,777,216-value int32 random walk: a typed integer array).
Fixed seed; only the Python standard library and numpy.
"""
import json, os, sys
import numpy as np

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
rng = np.random.default_rng(20261116)

# CSV: 1,000,000 rows, timestamp, id, price (2 dp), qty, venue, rate (4 dp), signed delta (3 dp)
n = 1_000_000
ts = 1_731_000_000_000 + np.cumsum(rng.integers(0, 2000, n))
oid = rng.integers(10_000_000, 99_999_999, n)
price = np.round(100 + np.cumsum(rng.normal(0, 0.05, n)), 2)
qty = rng.integers(1, 5000, n)
venue = np.array(["XNYS", "XNAS", "ARCX", "BATS", "IEXG"])[rng.integers(0, 5, n)]
rate = np.round(rng.uniform(0, 5, n), 4)
delta = np.round(rng.normal(0, 30, n), 3)
with open(os.path.join(out, "trades.csv"), "w") as f:
    f.write("ts,order_id,price,qty,venue,rate,delta\n")
    for i in range(n):
        f.write(f"{ts[i]},{oid[i]},{price[i]:.2f},{qty[i]},{venue[i]},{rate[i]:.4f},{delta[i]:.3f}\n")

# NDJSON: 600,000 records
m = 600_000
lv = np.array(["info", "warn", "error", "debug"])[rng.choice(4, m, p=[.7, .15, .05, .1])]
svc = np.array(["api", "auth", "billing", "search", "ingest", "web"])[rng.integers(0, 6, m)]
lat = np.round(rng.lognormal(3, 1, m), 3)
st = np.array([200, 200, 200, 201, 204, 301, 400, 404, 500])[rng.integers(0, 9, m)]
uid = rng.integers(1, 200_000, m)
t0 = 1_731_000_000
with open(os.path.join(out, "events.ndjson"), "w") as f:
    for i in range(m):
        f.write(json.dumps({"ts": int(t0 + i // 3), "level": str(lv[i]), "service": str(svc[i]), "user": int(uid[i]),
                            "status": int(st[i]), "latency_ms": float(lat[i]),
                            "path": f"/v1/{svc[i]}/items/{int(uid[i]) % 977}"}, separators=(",", ":")) + "\n")

# int32 random walk, 2^24 values
np.save(os.path.join(out, "walk_i32.npy"), np.cumsum(rng.integers(-3000, 3001, 1 << 24)).astype(np.int32))
