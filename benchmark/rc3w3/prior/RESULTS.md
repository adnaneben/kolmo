# rc3 wave 3, prior lane: per-kind priors and token classes for `-x`

Branch `claude/rc3w3-prior-20261118`, from `claude/release-rc3-20261116` at `58f7cee`, with the twopass lane
(`claude/rc3w2-twopass-20261117`, `274ab0e`) merged first (merge `e42eff7`; fix `cf4caf8`: the decode working set of
its tuned CM streams, which memdyn's `cm_stream_need` did not know). Design and decisions: `docs/PRIORS.md`. Raw outputs:
`raw/`. Tools: `cmprior.c` (study harness, `build.sh cmprior`), `mkcorpus.py`, `mkinc.py`, `typed_study.py`,
`bench.py`, `sz.sh`, `study*.sh`, `sizes_table.py`.

## Machine, builds and method

- Cloud VM, Intel Xeon, 4 vCPU, Linux 6.18, gcc 13, `make` defaults. "All threads" is 4. Every timing row says 1 or 4
  threads; comparisons never mix them. Timings: `bench.py`, one process at a time, wall time from `wait4`, decoded file
  compared byte for byte. Sizes: `sz.sh` (`-x -T1`, round trip checked), run four at a time (sizes do not depend on
  that; 1 and 4 threads give the same frames, checked by `benchmark/determinism/check.py`).
- Builds: **rc3** = `58f7cee`; **tp** = rc3 + twopass merged (`cf4caf8`), the baseline of this lane; research builds
  of this branch with `-DCM_ABL`: **kind** = per-kind priors, model 3 only (5-bit weights); **m4** = model 3/4 race with
  the shared prior only; **full** = both (5-bit weights, prefix race); **full2** = both, 4-bit weights, the race judged
  on the sample's second half for long streams; **final** = full2 with the shared prior from 4 MiB and without the
  UTF-8 routing (Q3), the branch head's coder. Two bugs found by AddressSanitizer and UBSan in `test_priors.c` were
  fixed before the final rows were measured (a stored stretch level of 16 read one past the squash table; model 4's
  prior columns for its own two state maps read past a stored row): against full2 they move sizes by −0.3% to +0.44%.
  The kind, m4, full and full2 columns and the timings were measured before those fixes (the fixes change start
  values only, not the work per bit).
- `test-memdyn` (check-rc) found that the race (twopass's and this lane's) could deadlock under a forced small
  budget: a trial thread waited in the governor for memory its parent's block job held while the parent joined it.
  Fixed as codec 25 does it (a trial thread starts only when its working set is granted at once); bytes unchanged.

## Data: train and test kept apart

All public downloads; the training text of a kind never comes from the source of any test file (`raw/SOURCES.txt`,
`mkcorpus.py`). Test inputs are the first 10 KB, 100 KB, 1 MB and 10 MB of each set, cut at a line end.

| kind | trained on (≤ 4 MB) | tested on |
|---|---|---|
| markup | Silesia `xml`, 5 W3C TR pages (HTML) | CLDR `common/main` XML (29 locales), the WHATWG HTML standard (one page) |
| JSON | simdjson-data examples (minus `twitter`, `github_events`), jsonplaceholder, npm registry documents | GH Archive 2024-01-15 12h (NDJSON), simdjson `twitter.json` |
| table | datasets/ CSVs (airports, CO2, population, GDP, S&P 500, country codes) | UCI household power (`;`-separated) |
| log | Loghub 2k samples of 14 systems (not Apache, not Zookeeper) | Loghub Zookeeper and Apache (full), Elastic examples nginx access log |
| C | vendored zstd, xz, Brotli, LZ4 sources | SQLite 3.46 amalgamation |
| Python | `/usr/lib/python3/dist-packages` (third-party packages) | CPython 3.12 standard library |
| JavaScript | lodash, Vue, React DOM, d3, jQuery, moment, RxJS, Chart.js | plotly.js 2.35, three.js 0.160 |
| prose | Silesia `dickens` | Canterbury large + corpus texts, Silesia `webster` |
| SQL | Sakila (MySQL sample) | MySQL employees dump |

The kind detector (`src/algo/texttype.c`) on the 100 KB test files: 13 of 14 right; the employees dump is taken for a
table at 100 KB (it is SQL at 10 KB and 1 MB). On the training sources: right except pretty-printed numeric JSON
arrays (`numbers`, `canada`, `mesh`, `marine_ik` → none) and HealthApp logs (`|`-delimited → table).

## Q1. What should a prior hold? (`raw/components*.txt`)

Codec 7, model 3, default rates, on the raw bytes; the prior is the state of a model-3 coder after coding the kind's
training text. Columns: A no prior; B the twopass shared prior; C per-kind mean mixer rows (the twopass method, per
kind); D every row of mixers w1, w2, w3 and the final mixer; E D + w4's mean row; F E + APM a1 + state maps + match
counters; G F with the counts of adaptive counters capped at 30; H G + the order-1 table; I H + APMs a2, a3 (4.3 MB
each); J I + all of w4 (16.5 MB); K a priming text (the coder learns the last 64 KB of the training text first; no
tables); L H + K. Size change against A:

| file | B | C | D | F | H | I | J | K | L |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| apache 10k | −9.5% | −9.1% | −19.8% | −21.9% | −23.4% | −24.0% | −25.2% | −14.0% | −21.6% |
| plotly 10k | −6.2% | −7.4% | −11.0% | −13.2% | −15.4% | −15.6% | −18.4% | −17.7% | −19.5% |
| stdlib 10k | −3.6% | −3.9% | −7.0% | −8.6% | −10.7% | −11.3% | −13.5% | −21.6% | −23.2% |
| whatwg 10k | −7.3% | −8.4% | −14.0% | −16.1% | −18.7% | −19.0% | −21.5% | −27.6% | −28.7% |
| zookeeper 100k | −5.0% | −3.9% | −11.5% | −12.8% | −14.1% | −14.7% | −16.1% | −8.4% | −14.3% |
| sqlite 100k | −1.2% | −1.6% | −2.2% | −3.0% | −3.4% | −3.7% | −4.3% | −4.3% | −5.3% |
| three 1m | −0.5% | −0.6% | −0.6% | −0.9% | −1.0% | −1.2% | −1.7% | −1.1% | −1.5% |
| cldr 1m | −1.2% | −1.3% | +2.1% | +1.8% | +1.6% | +1.6% | +1.4% | −1.5% | +1.7% |

(all 36 files: `raw/components.txt`.) Findings:

- Per-kind mean rows (C, the twopass method per kind) are no better than the shared prior (B), as twopass found. The
  gain is in the **full mixer tables** (D: 2–10 points more at 10 KB) and the state maps and order-1 table (F→H).
- The large tables (I, J) add 0.3–3 points at 10 KB for megabytes each: not shippable.
- A **priming text** (K) is as good as or better than the tables on code and HTML at 10 KB, but the coder must learn
  64 KB before every stream: in the harness, decode of sqlite 10k (model set-up included) 0.12 s with the tables, 0.37 s
  with priming (3.2×). Rejected by the 1.2× decode rule.
- Which tables (`raw/components2.txt`, 10 and 100 KB): each of w1, w2, w3 and the final mixer carries part of it (any
  one alone with the others as means keeps 55–80%); APM a1 nothing; state maps about 1 point at 10 KB.
- Precision (`raw/quant_model4.txt`): weights in 8, 7, 6, 5 bits within 0.1%; clamping the trained weights to ±2 even
  helped (apache 100k −9.3% → −10.5%); probabilities as 4-bit stretch within 0.1%. 4-bit weights cost 0.0–0.25% in the
  harness and 0.3–0.9% at 10 KB end to end (full vs full2 below), for 73 KB less binary.

## Q2. Regular expressions

**(ii) Token classes as CM context: model 4.** Two hashed contexts from a hand-written lexer (classes: digit run,
letter run, spaces, each punctuation byte): the classes of the last 8 tokens with the position in the current token,
and the token index with the bytes at the same place of the same token of the line before. Model 4 vs model 3, no
prior: 100 KB apache −1.0%, gharchive −0.9%, sqlite −0.6%, nginx −0.2%, zookeeper −0.1%, prose 0.0%, power CSV
+0.3%; 1 MB apache −1.3%, gharchive −1.2%, sqlite −1.4%, three −3.2% (`raw/model4_1m.txt`). Four variants of the two
contexts were within ±1% of the chosen pair at 100 KB. With a prior, model 4 loses to model 3 at 10 KB (two more
inputs to learn) and wins from about 100 KB on code, JSON and HTML, but not on logs (`raw/quant_model4.txt`), so the
encoder races the two models per stream. A model-3 prior mapped into model 4 (its two extra models at their start)
was better than a prior trained with model 4 (three 100k −0.9%, twitter 100k −1.3%, whatwg 100k −0.9%), so one prior
per kind serves both.

**(i) Typed streams for numbers** (`typed_study.py`, `raw/typed_streams.txt`): every digit run replaced by one byte in
the text, the numbers coded as a second stream (decimal lines, or per-slot zig-zag deltas against the same slot of
the previous line); both streams at `-x`:

| file (1 MB) | `-x` whole | text + numbers (decimal) | text + numbers (slot deltas) |
|---|---:|---:|---:|
| apache | 24,717 | 33,438 (+35.3%) | 39,969 (+61.7%) |
| nginx | 18,795 | 23,658 (+25.9%) | 47,750 (+154%) |
| zookeeper | 4,900 | 7,832 (+59.8%) | 6,993 (+42.7%) |
| gharchive | 70,723 | 75,853 (+7.3%) | 101,696 (+43.8%) |
| power (CSV) | 50,367 | 64,942 (+28.9%) | 68,906 (+36.8%) |

Context mixing predicts a number far better next to its key and template than in a stream of numbers; the log and
table transforms already apply typed coding where trials show it pays. Not adopted.

**Detection.** The kind detector is the regex part that stays: it picks the prior. Sending UTF-8 text that
`looks_text`'s ASCII test rejects (CLDR, tweets) to the text models through it was measured and not kept (Q3, Q5).

## Q3. End to end, `-x`, sizes

Summary, final build against tp (the twopass baseline; in brackets against rc3):

| kind (file) | 10 KB | 100 KB | 1 MB | 10 MB |
|---|---:|---:|---:|---:|
| markup (cldr) | +0.0% (-9.2%) | +0.0% (-3.5%) | +0.0% (-1.8%) | +0.0% (-1.0%) |
| markup (whatwg) | -11.8% (-17.9%) | -5.1% (-7.7%) | -2.5% (-3.1%) | -1.6% (-2.3%) |
| JSON (gharchive) | -6.0% (-10.1%) | -3.3% (-5.7%) | -1.7% (-2.5%) | -1.3% (-1.9%) |
| JSON (twitter) | +0.0% (-8.9%) | +0.0% (-4.3%) | — | — |
| table (power) | +0.0% (-0.7%) | -1.4% (-4.6%) | +0.0% (-2.0%) | +0.0% (-0.7%) |
| log (apache) | -14.7% (-22.5%) | -8.1% (-13.8%) | -2.2% (-4.6%) | — |
| log (nginx) | +0.0% (-9.7%) | -4.4% (-9.0%) | -1.3% (-3.2%) | — |
| log (zookeeper) | -11.6% (-17.9%) | -10.5% (-15.8%) | +0.0% (-5.7%) | +0.0% (-1.0%) |
| C (sqlite) | -8.1% (-11.9%) | -3.2% (-4.7%) | -1.7% (-2.3%) | -2.6% (-2.8%) |
| Python (stdlib) | -6.8% (-10.1%) | -3.7% (-5.3%) | -2.4% (-3.1%) | -2.5% (-2.7%) |
| JavaScript (plotly) | -9.7% (-15.1%) | -5.3% (-6.8%) | -4.5% (-5.0%) | -3.8% (-4.1%) |
| JavaScript (three) | -9.8% (-15.3%) | -4.8% (-6.8%) | -3.9% (-4.5%) | — |
| prose (cantlarge) | -5.8% (-8.9%) | -1.4% (-3.3%) | -0.2% (-1.1%) | -0.8% (-0.9%) |
| SQL (employees) | -0.3% (-4.6%) | +0.0% (-1.6%) | +0.0% (-0.8%) | +0.0% (-0.1%) |

CLDR XML and the tweets are UTF-8 text that `looks_text` (the planner's ASCII test) rejects: the final build codes them
with model 2 as before. The research builds sent them to the text models through the kind detector (columns kind, m4,
full, full2): −0.5% to −9.8% smaller, but up to 1.56× slower to decode (Q5), so that routing was not kept. The table
and SQL sets go through the table transform and CM over parts (codec 25) from 100 KB, where neither the race nor the
priors apply; zookeeper from 1 MB, nginx and power at 10 KB go through the log or table transform first (codec 7 on
transformed bytes, no kind recognised): unchanged there.

Every file, every build (bytes; % against tp; `raw/sizes.txt`):

| file | rc3 | tp | kind | m4 | full | full2 | final |
|---|---:|---:|---:|---:|---:|---:|---:|
| apache_10k | 1,149 (+10.0%) | 1,045 | 895 (-14.4%) | 1,039 (-0.6%) | 883 (-15.5%) | 889 (-14.9%) | 891 (-14.7%) |
| apache_100k | 3,118 (+6.5%) | 2,927 | 2,715 (-7.2%) | 2,897 (-1.0%) | 2,680 (-8.4%) | 2,689 (-8.1%) | 2,689 (-8.1%) |
| apache_1m | 25,911 (+2.5%) | 25,289 | 25,063 (-0.9%) | 24,921 (-1.5%) | 24,717 (-2.3%) | 24,733 (-2.2%) | 24,732 (-2.2%) |
| cantlarge_10k | 2,603 (+3.4%) | 2,518 | 2,366 (-6.0%) | 2,518 (+0.0%) | 2,366 (-6.0%) | 2,373 (-5.8%) | 2,371 (-5.8%) |
| cantlarge_100k | 20,917 (+1.9%) | 20,521 | 20,205 (-1.5%) | 20,521 (+0.0%) | 20,205 (-1.5%) | 20,229 (-1.4%) | 20,225 (-1.4%) |
| cantlarge_1m | 157,451 (+0.9%) | 156,102 | 155,719 (-0.2%) | 155,967 (-0.1%) | 155,719 (-0.2%) | 155,684 (-0.3%) | 155,794 (-0.2%) |
| cantlarge_10m | 1,649,150 (+0.1%) | 1,647,254 | 1,647,333 (+0.0%) | 1,634,810 (-0.8%) | 1,647,333 (+0.0%) | 1,635,614 (-0.7%) | 1,634,810 (-0.8%) |
| cldr_10k | 1,594 (+10.2%) | 1,447 | 1,441 (-0.4%) | 1,533 (+5.9%) | 1,431 (-1.1%) | 1,440 (-0.5%) | 1,447 (+0.0%) |
| cldr_100k | 9,704 (+3.6%) | 9,367 | 9,358 (-0.1%) | 9,435 (+0.7%) | 9,294 (-0.8%) | 9,321 (-0.5%) | 9,367 (+0.0%) |
| cldr_1m | 39,838 (+1.8%) | 39,131 | 37,849 (-3.3%) | 37,529 (-4.1%) | 37,495 (-4.2%) | 37,552 (-4.0%) | 39,131 (+0.0%) |
| cldr_10m | 312,444 (+1.0%) | 309,220 | 282,817 (-8.5%) | 278,943 (-9.8%) | 279,855 (-9.5%) | 280,244 (-9.4%) | 309,220 (+0.0%) |
| employees_10k | 2,308 (+4.6%) | 2,207 | 2,207 (+0.0%) | 2,201 (-0.3%) | 2,201 (-0.3%) | 2,201 (-0.3%) | 2,201 (-0.3%) |
| employees_100k | 15,702 (+1.6%) | 15,456 | 15,456 (+0.0%) | 15,456 (+0.0%) | 15,456 (+0.0%) | 15,456 (+0.0%) | 15,456 (+0.0%) |
| employees_1m | 110,180 (+0.8%) | 109,319 | 109,319 (+0.0%) | 109,319 (+0.0%) | 109,319 (+0.0%) | 109,319 (+0.0%) | 109,319 (+0.0%) |
| employees_10m | 1,009,101 (+0.1%) | 1,007,946 | 1,007,946 (+0.0%) | 1,007,946 (+0.0%) | 1,007,946 (+0.0%) | 1,007,946 (+0.0%) | 1,007,946 (+0.0%) |
| gharchive_10k | 2,824 (+4.6%) | 2,701 | 2,527 (-6.4%) | 2,696 (-0.2%) | 2,515 (-6.9%) | 2,528 (-6.4%) | 2,539 (-6.0%) |
| gharchive_100k | 10,156 (+2.5%) | 9,910 | 9,646 (-2.7%) | 9,811 (-1.0%) | 9,551 (-3.6%) | 9,577 (-3.4%) | 9,582 (-3.3%) |
| gharchive_1m | 72,585 (+0.8%) | 72,015 | 71,600 (-0.6%) | 71,093 (-1.3%) | 70,723 (-1.8%) | 70,809 (-1.7%) | 70,798 (-1.7%) |
| gharchive_10m | 500,259 (+0.6%) | 497,326 | 497,506 (+0.0%) | 490,529 (-1.4%) | 490,914 (-1.3%) | 491,312 (-1.2%) | 490,614 (-1.3%) |
| nginx_10k | 782 (+10.8%) | 706 | 706 (+0.0%) | 706 (+0.0%) | 706 (+0.0%) | 706 (+0.0%) | 706 (+0.0%) |
| nginx_100k | 2,636 (+5.1%) | 2,509 | 2,400 (-4.3%) | 2,509 (+0.0%) | 2,392 (-4.7%) | 2,395 (-4.5%) | 2,399 (-4.4%) |
| nginx_1m | 19,510 (+2.0%) | 19,130 | 18,880 (-1.3%) | 19,130 (+0.0%) | 18,795 (-1.8%) | 18,803 (-1.7%) | 18,886 (-1.3%) |
| plotly_10k | 1,906 (+6.4%) | 1,791 | 1,619 (-9.6%) | 1,790 (-0.1%) | 1,610 (-10.1%) | 1,617 (-9.7%) | 1,618 (-9.7%) |
| plotly_100k | 17,588 (+1.7%) | 17,299 | 16,785 (-3.0%) | 16,879 (-2.4%) | 16,353 (-5.5%) | 16,387 (-5.3%) | 16,389 (-5.3%) |
| plotly_1m | 144,658 (+0.5%) | 143,903 | 143,340 (-0.4%) | 138,190 (-4.0%) | 137,654 (-4.3%) | 137,801 (-4.2%) | 137,387 (-4.5%) |
| plotly_10m | 1,131,988 (+0.3%) | 1,128,292 | 1,135,799 (+0.7%) | 1,085,631 (-3.8%) | 1,093,116 (-3.1%) | 1,085,325 (-3.8%) | 1,085,631 (-3.8%) |
| power_10k | 1,263 (+0.7%) | 1,254 | 1,254 (+0.0%) | 1,254 (+0.0%) | 1,254 (+0.0%) | 1,254 (+0.0%) | 1,254 (+0.0%) |
| power_100k | 6,604 (+3.3%) | 6,393 | 6,302 (-1.4%) | 6,393 (+0.0%) | 6,302 (-1.4%) | 6,308 (-1.3%) | 6,302 (-1.4%) |
| power_1m | 51,395 (+2.0%) | 50,367 | 50,367 (+0.0%) | 50,367 (+0.0%) | 50,367 (+0.0%) | 50,367 (+0.0%) | 50,367 (+0.0%) |
| power_10m | 449,425 (+0.7%) | 446,334 | 446,334 (+0.0%) | 446,334 (+0.0%) | 446,334 (+0.0%) | 446,334 (+0.0%) | 446,334 (+0.0%) |
| sqlite_10k | 2,949 (+4.3%) | 2,828 | 2,586 (-8.6%) | 2,828 (+0.0%) | 2,585 (-8.6%) | 2,598 (-8.1%) | 2,599 (-8.1%) |
| sqlite_100k | 19,509 (+1.6%) | 19,208 | 18,674 (-2.8%) | 19,098 (-0.6%) | 18,539 (-3.5%) | 18,581 (-3.3%) | 18,594 (-3.2%) |
| sqlite_1m | 148,854 (+0.6%) | 147,926 | 147,458 (-0.3%) | 145,655 (-1.5%) | 145,185 (-1.9%) | 145,371 (-1.7%) | 145,375 (-1.7%) |
| sqlite_10m | 1,249,293 (+0.2%) | 1,246,201 | 1,250,580 (+0.4%) | 1,213,886 (-2.6%) | 1,218,478 (-2.2%) | 1,214,397 (-2.6%) | 1,213,886 (-2.6%) |
| stdlib_10k | 3,427 (+3.7%) | 3,306 | 3,070 (-7.1%) | 3,296 (-0.3%) | 3,057 (-7.5%) | 3,074 (-7.0%) | 3,081 (-6.8%) |
| stdlib_100k | 19,255 (+1.7%) | 18,936 | 18,398 (-2.8%) | 18,708 (-1.2%) | 18,164 (-4.1%) | 18,217 (-3.8%) | 18,231 (-3.7%) |
| stdlib_1m | 135,060 (+0.7%) | 134,115 | 133,522 (-0.4%) | 131,286 (-2.1%) | 130,721 (-2.5%) | 130,904 (-2.4%) | 130,911 (-2.4%) |
| stdlib_10m | 1,168,394 (+0.3%) | 1,165,148 | 1,170,031 (+0.4%) | 1,136,312 (-2.5%) | 1,141,295 (-2.0%) | 1,142,472 (-1.9%) | 1,136,312 (-2.5%) |
| three_10k | 2,301 (+6.4%) | 2,163 | 1,945 (-10.1%) | 2,156 (-0.3%) | 1,938 (-10.4%) | 1,948 (-9.9%) | 1,950 (-9.8%) |
| three_100k | 14,422 (+2.1%) | 14,119 | 13,659 (-3.3%) | 13,861 (-1.8%) | 13,398 (-5.1%) | 13,435 (-4.8%) | 13,439 (-4.8%) |
| three_1m | 106,382 (+0.6%) | 105,772 | 104,946 (-0.8%) | 102,303 (-3.3%) | 101,496 (-4.0%) | 101,624 (-3.9%) | 101,613 (-3.9%) |
| twitter_10k | 2,036 (+9.8%) | 1,855 | 1,779 (-4.1%) | 1,930 (+4.0%) | 1,762 (-5.0%) | 1,779 (-4.1%) | 1,855 (+0.0%) |
| twitter_100k | 8,051 (+4.5%) | 7,701 | 7,342 (-4.7%) | 7,418 (-3.7%) | 7,215 (-6.3%) | 7,253 (-5.8%) | 7,701 (+0.0%) |
| whatwg_10k | 1,669 (+7.5%) | 1,553 | 1,363 (-12.2%) | 1,553 (+0.0%) | 1,363 (-12.2%) | 1,370 (-11.8%) | 1,370 (-11.8%) |
| whatwg_100k | 10,523 (+2.9%) | 10,227 | 9,797 (-4.2%) | 10,100 (-1.2%) | 9,686 (-5.3%) | 9,705 (-5.1%) | 9,710 (-5.1%) |
| whatwg_1m | 84,135 (+0.6%) | 83,620 | 82,878 (-0.9%) | 82,178 (-1.7%) | 81,477 (-2.6%) | 81,578 (-2.4%) | 81,568 (-2.5%) |
| whatwg_10m | 553,810 (+0.7%) | 550,114 | 549,754 (-0.1%) | 541,099 (-1.6%) | 540,839 (-1.7%) | 541,361 (-1.6%) | 541,099 (-1.6%) |
| zookeeper_10k | 1,565 (+7.7%) | 1,453 | 1,278 (-12.0%) | 1,450 (-0.2%) | 1,275 (-12.3%) | 1,282 (-11.8%) | 1,285 (-11.6%) |
| zookeeper_100k | 2,414 (+6.2%) | 2,272 | 2,027 (-10.8%) | 2,268 (-0.2%) | 2,022 (-11.0%) | 2,032 (-10.6%) | 2,033 (-10.5%) |
| zookeeper_1m | 5,197 (+6.1%) | 4,900 | 4,900 (+0.0%) | 4,900 (+0.0%) | 4,900 (+0.0%) | 4,900 (+0.0%) | 4,900 (+0.0%) |
| zookeeper_10m | 64,408 (+1.0%) | 63,775 | 63,775 (+0.0%) | 63,775 (+0.0%) | 63,775 (+0.0%) | 63,775 (+0.0%) | 63,775 (+0.0%) |

## Q4. Library size

| | binary `kolmo` (bytes) | text segment |
|---|---:|---:|
| rc3 `58f7cee` | 3,578,656 | — |
| tp `cf4caf8` | 3,591,592 | 3,484,125 |
| final | 4,018,272 (+426,680) | 3,910,341 |

Priors (raw LZMA2, `src/codecs/cm_kprior.inc`), bytes in the binary per kind: markup 53,261; JSON 51,150; table 58,197;
log 44,866; C 44,983; Python 48,128; JavaScript 48,639; prose 38,481; total 387,705. SQL (46,528) was dropped: the SQL
dump sets never reach codec 7 at `-x` (table route), so it could not earn its place. Each blob unpacks to 373 KB once
per process on first use (LZMA2, about 3 ms).

## Q5. Speed

`bench.py`, one process at a time; sizes in bytes, times in seconds (encode, decode). `final` in these runs is the
build with the UTF-8 routing (Q3), so the CLDR and tweet rows show that routing's cost (the reason it was dropped); on
every other file it is the branch head's coder. Ratios are final / tp.

1 thread:

| file | tp size, enc s, dec s | final size, enc s, dec s | rc3 size, enc s, dec s | final/tp enc | final/tp dec |
|---|---|---|---|---:|---:|
| apache_10k | 1,045, 0.41, 0.14 | 889, 0.37, 0.11 | 1,149, 0.10, 0.08 | 0.91 | 0.77 |
| cantlarge_10k | 2,518, 0.44, 0.10 | 2,373, 0.41, 0.08 | 2,603, 0.09, 0.08 | 0.94 | 0.78 |
| cldr_10k | 1,447, 0.33, 0.09 | 1,440, 0.45, 0.10 | 1,594, 0.06, 0.08 | 1.35 | 1.10 |
| employees_10k | 2,207, 0.44, 0.16 | 2,201, 0.55, 0.11 | 2,308, 0.10, 0.14 | 1.25 | 0.70 |
| gharchive_10k | 2,701, 0.41, 0.11 | 2,528, 0.40, 0.08 | 2,824, 0.08, 0.08 | 0.99 | 0.74 |
| nginx_10k | 706, 0.42, 0.10 | 706, 0.60, 0.10 | 782, 0.15, 0.07 | 1.45 | 1.02 |
| plotly_10k | 1,791, 0.43, 0.11 | 1,617, 0.49, 0.08 | 1,906, 0.08, 0.08 | 1.15 | 0.75 |
| power_10k | 1,254, 0.30, 0.08 | 1,254, 0.30, 0.10 | 1,263, 0.07, 0.03 | 1.00 | 1.13 |
| sqlite_10k | 2,828, 0.41, 0.15 | 2,598, 0.43, 0.14 | 2,949, 0.14, 0.08 | 1.06 | 0.93 |
| stdlib_10k | 3,306, 0.47, 0.12 | 3,074, 0.42, 0.11 | 3,427, 0.08, 0.12 | 0.90 | 0.94 |
| three_10k | 2,163, 0.46, 0.19 | 1,948, 0.46, 0.09 | 2,301, 0.08, 0.09 | 1.00 | 0.46 |
| twitter_10k | 1,855, 0.32, 0.11 | 1,779, 0.38, 0.14 | 2,036, 0.06, 0.06 | 1.21 | 1.33 |
| whatwg_10k | 1,553, 0.37, 0.09 | 1,370, 0.27, 0.12 | 1,669, 0.06, 0.07 | 0.74 | 1.28 |
| zookeeper_10k | 1,453, 0.48, 0.15 | 1,282, 0.49, 0.08 | 1,565, 0.10, 0.07 | 1.03 | 0.55 |
| apache_100k | 2,927, 2.37, 0.48 | 2,689, 2.69, 0.58 | 3,118, 0.73, 0.41 | 1.14 | 1.21 |
| cantlarge_100k | 20,521, 1.99, 0.53 | 20,229, 2.70, 0.54 | 20,917, 0.56, 0.48 | 1.36 | 1.01 |
| cldr_100k | 9,367, 1.42, 0.43 | 9,321, 2.60, 0.55 | 9,704, 0.36, 0.32 | 1.84 | 1.30 |
| employees_100k | 15,456, 2.80, 0.81 | 15,456, 2.69, 0.71 | 15,702, 1.20, 0.49 | 0.96 | 0.88 |
| gharchive_100k | 9,910, 1.89, 0.51 | 9,577, 1.86, 0.42 | 10,156, 0.36, 0.37 | 0.99 | 0.84 |
| nginx_100k | 2,509, 5.05, 0.48 | 2,395, 5.22, 0.49 | 2,636, 1.22, 0.48 | 1.03 | 1.02 |
| plotly_100k | 17,299, 2.21, 0.57 | 16,387, 2.72, 0.54 | 17,588, 0.51, 0.45 | 1.23 | 0.95 |
| power_100k | 6,393, 5.26, 0.57 | 6,308, 5.24, 0.51 | 6,604, 1.26, 0.56 | 1.00 | 0.89 |
| sqlite_100k | 19,208, 2.25, 0.59 | 18,581, 2.77, 0.56 | 19,509, 0.87, 0.55 | 1.23 | 0.94 |
| stdlib_100k | 18,936, 2.18, 0.56 | 18,217, 2.89, 0.57 | 19,255, 0.50, 0.50 | 1.33 | 1.02 |
| three_100k | 14,119, 2.21, 0.58 | 13,435, 2.55, 0.55 | 14,422, 0.56, 0.47 | 1.16 | 0.95 |
| twitter_100k | 7,701, 1.35, 0.40 | 7,253, 2.37, 0.51 | 8,051, 0.36, 0.37 | 1.76 | 1.27 |
| whatwg_100k | 10,227, 2.68, 0.50 | 9,705, 3.10, 0.59 | 10,523, 1.16, 0.48 | 1.16 | 1.18 |
| zookeeper_100k | 2,272, 2.17, 0.63 | 2,032, 2.56, 0.56 | 2,414, 0.79, 0.44 | 1.18 | 0.90 |
| apache_1m | 25,289, 11.35, 4.02 | 24,733, 12.93, 4.35 | 25,911, 7.51, 4.21 | 1.14 | 1.08 |
| cantlarge_1m | 156,102, 14.81, 4.91 | 155,684, 17.14, 4.95 | 157,451, 10.35, 4.82 | 1.16 | 1.01 |
| cldr_1m | 39,131, 6.16, 2.89 | 37,552, 10.49, 4.51 | 39,838, 3.27, 2.98 | 1.70 | 1.56 |
| employees_1m | 109,319, 14.50, 3.03 | 109,319, 14.68, 2.92 | 110,180, 6.49, 2.85 | 1.01 | 0.97 |
| gharchive_1m | 72,015, 8.71, 4.35 | 70,809, 10.27, 4.50 | 72,585, 4.51, 4.08 | 1.18 | 1.04 |
| nginx_1m | 19,130, 23.05, 4.25 | 18,803, 24.56, 4.39 | 19,510, 9.34, 4.10 | 1.07 | 1.03 |
| plotly_1m | 143,903, 15.13, 4.95 | 137,801, 17.35, 5.19 | 144,658, 10.38, 5.03 | 1.15 | 1.05 |
| power_1m | 50,367, 15.21, 2.87 | 50,367, 14.21, 2.55 | 51,395, 6.73, 2.49 | 0.93 | 0.89 |
| sqlite_1m | 147,926, 15.47, 5.15 | 145,371, 17.85, 5.19 | 148,854, 10.33, 4.95 | 1.15 | 1.01 |
| stdlib_1m | 134,115, 14.30, 4.64 | 130,904, 16.76, 4.95 | 135,060, 10.08, 4.63 | 1.17 | 1.07 |
| three_1m | 105,772, 14.64, 4.64 | 101,624, 16.53, 5.10 | 106,382, 10.08, 4.82 | 1.13 | 1.10 |
| whatwg_1m | 83,620, 14.76, 4.86 | 81,578, 16.38, 4.89 | 84,135, 9.87, 4.38 | 1.11 | 1.01 |
| zookeeper_1m | 4,900, 3.53, 0.22 | 4,900, 3.44, 0.29 | 5,197, 3.24, 0.16 | 0.98 | 1.32 |
| gharchive_10m | 497,326, 57.81, 47.97 | 490,614, 66.07, 49.81 | — | 1.14 | 1.04 |
| plotly_10m | 1,128,292, 60.60, 51.71 | 1,085,631, 68.24, 53.92 | — | 1.13 | 1.04 |

median final/tp: encode 1.14, decode 1.01; max decode 1.56

4 threads:

| file | tp size, enc s, dec s | final size, enc s, dec s | rc3 size, enc s, dec s | final/tp enc | final/tp dec |
|---|---|---|---|---:|---:|
| apache_1m | 25,289, 8.03, 4.19 | 24,733, 10.45, 4.71 | 25,911, 6.53, 4.41 | 1.30 | 1.12 |
| cantlarge_1m | 156,102, 9.26, 4.93 | 155,684, 12.23, 5.41 | 157,451, 8.42, 5.21 | 1.32 | 1.10 |
| cldr_1m | 39,131, 4.58, 3.45 | 37,552, 7.33, 4.74 | 39,838, 3.16, 3.01 | 1.60 | 1.37 |
| employees_1m | 109,319, 7.37, 1.32 | 109,319, 7.23, 1.20 | 110,180, 3.95, 1.20 | 0.98 | 0.91 |
| gharchive_1m | 72,015, 6.05, 4.40 | 70,809, 7.39, 5.02 | 72,585, 4.48, 4.57 | 1.22 | 1.14 |
| nginx_1m | 19,130, 13.88, 4.32 | 18,803, 16.01, 4.78 | 19,510, 8.30, 4.29 | 1.15 | 1.11 |
| plotly_1m | 143,903, 9.84, 5.10 | 137,801, 11.86, 5.31 | 144,658, 8.56, 5.02 | 1.21 | 1.04 |
| power_1m | 50,367, 6.37, 0.91 | 50,367, 6.09, 0.91 | 51,395, 3.06, 0.90 | 0.96 | 1.00 |
| sqlite_1m | 147,926, 9.75, 4.91 | 145,371, 11.17, 5.26 | 148,854, 8.49, 5.06 | 1.15 | 1.07 |
| stdlib_1m | 134,115, 9.54, 4.80 | 130,904, 11.48, 5.13 | 135,060, 8.39, 4.93 | 1.20 | 1.07 |
| three_1m | 105,772, 9.99, 4.93 | 101,624, 10.89, 5.01 | 106,382, 8.05, 4.63 | 1.09 | 1.02 |
| whatwg_1m | 83,620, 9.92, 5.11 | 81,578, 11.32, 5.53 | 84,135, 8.42, 5.08 | 1.14 | 1.08 |
| zookeeper_1m | 4,900, 3.57, 0.30 | 4,900, 3.81, 0.21 | 5,197, 3.10, 0.28 | 1.07 | 0.69 |
| gharchive_10m | 497,326, 50.54, 44.94 | 490,614, 53.56, 48.01 | — | 1.06 | 1.07 |
| plotly_10m | 1,128,292, 54.62, 52.41 | 1,085,631, 59.65, 54.40 | — | 1.09 | 1.04 |

median final/tp: encode 1.15, decode 1.07; max decode 1.37

**Repeats** (`raw/timing_repeat.jsonl`, branch head, 1 thread, three runs each, median ratio to tp): decode whatwg 10k
0.68, whatwg 100k 1.06, apache 100k 0.92, zookeeper 1m 0.90 (same frame), apache 1m 1.12, three 1m 1.03, cldr 1m 1.00
(same frame as tp), tweets 100k 0.86 (same frame). Single runs of small files vary by ±20%; no file decodes more than
1.2× slower than tp at the branch head. Model 4 costs 1.03–1.12× decode where it is chosen (1 MB, 1 thread); the
priors cost nothing per byte (2–4 ms to unpack a prior once per process). Encoding is 1.1–1.2×
tp's (five race trials of 256 KiB instead of four; model 4 when chosen).

**Against other coders** (1 thread; branch head; size, encode s / decode s; ² branch head = tp's frame, decode from the
repeats; zpaq 7.15, zstd 1.5.5, xz 5.4.5, brotli 1.1.0 of Ubuntu 24.04):

| file | final `-x` | zstd -22 --long=27 | xz -9e | brotli -q 11 -w 24 | zpaq -m5 |
|---|---:|---:|---:|---:|---:|
| apache_1m | 24,733 (12.9 / 4.35) | 53,829 (0.7 / 0.00) | 48,344 (0.5 / 0.01) | 49,356 (1.5 / 0.00) | 29,048 (3.0 / 2.79) |
| cantlarge_1m | 155,684 (17.1 / 4.95) | 210,672 (0.5 / 0.00) | 208,980 (0.5 / 0.01) | 208,363 (1.9 / 0.01) | 160,435 (2.4 / 2.39) |
| cldr_1m | 39,131 (6.2 / 2.87)² | 72,933 (0.9 / 0.00) | 67,456 (0.5 / 0.01) | 66,682 (1.2 / 0.00) | 45,931 (1.6 / 2.00) |
| employees_1m | 109,319 (14.7 / 2.92) | 226,898 (0.4 / 0.01) | 214,688 (0.5 / 0.02) | 222,946 (1.8 / 0.01) | 124,948 (2.4 / 2.23) |
| gharchive_1m | 70,809 (10.3 / 4.50) | 108,585 (0.6 / 0.00) | 103,292 (0.3 / 0.02) | 97,385 (1.3 / 0.00) | 84,109 (2.0 / 2.42) |
| nginx_1m | 18,803 (24.6 / 4.39) | 41,048 (0.5 / 0.00) | 38,132 (0.5 / 0.01) | 38,756 (2.2 / 0.00) | 21,593 (2.6 / 2.29) |
| plotly_1m | 137,801 (17.3 / 5.19) | 209,168 (0.4 / 0.01) | 203,728 (0.5 / 0.02) | 200,935 (1.8 / 0.01) | 155,969 (2.3 / 2.36) |
| power_1m | 50,367 (14.2 / 2.55) | 120,853 (1.1 / 0.01) | 103,988 (1.1 / 0.01) | 109,054 (1.8 / 0.01) | 67,893 (2.1 / 2.08) |
| sqlite_1m | 145,371 (17.8 / 5.19) | 214,548 (0.4 / 0.01) | 211,024 (0.6 / 0.02) | 208,100 (1.7 / 0.01) | 155,067 (2.3 / 2.28) |
| stdlib_1m | 130,904 (16.8 / 4.95) | 188,437 (0.4 / 0.00) | 184,516 (0.5 / 0.01) | 181,288 (1.7 / 0.01) | 143,898 (2.5 / 2.36) |
| three_1m | 101,624 (16.5 / 5.10) | 164,892 (0.5 / 0.01) | 158,840 (0.4 / 0.02) | 157,637 (1.6 / 0.01) | 117,674 (2.2 / 2.69) |
| whatwg_1m | 81,578 (16.4 / 4.89) | 134,709 (0.3 / 0.00) | 129,588 (0.4 / 0.01) | 126,503 (1.4 / 0.00) | 95,475 (2.6 / 2.33) |
| zookeeper_1m | 4,900 (3.4 / 0.29) | 23,926 (1.9 / 0.00) | 18,728 (2.5 / 0.01) | 21,411 (2.6 / 0.00) | 9,221 (2.8 / 2.50) |
| gharchive_10m | 490,614 (66.1 / 49.81) | 772,076 (5.1 / 0.02) | 737,144 (3.7 / 0.06) | 705,628 (13.1 / 0.03) | 566,497 (22.3 / 21.44) |
| plotly_10m | 1,085,631 (68.2 / 53.92) | 1,677,529 (6.2 / 0.03) | 1,623,572 (5.8 / 0.15) | 1,622,138 (19.3 / 0.03) | 1,218,369 (26.5 / 26.20) |

## Decision

Rule: keep a feature only if it gains at least 0.5% on its kind at its level without making decode more than 1.2×
slower.

| feature | gain on its kind (`-x`, vs tp) | decode | kept |
|---|---|---|---|
| model 4 (token-class contexts), raced against model 3 | alone (build m4): code, JSON, HTML, apache logs −1.3% to −4.0% at 1 MB, −1.4% to −3.8% at 10 MB; prose −0.8% at 10 MB | 1.03–1.12× | **yes** |
| prior: markup (HTML) | −11.8% / −5.1% at 10 / 100 KB (whatwg) | same | **yes** |
| prior: JSON | −6.0% / −3.3% (gharchive) | same | **yes** |
| prior: log | −14.7% / −8.1% (apache), −11.6% / −10.5% (zookeeper), −4.4% (nginx 100 KB) | same | **yes** |
| prior: C | −8.1% / −3.2% (sqlite) | same | **yes** |
| prior: Python | −6.8% / −3.7% (stdlib) | same | **yes** |
| prior: JavaScript | −9.7% / −5.3% (plotly), −9.8% / −4.8% (three) | same | **yes** |
| prior: prose | −5.8% / −1.4% (Canterbury) | same | **yes** |
| prior: table | −1.4% at 100 KB (power, the one size where codec 7 sees raw CSV) | same | **yes**, marginal: CSV usually takes the table route |
| prior: SQL | 0 (dumps take the table route) | — | no (46.5 KB saved) |
| kind priors from 4 MiB | −0.1% to −0.3% at 2–4 MB in the harness, +0.2% to +0.5% at 10 MB end to end | — | no: shared prior from 4 MiB |
| UTF-8 text to the text models | −0.5% to −9.8% (CLDR, tweets) | up to 1.56× | no |
| priming text | ≈ priors, more at 10 KB on code/HTML | 3.2× at 10 KB | no |
| numbers as a typed stream | +7% to +154% | — | no |

Format: container format 2 (version 11), codec 7; the prior id is tune byte 2's low 7 bits (2–9), model 4 its bit 7
(`SPEC.md`). No new codec or transform id was needed: both are properties of a CM stream. Memory and thread count
change speed only: 1 and 4 threads give the same frames (`benchmark/determinism/check.py`), and the race's choice
depends on the stream's bytes only. `balanced`, `ratio` and the other levels have no CM path and are byte-identical
(determinism check: only `log.txt` at `-x` and `-x -l 2` changed against tp). Binary cost: +427 KB (388 KB of priors).
