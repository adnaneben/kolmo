# Reusing KLZMA integer price prefixes

The candidate computes all forward-tree leaf prices with one expansion per internal node, preserving the range-coder models and exact integer sums. An eight-bit table uses 510 edge prices per refresh. The scalar per-leaf calculation uses 2,048. This change affects encoding. The decoder is unchanged.

## What the measurements establish

The raw codec emitted identical payloads and stock main decoded every arm across 24 settings and 576 runs including warmups. Median cell combined throughput was 1.42% higher across the test matrix. Individual paired gains spanned −22.65% to +15.71%. Same-binary medians spanned −7.95% to +42.04% on this shared host. The matrix covers two codec levels, requested T1/T4, two corpus prefixes and the candidate source code as a control.

Normal CLI frames were independently checked for actual codec 24 use. Four data sources at L8/L9, T1/T4 gave 16 cells and another 576 exact restores. Gains against control A ranged from −4.20% to +3.60%, while A/A varied from −10.28% to +7.14%. Keep the code as a research branch. These results do not qualify a shipping performance change.

## Replay

On the benchmark Mac, build pinned main and candidate with the default Makefile. For each revision create a direct-codec library using `clang -O3 -mcpu=native -ffp-contract=off -dynamiclib -Isrc/algo src/algo/klzma.c src/algo/par.c -o /tmp/REV.dylib`. Run `python3 measure.py /tmp/main.dylib /tmp/candidate.dylib INPUT... > raw.csv` followed by `python3 score.py`. The input and payload SHA-256 values in raw.csv identify the exact bytes. Round zero is warmup. Each scored triplet includes two calls to the same main binary and one candidate. The score uses the average time of the two controls per round.
