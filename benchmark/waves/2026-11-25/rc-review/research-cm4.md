# Ultra+ research — incremental model value and model resets

## Decision

Drop the 6-of-7 sparse expert from the current RC. It reduced the real CM4 arithmetic payload in 26 of 36 file/layout comparisons, tied in seven and enlarged three. The best reduction was 0.04961%. No claim about PAQ8PX or cmix leadership follows from this experiment. The ordinary v12 codec and its grammar are untouched. Candidate payloads existed only in the research process, which knew the experiment's fixed model from its arguments.

The test includes 144 decodes in the primary screen and 96 in the worker-count follow-up, all with full original-byte comparisons. Every control arithmetic payload also matched the original Kolmo CM4 implementation byte for byte. Scores compare raw arithmetic payload sizes, including termination and excluding the 12-byte native CM4 stream header and external container. A production model would also need its own unambiguous model/version signal, decoder, resource admission and compatibility fixtures. Any extra header bytes consume these small savings.

## Measurement

The reference is the contents of Wave 2 #73, commit dcbf483184e77e465c02e33bd1aae674ca26b5bc; src/codecs/cm.c SHA-256 2c0ec70d797f38a2ee5225389a403dd35996a0c6d875dd48b6e07964a92b4099. Both models use the CM4 text context mask and production CM4 tuning, priors, integer mixture, state updates and interval coder. A simple wrapper replaces only the probability fed to the final interval split. Its range coder calls the core's exact division and shift functions. All equality checks run in ordinary process memory. Producer files cover C/C++/Python source, prose, dictionary, structured events and XML. This is direct CM4 and does not exercise CLI plan selection.

The experiment design was fixed before looking at corpus results. All arms use exactly the same decoder-visible history, parameters, input offset 256 KiB and effective model initialization. The ten project corpus files already participated in earlier research. Project “transfer” means file-disjoint from the development split. Two further transfer inputs, the machine's SQLite public header and dictionary, were pinned by whole-file hash before those files were tested with the frozen code. These are limited diagnostic transfer fixtures. A separately custodial, previously inaccessible gate still belongs in release validation.

Each producer contributed one 64 KiB, one 256 KiB, and the same 256 KiB divided into four independent 64 KiB streams. The short prefix appears inside the longer cases. The 36 contrasts are settings, not 36 statistically independent producer observations. The primary run used one worker; the follow-up used one versus four concurrent processes on identical boundaries. Each candidate decodes with its own experimental probabilities. The wrapper checks the control against the actual native payload using cm_compress_raced and checks exact byte recovery. Timings in raw files are one-sample, instrumented process CPU, encoding first, with fixed order and diagnostic bookkeeping. They do not support a speed claim.

## Exact outcomes

| Source and layout | Ordinary CM4 payload B | Experimental B | Saved B |
| --- | ---: | ---: | ---: |
| enwik8, 256 KiB | 64,760 | 64,752 | 8 |
| enwik8, four 64 KiB blocks | 73,755 | 73,749 | 6 |
| Reymont, 256 KiB | 42,084 | 42,066 | 18 |
| Bash HTML, 256 KiB | 31,710 | 31,695 | 15 |
| SQLite public header, 256 KiB fresh transfer | 41,875 | 41,862 | 13 |
| Dictionary, 256 KiB fresh transfer | 38,006 | 38,001 | 5 |
| BGL, 256 KiB | 5,014 | 5,014 | 0 |
| GHArchive, four 64 KiB blocks | 28,090 | 28,091 | −1 |
| NASA, four 64 KiB blocks | 15,401 | 15,402 | −1 |

Timing is not the reason to reject a measured tradeoff without rechecking it. Three potentially adverse cases were re-run with one and four physical process workers and four independent blocks of 16 or 64 KiB. Both worker configurations gave identical sizes and ideal bit totals for every block. On 16 KiB blocks all three enlarged by one byte in total. On 64 KiB blocks the API case tied, and NASA and GHArchive each enlarged by one byte. With one large 256 KiB block these same three each shrank one byte. The deterministic context-dependent reversals and tiny uplifts disqualify a general model. Every run is retained in results.jsonl, fresh.jsonl and worker-retest.jsonl. summary.csv covers every primary case.

## Mathematics and computational cost

Let N be input bytes, C compressed bytes, and T the sum of encode and decode wall seconds. Report ratio R=N/C and combined speed V=N/T separately. A point improving both dominates. A useful secondary scalar is R V=N²/(CT), equal weighting in log space. A candidate can spend at most C0/C1−1 extra fractional combined time to break even on this scalar. Users paying for time, capacity or transmission can apply a different explicit weight. The 256 KiB enwik8 result allows only 0.01235% extra combined time; even the best case allows less than 0.050%. Strict speed-and-ratio leadership additionally requires no speed regression. For bandwidth B bytes/s, end-to-end time including one network transfer is T+C/B, which makes the savings from a few bytes directly comparable with local processing cost.

For bit t with actual symbol y and probability p, the sequential cost is −log2 P(y | decoded past). The candidate stores the most recent position in a 65,536-entry table, verifies its six keyed historical bytes, and backs off on a strong CM4 match, an agreeing match prediction, or a wrong partial-byte prefix. For an active bit it boosts the suggested bit's odds by 4/3, mixes against unmodified CM4 using an online posterior from previous bits in the same class, and updates that posterior after observing the bit. Sixty-four classes distinguish bit position, native confidence and native match presence. An unclipped, exact Bayesian mixture of two fixed prediction strategies in one class has likelihood at least half that of either strategy, hence at most one extra bit versus the best fixed expert in hindsight for that class. Quantization, clipping, finite interval coding and the several classes preclude importing that exact guarantee into this implementation. The baseline's CM4 state always updates with its unmodified internal predictions.

Additional state is 262,144 bytes for previous positions and 128 bytes for 64 16-bit posteriors per concurrently active stream, plus bounded diagnostic counters. The lookup is O(1) per byte and at most eight probability/weight steps; total additional runtime is O(N) and extra space O(2^16) for the fixed table. Initializing it per tiny block costs O(2^16). At four workers the extra model state approaches 1 MiB. Real branch, cache, division and startup costs must fit the above break-even threshold for any deployment.

## Where the next high-ratio effort should go

One standard 256 KiB CM4 stream used 8.76–35.08% fewer payload bytes than four independent 64 KiB CM4 streams over the identical 256 KiB across all twelve producers. This combines retained history, warm online state and CM4's input-length-dependent table sizing. It is neither a demonstrated improvement over the current Ultra default nor an attribution entirely to state resets. The first follow-up is to hold model-table size fixed, vary block boundaries, then sweep actual multi-MiB CLI defaults and schedule existing independent files in parallel before dividing them. Evaluate throughput, first-output p95, RSS, seek granularity, hashes and random-access latency with actual T1/T4/auto. Do not adopt this microbenchmark's block sizes as defaults. For one 256 KiB enwik8 input, a monolithic stream could spend at most 13.89% more combined time than the four resets on the R V score. It would still have to win both time and size to dominate.

On the 256 KiB enwik8 slice 230,853 of 262,144 bytes (88.06%) started without a 16-byte live match. They caused 498,145.85 of 518,056.91 ideal bits (96.16%). The sparse advisor only activated on 10,537 bytes (4.02%); its ideal savings were about 71.2 bits there and eight actual payload bytes. In BGL, 16.51% of source bytes caused 82.86% of the baseline ideal bits; a parser that distinguishes event IDs, counters and paths could concentrate a meaningful amount of modeling effort on a small subset. NASA's absent/weak share was 48.85% of total loss, so a text-only gate would miss half its costs. Next quantify how much independent signal line/field context contributes to those high-loss positions with a strict memory bound and the same exact coder. The regression receipts determine whether a candidate remains research-only.

The sequence mixture formalism is consistent with [Willems, Shtarkov and Tjalkens, 1995](https://doi.org/10.1109/18.382012). This experiment does not implement their tree recursion. [Mahoney's 2005 report](https://repository.fit.edu/ces_faculty/166/) describes context mixing, supporting measurement of one new model conditional on existing predictors. [Knoll's cmix benchmark](https://byronknoll.com/cmix.html) reports 14,623,723 bytes for full 100,000,000-byte enwik8. A local fragment result cannot be extrapolated to that benchmark. [Bellard's NNCP study](https://bellard.org/nncp/nncp.pdf) describes online neural probability estimation over a stream. A small deterministic byte-rate expert gated on measured residuals can be tested without pretraining; any change to decode probabilities needs versioned semantics, portable bit equality, cost measurements for both endpoints and a separately sealed promotion set.

## Replay

Build against the stated Wave 2 tree. The compiler invocations below use Apple Silicon and Darwin linker flags. For Linux omit the Apple dead-strip linker option and choose the target CPU flag.

```sh
make -C "$KOLMO_ROOT" build/libkolmoarch.a
clang -O2 -mcpu=native -ffp-contract=off -I"$KOLMO_ROOT/src" second_match.c "$KOLMO_ROOT/build/libkolmoarch.a" -Wl,-dead_strip -lpthread -lm -o test-match
python3 replay.py ./test-match "$IMPACT_CORPUS" results.jsonl
python3 replay_fresh.py ./test-match / fresh.jsonl
python3 retest_workers.py ./test-match "$IMPACT_CORPUS" worker-retest.jsonl
python3 summarize.py .
```

Fresh replay checks the two whole-file SHA-256 values in its script before coding. The project samples record SHA-256 for each tested slice; check these against the supplied raw receipts. No producer file contents are included. Experimental encoded byte sequences are intentionally isolated in memory in this standalone harness and the ordinary decoder continues to accept standard v12.
