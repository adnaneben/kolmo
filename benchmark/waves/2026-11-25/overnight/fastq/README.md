# B30 FASTQ block boundaries

[Draft PR #107](https://github.com/adnaneben/kolmo/pull/107) carries the opt-in experiment on the #106 base at `31fd3acd922490143059ac2438e988e38d89be5f`. The figures below are from the initial #104 qualification. The separately rebuilt draft repeated the improvements and stock-readback checks. [Final PR measurements](review/README.md)

Using an automatically selected 8 MiB block under `--memlimit 256M`, draft #107 also reduced the full two-block archives by **18.54%** and **9.98%** on larger contiguous original windows. Combined throughput increased 11.06× and 9.14×. The available sources do not cross the unconstrained 128 MiB threshold. [Capped results on PR #107](review/README.md)

An opt-in Ultra encoder that ends outer blocks at complete four-line records made the archives 7.73% and 2.58% smaller on two independently accessioned inputs when the caller explicitly requested 1 MiB blocks. Combined encode-and-decode throughput increased 10.29× and 8.77×. At the ordinary automatic block size, each of these 2.36 MB inputs already fitted one block. The automatic archives were byte-identical with the option enabled and smaller than either explicit-block arm.

| Accession | Ultra block setting | Archive B | Encode ms | Stock decode ms | Combined MB/s |
| --- | --- | ---: | ---: | ---: | ---: |
| SRR1770413 | 1 MiB, fixed bytes | 471,176 | 2,227.7 | 2,320.0 | 0.519 |
| | 1 MiB, record-aligned | 434,740 | 203.7 | 238.0 | 5.342 |
| | Automatic, stock / option | 405,778 / 405,778 | 198.4 / 194.5 | 222.0 / 216.5 | 5.652 / 5.756 |
| SRR2584863 | 1 MiB, fixed bytes | 514,763 | 2,271.0 | 2,217.3 | 0.525 |
| | 1 MiB, record-aligned | 501,487 | 236.5 | 276.3 | 4.602 |
| | Automatic, stock / option | 485,938 / 485,938 | 261.6 / 259.4 | 296.0 / 302.1 | 4.233 / 4.210 |

The fixed cuts landed inside records at both 1 MiB boundaries in both inputs. All three resulting blocks used codec 7. The new cuts moved the boundaries back to complete records, and all three blocks used Kolmo's existing FASTQ codec 16, including its existing inner read-aligned jobs. Each automatic Ultra frame contained one codec-16 block. Level 3 kept its stock output in every final experiment. The end-to-end times above are medians of three alternating paired T1 trials on a shared Apple M5. Combined rate is input bytes divided by paired encode-plus-decode time. [Phase data](raw.csv) · [Automatic block data](automatic.csv) · [Sizes, speeds and block layout](summary.csv)

## Inputs and verification

The original retained public-run prefixes carry distinct run accessions. Each benchmark used an unchanged initial slice through the first record ending after 2.25 MiB, plus a separate single-block control beginning at zero-based record 20,100. The initial slices held 3,749 records / 2,359,656 bytes for SRR1770413 and 6,460 / 2,359,537 bytes for SRR2584863. All source, slice, archive and boundary hashes are recorded. Boundary hashes cover little-endian 64-bit byte offsets including zero and EOF. No record contents are distributed. These runs have distinct accessions and represent the same bacterial species. No controlled shift in quality distribution or transfer to other producer families was measured. [Input and binary hashes](inputs.json) · [Published SRR1770413 run identification, Table 3](https://doi.org/10.1186/s13059-016-0997-x) · [SRR2584863 run record](https://www.ncbi.nlm.nih.gov/sra/SRR2584863)

The unmodified #104 binary restored all 56 final frames. The tagged v0.0.2 reader independently restored 24 first-pair frames. Each fixed-arm frame matched the unmodified #104 encoder byte-for-byte. The pilot's original patch and all 32 original rows are retained separately. [Pilot receipts](pilot/raw.csv) · [Final receipt check](score.py)

Eight regression cases exercise a variable-width record stream, level 3, trailing mixed text, mismatched paired headers, unequal field lengths, a missing final newline, an oversized record, and ordinary text. The variable-width Ultra case changed the cut points and restored exactly. The seven remaining cases matched the stock archive byte-for-byte. Each case used stock, experimental-off and experimental-on encoding followed by independent stock readback.

## Isolated implementation

The prototype applies to plain Ultra segments in builds compiled with `KOLMO_FASTQ_RECORD_BLOCKS`, when `KOLMO_BENCH_READ_CUTS` is present. Other builds and ordinary invocations follow the existing path. It scans the entire candidate segment, checks record framing and matching field lengths, and retains cut offsets only after the whole segment qualifies. Unsupported, mixed, truncated, and oversized-record inputs fall back to the original planner. The allocation holds block offsets. The encoder writes ordinary v12 frames.

The branch is `codex/ppp-optin-fastq-record-blocks-20261125` on combined source `ac10ce80048ab0664074f7ba031634055f4d2225`. The two-source-file implementation is commit `98f78a6307c5d0612988e45577a68829badec930`. Apply the source patch in an isolated checkout, compile with `make DEFS=-DKOLMO_FASTQ_RECORD_BLOCKS build/kolmo`, and pass the experiment binary and a separately built stock binary to the regression script. The measured automatic controls cover inputs below the default Ultra block target. Performance on a complete file crossing that automatic target remains unmeasured. [Source patch](experiment.patch) · [Regression tests](regression.py) · [Regression receipts](regression.jsonl) · [Paired benchmark](fastq_panel.py) · [Automatic comparison](automatic.py)
