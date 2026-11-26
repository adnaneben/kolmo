# Ultra+ (UPX, `--archive-max`): measured results

All rows are SHA- or `cmp`-checked round trips, run single-threaded (`-T 1`). Sizes are deterministic. Times and RSS were taken
on a heavily loaded Mac (load average 40-100 from other lanes; niced). They show the order of magnitude only; the
timed run through `timed.sh` at load < 12 is still to come.

Binaries: `-U` and `-U --codec-level 2` are main `a1d39559`. Ultra+ (up3) is #122 at `7c14df3a`; the later
`c17a3ac8` changes only the summation order and gives the same d200k bytes. The cuts are the first 256 KiB of each
impact-corpus file (PR #48), and their SHA-256 values are posted on #57. cmix v21 rows fill in as its local run
progresses (about 20 min per cut under this load).

## Dev split (22 cuts)
| cut | bytes | -U | -U level 2 | **Ultra+** | vs -U | vs level 2 | cmix v21 | enc s | dec s | RSS enc/dec MB | frame | RT |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| audio__esc50-1-100032 | 262,144 | 13,940 | 13,940 | **13,940** | +0.00% | +0.00% | 14,478 | 72 | 1 | 340/3 | v12 | OK |
| audio__esc50-1-28808 | 262,144 | 148,939 | 148,939 | **148,939** | +0.00% | +0.00% | 153,334 | 2458 | 1 | 590/3 | v12 | OK |
| code__flac-c | 262,144 | 28,793 | 28,793 | **27,264** | -5.31% | -5.31% |  | 138 | 106 | 310/313 | v13 | OK |
| container__kennedy-xls | 262,144 | 5,737 | 5,737 | **4,287** | -25.27% | -25.27% |  | 142 | 85 | 311/332 | v13 | OK |
| csv__energy-130col | 262,144 | 32,906 | 32,906 | **32,149** | -2.30% | -2.30% |  | 86 | 59 | 332/337 | v13 | OK |
| csv__ghcn | 262,144 | 3,874 | 3,874 | **3,595** | -7.20% | -7.20% |  | 94 | 66 | 330/296 | v13 | OK |
| exe__ooffice-x86 | 262,144 | 92,680 | 92,680 | **86,524** | -6.64% | -6.64% |  | 121 | 95 | 333/328 | v13 | OK |
| genomics__srr835775 | 262,144 | 48,800 | 48,800 | **47,042** | -3.60% | -3.60% |  | 106 | 88 | 296/298 | v13 | OK |
| img16__x-ray | 262,144 | 113,807 | 113,807 | **113,515** | -0.26% | -0.26% |  | 983 | 1014 | 500/524 | v13 | OK |
| img8__hubble | 262,144 | 135,082 | 135,082 | **129,233** | -4.33% | -4.33% |  | 62 | 60 | 332/329 | v13 | OK |
| img8__retina | 262,144 | 15,898 | 15,898 | **15,898** | +0.00% | +0.00% |  | 50 | 4 | 292/121 | v12 | OK |
| json__api | 262,144 | 13,876 | 13,876 | **13,066** | -5.84% | -5.84% |  | 48 | 53 | 301/300 | v13 | OK |
| log__loghub | 262,144 | 8,598 | 8,598 | **8,153** | -5.18% | -5.18% |  | 54 | 47 | 312/315 | v13 | OK |
| numeric__owid-f64 | 262,144 | 19,349 | 17,734 | **17,471** | -9.71% | -1.48% |  | 69 | 4 | 340/149 | v13 | OK |
| numeric__population-i64 | 151,152 | 37,458 | 36,039 | **35,615** | -4.92% | -1.18% |  | 79 | 29 | 463/345 | v13 | OK |
| tar__canterbury | 262,144 | 8,523 | 8,523 | **6,992** | -17.96% | -17.96% |  | 43 | 33 | 322/321 | v13 | OK |
| text__dickens | 262,144 | 65,152 | 65,152 | **63,098** | -3.15% | -3.15% |  | 60 | 46 | 363/306 | v13 | OK |
| tsv__ghcn-rows | 262,144 | 11,508 | 11,508 | **10,784** | -6.29% | -6.29% |  | 57 | 41 | 291/314 | v13 | OK |
| video__akiyo | 262,144 | 68,579 | 68,579 | **68,579** | +0.00% | +0.00% |  | 44 | 5 | 363/125 | v12 | OK |
| weights__bf16-10mb | 262,144 | 173,180 | 173,180 | **172,775** | -0.23% | -0.23% |  | 82 | 37 | 367/343 | v13 | OK |
| weights__magika-onnx | 262,144 | 199,325 | 199,325 | **198,206** | -0.56% | -0.56% |  | 90 | 45 | 350/351 | v13 | OK |
| xml__silesia-xml | 262,144 | 7,663 | 7,663 | **6,999** | -8.67% | -8.67% |  | 56 | 46 | 299/293 | v13 | OK |
| **total** | 5,656,176 | 1,253,667 | 1,250,633 | **1,224,124** | -2.36% | -2.12% | | | | | | |

Reading:
- Ultra+ is never larger than ultra, because each block keeps whichever stream is smaller.
- It is smaller on 18 of 22 cuts. The 4 ties (two audio cuts, retina, akiyo) are blocks that ultra codes with kaud, kvid or kimg. UPX is not wired into those paths yet.
- Largest gains: containers and tar (−18 to −25%), xml (−8.7%), x86 (−6.6% on the BCJ-filtered stream), tables (−6.3 to −7.2%), JSON and logs (−5.2 to −5.8%), code (−5.3%) and prose (−3.2%).
- Floats and weights: −0.2 to −0.6%. 16-bit image: −0.3%.
- Against ultra level 2 the numeric graph cuts gain −1.2 to −1.5%. Level 2 itself already took most of the rest.

## Probe slices (dickens; the 24-Nov cmix/paq8px probe)

| input | -U | Ultra+ | cmix v21 | paq8px -8 |
|---|---:|---:|---:|---:|
| first 200,000 B | 51,151 | 49,419 (−3.4%, bare stream at 7c14df3a) | 47,064 | 46,745 |
| first 1,000,000 B | 218,145 | 211,692 (−3.0%, harness, 38 contexts) | — | 198,643 |

## Leader points (Codex's runs, from fleet4/CODEX_REASSESS.md item 8)

| input | -U | Ultra+ (up3) | paq8px v217 -8L | gap |
|---|---:|---:|---:|---:|
| enwik8[0:262144] (the holdout cut `xml__enwik8`, byte-identical prefix) | 61,966 | 59,528 | 54,648 | paq8px 8.2% smaller |
| full enwik8 | 18,651,472 | (not run) | 16,081,149 | ultra +16.0% (paq8px −13.8% vs ultra) |

Codex's A/A noise prior is ±8% for timings. Sizes are deterministic.

## cmix v21 on the cuts (local, one niced process)

Only two cuts finished: ESC-50 100032 (cmix 14,478 B, Kolmo 13,940 B) and ESC-50 28808 (cmix 153,334 B, Kolmo
148,939 B). Kolmo wins both by 3-4%. Under this load cmix took 1,372 s and 6,499 s per 256 KiB cut, so the run was
stopped on 26 Nov to keep this lane to one process. Comparisons with cmix use the 24-Nov probe slice and the published
full-file figures (PLAN.md).

## Negative results

- **Kolmo's word-dictionary transform (tflag 9) before UPX:** d200k gives 52,704 B against 49,419 B without it, which is
  6.6% larger. Ultra already disables that transform for model 4. A dictionary for Ultra+ would need a cmix-style
  design (capitalisation flags, codes ordered by similarity) and is a separate step.
- **A learned final mixer over the APM outputs (par FM):** the quick dev set gave 97,964 B against 97,857 B, which is
  worse. It stays off (FM = 0).

## Sealed holdout (31 cuts)

One pass with the dev binary (up3, `7c14df3a`), with no change after the dev decision. Ultra+ is never larger than `-U`, which already confirms the never-worse claim.

| cut | bytes | -U | -U level 2 | **Ultra+** | vs -U | vs level 2 | cmix v21 | enc s | dec s | RSS enc/dec MB | frame | RT |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| audio__esc50-3-100018 | 262,144 | 132,173 | 132,173 | **132,173** | +0.00% | +0.00% |  | 1344 | 1 | 561/3 | v12 | OK |
| audio__esc50-5-103415 | 262,144 | 120,606 | 120,606 | **120,606** | +0.00% | +0.00% |  | 1604 | 0 | 586/3 | v12 | OK |
| code__jxl-cc | 262,144 | 37,250 | 37,250 | **35,087** | -5.81% | -5.81% |  | 162 | 151 | 332/305 | v13 | OK |
| code__py-stdlib | 262,144 | 40,208 | 40,208 | **38,166** | -5.08% | -5.08% |  | 161 | 140 | 305/304 | v13 | OK |
| container__bash-pdf | 262,144 | 242,455 | 242,455 | **241,879** | -0.24% | -0.24% |  | 147 | 165 | 346/346 | v13 | OK |
| container__comm-jar | 262,144 | 243,797 | 243,797 | **242,648** | -0.47% | -0.47% |  | 142 | 170 | 345/346 | v13 | OK |
| container__highway-pdf | 262,144 | 163,480 | 163,480 | **161,745** | -1.06% | -1.06% |  | 199 | 190 | 345/347 | v13 | OK |
| csv__backblaze | 262,144 | 19,220 | 19,220 | **18,809** | -2.14% | -2.14% |  | 144 | 174 | 336/337 | v13 | OK |
| csv__gaia | 262,144 | 64,168 | 64,168 | **59,605** | -7.11% | -7.11% |  | 141 | 177 | 303/301 | v13 | OK |
| csv__owid-co2 | 262,144 | 22,627 | 22,627 | **22,627** | +0.00% | +0.00% |  | 133 | 2 | 328/42 | v12 | OK |
| exe__osdb | 262,144 | 69,636 | 69,636 | **66,858** | -3.99% | -3.99% |  | 73 | 82 | 369/357 | v13 | OK |
| exe__wasmtime-arm64 | 262,144 | 99,731 | 99,731 | **90,293** | -9.46% | -9.46% |  | 86 | 74 | 335/338 | v13 | OK |
| genomics__srr2584863 | 262,144 | 59,584 | 59,584 | **58,366** | -2.04% | -2.04% |  | 73 | 53 | 310/335 | v13 | OK |
| html__bashref | 262,144 | 29,347 | 29,347 | **28,077** | -4.33% | -4.33% |  | 76 | 45 | 374/348 | v13 | OK |
| img16__mr | 262,144 | 50,841 | 50,841 | **50,356** | -0.95% | -0.95% |  | 40 | 32 | 585/574 | v13 | OK |
| img8__astronaut | 262,144 | 114,087 | 114,087 | **114,087** | +0.00% | +0.00% |  | 79 | 0 | 353/3 | v12 | OK |
| img8__coffee | 262,144 | 111,544 | 111,544 | **111,544** | +0.00% | +0.00% |  | 28 | 1 | 386/130 | v12 | OK |
| json__gharchive | 262,144 | 28,105 | 28,105 | **26,333** | -6.30% | -6.30% |  | 44 | 50 | 375/351 | v13 | OK |
| log__bgl | 262,144 | 4,362 | 4,362 | **4,342** | -0.46% | -0.46% |  | 3 | 1 | 173/144 | v13 | OK |
| log__nasa | 262,144 | 11,260 | 11,260 | **10,542** | -6.38% | -6.38% |  | 32 | 27 | 375/353 | v13 | OK |
| numeric__ncep-nc | 262,144 | 233,845 | 233,845 | **233,434** | -0.18% | -0.18% |  | 23 | 23 | 367/364 | v13 | OK |
| numeric__sao | 262,144 | 79,108 | 79,108 | **78,653** | -0.58% | -0.58% |  | 12 | 11 | 366/361 | v13 | OK |
| tar__mozilla | 262,144 | 110,361 | 110,361 | **108,424** | -1.76% | -1.76% |  | 23 | 23 | 375/364 | v13 | OK |
| tar__samba | 262,144 | 146,685 | 146,685 | **145,542** | -0.78% | -0.78% |  | 53 | 25 | 374/364 | v13 | OK |
| text__ld30 | 262,144 | 47,254 | 47,254 | **44,397** | -6.05% | -6.05% |  | 25 | 25 | 332/307 | v13 | OK |
| text__reymont | 262,144 | 41,979 | 41,979 | **40,075** | -4.54% | -4.54% |  | 28 | 27 | 377/356 | v13 | OK |
| tsv__bb-rows | 262,144 | 19,432 | 19,432 | **19,080** | -1.81% | -1.81% |  | 24 | 19 | 378/365 | v13 | OK |
| video__foreman | 262,144 | 117,740 | 117,740 | **114,408** | -2.83% | -2.83% |  | 48 | 22 | 380/363 | v13 | OK |
| weights__gpt2-f32 | 262,144 | 213,216 | 213,216 | **211,723** | -0.70% | -0.70% |  | 45 | 23 | 482/364 | v13 | OK |
| weights__smollm2-bf16 | 262,144 | 146,964 | 146,964 | **146,388** | -0.39% | -0.39% |  | 48 | 22 | 509/364 | v13 | OK |
| xml__enwik8 | 262,144 | 61,966 | 61,966 | **59,528** | -3.93% | -3.93% |  | 26 | 23 | 376/362 | v13 | OK |
| **total** | 8,126,464 | 2,883,031 | 2,883,031 | **2,835,795** | -1.64% | -1.64% | | | | | | |

Reading:
- 31 of 31 round trips OK. Smaller than `-U` on 26 cuts and equal on 5: two audio cuts, two 8-bit photos and owid-co2,
  where kaud, kimg or the table path keep their streams.
- The holdout total is −1.64% against −2.36% on dev. The holdout has more containers and weights, where the gain is small.
- The per-type pattern repeats:
  - text −4.5 to −6.1% (xml from enwik8 −3.9%), code −5.1 to −5.8%, JSON −6.3%, logs −0.5 to −6.4%, html −4.3%;
  - x86 −4.0%, ARM64 −9.5%;
  - tables −1.8 to −7.1%;
  - floats, weights and scientific arrays −0.2 to −0.7%.
- New on the holdout: the foreman video cut is −2.8%. Its blocks went through a CM path that UPX now covers.

## Step 2 candidates (bare stream, one niced process)

- **Three binary contexts** (position mod 4 with the last two bytes; x86 prefix/escape bytes with the bytes after them;
  the bytes 4 and 8 back with the position mod 4). Results: exe −0.21% (97,582 → 97,377), owid-f64 −0.57%, bf16 +0.003%,
  d200k +0.05%. **Kept.** Binary streams gain, and text changes by noise.
- **LSTM H=128** on the first 1 MB of dickens: 209,402 B against 210,296 B without it (−0.43%), at 3.2× the coding time
  (204 s against 63 s under load). It stays **off by default** for now. #116 measured −4.85% at 4 MB (reymont), so the
  LSTM gains with size.
- **LSTM H=128 on dev dickens4m (4,194,258 B):** 847,976 B against 852,404 B without it (−0.52%), at 2.3× the time
  (663 s against 289 s under load). For reference, stock `-U` gives 886,241 B, so Ultra+ is −3.82% without the LSTM and
  −4.32% with it. **Default from `6b4c9dc0` on: LSTM on for text streams of 1 MiB and more** (an encoder choice that the
  stream records in its flags). Holdout check queued.
