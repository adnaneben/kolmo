# B24 BF16 competition

Stock PR #104 level 3 and ZipNN 0.5.4 split the size result across two complete BF16 checkpoints from independent producers. ZipNN saved 75,040 B on Hugging Face SmolLM2-135M (0.042%). Kolmo saved 291,210 B on Qwen2.5-0.5B (0.044%). Kolmo used substantially less memory and finished the fresh-process comparison faster on both files. With Python and its dependencies loaded before timing, ZipNN completed both file-to-file cycles faster.

## Complete checkpoints

| Checkpoint | Codec | Archive B | Encode s | Decode s | Combined MB/s | Peak RSS encode / decode MiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| SmolLM2, 269,060,552 B | #104 L3 | 179,493,188 | 0.526 | 0.155 | 395.65 | 203.5 / 21.7 |
| | ZipNN | 179,418,148 | 1.489 | 1.281 | 93.50 | 1,066.5 / 635.5 |
| Qwen2.5, 988,097,824 B | #104 L3 | 657,983,155 | 1.898 | 0.613 | 392.02 | 203.9 / 22.3 |
| | ZipNN | 658,274,365 | 2.532 | 2.016 | 211.09 | 2,647.8 / 1,778.5 |

Both arms used one codec thread and a fresh process for each phase. One warm-up preceded three alternating paired trials per checkpoint. Wall time includes input, output and process startup. Combined throughput uses the median of the paired encode-plus-decode times. RSS is the maximum measured per phase across the timed trials. ZipNN also reported file-to-file timing inside each process after imports. Those paired rates were 465.71 MB/s on SmolLM2 and 424.32 MB/s on Qwen. These shared-host, warm-filesystem measurements do not estimate GPU, multi-thread or model-inference throughput. [Raw phase times and peak RSS](whole-raw.csv) · [Scorecard with both timing boundaries and hashes](whole-score.csv)

Every arm compressed the entire original file including the 8-byte header length, JSON, metadata and all tensor payloads. SmolLM2 contains 272 BF16 tensors and a 30,528 B JSON header. Qwen contains 290 BF16 tensors and a 32,280 B JSON header. The exact originals match the pinned upstream SHA-256 values. ZipNN encoded with AUTO and byte-mode BF16 grouping, trained no model, used zero sidecar bytes and decoded with a default instance supplied only the archived bytes. All 16 restores matched their original hashes. The released v0.0.2 binary independently restored both #104 archives. Each phase ran alone, and its process exited before the next. Hashing used a 4 MiB buffer. [Pinned inputs, header digests and executable identities](whole-inputs.json)

Pinned main reproduced #104's level-3 archives byte-for-byte on both complete checkpoints. The earlier tagged v0.0.2 measurement used 229 fewer bytes for Qwen and the same size for SmolLM2. B24's two-producer stock-versus-ZipNN measurement is complete for these settings. The reviewed B24 proposal had no separate boundary implementation, so no transform change or benefit is attributable to it. [Pinned-main control](whole-main.csv) · [Historical tagged results](historical-checkpoints.json)

## Complete single-tensor files

On the complete 47 MB visual tensor file, stock PR #104 level 3 produced a 4.22% smaller frame than ZipNN 0.5.4. ZipNN running in an existing Python process processed the full pack-and-restore cycle 4.25× faster. On the 10 MB attention tensor, ZipNN produced a 1,360-byte smaller frame and ran 3.21× faster than level 3. Level 7 improved the 47 MB file's size advantage to 9.11% and reduced combined throughput to 2.01 MB/s.

The stored sizes include the complete safetensors header, payload and codec framing. Each restore reproduced the original SHA-256. Main and PR #104 produced identical level-3 frames for both files. The tagged v0.0.2 reader independently restored both #104 level-3 frames. ZipNN's default decoder restored its frames without a sidecar. [Input and executable hashes](inventory.json) · [Independent restores](independent-restore.json)

| Source | Method | Stored B | Encode MB/s | Decode MB/s | Combined MB/s |
| --- | --- | ---: | ---: | ---: | ---: |
| 10,485,880 B | Pinned main L3 | 6,992,158 | 278.8 | 1,058.2 | 208.9 |
| | #104 L1 | 10,486,072 | 1,283.2 | 1,517.8 | 695.3 |
| | #104 L3 | 6,992,158 | 284.5 | 869.2 | 214.4 |
| | #104 L7 | 6,991,734 | 13.0 | 812.4 | 12.8 |
| | ZipNN, worker loaded | 6,990,798 | 1,074.2 | 1,856.8 | 689.2 |
| | ZipNN, fresh process | 6,990,798 | 8.9 | 12.0 | 5.0 |
| 47,186,040 B | Pinned main L3 | 30,012,302 | 204.2 | 1,011.7 | 169.9 |
| | #104 L1 | 32,993,118 | 552.4 | 1,568.4 | 405.8 |
| | #104 L3 | 30,012,302 | 185.3 | 1,257.3 | 158.0 |
| | #104 L7 | 28,480,570 | 2.0 | 1,047.4 | 2.0 |
| | ZipNN, worker loaded | 31,334,986 | 950.0 | 1,996.6 | 671.3 |
| | ZipNN, fresh process | 31,334,986 | 45.5 | 47.9 | 23.6 |

Each row covers three warm-filesystem repetitions on a shared Apple M5. Combined MB/s uses input bytes divided by the median paired encode-plus-decode time. All arms used one codec thread. Kolmo used a fresh native CLI per phase. ZipNN used its default AUTO method and byte mode on the entire original file. Both ZipNN arms include codec setup and file reads and writes. The fresh-process arm also includes Python and dependency startup. The latter had separate repetitions after the randomized primary panel. Hash checks ran outside each timed phase. The reader process represented the source independently from the mutable native input buffer. [Raw phase times](raw.csv) · [Fresh-process times](cold.csv) · [Scorecard](score.csv)

The small files hold an attention projection and a visual merger. The original manifests identify each producer only as `llm`. Count those as two distinct tensors. The named complete checkpoints above establish producer independence.

## Replay

Run `python3 whole_score.py` and `python3 score.py` here to verify the saved evidence and reproduce the scorecards. `whole_checkpoints.py --help` lists the local paths needed for a whole-checkpoint rerun. Run `python3 whole_baseline.py /path/to/pinned/main/kolmo` after it for the baseline. `measure.py --help` describes the small-file panel. Repeat its arguments with `--cold-only` for the separate fresh-process ZipNN arm. [Whole-model harness](whole_checkpoints.py) · [Whole-model verifier](whole_score.py) · [Small-file harness](measure.py) · [Small-file verifier](score.py)
