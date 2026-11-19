# C3 and C4: benchmark harness audit (code reading and counts from existing rows)

2026-11-19. Read-only. Line numbers are for `benchmark/rc2/run_cli2.py` and `benchmark/rc3/leaders/*` at `5071f388`. Neither file changed in the two
commits that later appeared on the review branch.

## C3: what the runners record

| question | run_cli2.py (CLI tiers; both Mac reports) | run_spec3.py (specialists, in process) | run_img8.py (8-bit images, in process) |
|---|---|---|---|
| Timing statistic | Fastest only. `best_c` and `best_d` are kept separately, as the minimum over up to 3 rounds. After the first round, it stops once `--budget` (20 s) is spent (L90-110). No per-round times or spread are written. Wall time comes from `perf_counter` around `Popen`+`wait4` (L36-39), so it includes process start. | Fastest only. `timed()` repeats until 3 runs and 0.25 s, or 3 s, or 5,000 runs, and keeps `min` (L39-48). The run counts `c_runs`/`d_runs` are written; there is no spread. | Fastest only (L17-23). The run count is not recorded. |
| Average (time-weighted) RSS | **No.** Only `ru_maxrss` (peak) from `wait4`, for the fastest round (L38, L44, L114-115). | **No RSS at all.** | **No RSS at all.** |
| Input hash | Computed (`sha(path)`, L81) and used to check the round trip (L101). **Not written to the row.** | No. | No. |
| Binary hash | **No.** The env row has machine, OS and load only (L78-79). | Yes, for the library: `lib_sha256` in the env row (L87-89). No versions are recorded for imagecodecs, PyAV, pysam, zipnn or libsndfile. | **No.** There is no env row at all. |
| argv | **No.** Only `label` is written. argv lives in `tools.json`, outside the rows. | No (labels only). | No. |
| Resume key | `(group, basename(file), label)` over rows without `"error"` (L71-76). Thread count is not in the key, nor binary, input hash or argv: a rebuilt binary under the same label reuses old rows, and two inputs with the same basename collide. Error rows stay in the file, and a rerun appends a new row. | **No resume.** A rerun appends rows (`open(..., "a")`, L71). Example: `lead/S1.jsonl` still holds the discarded ZipNN rows marked "ROUND TRIP FAILED", and `SW1.jsonl` holds the rerun. | `(file, label)` (L51-54). Group and threads are not in the key. |
| Busy flag | `others()` (L48-57) runs `ps -Ao pcpu=,pid=,comm=` and lists every process other than itself at >= 50% CPU (`ps` %CPU is a decaying average on macOS). It samples once before the setting's rounds (`busy_before`, L89) and once after (`busy_after`, L117), never during a timed run. | None. | None. |
| Busy rows excluded? | **No.** `leaders/summarize.py` L20 drops only `error` and `roundtrip == false`, and `mac/compare.py` does the same. The `threads` flag (L111-112) is not used to exclude rows either. | n/a | n/a |
| Other | 7-Zip and zpaq are timed through `sh` wrappers (`7z_c.sh`, `zpaq_c.sh`). The timed process also runs `mktemp -d`, `ln` or `cp`, a subshell and `rm -rf`. | The docstring says `run_spec2.py`. The comment at L116-117 says the ZipNN copy is made outside the timed call, but L118 makes it inside (RESULTS.md says inside). | `open(a.out)` for the resume read comes after `open(a.out, "a")` creates it (L50-52), so it works. |

### Counts from the raw rows

The leaders run is the one with 1,736 successful rows per thread count. These files are counted in `INPUT_HASHES.sha256`, which also gives their sha256:

`Kolmo-Preserved/review-20261119T083123Z.lKHULl/raw/scratch-001/bench/lead/T1-{audio,general,genomics,img8,numeric,numeric_npy,structured,text,video,weights}.jsonl` (10 files, `run_cli2.py` rows, `-T 1`).

| quantity | runbook | re-derived | match |
|---|---:|---:|---|
| successful rows (`type=row`, no `error`) | 1,736 | **1,736** (0 error rows) | yes |
| rows with a non-empty `busy_before` or `busy_after` | 328 | **328** | yes |
| structured rows busy / total | 144 / 144 | **144 / 144** | yes |
| busy rows naming a compiler, test or other codec job | >= 200 | **200** | yes, exactly at the bound |

The 200 is sensitive to the classification, as `classify_busy.py` defines it:

- Compilers: `clang`, `(clang)`, `cc`, `ld`, ...
- Tests: `kolmotest`, `test-*`
- Other codec jobs: `kolmo`, `(kolmo)`, `gpu2bench`, `zstd`, `xz`, ...

`Python` is not counted. Counting it gives 201.

Overlapping categories among the 328 busy rows: 83 name a compiler, 134 a test binary and 8 another kolmo or codec job. 127 name only system processes (DisplaysExt, corespotlightd, ...).

Busy rows per group at `-T 1`:

| group | busy | of which named |
|---|---:|---:|
| structured | 144 | 144 |
| numeric | 43 | 27 |
| text | 38 | 29 |
| audio | 32 | 0 |
| general | 30 | 0 |
| numeric_npy | 23 | 0 |
| genomics | 8 | 0 |
| weights | 6 | 0 |
| video | 4 | 0 |
| img8 | 0 | 0 |

Most frequent names in the structured rows: clang 59, kolmotest 32, test-cmpar-asan 20, test-bwt-asan 17, DisplaysExt 14, test-dec64-asan 11.

Other sets, for context:

- `lead/T10-*.jsonl`: 1,736 successful rows, 93 busy (structured 36, numeric 42), 55 named.
- Mac rc3-vs-rc2 rows (`repo@5071f388:benchmark/rc3/mac/*.jsonl`, byte-identical to `scratch-001/bench/rc3cmp/*.jsonl`): 1,152 successful, 48 busy, all system daemons. Yet `mac/RESULTS.md` L3 says "nothing else running".
- `lead2/T*` (rc3w2 binary): 1,332 rows, 37 busy.
- `lead3/T*` (rc3final binary): 677 rows, 12 busy.
- The in-process files (`S*`, `SW*`, `I*`) have no busy fields, so contamination there is unknown.

**Consequence.** The leaders CLI tables (RESULTS.md L52-310) re-render byte for byte from these rows (`summarize_lead_rerender.txt`). Their speed columns therefore include all 328 busy `-T 1` rows. Size columns are not affected.

Each `T1-*.jsonl` holds 2 to 4 env rows. The runner was relaunched (at 22:47, 06:06 and 08:06) and resumed: later launches found the rows done and added nothing.

Scripts: `count_busy.py`, `classify_busy.py` (run with `python3 -I`). Outputs: `c3_counts_lead_T1.txt`, `c3_classify_lead_T1.txt`, `c3_classify_lead_T10.txt`, `c3_busy_rows_lead_T1.txt` (one line per busy row), `c3_counts_mac.txt`.

## C4: pipe deadlock risk in run_cli2.py

Yes, the risk is there:

```
37	    p = subprocess.Popen(argv, stdout=fo, stderr=subprocess.PIPE)
38	    _, status, ru = os.wait4(p.pid, 0)
39	    t = time.perf_counter() - t0
40	    err = p.stderr.read().decode(errors="replace")[-300:]
```

- The parent blocks in `wait4` (L38) before it reads stderr (L40). A child that writes more than the pipe buffer to stderr blocks on write and never exits, so `wait4` never returns. The buffer is 16 KiB by default on macOS, up to 64 KiB, and about 64 KiB on Linux.
- There is no timeout anywhere in `run()`.
- `wait4` bypasses `Popen.wait`, so `p.returncode` is never set.
- stdout is not at risk: it goes to `DEVNULL` or to a file (L35).

Whether this hang ever happened was not checked: the `.log` files were not searched for stalled settings. It is a latent hang for verbose tools or error paths. The fix is `communicate()`, or draining stderr in a thread before or while waiting, and getting rusage another way: `os.wait4` after `communicate` cannot work, because `communicate` reaps the child; `resource.getrusage(RUSAGE_CHILDREN)` deltas would do.

`run_spec3.py` and `run_img8.py` start no subprocesses, so they are not affected.
