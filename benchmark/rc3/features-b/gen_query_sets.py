"""Builds the two generated files of the query benchmark (features-b report). Run with python3 -I.

usage: python3 -I gen_query_sets.py OUTDIR
  sensor.csv    3,000,000 rows of 6 stations (timestamp, station, co2, temp_c, humidity, pressure_hpa, pm25, status),
                153,379,795 bytes. Seed 1.
  sensor.jsonl  1,000,000 JSON objects (ts, station, co2, temp, status, tags), 87,542,646 bytes. Seed 5.
The downloaded files: owid-co2-data.csv (https://raw.githubusercontent.com/owid/co2-data/master/owid-co2-data.csv,
15,242,657 bytes on 16 November 2026) and hdfs256.log (the first 256 MiB of HDFS.log of loghub's HDFS_v1, cut at the
last whole line: https://zenodo.org/records/8196385/files/HDFS_v1.zip). """
import json, math, os, random, sys

out = sys.argv[1]

def sensor_csv(n=3000000):
    random.seed(1)
    with open(os.path.join(out, "sensor.csv"), "w") as f:
        f.write("timestamp,station,co2,temp_c,humidity,pressure_hpa,pm25,status\n")
        t = 1700000000; st = ["ber-01", "ber-02", "par-07", "lis-03", "osl-11", "mad-04"]
        state = {s: [415.0 + random.random() * 20, 12 + random.random() * 5, 55.0, 1013.0, 8.0] for s in st}
        for i in range(n):
            s = st[i % len(st)]; v = state[s]
            if i % len(st) == 0: t += random.choice((10, 10, 10, 11, 9))
            v[0] += random.gauss(0, 0.8); v[1] += random.gauss(0, 0.05) + 0.01 * math.sin(i / 5000)
            v[2] = min(100, max(0, v[2] + random.gauss(0, 0.3))); v[3] += random.gauss(0, 0.05); v[4] = max(0, v[4] + random.gauss(0, 0.4))
            stat = "ok" if random.random() > 0.002 else random.choice(("calib", "fault", "maint"))
            f.write(f"{t},{s},{v[0]:.1f},{v[1]:.2f},{v[2]:.1f},{v[3]:.2f},{v[4]:.1f},{stat}\n")

def sensor_jsonl(n=1000000):
    random.seed(5); st = ["ber-01", "par-07", "lis-03", "osl-11"]; t = 1700000000; c = 415.0
    with open(os.path.join(out, "sensor.jsonl"), "w") as f:
        for i in range(n):
            c += random.gauss(0, 0.8); t += 3
            s = "ok" if random.random() > 0.001 else random.choice(["calib", "fault", "maint"])
            f.write(json.dumps({"ts": t, "station": st[i % 4], "co2": round(c, 1), "temp": round(12 + random.random() * 3, 2),
                                "status": s, "tags": ["a", "b"] if i % 7 == 0 else []}, separators=(",", ":")) + "\n")

sensor_csv(); sensor_jsonl()
