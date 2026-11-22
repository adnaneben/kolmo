# Single-thread benchmark of 1.2.0-rc1 on macOS

Kolmo and its rivals measured on one Mac, in one session, each on one thread. The rules were written down before the
first timed run: `PROTOCOL.md`. The measurements are in `results/`; `results/tables.md` is made from them by
`report.py` and nothing in it is typed by hand.

| File | What it is |
|---|---|
| `PROTOCOL.md` | the rules, the comparisons fixed in advance, and what changed after freezing |
| `run_lzbench.py` | tier A: one lzbench process per file and setting; reads CPU time and peak memory of each |
| `run_inprocess.py` | tier S1: pcodec, FLAC and JPEG XL beside Kolmo's Python binding in one process |
| `run_cli.py` | tier S2: OpenZL's `zli` and 7-Zip beside the Kolmo command line, whole processes |
| `report.py` | turns the result files into tables |
| `check_wrapper_sizes.py` | checks that Kolmo inside lzbench writes the sizes the command line writes |
| `results/A1.jsonl`, `A2.jsonl`, `A3.jsonl` | tier A rows: `silesia.tar`, 35 files, 18 typed arrays |
| `results/inprocess.jsonl`, `results/cli.jsonl` | tier S rows |
| `results/tables.md` | every table |

## Running it again

lzbench with Kolmo inside (`../lzbench/setup_macos.sh` explains what it changes in the lzbench checkout):

    git clone https://github.com/inikep/lzbench
    sh benchmark/lzbench/setup_macos.sh ./lzbench <new work dir>

Tier A:

    python3 benchmark/rc1/run_lzbench.py --lzbench ./lzbench/lzbench --out <dir> --tier A1 --silesia-tar <silesia.tar>
    python3 benchmark/rc1/run_lzbench.py --lzbench ./lzbench/lzbench --out <dir> --tier A2 --data <dir with the 35 files>
    python3 benchmark/rc1/run_lzbench.py --lzbench ./lzbench/lzbench --out <dir> --tier A3 --data <dir with the 35 files> \
        --typed <dir with .npy arrays> --raw <dir with raw integer arrays>

A run that is interrupted can be started again with the same command: rows that are already measured and clean are
kept, rows that failed or were marked are measured again.

Tier S (needs Python with `pcodec`, `soundfile`, `pillow`, `pillow-jxl-plugin` and `numpy`; OpenZL's `zli`; `7zz`):

    make ARCH= all lib
    python3 benchmark/rc1/run_inprocess.py --binding bindings/python --lib build/libkolmo.dylib --data <dir> \
        --raw <dir> --audio <dir with .wav files> --work <new dir> --out <dir>/inprocess.jsonl
    python3 benchmark/rc1/run_cli.py --kolmo build/kolmo --zli <zli> --data <dir> \
        --fresh <dir with OpenZL's test tables and trained compressors> --work <new dir> --out <dir>/cli.jsonl

Tables:

    python3 benchmark/rc1/report.py --dir <dir> --inprocess <dir>/inprocess.jsonl --cli <dir>/cli.jsonl --out tables.md

## Reading a row

A tier A row records the command, the time of day, the load, the elapsed time and the CPU time of the lzbench
process, its peak memory, and other processes that used half a core or more just before it started. Tier S rows
record the times and CPU times around the calls (S1) or of the whole process, with its peak memory (S2). A row whose
CPU time exceeds its elapsed time is marked `threads`: something in it ran on more than one thread. Speeds are from
the fastest iteration, in MB of 1,000,000 bytes per second. lzbench checks every round trip; the tier S drivers
compare the decoded bytes with the input themselves.

The first line of each result file describes the machine and the builds. In the copies kept here the home directory
of the machine is written `~` and the work directory of the session `<scratch>`. Nothing else differs from the files
the run wrote, and `report.py` makes the same `tables.md` from them.
