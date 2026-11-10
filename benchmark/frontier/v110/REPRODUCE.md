# Reproducing the v1.1.0 comparisons

The source baseline is [87dfc86](https://github.com/adnaneben/kolmo/tree/87dfc86b2665cac637c98179fb8afa8db3eeb668). It reports version 1.1.0. The release comparison uses that source twice, once with the ordinary build and once with the opt-in fast recipes. The November 7 M1 executable and the distributed 1.0.0 executable are separate rows. The six-genome comparison also includes the selector in this branch. The exact executables used on the M5 have SHA-256 pins in the receipts.

## Get the inputs

The [input manifest](inputs.json) records the source URL, source digest and final digest for each new electricity or genome case. Download the artifact once and prepare the bytes before timing. For example, from the repository root:

```sh
python3 benchmark/frontier/v110/prepare_input.py eia EIA930_BALANCE_2020_Jan_Jun.csv /tmp/kolmo-eia
python3 benchmark/frontier/v110/prepare_input.py ce11 ce11.fa.gz /tmp/kolmo-ce11
```

The EIA test includes all rows whose hour falls in the final three quarters of distinct UTC hours. It retains the original header, ordering and line bytes. Training uses at most 4 MiB of earlier complete rows. Budding yeast and the two bacterial cases use complete assemblies. The chromosome cases preserve the published FASTA header, wrapping, case and ambiguous bases. The worm and zebrafish inputs are individual records from the UCSC whole-genome download. Earlier European inputs are specified in the [grid](../../electricity/protocol.md), [Portuguese meter](../../wide/protocol.md), and [CoSSMic household](../protocol.md) protocols.

## Replay a matched comparison

Use separate builds for the pinned source and the candidate. A normal build produces `build/kolmo`. Use `make build/kolmo-frontier` for the opt-in FASTA and wide-table recipes. The draft also provides a separately selectable compact-table ultra build.

```sh
python3 benchmark/frontier/v110/replay_file.py --list
python3 benchmark/frontier/v110/replay_file.py \
  --case release/eia --source /tmp/kolmo-eia/test.csv \
  --codec v110 --codec zstd --codec openzl \
  --bin v110=/path/to/v110/build/kolmo --bin zstd=/path/to/zstd --bin openzl=/path/to/zli \
  --output /tmp/kolmo-eia-run

python3 benchmark/frontier/v110/replay_file.py \
  --case sampling/ce11 --source /tmp/kolmo-ce11/ce11.fa \
  --codec guard --codec frontier --codec zstd --codec naf1 --codec naf19 \
  --bin guard=/path/to/this-branch/build/kolmo-frontier \
  --bin frontier=/path/to/v110/build/kolmo-frontier --bin zstd=/path/to/zstd \
  --bin ennaf=/path/to/ennaf --bin unnaf=/path/to/unnaf --output /tmp/kolmo-genome-run
```

The replay verifies the input digest and uses the archived commands. OpenZL loads the exact graph bytes from the repository. Use a new output directory when changing a binary or codec list. The harness records local executable digests and resumes an interrupted run with matching settings. Supply `--strict-binaries` to require the original macOS executable bytes. On a different architecture, build the corresponding source versions and compare the codecs on that machine. A run should use an idle host, a common output location, and the same worker count for every codec.

## Read the evidence

[Whole-file receipts](file-trials.json.gz) include normalized commands, executable hashes, every warm-up and timed trial, frame and recovery hashes, elapsed seconds, and maximum resident memory. The original local receipt digest is retained for auditing. For the 1.1.0 and genome trials, the harness shuffles codec order for one warm-up and seven timed rounds. The charts use median wall time and the highest measured RSS per phase. Timing includes process startup and file I/O. Hash verification runs after timing. The 20% gate compares both elapsed times and both memory peaks with each rival separately. It uses observed resource use, without an operating-system limit. OpenZL training is a one-time separate measurement.

The [in-memory replay](../v110-lz/README.md) uses a pinned lzbench harness and a complete 211,947,520-byte Silesia tar. The project's separate cloud measurement used a 211,957,760-byte tar. Compare speeds within a hardware and harness group. The offline [chart](index.html) exposes the published M5 CLI panel, cloud measurements, new validation, and earlier file-format specialists as separate selections. Small speed differences can change on another run.

## Follow-up tests

The later US holdout uses `prepare_input.py eia2022` on the January–June 2022 source listed in the manifest. Replay case `electricity/eia2022` uses `stock`, `fixed_quarter_mib`, `zstd` and `openzl`. Supply the same 1.1.0 executable for both Kolmo rows. The quarter-MiB block setting was selected on 2020 data and frozen before retrieving 2022. The later holdout uses its own trained OpenZL graph.

The `file-planner/archive` case uses the complete official Silesia tar. Two additional synthetic tar cases isolate multi-member behavior on other bytes. Use `prepare_tar.py structured out.tar bsub.fa test.csv events.jsonl` for the bacterial genome, 2020 EIA test and GH Archive event file. Use `prepare_tar.py binary out.tar chinook.sqlite akiyo.y4m saut_page.docx` for the binary controls. Headers, order and padding are deterministic. The [tar manifest](experiments/tar-freeze.json) pins each member.

The `ultra` cases use the source prefixes or complete Silesia member recorded in the [compact-build manifest](experiments/compact-freeze.json). The prefix screens used one warm-up and three timed pairs per codec. Use `make check-cm-compact` to test both context-model versions, a legacy frame and allocation recovery. Use `make check-frontier-dna` for the experimental FASTA route. Regular FASTA frames can be read by the stock binary. Experimental DNA frames require the frontier-capable decoder.

## Rebuild the charts

From the repository root, the offline explorer can be regenerated from the committed evidence with standard Python. The two PNG renders also need Matplotlib.

```sh
python3 benchmark/frontier/v110/explorer.py benchmark/lzbench/results
python3 benchmark/frontier/v110/board.py
python3 benchmark/frontier/v110_chart.py benchmark/frontier/v110/measurements.json benchmark/frontier/v110
```
