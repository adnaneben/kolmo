# All-level comparison on four historical inputs

The combined candidate at `ac10ce80048ab0664074f7ba031634055f4d2225` emitted the same complete frame as pinned main `a1d39559686a9ec17c8614b9154a64d7b5b98672` at every one of the 80 settings. These frames also match all 80 archived frame digests from the historical comparison with `4db0425b361012523d8074ca6f2a760d9466e9f4`. The encode-plus-decode speed change on the ordinary file CLI was within the measured same-binary variation.

The four inputs were the 16,384-byte LD30 text prefix, the 65,536-byte Python standard library source prefix, the 262,144-byte Gaia CSV prefix, and the complete 441,044-byte ESC-50 WAV. Each ran at levels 1 through 9 and Ultra with one and four requested threads. The command used ordinary preset defaults without runtime research switches. The particular experimental PCM shortcut requires `KOLMO_AUDIO_SCREEN=1`. Direct archive throughput and explicit codec options are separate experiments. Source provenance, offsets, byte counts, input and binary SHA-256s appear in [manifest.json](manifest.json).

## Paired combined speed

Each row takes the median across the eight source/thread settings for a preset. The underlying estimate for each setting is the median of three paired throughput ratios. Positive values mean the executable named first finished faster. The A and AA controls invoked the same pinned-main executable.

| Level | Candidate vs A | Candidate vs AA | AA vs A |
| --- | ---: | ---: | ---: |
| 1 | -0.51% | -0.39% | +1.24% |
| 2 | +0.00% | +1.24% | -1.28% |
| 3 | +0.16% | +0.77% | -0.09% |
| 4 | -0.84% | -0.46% | +0.25% |
| 5 | +0.83% | -0.36% | -0.29% |
| 6 | +0.41% | +0.14% | +0.39% |
| 7 | +0.07% | -0.01% | +0.15% |
| 8 | -0.39% | -1.03% | +0.78% |
| 9 | -1.39% | -1.01% | -0.27% |
| Ultra | -0.77% | -0.53% | -0.42% |

Across all 80 settings the median paired change was −0.15% against A and −0.10% against AA. Median paired CPU change against A was −0.22%. Candidate-versus-A wall changes ranged from −5.51% to +12.38%. Same-binary AA-versus-A changes ranged from −32.20% to +14.32%. Three scored triplets on a shared host offer limited sensitivity to small changes, especially on millisecond CLI invocations.

The follow-up selected cells that were over 5% slower than both controls and lost at least two of the three pairings against each. There were zero qualifying cells. The strongest common apparent improvement, text16k at L3/T4, measured +5.28% and +4.79% against the controls. With eleven fresh scored triplets those estimates became −1.19% and −2.19%, with +2.23% same-binary drift. These runs identified neither a convincing ordinary-file regression nor a repeatable general speedup on these four inputs.

## Frames, verification and timing

The main run made 960 byte-exact timed round trips including 240 warmups. Pinned main also decoded every candidate warmup frame in 80 extra compatibility checks. The independent confirmation made 36 more byte-exact timed round trips including three warmups. Every frame size and SHA was stable across repetitions. Each cell used warm caches, sequential CLI invocations, randomized arm order, shuffled cell order and a full byte comparison after timing. Actual worker counts were uninstrumented. Small inputs can supply too little work to use four workers. The data cover four specific source/size pairs on one Apple arm64 host.

Complete frame bytes, ratio, per-phase throughput, combined throughput, measured memory, paired CPU, and SHA-256s are in [score.csv](score.csv). Combined throughput uses original decimal MB divided by median per-run encode-plus-decode wall seconds, including process startup and file I/O. Read the paired columns for within-triplet changes. The phases have independent medians. [Raw runs](rounds.csv) · [confirmation](confirmation-score.csv) · [raw confirmation](confirmation-rounds.csv) · [selection criteria](confirmation-selection.json).

The replay requires the four files named in the manifest and builds matching the recorded executable digests. Run `python3 replay.py --before MAIN --after CANDIDATE --corpus INPUT_DIRECTORY --out FRESH_OUTPUT`, then `python3 confirm.py` with the same arguments. Both scripts retain raw observations. Replay with runtime Kolmo environment overrides cleared.
