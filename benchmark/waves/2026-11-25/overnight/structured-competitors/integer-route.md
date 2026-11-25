# Integer routing gate · stock v12

Tested whether shipping presets or an inexpensive type declaration could close the untrained OpenZL7 gap. One separate development station was screened once (12 exact runs); the transfer panel then ran 48 settings with one warmup and five scored randomized rounds. All 288 transfer and 12 development encodes decoded exactly. All Kolmo frames, including the experimental encoder outputs, were decoded by the unmodified a1d39559 CLI. No model, dictionary or external side bytes were required for any tested route. The file-to-file phase times include spawn and I/O; every result checks complete decoded SHA-256 outside timing. The four weather stations were previously examined in B25 and are transfer controls, not a new sealed test set. The S.M.A.R.T. series is from a different producer, storage telemetry, never used to design the raw prototype.

| Matched input | OpenZL7 bytes / MB/s | Stock L3 bytes / MB/s | Stock L3 native-leaf bytes / MB/s | Stock L9 bytes / MB/s | Stock Ultra bytes / MB/s | Explicit raw-i32 or declared typed-huf bytes / MB/s |
|---|---:|---:|---:|---:|---:|---:|
| USW00012839.i32 | 13,340 / 7.07 | 15,877 / 6.42 | 15,772 / 7.48 | 15,748 / 1.83 | 13,394 / 0.44 | 19,429 / 7.68 |
| USW00012839.npy | 13,430 / 6.51 | 15,973 / 7.03 | 15,868 / 6.41 | 15,825 / 2.27 | 13,512 / 0.34 | 19,560 / 9.79 |
| USW00013874.i32 | 21,294 / 7.33 | 24,315 / 8.04 | 24,198 / 6.77 | 24,197 / 1.98 | 21,372 / 0.47 | 28,486 / 13.53 |
| USW00023183.i32 | 17,041 / 7.41 | 20,090 / 4.90 | 20,040 / 7.05 | 19,989 / 2.08 | 17,920 / 0.48 | 32,012 / 10.65 |
| USW00024233.i32 | 16,324 / 6.89 | 18,854 / 7.77 | 18,757 / 6.67 | 18,756 / 2.27 | 16,892 / 0.33 | 22,341 / 8.28 |
| backblaze-194.i32 | 138,609 / 36.48 | 152,342 / 39.14 | 152,342 / 25.91 | 145,337 / 4.65 | 114,114 / 0.37 | 308,402 / 72.32 |
| backblaze-194.npy | 138,704 / 38.95 | 152,439 / 35.86 | 152,439 / 23.88 | 145,415 / 4.68 | 114,177 / 0.39 | 308,533 / 74.31 |
| esc50.wav | 406,279 / 22.99 | 263,016 / 11.36 | 263,016 / 8.21 | 252,294 / 1.58 | 252,294 / 0.56 | 348,284 / 33.16 |

## Representation and decisions

The four TMAX vectors contain every daily station row in order, signed little-endian int32 with INT32_MIN marking a missing value. The independent Backblaze score includes every one of 338,817 SMART-194 normalized values (439 missing) in the 133,373,544-byte daily report, yielding a complete 1,355,268-byte integer series. It does not represent the report’s other columns. The Miami and Backblaze NPY inputs are distinct lossless products with identical values and full 128-byte NPY headers charged to every compressor. Stock Kolmo decodes raw routes directly to headerless bytes; it decodes NPY routes to the original NPY including header, with no postprocessing. The 441,044-byte ESC-50 WAV control is an original whole PCM file, header and samples together. The WAV OpenZL arm uses its generic serial profile; it is a fallback/control rather than a comparison against the best possible audio-specific codec.

The research patch affects only an isolated build of the stock level-3 opt-in typed encoder. When the existing typed NPY/WAV parser has no match and the experiment supplies an aligned sequence of at least 4 KiB, its caller explicitly declares 32-bit elements and reuses the shipped integer forward/Huffman leaf and frame writer with header length zero. The shipping decoder, wire syntax, index and checksums are unchanged. It is an explicit representation hypothesis, not a proposed automatic classifier. This producer-independent test used the same frozen prototype for all raw vectors. No unrecognized input is enabled on main or the RC.

The single training-station screen tested stock levels 1, 2, 3, 5, 8, 9, Ultra, full analysis, native leaf, typed Huffman, the raw type-declaration patch and OpenZL7. The fixed transfer panel was chosen to expose the speed/size trade-off rather than fit a separate level per input. Fast-stock L3 and the native-leaf path remain useful speed-biased alternatives. The explicit raw-Huffman pass lacks a blanket size/speed win. Stock Ultra can approach or beat the rival frame size on selected vectors but pays much more time. Preserve the negative prototype and the complete frontier; do not promote a universal raw integer route.

## Replay

Start from pinned main a1d39559686a9ec17c8614b9154a64d7b5b98672. Compile stock with make build/kolmo; in a separate copy apply explicit-i32.patch and build the same target with identical flags. Use prepare.py OUTPUT PRIOR_B25_DERIVED BACKBLAZE_CSV ORIGINAL_WAV. Run test.py STOCK_CLI PROTOTYPE_CLI OPENZL_ZLI OUTPUT and then score.py. The B25 receipt contains the station CSV-to-vector construction. inputs.json identifies the entire original Backblaze file, the control WAV, the raw series and the full NPY inputs. protocol.json pins binary hashes and options. rows.csv retains every raw observation, frame and restore hash. score.csv contains individual encode, decode and matched combined rates. Some short CLI launches run near scheduler noise; this single host does not establish tail latency or independent x86 behavior.
