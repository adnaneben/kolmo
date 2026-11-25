# Empty-policy native deployment

A validated empty policy has no learned option to apply. Exporting the exact stock binary once removed the cost of starting Python on every compression. The export pinned both the encoder and stock reader identities. Its output file and the control CLI have identical SHA-256 b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f.

Apple M5, level 1, one thread, two previously unused upstream producers, complete Homebrew binaries. Each file had one warmup triple and fifteen shuffled scored triples, each including encode and full stock v0.0.2 decode. All 96 restores matched their original hashes. Every frame matched stock bytes, including the warmups. No new route was trained or selected. The Python arm used its normal entry point. The native and stock processes used identical arguments.

| Input | Stored B | Path | Encode ms | Decode ms | Combined ms | Combined MB/s |
|---|---:|---|---:|---:|---:|---:|
| cwebp 1.6.0, 76,016 bytes | 32,198 | Stock CLI | 4.460 | 4.146 | 8.711 | 8.73 |
| | 32,198 | Exported CLI | 4.520 | 4.102 | 8.929 | 8.51 |
| | 32,198 | Python wrapper | 53.802 | 3.851 | 57.396 | 1.32 |
| pdfinfo 26.11.0, 115,704 bytes | 38,892 | Stock CLI | 4.879 | 4.179 | 9.008 | 12.84 |
| | 38,892 | Exported CLI | 4.854 | 4.028 | 8.911 | 12.99 |
| | 38,892 | Python wrapper | 54.160 | 4.329 | 58.255 | 1.99 |

Rates are input bytes divided by median measured encode-plus-decode time in decimal MB/s. Against the Python wrapper, median within-triple combined speedups were 6.54× (cwebp) and 6.35× (pdfinfo). Exported vs stock paired combined time was +2.24% and −1.08%, within normal variability for these short shared-host runs of identical binary images. This measures removal of wrapper overhead for empty policies only. Use the policy wrapper for real routes.

Run the export command in docs/ENCODER-POLICY.md with benchmark/learning-speed-ppp as the experiment, then run replay.py with --inputs pointing to the two source binaries, --stock to the pinned encoder, --decoder to the released v0.0.2 binary, --exported to the export and --output to a new JSON file. freeze.json records exact whole-file hashes and the preregistered order seed. measurements.json records all rounds, frame hashes, stored sizes and phase timings.
