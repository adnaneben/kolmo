# Capped Ultra comparison on two previously recorded inputs

The experiment compares ordinary #109 (`258ca7b6`) against `516e1dca02ac8165ed91ef5068af84e44335a109` built with the optional record splitter. Both encoders used Ultra, one thread and a 256 MiB budget. The block size was selected automatically. Only the candidate had `KOLMO_BENCH_READ_CUTS=1`. All frames were restored using the independently built ordinary #109 CLI.

The two 8.65 MB windows were taken unchanged from the earlier [capped comparison](../../review/capped-large.csv), with an exact SHA-256 check for each input and the corresponding previous stock or opted-in frame. With fixed files, two alternating pairs per input and a shared ARM64 Mac, these trials support the measured transfer for these settings. They do not measure an unconstrained 128 MiB block crossing or other producers.

| Input | Archive bytes, stock → guarded | Encode ms | Stock decode ms | Combined MB/s |
|---|---:|---:|---:|---:|
| SRR1770413 | 1,658,579 → 1,351,098 | 12,277.0 → 979.6 | 15,642.4 → 1,222.6 | 0.310 → 3.928 |
| SRR2584863 | 1,856,619 → 1,671,261 | 20,089.1 → 1,709.4 | 19,371.8 → 1,949.7 | 0.219 → 2.364 |

All eight complete frames matched their previously recorded hashes and restored their exact input hash with the stock decoder. Combined throughput uses original input bytes divided by median paired encode-plus-decode duration.

[Scorecard](scores.csv) · [Phase times and SHA-256 values](trials.csv) · [Exact build hashes and settings](manifest.json) · [Replay](replay.py)

From a checkout with the saved source windows:

```sh
python3 replay.py --stock /path/to/default-109 --candidate /path/to/trial-cli \
  --inputs /path/to/saved-windows --out /tmp/kolmo-guard-comparison \
  --reference ../../review/capped-large.csv --candidate-sha 516e1dca02ac8165ed91ef5068af84e44335a109
```
